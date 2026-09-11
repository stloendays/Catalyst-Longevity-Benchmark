#include "UpdateManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QVersionNumber>
#include <QWidget>

namespace {
constexpr auto kReleaseApi = "https://api.github.com/repos/stloendays/Catalyst-Longevity-Benchmark/releases?per_page=20";
constexpr auto kTagPrefix = "tony-v";
constexpr auto kMarkerFile = "tony-install-root.marker";

QNetworkRequest githubRequest(const QUrl &url) {
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "TonyDesktopPet-Updater");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QString shortenedNotes(QString notes) {
    notes = notes.trimmed();
    if(notes.size() > 900) notes = notes.left(900) + QStringLiteral("\n…");
    return notes;
}
}

UpdateManager::UpdateManager(QWidget *parentWidget)
    : QObject(parentWidget), parentWidget_(parentWidget) {
    cleanupOwnedUpdateArtifacts();
}

QString UpdateManager::cacheDirectory() const {
    const QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(root).filePath(QStringLiteral("update-cache"));
}

void UpdateManager::cleanupOwnedUpdateArtifacts() {
    QDir dir(cacheDirectory());
    if(!dir.exists()) return;

    const QStringList patterns{
        QStringLiteral("TonyDesktopPet-v*-windows-x64.zip"),
        QStringLiteral("TonyDesktopPet-Setup-v*.exe"),
        QStringLiteral("*.part"),
        QStringLiteral("*.tmp")
    };
    for(const auto &pattern : patterns) {
        const auto files = dir.entryList({pattern}, QDir::Files);
        for(const auto &file : files) dir.remove(file);
    }

    const QString stagePath = dir.filePath(QStringLiteral("stage"));
    if(QDir(stagePath).exists()) QDir(stagePath).removeRecursively();

    const auto scripts = dir.entryList({QStringLiteral("apply-update-*.ps1")}, QDir::Files);
    for(const auto &script : scripts) dir.remove(script);

    if(dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        QDir().rmdir(dir.absolutePath());
    }
}

void UpdateManager::scheduleAutomaticCheck() {
    QTimer::singleShot(3500, this, [this] {
        QSettings s;
        const auto last = s.value(QStringLiteral("updater/last_check_utc")).toDateTime();
        if(last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < 6 * 60 * 60) return;
        checkForUpdates(false);
    });
}

void UpdateManager::checkForUpdates(bool manual) {
    if(busy_) return;
    busy_ = true;

    auto *nam = new QNetworkAccessManager(this);
    auto *reply = nam->get(githubRequest(QUrl(QString::fromLatin1(kReleaseApi))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, manual] {
        const auto guard = std::unique_ptr<QNetworkReply, QScopedPointerDeleteLater>(reply);
        nam->deleteLater();
        busy_ = false;

        if(reply->error() != QNetworkReply::NoError) {
            if(manual) QMessageBox::warning(parentWidget_, QStringLiteral("Tony Update"),
                                             QStringLiteral("检查更新失败：%1").arg(reply->errorString()));
            return;
        }

        QSettings().setValue(QStringLiteral("updater/last_check_utc"), QDateTime::currentDateTimeUtc());
        handleReleaseList(reply->readAll(), manual);
    });
}

void UpdateManager::handleReleaseList(const QByteArray &data, bool manual) {
    QJsonParseError error{};
    const auto doc = QJsonDocument::fromJson(data, &error);
    if(error.error != QJsonParseError::NoError || !doc.isArray()) {
        if(manual) QMessageBox::warning(parentWidget_, QStringLiteral("Tony Update"),
                                         QStringLiteral("GitHub 更新信息格式无效。"));
        return;
    }

    const QVersionNumber current = QVersionNumber::fromString(QCoreApplication::applicationVersion());
    QVersionNumber bestVersion;
    ReleaseInfo best;

    for(const auto &value : doc.array()) {
        const auto release = value.toObject();
        if(release.value(QStringLiteral("draft")).toBool() || release.value(QStringLiteral("prerelease")).toBool()) continue;
        const QString tag = release.value(QStringLiteral("tag_name")).toString();
        if(!tag.startsWith(QString::fromLatin1(kTagPrefix), Qt::CaseInsensitive)) continue;

        const QString versionText = tag.mid(int(strlen(kTagPrefix)));
        const QVersionNumber version = QVersionNumber::fromString(versionText);
        if(version.isNull() || QVersionNumber::compare(version, current) <= 0 ||
           (!bestVersion.isNull() && QVersionNumber::compare(version, bestVersion) <= 0)) continue;

        QUrl zipUrl;
        QUrl shaUrl;
        const QString zipName = QStringLiteral("TonyDesktopPet-v%1-windows-x64.zip").arg(versionText);
        const QString shaName = zipName + QStringLiteral(".sha256");
        for(const auto &assetValue : release.value(QStringLiteral("assets")).toArray()) {
            const auto asset = assetValue.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
            if(name.compare(zipName, Qt::CaseInsensitive) == 0) zipUrl = url;
            if(name.compare(shaName, Qt::CaseInsensitive) == 0) shaUrl = url;
        }
        if(!zipUrl.isValid() || !shaUrl.isValid()) continue;

        bestVersion = version;
        best.version = versionText;
        best.notes = release.value(QStringLiteral("body")).toString();
        best.zipUrl = zipUrl;
        best.shaUrl = shaUrl;
    }

    if(best.version.isEmpty()) {
        if(manual) QMessageBox::information(parentWidget_, QStringLiteral("Tony Update"),
                                             QStringLiteral("Tony 已经是最新版本。"));
        return;
    }

    fetchChecksumAndPrompt(best, manual);
}

bool UpdateManager::isValidSha256(const QString &value) {
    static const QRegularExpression rx(QStringLiteral("^[0-9a-fA-F]{64}$"));
    return rx.match(value).hasMatch();
}

void UpdateManager::fetchChecksumAndPrompt(const ReleaseInfo &release, bool manual) {
    busy_ = true;
    auto *nam = new QNetworkAccessManager(this);
    auto *reply = nam->get(githubRequest(release.shaUrl));
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, release, manual] {
        nam->deleteLater();
        busy_ = false;
        if(reply->error() != QNetworkReply::NoError) {
            if(manual) QMessageBox::warning(parentWidget_, QStringLiteral("Tony Update"),
                                             QStringLiteral("无法取得更新校验值。"));
            reply->deleteLater();
            return;
        }

        const QString text = QString::fromUtf8(reply->readAll()).trimmed();
        reply->deleteLater();
        const QString sha = text.section(QRegularExpression(QStringLiteral("\\s+")), 0, 0).trimmed();
        if(!isValidSha256(sha)) {
            if(manual) QMessageBox::warning(parentWidget_, QStringLiteral("Tony Update"),
                                             QStringLiteral("更新包缺少有效 SHA-256，已拒绝更新。"));
            return;
        }
        promptAndDownload(release, sha.toLower());
    });
}

void UpdateManager::promptAndDownload(const ReleaseInfo &release, const QString &sha256) {
    QString text = QStringLiteral("发现 Tony v%1。\n\n更新会自动退出并重新启动 Tony。\n旧的 Tony 更新 ZIP、临时解压目录和缓存会在完成后删除。")
                       .arg(release.version);
    const QString notes = shortenedNotes(release.notes);
    if(!notes.isEmpty()) text += QStringLiteral("\n\n更新内容：\n") + notes;

    const auto choice = QMessageBox::question(parentWidget_, QStringLiteral("Tony 有新版本"), text,
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if(choice == QMessageBox::Yes) downloadAndApply(release, sha256);
}

void UpdateManager::downloadAndApply(const ReleaseInfo &release, const QString &sha256) {
    cleanupOwnedUpdateArtifacts();
    QDir().mkpath(cacheDirectory());
    const QString zipPath = QDir(cacheDirectory()).filePath(
        QStringLiteral("TonyDesktopPet-v%1-windows-x64.zip").arg(release.version));

    busy_ = true;
    auto *progress = new QProgressDialog(QStringLiteral("正在下载 Tony v%1…").arg(release.version), QString(), 0, 100, parentWidget_);
    progress->setWindowTitle(QStringLiteral("Tony Update"));
    progress->setCancelButton(nullptr);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->show();

    auto *nam = new QNetworkAccessManager(this);
    auto *reply = nam->get(githubRequest(release.zipUrl));
    connect(reply, &QNetworkReply::downloadProgress, progress,
            [progress](qint64 received, qint64 total) {
                if(total > 0) progress->setValue(int((received * 100) / total));
            });

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, progress, release, sha256, zipPath] {
        nam->deleteLater();
        busy_ = false;
        progress->close();
        progress->deleteLater();

        if(reply->error() != QNetworkReply::NoError) {
            QMessageBox::warning(parentWidget_, QStringLiteral("Tony Update"),
                                 QStringLiteral("更新下载失败：%1").arg(reply->errorString()));
            reply->deleteLater();
            cleanupOwnedUpdateArtifacts();
            return;
        }

        const QByteArray payload = reply->readAll();
        reply->deleteLater();
        const QString actual = QString::fromLatin1(QCryptographicHash::hash(payload, QCryptographicHash::Sha256).toHex());
        if(actual.compare(sha256, Qt::CaseInsensitive) != 0) {
            QMessageBox::critical(parentWidget_, QStringLiteral("Tony Update"),
                                  QStringLiteral("更新包 SHA-256 校验失败，已删除下载内容。"));
            cleanupOwnedUpdateArtifacts();
            return;
        }

        QSaveFile file(zipPath);
        if(!file.open(QIODevice::WriteOnly) || file.write(payload) != payload.size() || !file.commit()) {
            QMessageBox::warning(parentWidget_, QStringLiteral("Tony Update"),
                                 QStringLiteral("无法写入本地更新缓存。"));
            cleanupOwnedUpdateArtifacts();
            return;
        }

        QString error;
        if(!stageApplyScript(zipPath, release.version, &error)) {
            QMessageBox::critical(parentWidget_, QStringLiteral("Tony Update"), error);
            cleanupOwnedUpdateArtifacts();
        }
    });
}

bool UpdateManager::stageApplyScript(const QString &zipPath, const QString &targetVersion, QString *error) {
    const QString installDir = QCoreApplication::applicationDirPath();
    if(!QFileInfo::exists(QDir(installDir).filePath(QString::fromLatin1(kMarkerFile)))) {
        if(error) *error = QStringLiteral("当前 Tony 不是由正式安装包/更新包部署，缺少安装目录标记。为避免误删其他文件，本次自动更新已取消。请使用新的 Tony 安装包覆盖安装一次。 ");
        return false;
    }

    QDir().mkpath(cacheDirectory());
    const QString scriptPath = QDir(cacheDirectory()).filePath(
        QStringLiteral("apply-update-%1.ps1").arg(targetVersion));

    static const char script[] = R"PS1(param(
    [Parameter(Mandatory=$true)][int]$TonyPid,
    [Parameter(Mandatory=$true)][string]$ZipPath,
    [Parameter(Mandatory=$true)][string]$InstallDir,
    [Parameter(Mandatory=$true)][string]$CacheDir,
    [Parameter(Mandatory=$true)][string]$TargetVersion
)
$ErrorActionPreference = 'Stop'
$marker = Join-Path $InstallDir 'tony-install-root.marker'
if (-not (Test-Path -LiteralPath $marker)) { throw 'Tony install marker missing; refusing destructive update.' }

try {
    Wait-Process -Id $TonyPid -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 700

    $stage = Join-Path $CacheDir 'stage'
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    Expand-Archive -LiteralPath $ZipPath -DestinationPath $stage -Force

    if (-not (Test-Path -LiteralPath (Join-Path $stage 'tony-install-root.marker'))) {
        throw 'Downloaded package does not contain the Tony install marker.'
    }

    Get-ChildItem -LiteralPath $InstallDir -Force | ForEach-Object {
        Remove-Item -LiteralPath $_.FullName -Recurse -Force
    }
    Copy-Item -Path (Join-Path $stage '*') -Destination $InstallDir -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $stage 'tony-install-root.marker') -Destination $InstallDir -Force

    Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $ZipPath -Force -ErrorAction SilentlyContinue

    Get-ChildItem -LiteralPath $CacheDir -File -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -like 'TonyDesktopPet-v*-windows-x64.zip' -or
        $_.Name -like 'TonyDesktopPet-Setup-v*.exe' -or
        $_.Name -like '*.part' -or $_.Name -like '*.tmp'
    } | Remove-Item -Force -ErrorAction SilentlyContinue

    $downloads = Join-Path $env:USERPROFILE 'Downloads'
    if (Test-Path -LiteralPath $downloads) {
        Get-ChildItem -LiteralPath $downloads -File -ErrorAction SilentlyContinue | Where-Object {
            ($_.Name -like 'TonyDesktopPet-Setup-v*.exe' -and $_.Name -notlike "*v$TargetVersion*.exe") -or
            ($_.Name -like 'TonyDesktopPet-v*-windows-x64.zip' -and $_.Name -notlike "*v$TargetVersion*.zip")
        } | Remove-Item -Force -ErrorAction SilentlyContinue
    }

    Start-Process -FilePath (Join-Path $InstallDir 'TonyDesktopPet.exe') -WorkingDirectory $InstallDir
}
finally {
    $self = $MyInvocation.MyCommand.Path
    $quotedCache = '"' + $CacheDir + '"'
    $quotedSelf = '"' + $self + '"'
    Start-Process -FilePath 'cmd.exe' -WindowStyle Hidden -ArgumentList '/c', "ping 127.0.0.1 -n 3 >nul & del /f /q $quotedSelf & rmdir /s /q $quotedCache"
}
)PS1";

    QFile scriptFile(scriptPath);
    if(!scriptFile.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
       scriptFile.write(script) != qint64(sizeof(script) - 1)) {
        if(error) *error = QStringLiteral("无法创建本地更新脚本。 ");
        return false;
    }
    scriptFile.close();

    const QStringList args{
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
        QStringLiteral("-File"), scriptPath,
        QStringLiteral("-TonyPid"), QString::number(QCoreApplication::applicationPid()),
        QStringLiteral("-ZipPath"), zipPath,
        QStringLiteral("-InstallDir"), installDir,
        QStringLiteral("-CacheDir"), cacheDirectory(),
        QStringLiteral("-TargetVersion"), targetVersion
    };

    if(!QProcess::startDetached(QStringLiteral("powershell.exe"), args)) {
        if(error) *error = QStringLiteral("无法启动 Tony 本地更新程序。 ");
        return false;
    }

    QTimer::singleShot(150, qApp, [] { QCoreApplication::quit(); });
    return true;
}
