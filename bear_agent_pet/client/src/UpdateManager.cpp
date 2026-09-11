#include "UpdateManager.h"

#include "AppLogger.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrlQuery>
#include <QVersionNumber>

namespace {
QUrl manifestUrl() {
    QUrl url(QStringLiteral(
        "https://github.com/stloendays/Catalyst-Longevity-Benchmark/releases/download/tony-desktop-latest/latest.json"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("t"), QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);
    return url;
}

QNetworkRequest requestFor(const QUrl &url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QByteArray("TonyDesktopPet/") + QCoreApplication::applicationVersion().toUtf8());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    return request;
}

QString updateArchivePath(const QString &version) {
    QString safe = version;
    safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9._-]")), QStringLiteral("_"));
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("TonyDesktopPet-update-%1.zip").arg(safe));
}

void cleanupStaleUpdaterArtifacts() {
    QDir temp(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
    const QStringList patterns{
        QStringLiteral("TonyDesktopPet-update-*.zip"),
        QStringLiteral("TonyDesktopPet-apply-update*.ps1"),
        QStringLiteral("TonyUpdateStage-*")
    };
    for(const auto &pattern : patterns) {
        const auto entries = temp.entryInfoList({pattern}, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for(const auto &entry : entries) {
            if(entry.isDir()) QDir(entry.absoluteFilePath()).removeRecursively();
            else QFile::remove(entry.absoluteFilePath());
        }
    }
}
}

UpdateManager::UpdateManager(QObject *parent) : QObject(parent) {
    cleanupStaleUpdaterArtifacts();
}

bool UpdateManager::automaticUpdatesEnabled() const {
    return QSettings().value(QStringLiteral("update/automatic"), true).toBool();
}

void UpdateManager::setAutomaticUpdatesEnabled(bool enabled) {
    QSettings().setValue(QStringLiteral("update/automatic"), enabled);
    qInfo().noquote() << "Automatic updates" << (enabled ? "enabled" : "disabled");
}

QString UpdateManager::latestVersion() const { return latestVersion_; }

bool UpdateManager::hasDownloadedUpdate() const {
    return !downloadedPath_.isEmpty() && QFile::exists(downloadedPath_);
}

QString UpdateManager::currentVersion() const {
    const QString version = QCoreApplication::applicationVersion().trimmed();
    return version.isEmpty() ? QStringLiteral("0.0.0") : version;
}

bool UpdateManager::isNewerVersion(const QString &candidate) const {
    const QVersionNumber current = QVersionNumber::fromString(currentVersion());
    const QVersionNumber offered = QVersionNumber::fromString(candidate.trimmed());
    if(current.isNull() || offered.isNull()) return candidate.trimmed() != currentVersion();
    return QVersionNumber::compare(offered, current) > 0;
}

void UpdateManager::scheduleStartupCheck() {
    if(!automaticUpdatesEnabled()) return;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDateTime last = QDateTime::fromString(
        QSettings().value(QStringLiteral("update/last_check_utc")).toString(), Qt::ISODate);
    if(last.isValid() && last.secsTo(now) < 12 * 60 * 60) return;
    QTimer::singleShot(10000, this, [this]{ checkNow(false); });
}

void UpdateManager::checkNow(bool userInitiated) {
    if(checking_ || downloading_) return;
    checking_ = true;
    userInitiatedCheck_ = userInitiated;
    emit statusChanged(userInitiated ? QStringLiteral("Checking for updates…")
                                     : QStringLiteral("Checking for updates in the background…"));
    const QUrl manifest = manifestUrl();
    qInfo().noquote() << "Checking Tony update manifest" << manifest.toString();

    auto *reply = network_.get(requestFor(manifest));
    connect(reply, &QNetworkReply::finished, this, [this, reply]{
        checking_ = false;
        const QByteArray raw = reply->readAll();
        if(reply->error() != QNetworkReply::NoError) {
            const QString message = QStringLiteral("Update check failed: %1").arg(reply->errorString());
            qWarning().noquote() << message;
            if(userInitiatedCheck_) emit updateError(message);
            else emit statusChanged(message);
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};
        const int schema = obj.value(QStringLiteral("schema")).toInt(0);
        const QString version = obj.value(QStringLiteral("version")).toString().trimmed();
        const QUrl package(obj.value(QStringLiteral("url")).toString());
        const QByteArray sha = obj.value(QStringLiteral("sha256")).toString().trimmed().toLatin1().toLower();
        const QString platform = obj.value(QStringLiteral("platform")).toString();
        if(schema != 1 || version.isEmpty() || platform != QStringLiteral("windows-x64") ||
           !package.isValid() || package.scheme().toLower() != QStringLiteral("https") || sha.size() != 64) {
            const QString message = QStringLiteral("Update manifest is incomplete or invalid.");
            qWarning().noquote() << message;
            emit updateError(message);
            reply->deleteLater();
            return;
        }

        QSettings().setValue(QStringLiteral("update/last_check_utc"),
                             QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        latestVersion_ = version;
        packageUrl_ = package;
        expectedSha256_ = sha;

        if(isNewerVersion(version)) {
            qInfo().noquote() << "Tony update available" << currentVersion() << "->" << version;
            emit updateFound(version);
            if(automaticUpdatesEnabled()) downloadAvailableUpdate();
        } else {
            emit statusChanged(QStringLiteral("Tony is up to date."));
            emit noUpdateAvailable(currentVersion());
            qInfo().noquote() << "Tony is up to date at" << currentVersion();
        }
        reply->deleteLater();
    });
}

void UpdateManager::downloadAvailableUpdate() {
    if(downloading_ || packageUrl_.isEmpty() || latestVersion_.isEmpty()) return;
    downloading_ = true;
    emit statusChanged(QStringLiteral("Downloading Tony %1…").arg(latestVersion_));
    qInfo().noquote() << "Downloading Tony update" << latestVersion_;

    auto *reply = network_.get(requestFor(packageUrl_));
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateManager::downloadProgress);
    connect(reply, &QNetworkReply::finished, this, [this, reply]{
        downloading_ = false;
        const QByteArray payload = reply->readAll();
        if(reply->error() != QNetworkReply::NoError) {
            const QString message = QStringLiteral("Update download failed: %1").arg(reply->errorString());
            qWarning().noquote() << message;
            emit updateError(message);
            reply->deleteLater();
            return;
        }

        const QByteArray actual = QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex().toLower();
        if(expectedSha256_.isEmpty() || actual != expectedSha256_) {
            const QString message = QStringLiteral("Downloaded update failed SHA-256 verification.");
            qCritical().noquote() << message << "expected" << expectedSha256_ << "actual" << actual;
            emit updateError(message);
            reply->deleteLater();
            return;
        }

        const QString path = updateArchivePath(latestVersion_);
        QSaveFile file(path);
        if(!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size() || !file.commit()) {
            const QString message = QStringLiteral("Could not save the downloaded update.");
            qWarning().noquote() << message << path;
            emit updateError(message);
            reply->deleteLater();
            return;
        }

        downloadedPath_ = path;
        emit statusChanged(QStringLiteral("Tony %1 is verified and ready to install.").arg(latestVersion_));
        emit updateDownloaded(latestVersion_);
        qInfo().noquote() << "Verified Tony update ready" << latestVersion_ << downloadedPath_;
        reply->deleteLater();

        if(automaticUpdatesEnabled() && !userInitiatedCheck_) {
            emit statusChanged(QStringLiteral("Installing the verified update automatically…"));
            QTimer::singleShot(1500, this, [this]{ applyDownloadedUpdate(); });
        }
    });
}

void UpdateManager::applyDownloadedUpdate() {
    if(!hasDownloadedUpdate()) {
        emit updateError(QStringLiteral("No downloaded update is ready to install."));
        return;
    }

#ifdef Q_OS_WIN
    const QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString scriptPath = QDir(tempDir).filePath(QStringLiteral("TonyDesktopPet-apply-update.ps1"));
    const QString logPath = QDir(AppLogger::logDirectory()).filePath(QStringLiteral("tony-update.log"));

    const QByteArray script = R"PS1(param(
    [int]$ProcessId,
    [string]$Archive,
    [string]$Destination,
    [string]$Executable,
    [string]$LogPath,
    [string]$TargetVersion
)
$ErrorActionPreference = 'Stop'
function Write-UpdateLog([string]$Message) {
    $dir = Split-Path -Parent $LogPath
    if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    Add-Content -LiteralPath $LogPath -Value ("{0:o} {1}" -f (Get-Date), $Message)
}
function Get-SafeManagedPath([string]$Root, [string]$Relative) {
    if ([string]::IsNullOrWhiteSpace($Relative) -or [IO.Path]::IsPathRooted($Relative)) { return $null }
    $rootFull = [IO.Path]::GetFullPath($Root).TrimEnd('\') + '\'
    $candidate = [IO.Path]::GetFullPath((Join-Path $Root $Relative.Trim()))
    if (-not $candidate.StartsWith($rootFull, [StringComparison]::OrdinalIgnoreCase)) { return $null }
    return $candidate
}
try {
    Write-UpdateLog "Waiting for Tony process $ProcessId to exit"
    Wait-Process -Id $ProcessId -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500

    $stage = Join-Path $env:TEMP ("TonyUpdateStage-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Force -Path $stage | Out-Null
    Expand-Archive -LiteralPath $Archive -DestinationPath $stage -Force

    $newMarker = Join-Path $stage 'tony-install-root.marker'
    $newManaged = Join-Path $stage 'tony-file-list.txt'
    if (-not (Test-Path -LiteralPath $newMarker) -or -not (Test-Path -LiteralPath $newManaged)) {
        throw 'Verified update package is missing Tony managed-install metadata.'
    }

    $oldManaged = Join-Path $Destination 'tony-file-list.txt'
    if (Test-Path -LiteralPath $oldManaged) {
        Write-UpdateLog 'Removing files owned by the previous Tony package'
        Get-Content -LiteralPath $oldManaged -ErrorAction Stop | ForEach-Object {
            $oldPath = Get-SafeManagedPath $Destination $_
            if ($oldPath -and (Test-Path -LiteralPath $oldPath -PathType Leaf)) {
                Remove-Item -LiteralPath $oldPath -Force -ErrorAction SilentlyContinue
            }
        }
    } else {
        Write-UpdateLog 'Legacy package has no managed-file list; using one-time overlay migration'
    }

    Write-UpdateLog "Copying verified Tony $TargetVersion into $Destination"
    & robocopy $stage $Destination /E /R:3 /W:1 /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy failed with exit code $LASTEXITCODE" }

    Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $Archive -Force -ErrorAction SilentlyContinue

    Get-ChildItem -LiteralPath $env:TEMP -File -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -like 'TonyDesktopPet-update-*.zip'
    } | Remove-Item -Force -ErrorAction SilentlyContinue

    # Remove only Tony packages with the exact official naming patterns, and keep
    # the current target version. Unrelated downloads are never touched.
    $downloads = Join-Path $env:USERPROFILE 'Downloads'
    if (Test-Path -LiteralPath $downloads) {
        Get-ChildItem -LiteralPath $downloads -File -ErrorAction SilentlyContinue | ForEach-Object {
            $name = $_.Name
            $oldVersion = $null
            if ($name -match '^TonyDesktopPet-Setup-v(?<v>\d+\.\d+\.\d+)\.exe$') {
                $oldVersion = $Matches['v']
            } elseif ($name -match '^TonyDesktopPet-(?:v)?(?<v>\d+\.\d+\.\d+)-windows-x64\.zip$') {
                $oldVersion = $Matches['v']
            }
            if ($oldVersion -and $oldVersion -ne $TargetVersion) {
                Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue
                Write-UpdateLog "Removed old Tony package $name"
            }
        }
    }

    Write-UpdateLog "Update installed; restarting Tony $TargetVersion"
    Start-Process -FilePath (Join-Path $Destination $Executable) -WorkingDirectory $Destination
    Remove-Item -LiteralPath $PSCommandPath -Force -ErrorAction SilentlyContinue
} catch {
    Write-UpdateLog ("Update failed: " + $_.Exception.Message)
    exit 1
}
)PS1";

    QSaveFile scriptFile(scriptPath);
    if(!scriptFile.open(QIODevice::WriteOnly) || scriptFile.write(script) != script.size() || !scriptFile.commit()) {
        emit updateError(QStringLiteral("Could not prepare the updater helper."));
        return;
    }

    const QStringList args{
        QStringLiteral("-NoProfile"),
        QStringLiteral("-NonInteractive"),
        QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
        QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
        QStringLiteral("-File"), scriptPath,
        QStringLiteral("-ProcessId"), QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("-Archive"), downloadedPath_,
        QStringLiteral("-Destination"), QCoreApplication::applicationDirPath(),
        QStringLiteral("-Executable"), QFileInfo(QCoreApplication::applicationFilePath()).fileName(),
        QStringLiteral("-LogPath"), logPath,
        QStringLiteral("-TargetVersion"), latestVersion_
    };

    qInfo().noquote() << "Handing Tony update to detached PowerShell updater" << latestVersion_;
    if(!QProcess::startDetached(QStringLiteral("powershell.exe"), args)) {
        emit updateError(QStringLiteral("Could not start the Windows updater helper."));
        return;
    }
    QCoreApplication::quit();
#else
    emit updateError(QStringLiteral("Automatic installation is currently supported on Windows only."));
#endif
}
