#include "PetWindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>

namespace {

QString canonicalChild(const QString &root, const QString &relative, bool directory=false) {
    if(relative.trimmed().isEmpty() || QDir::isAbsolutePath(relative)) return {};
    const QString clean = QDir::cleanPath(QDir::fromNativeSeparators(relative.trimmed()));
    if(clean == QStringLiteral("..") || clean.startsWith(QStringLiteral("../"))) return {};

    const QString canonicalRoot = QDir::fromNativeSeparators(QFileInfo(root).canonicalFilePath());
    if(canonicalRoot.isEmpty()) return {};
    const QFileInfo childInfo(QDir(canonicalRoot).filePath(clean));
    const QString canonical = QDir::fromNativeSeparators(childInfo.canonicalFilePath());
    if(canonical.isEmpty()) return {};

#ifdef Q_OS_WIN
    constexpr auto cs = Qt::CaseInsensitive;
#else
    constexpr auto cs = Qt::CaseSensitive;
#endif
    const QString prefix = canonicalRoot.endsWith(QLatin1Char('/'))
        ? canonicalRoot
        : canonicalRoot + QLatin1Char('/');
    if(canonical.compare(canonicalRoot, cs) != 0 && !canonical.startsWith(prefix, cs)) return {};
    if(directory ? !childInfo.isDir() : !childInfo.isFile()) return {};
    return canonical;
}

QString configuredPetRoot() {
    const QString envRoot = qEnvironmentVariable("TONY_PET_ROOT").trimmed();
    if(!envRoot.isEmpty()) return QDir(envRoot).absolutePath();
    const QString saved = QSettings().value(QStringLiteral("pet/asset_root")).toString().trimmed();
    return saved.isEmpty() ? QString{} : QDir(saved).absolutePath();
}

void setError(QString *error, const QString &message) {
    if(error) *error = message;
}

} // namespace

bool PetWindow::applyActivePetPackage(QString *error) {
    if(error) error->clear();
    const QString root = configuredPetRoot();
    if(root.isEmpty()) return true; // Built-in Tony assets already loaded by the constructor.

    const QString builtInRoot = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("assets"));
    const QString canonicalRoot = QDir::fromNativeSeparators(QFileInfo(root).canonicalFilePath());
    const QString canonicalBuiltIn = QDir::fromNativeSeparators(QFileInfo(builtInRoot).canonicalFilePath());
    if(!canonicalRoot.isEmpty() && canonicalRoot == canonicalBuiltIn) return true;

    const QString manifestPath = canonicalChild(root, QStringLiteral("pet.json"));
    if(manifestPath.isEmpty()) {
        setError(error, QStringLiteral("The selected pet folder does not contain a valid pet.json."));
        return false;
    }

    QFile manifestFile(manifestPath);
    if(!manifestFile.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Tony could not read the selected pet.json."));
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(manifestFile.readAll());
    if(!doc.isObject()) {
        setError(error, QStringLiteral("The selected pet.json is not a JSON object."));
        return false;
    }

    const QJsonObject manifest = doc.object();
    if(manifest.value(QStringLiteral("schema")).toInt() != 1) {
        setError(error, QStringLiteral("This pet package uses an unsupported schema version."));
        return false;
    }
    const QString petId = manifest.value(QStringLiteral("id")).toString().trimmed();
    const QString petName = manifest.value(QStringLiteral("name")).toString().trimmed();
    const QString petVersion = manifest.value(QStringLiteral("version")).toString().trimmed();
    const QJsonObject rights = manifest.value(QStringLiteral("rights")).toObject();
    const QJsonObject poses = manifest.value(QStringLiteral("poses")).toObject();
    if(petId.isEmpty() || petName.isEmpty() || petVersion.isEmpty() ||
       rights.value(QStringLiteral("confirmed")).toBool(false) != true ||
       poses.isEmpty() || !poses.contains(QStringLiteral("idle"))) {
        setError(error, QStringLiteral("The pet package is missing identity, rights confirmation, or its idle pose."));
        return false;
    }

    QHash<QString,QPixmap> candidateStates;
    QHash<QString,QVector<QPixmap>> candidateAnimations;

    for(auto it = poses.constBegin(); it != poses.constEnd(); ++it) {
        if(!it.value().isObject()) continue;
        const QString key = it.key().trimmed();
        if(key.isEmpty()) continue;
        const QJsonObject pose = it.value().toObject();

        const QString stateRelative = pose.value(QStringLiteral("state")).toString();
        if(!stateRelative.isEmpty()) {
            const QString statePath = canonicalChild(root, stateRelative);
            QPixmap sprite;
            if(!statePath.isEmpty() && sprite.load(statePath))
                candidateStates.insert(key, sprite);
        }

        const QString animationRelative = pose.value(QStringLiteral("animation_dir")).toString();
        if(!animationRelative.isEmpty()) {
            const QString animationPath = canonicalChild(root, animationRelative, true);
            if(!animationPath.isEmpty()) {
                QDir dir(animationPath);
                const QStringList filters{
                    QStringLiteral("frame_*.png"), QStringLiteral("frame_*.webp"),
                    QStringLiteral("frame_*.ppm"), QStringLiteral("frame_*.pgm")
                };
                const QStringList files = dir.entryList(filters, QDir::Files, QDir::Name);
                QVector<QPixmap> frames;
                frames.reserve(files.size());
                for(const auto &file : files) {
                    QPixmap frame;
                    if(frame.load(dir.filePath(file))) frames.push_back(frame);
                }
                if(frames.size() >= 2) candidateAnimations.insert(key, frames);
            }
        }
    }

    const auto idleIt = candidateStates.constFind(QStringLiteral("idle"));
    if(idleIt == candidateStates.constEnd() || idleIt.value().isNull()) {
        setError(error, QStringLiteral("The selected pet needs a loadable poses.idle.state image."));
        return false;
    }

    stateAssets_ = candidateStates;
    animationAssets_ = candidateAnimations;
    pet_ = idleIt.value();

    const QJsonObject presentation = manifest.value(QStringLiteral("presentation")).toObject();
    const int width = qBound(48, presentation.value(QStringLiteral("default_width")).toInt(280), 2048);
    const int height = qBound(48, presentation.value(QStringLiteral("default_height")).toInt(250), 2048);
    setFixedSize(width, height);
    setWindowTitle(petName);
    if(tray_.isVisible()) tray_.setToolTip(QStringLiteral("%1 · Desktop Agent").arg(petName));

    QSettings settings;
    settings.setValue(QStringLiteral("pet/active_id"), petId);
    settings.setValue(QStringLiteral("pet/active_name"), petName);
    settings.setValue(QStringLiteral("pet/active_version"), petVersion);
    qInfo().noquote() << "Activated custom Tony Pet package" << petId << petVersion << canonicalRoot;
    update();
    return true;
}
