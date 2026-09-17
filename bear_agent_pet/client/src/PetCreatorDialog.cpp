#include "PetCreatorDialog.h"

#include "AppLogger.h"
#include "PetWindow.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {
bool creatorUiChinese() {
    return QSettings().value(QStringLiteral("ui/language"), QStringLiteral("en"))
        .toString().startsWith(QStringLiteral("zh"), Qt::CaseInsensitive);
}

bool pathInsideRoot(const QString &root, const QString &candidate) {
    const QString rootPath = QDir::fromNativeSeparators(QFileInfo(root).canonicalFilePath());
    const QString childPath = QDir::fromNativeSeparators(QFileInfo(candidate).canonicalFilePath());
    if(rootPath.isEmpty() || childPath.isEmpty()) return false;
#ifdef Q_OS_WIN
    constexpr auto cs = Qt::CaseInsensitive;
#else
    constexpr auto cs = Qt::CaseSensitive;
#endif
    const QString prefix = rootPath.endsWith(QLatin1Char('/')) ? rootPath : rootPath + QLatin1Char('/');
    return childPath.compare(rootPath, cs) == 0 || childPath.startsWith(prefix, cs);
}
}

PetCreatorDialog::PetCreatorDialog(PetWindow *pet, QWidget *parent)
    : QDialog(parent), pet_(pet) {
    const bool zh = creatorUiChinese();
    setWindowTitle(zh ? QStringLiteral("宠物与创作") : QStringLiteral("Pets & Creator"));
    setModal(false);
    resize(560, 520);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 22, 22, 22);
    layout->setSpacing(14);

    auto *title = new QLabel(zh ? QStringLiteral("创建你自己的 Agent Pet")
                                : QStringLiteral("Create your own Agent Pet"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    title->setFont(titleFont);
    layout->addWidget(title);

    auto *intro = new QLabel(
        zh ? QStringLiteral("选择一个包含 pet.json 和透明宠物素材的文件夹。Tony 会先验证，再在当前桌面上预览。AI、记忆和工具运行时仍由 Tony Runtime 提供。")
           : QStringLiteral("Choose a folder containing pet.json and transparent pet artwork. Tony validates it before previewing it on the desktop. AI, memory, and tools remain provided by Tony Runtime."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *form = new QFormLayout;
    activeValue_ = new QLabel(this);
    activeValue_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(zh ? QStringLiteral("当前宠物") : QStringLiteral("Active pet"), activeValue_);

    pathValue_ = new QLineEdit(this);
    pathValue_->setReadOnly(true);
    pathValue_->setPlaceholderText(zh ? QStringLiteral("内置 Tony") : QStringLiteral("Built-in Tony"));
    form->addRow(zh ? QStringLiteral("创作目录") : QStringLiteral("Creator folder"), pathValue_);
    layout->addLayout(form);

    preview_ = new QLabel(this);
    preview_->setMinimumHeight(190);
    preview_->setAlignment(Qt::AlignCenter);
    preview_->setFrameShape(QFrame::StyledPanel);
    preview_->setText(zh ? QStringLiteral("Tony 是默认参考宠物") : QStringLiteral("Tony is the default reference pet"));
    layout->addWidget(preview_, 1);

    status_ = new QLabel(this);
    status_->setWordWrap(true);
    layout->addWidget(status_);

    auto *actions = new QHBoxLayout;
    activateButton_ = new QPushButton(zh ? QStringLiteral("选择文件夹并预览…")
                                         : QStringLiteral("Choose Folder & Preview…"), this);
    auto *resetButton = new QPushButton(zh ? QStringLiteral("恢复 Tony") : QStringLiteral("Restore Tony"), this);
    actions->addWidget(activateButton_);
    actions->addWidget(resetButton);
    actions->addStretch(1);
    layout->addLayout(actions);

    auto *hint = new QLabel(
        zh ? QStringLiteral("最小要求：pet.json、poses.idle.state，以及你拥有使用权的图片素材。打包分享前可使用 tools/tonypet.py 生成 .tonypet 文件。")
           : QStringLiteral("Minimum: pet.json, poses.idle.state, and artwork you have the right to use. Before sharing, tools/tonypet.py can package the folder as a .tonypet file."),
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::hide);
    layout->addWidget(buttons);

    connect(activateButton_, &QPushButton::clicked, this, &PetCreatorDialog::browseAndActivate);
    connect(resetButton, &QPushButton::clicked, this, &PetCreatorDialog::resetToTony);
    refresh();
}

bool PetCreatorDialog::validateFolder(const QString &root, QString *name, QString *version,
                                      QString *idlePath, QString *error) const {
    if(name) name->clear();
    if(version) version->clear();
    if(idlePath) idlePath->clear();
    if(error) error->clear();

    const QFileInfo rootInfo(root);
    if(!rootInfo.exists() || !rootInfo.isDir()) {
        if(error) *error = QStringLiteral("The selected path is not a folder.");
        return false;
    }

    const QString manifestPath = QDir(root).filePath(QStringLiteral("pet.json"));
    QFile file(manifestPath);
    if(!file.open(QIODevice::ReadOnly)) {
        if(error) *error = QStringLiteral("pet.json is missing or cannot be read.");
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if(!doc.isObject()) {
        if(error) *error = QStringLiteral("pet.json must contain a JSON object.");
        return false;
    }

    const QJsonObject manifest = doc.object();
    const QJsonObject rights = manifest.value(QStringLiteral("rights")).toObject();
    const QJsonObject idle = manifest.value(QStringLiteral("poses")).toObject()
        .value(QStringLiteral("idle")).toObject();
    const QString petName = manifest.value(QStringLiteral("name")).toString().trimmed();
    const QString petVersion = manifest.value(QStringLiteral("version")).toString().trimmed();
    const QString idleRelative = idle.value(QStringLiteral("state")).toString().trimmed();

    if(manifest.value(QStringLiteral("schema")).toInt() != 1 || petName.isEmpty() || petVersion.isEmpty()) {
        if(error) *error = QStringLiteral("pet.json needs schema=1, name, and version.");
        return false;
    }
    if(!rights.value(QStringLiteral("confirmed")).toBool(false)) {
        if(error) *error = QStringLiteral("Confirm rights.confirmed=true before using creator artwork.");
        return false;
    }
    if(idleRelative.isEmpty() || QDir::isAbsolutePath(idleRelative) || idleRelative.contains(QStringLiteral(".."))) {
        if(error) *error = QStringLiteral("poses.idle.state must be a safe relative image path.");
        return false;
    }

    const QString candidate = QDir(root).filePath(QDir::cleanPath(idleRelative));
    const QFileInfo idleInfo(candidate);
    if(!idleInfo.isFile() || !pathInsideRoot(root, candidate)) {
        if(error) *error = QStringLiteral("The idle artwork is missing or outside the pet folder.");
        return false;
    }
    QPixmap test(candidate);
    if(test.isNull()) {
        if(error) *error = QStringLiteral("The idle artwork could not be decoded as an image.");
        return false;
    }

    if(name) *name = petName;
    if(version) *version = petVersion;
    if(idlePath) *idlePath = idleInfo.canonicalFilePath();
    return true;
}

void PetCreatorDialog::updatePreview(const QString &root) {
    const bool zh = creatorUiChinese();
    if(root.trimmed().isEmpty()) {
        preview_->setPixmap({});
        preview_->setText(zh ? QStringLiteral("Tony 是默认参考宠物") : QStringLiteral("Tony is the default reference pet"));
        return;
    }

    QString name;
    QString version;
    QString idlePath;
    QString error;
    if(!validateFolder(root, &name, &version, &idlePath, &error)) {
        preview_->setPixmap({});
        preview_->setText(error);
        return;
    }

    QPixmap art(idlePath);
    preview_->setText({});
    preview_->setPixmap(art.scaled(190, 190, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void PetCreatorDialog::refresh() {
    const bool zh = creatorUiChinese();
    QSettings settings;
    const QString root = settings.value(QStringLiteral("pet/asset_root")).toString().trimmed();
    const QString name = settings.value(QStringLiteral("pet/active_name"), QStringLiteral("Tony")).toString();
    const QString version = settings.value(QStringLiteral("pet/active_version")).toString();

    if(root.isEmpty()) {
        activeValue_->setText(QStringLiteral("Tony · built-in reference pet"));
        pathValue_->clear();
        status_->setText(zh ? QStringLiteral("使用内置 Tony。选择一个创作目录即可即时预览其他宠物。")
                            : QStringLiteral("Using built-in Tony. Choose a creator folder to preview another pet instantly."));
    } else {
        activeValue_->setText(version.isEmpty() ? name : QStringLiteral("%1 · v%2").arg(name, version));
        pathValue_->setText(QDir::toNativeSeparators(root));
        status_->setText(zh ? QStringLiteral("这个目录会保留为当前宠物，下一次启动继续使用。")
                            : QStringLiteral("This folder remains the active pet and will be reused on the next start."));
    }
    updatePreview(root);
}

void PetCreatorDialog::browseAndActivate() {
    const bool zh = creatorUiChinese();
    QSettings settings;
    const QString start = settings.value(QStringLiteral("pet/asset_root"), QDir::homePath()).toString();
    const QString root = QFileDialog::getExistingDirectory(
        this,
        zh ? QStringLiteral("选择宠物创作目录") : QStringLiteral("Choose Pet Creator Folder"),
        start);
    if(root.isEmpty()) return;

    QString name;
    QString version;
    QString idlePath;
    QString validationError;
    if(!validateFolder(root, &name, &version, &idlePath, &validationError)) {
        QMessageBox::warning(this,
                             zh ? QStringLiteral("宠物无法加载") : QStringLiteral("Pet could not be loaded"),
                             validationError);
        return;
    }

    const QString previous = settings.value(QStringLiteral("pet/asset_root")).toString();
    settings.setValue(QStringLiteral("pet/asset_root"), QFileInfo(root).canonicalFilePath());
    QString runtimeError;
    if(!pet_ || !pet_->applyActivePetPackage(&runtimeError)) {
        if(previous.isEmpty()) settings.remove(QStringLiteral("pet/asset_root"));
        else settings.setValue(QStringLiteral("pet/asset_root"), previous);
        QMessageBox::warning(this,
                             zh ? QStringLiteral("运行时验证失败") : QStringLiteral("Runtime validation failed"),
                             runtimeError.isEmpty() ? QStringLiteral("The pet package could not be activated.") : runtimeError);
        return;
    }

    AppLogger::recordOperatorEvent(
        QStringLiteral("creator_pet_activated"), {},
        QJsonObject{{QStringLiteral("pet_name"), name.left(80)},
                    {QStringLiteral("pet_version"), version.left(40)}});
    refresh();
}

void PetCreatorDialog::resetToTony() {
    if(pet_) pet_->resetToBuiltInPet();
    AppLogger::recordOperatorEvent(QStringLiteral("creator_pet_reset_to_tony"));
    refresh();
}
