#include "SettingsDialog.h"

#include "AppLogger.h"
#include "UpdateManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTabWidget>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(UpdateManager *updater, QWidget *parent)
    : QDialog(parent), updater_(updater) {
    setWindowTitle(trUi("Tony Settings", "Tony 设置"));
    setMinimumSize(680, 500);
    resize(720, 540);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(18, 18, 18, 14);
    root->setSpacing(12);

    auto *tabs = new QTabWidget(this);
    root->addWidget(tabs, 1);

    auto *general = new QWidget(tabs);
    auto *generalLayout = new QVBoxLayout(general);
    generalLayout->setContentsMargins(18, 18, 18, 18);
    generalLayout->setSpacing(16);

    auto *identity = new QGroupBox(trUi("General", "常规"), general);
    auto *identityForm = new QFormLayout(identity);
    languageBox_ = new QComboBox(identity);
    languageBox_->addItem(QStringLiteral("English"), QStringLiteral("en"));
    languageBox_->addItem(QStringLiteral("简体中文"), QStringLiteral("zh"));
    const QString language = QSettings().value(QStringLiteral("ui/language"), QStringLiteral("en")).toString();
    languageBox_->setCurrentIndex(language.startsWith(QStringLiteral("zh"), Qt::CaseInsensitive) ? 1 : 0);
    identityForm->addRow(trUi("Language", "语言"), languageBox_);
    identityForm->addRow(trUi("Version", "版本"), new QLabel(QCoreApplication::applicationVersion(), identity));
    generalLayout->addWidget(identity);

    auto *connection = new QGroupBox(trUi("Connection", "连接"), general);
    auto *connectionLayout = new QVBoxLayout(connection);
    auto *connectionText = new QLabel(
        trUi("Tony reconnects automatically with the device token stored on this computer. You normally do not need to sign in again.",
             "Tony 会使用保存在这台电脑上的设备令牌自动重连，通常不需要再次登录。"), connection);
    connectionText->setWordWrap(true);
    connectionLayout->addWidget(connectionText);
    auto *reconnectButton = new QPushButton(trUi("Pair / reconnect this computer…", "配对 / 重新连接这台电脑…"), connection);
    connectionLayout->addWidget(reconnectButton, 0, Qt::AlignLeft);
    generalLayout->addWidget(connection);
    generalLayout->addStretch(1);
    tabs->addTab(general, trUi("General", "常规"));

    connect(languageBox_, &QComboBox::currentIndexChanged, this, [this](int index){
        const QString code = languageBox_->itemData(index).toString();
        QSettings().setValue(QStringLiteral("ui/language"), code);
        emit languageChanged(code);
    });
    connect(reconnectButton, &QPushButton::clicked, this, &SettingsDialog::reconnectRequested);

    auto *updates = new QWidget(tabs);
    auto *updatesLayout = new QVBoxLayout(updates);
    updatesLayout->setContentsMargins(18, 18, 18, 18);
    updatesLayout->setSpacing(14);

    automaticUpdates_ = new QCheckBox(
        trUi("Automatically check for and download Tony updates", "自动检查并下载 Tony 更新"), updates);
    automaticUpdates_->setChecked(updater_ && updater_->automaticUpdatesEnabled());
    updatesLayout->addWidget(automaticUpdates_);

    auto *updateHint = new QLabel(
        trUi("When enabled, Tony checks at most twice per day. Update packages are verified with SHA-256 before installation. A verified update can then restart Tony and install itself with one click.",
             "开启后，Tony 最多每天检查两次更新。更新包会先进行 SHA-256 完整性校验，校验通过后可一键重启并自动安装。"), updates);
    updateHint->setWordWrap(true);
    updatesLayout->addWidget(updateHint);

    auto *versions = new QFormLayout;
    currentVersion_ = new QLabel(QCoreApplication::applicationVersion(), updates);
    latestVersion_ = new QLabel(trUi("Not checked yet", "尚未检查"), updates);
    versions->addRow(trUi("Current version", "当前版本"), currentVersion_);
    versions->addRow(trUi("Latest version", "最新版本"), latestVersion_);
    updatesLayout->addLayout(versions);

    updateStatus_ = new QLabel(trUi("Ready", "就绪"), updates);
    updateStatus_->setWordWrap(true);
    updatesLayout->addWidget(updateStatus_);

    progress_ = new QProgressBar(updates);
    progress_->setRange(0, 100);
    progress_->setValue(0);
    progress_->setVisible(false);
    updatesLayout->addWidget(progress_);

    auto *updateButtons = new QHBoxLayout;
    checkNow_ = new QPushButton(trUi("Check now", "立即检查"), updates);
    installNow_ = new QPushButton(trUi("Download and install", "下载并安装"), updates);
    installNow_->setEnabled(false);
    updateButtons->addWidget(checkNow_);
    updateButtons->addWidget(installNow_);
    updateButtons->addStretch(1);
    updatesLayout->addLayout(updateButtons);
    updatesLayout->addStretch(1);
    tabs->addTab(updates, trUi("Automatic Updates", "自动更新"));

    if(updater_) {
        connect(automaticUpdates_, &QCheckBox::toggled, updater_, &UpdateManager::setAutomaticUpdatesEnabled);
        connect(checkNow_, &QPushButton::clicked, this, [this]{
            progress_->setVisible(false);
            installNow_->setEnabled(false);
            updater_->checkNow(true);
        });
        connect(installNow_, &QPushButton::clicked, this, [this]{
            if(updater_->hasDownloadedUpdate()) updater_->applyDownloadedUpdate();
            else updater_->downloadAvailableUpdate();
        });
        connect(updater_, &UpdateManager::statusChanged, this, [this](const QString &text){
            updateStatus_->setText(text);
        });
        connect(updater_, &UpdateManager::updateFound, this, [this](const QString &version){
            latestVersion_->setText(version);
            installNow_->setEnabled(true);
            installNow_->setText(trUi("Download and install", "下载并安装"));
            updateStatus_->setText(trUi("A newer version is available.", "发现新版本。"));
        });
        connect(updater_, &UpdateManager::noUpdateAvailable, this, [this](const QString &version){
            latestVersion_->setText(version);
            installNow_->setEnabled(false);
            progress_->setVisible(false);
            updateStatus_->setText(trUi("Tony is already up to date.", "Tony 已经是最新版本。"));
        });
        connect(updater_, &UpdateManager::downloadProgress, this, [this](qint64 received, qint64 total){
            progress_->setVisible(true);
            if(total > 0) {
                progress_->setRange(0, 100);
                progress_->setValue(static_cast<int>((received * 100) / total));
            } else {
                progress_->setRange(0, 0);
            }
        });
        connect(updater_, &UpdateManager::updateDownloaded, this, [this](const QString &version){
            latestVersion_->setText(version);
            progress_->setVisible(false);
            installNow_->setEnabled(true);
            installNow_->setText(trUi("Restart and install now", "立即重启并安装"));
            updateStatus_->setText(trUi("Update verified and ready to install.", "更新已校验，可以安装。"));
        });
        connect(updater_, &UpdateManager::updateError, this, [this](const QString &message){
            progress_->setVisible(false);
            updateStatus_->setText(message);
        });
    }

    auto *logsTab = new QWidget(tabs);
    auto *logsLayout = new QVBoxLayout(logsTab);
    logsLayout->setContentsMargins(18, 18, 18, 18);
    logsLayout->setSpacing(10);

    auto *pathLabel = new QLabel(trUi("Log folder: ", "日志目录：") + AppLogger::logDirectory(), logsTab);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLabel->setWordWrap(true);
    logsLayout->addWidget(pathLabel);

    logs_ = new QTextEdit(logsTab);
    logs_->setReadOnly(true);
    logs_->setLineWrapMode(QTextEdit::NoWrap);
    QFont mono = logs_->font();
    mono.setFamily(QStringLiteral("Consolas"));
    mono.setPointSize(9);
    logs_->setFont(mono);
    logsLayout->addWidget(logs_, 1);

    auto *logButtons = new QHBoxLayout;
    auto *refresh = new QPushButton(trUi("Refresh", "刷新"), logsTab);
    auto *openFolder = new QPushButton(trUi("Open log folder", "打开日志目录"), logsTab);
    auto *clear = new QPushButton(trUi("Clear logs", "清空日志"), logsTab);
    logButtons->addWidget(refresh);
    logButtons->addWidget(openFolder);
    logButtons->addWidget(clear);
    logButtons->addStretch(1);
    logsLayout->addLayout(logButtons);
    tabs->addTab(logsTab, trUi("Logs", "日志"));

    connect(refresh, &QPushButton::clicked, this, &SettingsDialog::refreshLogs);
    connect(openFolder, &QPushButton::clicked, this, []{
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppLogger::logDirectory()));
    });
    connect(clear, &QPushButton::clicked, this, [this]{
        if(QMessageBox::question(this, trUi("Clear logs", "清空日志"),
                                 trUi("Clear Tony's local application and updater logs?", "清空 Tony 的本地应用日志和更新日志？"))
           == QMessageBox::Yes) {
            AppLogger::clear();
            refreshLogs();
        }
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
    root->addWidget(buttons);

    refreshLogs();
    refreshUpdateSummary();
}

QString SettingsDialog::trUi(const QString &en, const QString &zh) const {
    const QString language = QSettings().value(QStringLiteral("ui/language"), QStringLiteral("en")).toString();
    return language.startsWith(QStringLiteral("zh"), Qt::CaseInsensitive) ? zh : en;
}

void SettingsDialog::refreshLogs() {
    if(!logs_) return;
    QString text = AppLogger::readRecent();
    if(text.isEmpty()) text = trUi("No logs yet.", "暂时没有日志。 ");
    logs_->setPlainText(text);
    logs_->verticalScrollBar()->setValue(logs_->verticalScrollBar()->maximum());
}

void SettingsDialog::refreshUpdateSummary() {
    if(!updater_) return;
    if(!updater_->latestVersion().isEmpty()) latestVersion_->setText(updater_->latestVersion());
    if(updater_->hasDownloadedUpdate()) {
        installNow_->setEnabled(true);
        installNow_->setText(trUi("Restart and install now", "立即重启并安装"));
        updateStatus_->setText(trUi("A verified update is ready to install.", "已有校验完成的更新，可以安装。"));
    }
}
