#include "SettingsDialog.h"

#include "AppLogger.h"
#include "UpdateManager.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {
QString unprotectSettingsSecret(const QString &stored) {
    if(stored.isEmpty()) return {};
#ifdef Q_OS_WIN
    if(stored.startsWith(QStringLiteral("dpapi:"))) {
        const QByteArray encrypted = QByteArray::fromBase64(stored.mid(6).toLatin1());
        DATA_BLOB input{};
        input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.constData()));
        input.cbData = static_cast<DWORD>(encrypted.size());
        DATA_BLOB output{};
        if(CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                              CRYPTPROTECT_UI_FORBIDDEN, &output)) {
            const QByteArray plain(reinterpret_cast<const char*>(output.pbData),
                                   static_cast<int>(output.cbData));
            LocalFree(output.pbData);
            return QString::fromUtf8(plain);
        }
        return {};
    }
#endif
    return stored;
}

QUrl ownerApiUrl(const QString &path) {
    QSettings settings;
    QUrl endpoint = settings.value(
        QStringLiteral("agent/url"),
        QStringLiteral("wss://150.158.27.206/agent/ws")).toUrl();
    const QString scheme = endpoint.scheme().toLower();
    if(scheme == QStringLiteral("wss")) endpoint.setScheme(QStringLiteral("https"));
    else if(scheme == QStringLiteral("ws")) endpoint.setScheme(QStringLiteral("http"));
    endpoint.setPath(path);
    endpoint.setQuery({});
    endpoint.setFragment({});
    return endpoint;
}

QNetworkRequest ownerApiRequest(const QString &path) {
    QNetworkRequest request(ownerApiUrl(path));
    const QString token = unprotectSettingsSecret(
        QSettings().value(QStringLiteral("agent/token"), QString()).toString());
    if(!token.isEmpty())
        request.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("TonyDesktopPet/%1").arg(QCoreApplication::applicationVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QString localTimeText(qint64 epochSeconds) {
    if(epochSeconds <= 0) return QStringLiteral("—");
    return QDateTime::fromSecsSinceEpoch(epochSeconds).toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString apiErrorText(QNetworkReply *reply, const QByteArray &raw) {
    const auto doc = QJsonDocument::fromJson(raw);
    if(doc.isObject()) {
        const QString detail = doc.object().value(QStringLiteral("detail")).toString();
        if(!detail.isEmpty()) return detail;
    }
    return reply->errorString();
}

void configureTable(QTableWidget *table) {
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
}
}

SettingsDialog::SettingsDialog(UpdateManager *updater, QWidget *parent)
    : QDialog(parent), updater_(updater) {
    setWindowTitle(trUi("Tony Settings", "Tony 设置"));
    setMinimumSize(760, 560);
    resize(820, 610);

    ownerNetwork_ = new QNetworkAccessManager(this);

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
        trUi("Tony reconnects automatically with the device token stored on this computer. Use the button below to test the secure server path or create a fresh pairing code.",
             "Tony 会使用保存在这台电脑上的设备令牌自动重连。可使用下方按钮测试安全连接，或重新生成配对码。"), connection);
    connectionText->setWordWrap(true);
    connectionLayout->addWidget(connectionText);
    auto *reconnectButton = new QPushButton(trUi("Test connection / pair this computer…", "测试连接 / 配对这台电脑…"), connection);
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

    auto *ownerTab = new QWidget(tabs);
    auto *ownerLayout = new QVBoxLayout(ownerTab);
    ownerLayout->setContentsMargins(18, 18, 18, 18);
    ownerLayout->setSpacing(12);

    auto *ownerIntro = new QLabel(
        trUi("Approve friends' Tony computers here. Only the original owner device can use these controls; friend devices cannot approve themselves.",
             "在这里审批朋友的 Tony 电脑。只有原始主人设备可以使用这些功能，朋友设备不能自行批准自己。"), ownerTab);
    ownerIntro->setWordWrap(true);
    ownerLayout->addWidget(ownerIntro);

    ownerStatus_ = new QLabel(trUi("Owner access has not been checked yet.", "尚未检查主人权限。"), ownerTab);
    ownerStatus_->setWordWrap(true);
    ownerLayout->addWidget(ownerStatus_);

    auto *pendingGroup = new QGroupBox(trUi("Pending pairing requests", "待审批设备"), ownerTab);
    auto *pendingLayout = new QVBoxLayout(pendingGroup);
    pendingPairings_ = new QTableWidget(pendingGroup);
    pendingPairings_->setColumnCount(4);
    pendingPairings_->setHorizontalHeaderLabels({
        trUi("Computer", "电脑"), trUi("Requested", "申请时间"),
        trUi("Expires", "过期时间"), trUi("Status", "状态")
    });
    configureTable(pendingPairings_);
    pendingPairings_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    pendingPairings_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    pendingPairings_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    pendingPairings_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    pendingLayout->addWidget(pendingPairings_);

    auto *pendingButtons = new QHBoxLayout;
    auto *refreshOwner = new QPushButton(trUi("Refresh", "刷新"), pendingGroup);
    approveSelected_ = new QPushButton(trUi("Approve selected", "批准选中设备"), pendingGroup);
    pendingButtons->addWidget(refreshOwner);
    pendingButtons->addWidget(approveSelected_);
    pendingButtons->addStretch(1);
    pendingLayout->addLayout(pendingButtons);

    auto *codeRow = new QHBoxLayout;
    pairingCodeInput_ = new QLineEdit(pendingGroup);
    pairingCodeInput_->setPlaceholderText(trUi("Connection code, e.g. PT2S-DFVS", "连接码，例如 PT2S-DFVS"));
    approveCode_ = new QPushButton(trUi("Approve code", "批准连接码"), pendingGroup);
    codeRow->addWidget(pairingCodeInput_, 1);
    codeRow->addWidget(approveCode_);
    pendingLayout->addLayout(codeRow);
    ownerLayout->addWidget(pendingGroup, 1);

    auto *devicesGroup = new QGroupBox(trUi("Paired devices", "已配对设备"), ownerTab);
    auto *devicesLayout = new QVBoxLayout(devicesGroup);
    ownerDevices_ = new QTableWidget(devicesGroup);
    ownerDevices_->setColumnCount(4);
    ownerDevices_->setHorizontalHeaderLabels({
        trUi("Computer", "电脑"), trUi("Device ID", "设备 ID"),
        trUi("Paired via", "配对方式"), trUi("Status", "状态")
    });
    configureTable(ownerDevices_);
    ownerDevices_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ownerDevices_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ownerDevices_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ownerDevices_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    devicesLayout->addWidget(ownerDevices_);
    revokeDevice_ = new QPushButton(trUi("Revoke selected device", "撤销选中设备"), devicesGroup);
    devicesLayout->addWidget(revokeDevice_, 0, Qt::AlignLeft);
    ownerLayout->addWidget(devicesGroup, 1);

    const int ownerTabIndex = tabs->addTab(ownerTab, trUi("Owner Devices", "设备审批"));
    connect(tabs, &QTabWidget::currentChanged, this, [this, ownerTabIndex](int index){
        if(index == ownerTabIndex) refreshOwnerPanel();
    });
    connect(refreshOwner, &QPushButton::clicked, this, &SettingsDialog::refreshOwnerPanel);
    connect(approveSelected_, &QPushButton::clicked, this, &SettingsDialog::approveSelectedPairing);
    connect(approveCode_, &QPushButton::clicked, this, &SettingsDialog::approveTypedPairingCode);
    connect(pairingCodeInput_, &QLineEdit::returnPressed, this, &SettingsDialog::approveTypedPairingCode);
    connect(revokeDevice_, &QPushButton::clicked, this, &SettingsDialog::revokeSelectedDevice);

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

void SettingsDialog::refreshOwnerPanel() {
    if(!ownerNetwork_ || !ownerStatus_) return;
    const QString token = unprotectSettingsSecret(
        QSettings().value(QStringLiteral("agent/token"), QString()).toString());
    if(token.isEmpty()) {
        ownerStatus_->setText(trUi(
            "This computer is not paired yet. Pair it before using owner controls.",
            "这台电脑尚未配对，请先完成配对后再使用主人管理功能。"));
        pendingPairings_->setRowCount(0);
        ownerDevices_->setRowCount(0);
        return;
    }

    ownerStatus_->setText(trUi("Checking owner authorization…", "正在检查主人权限…"));
    auto *statusReply = ownerNetwork_->get(ownerApiRequest(QStringLiteral("/owner/status")));
    connect(statusReply, &QNetworkReply::finished, this, [this, statusReply]{
        const QByteArray raw = statusReply->readAll();
        const int http = statusReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(statusReply->error() != QNetworkReply::NoError || http < 200 || http >= 300) {
            const QString detail = apiErrorText(statusReply, raw);
            ownerStatus_->setText(http == 403
                ? trUi("Owner controls are available only on the original owner computer.",
                       "只有原始主人电脑可以使用设备审批功能。")
                : trUi("Could not load owner controls: ", "无法加载主人管理功能：") + detail);
            pendingPairings_->setRowCount(0);
            ownerDevices_->setRowCount(0);
            statusReply->deleteLater();
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(raw).object();
        const QString ownerName = obj.value(QStringLiteral("device")).toObject().value(QStringLiteral("name")).toString();
        ownerStatus_->setText(trUi("Owner access confirmed", "已确认主人权限") +
                              (ownerName.isEmpty() ? QString() : QStringLiteral(" · ") + ownerName));
        statusReply->deleteLater();

        auto *requestsReply = ownerNetwork_->get(ownerApiRequest(QStringLiteral("/owner/pairing/requests")));
        connect(requestsReply, &QNetworkReply::finished, this, [this, requestsReply]{
            const QByteArray requestRaw = requestsReply->readAll();
            const int requestHttp = requestsReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if(requestsReply->error() != QNetworkReply::NoError || requestHttp < 200 || requestHttp >= 300) {
                ownerStatus_->setText(trUi("Could not load pending devices: ", "无法加载待审批设备：") +
                                      apiErrorText(requestsReply, requestRaw));
                requestsReply->deleteLater();
                return;
            }

            const QJsonArray rows = QJsonDocument::fromJson(requestRaw).object()
                                        .value(QStringLiteral("requests")).toArray();
            pendingPairings_->setRowCount(rows.size());
            for(int row = 0; row < rows.size(); ++row) {
                const QJsonObject item = rows.at(row).toObject();
                auto *name = new QTableWidgetItem(item.value(QStringLiteral("device_name")).toString());
                name->setData(Qt::UserRole, item.value(QStringLiteral("approval_id")).toString());
                pendingPairings_->setItem(row, 0, name);
                pendingPairings_->setItem(row, 1, new QTableWidgetItem(
                    localTimeText(item.value(QStringLiteral("created_at")).toVariant().toLongLong())));
                pendingPairings_->setItem(row, 2, new QTableWidgetItem(
                    localTimeText(item.value(QStringLiteral("expires_at")).toVariant().toLongLong())));
                const bool paired = item.value(QStringLiteral("paired")).toBool();
                const bool approved = item.value(QStringLiteral("approved")).toBool();
                const QString state = paired
                    ? trUi("Paired", "已配对")
                    : approved ? trUi("Approved · waiting for device", "已批准 · 等待设备领取")
                               : trUi("Waiting approval", "等待批准");
                pendingPairings_->setItem(row, 3, new QTableWidgetItem(state));
            }
            requestsReply->deleteLater();
            refreshOwnerDeviceList();
        });
    });
}

void SettingsDialog::refreshOwnerDeviceList() {
    auto *reply = ownerNetwork_->get(ownerApiRequest(QStringLiteral("/owner/devices")));
    connect(reply, &QNetworkReply::finished, this, [this, reply]{
        const QByteArray raw = reply->readAll();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(reply->error() != QNetworkReply::NoError || http < 200 || http >= 300) {
            ownerStatus_->setText(trUi("Could not load paired devices: ", "无法加载已配对设备：") +
                                  apiErrorText(reply, raw));
            reply->deleteLater();
            return;
        }
        const QJsonArray rows = QJsonDocument::fromJson(raw).object().value(QStringLiteral("devices")).toArray();
        ownerDevices_->setRowCount(rows.size());
        for(int row = 0; row < rows.size(); ++row) {
            const QJsonObject item = rows.at(row).toObject();
            const bool owner = item.value(QStringLiteral("owner")).toBool();
            const bool revoked = item.value(QStringLiteral("revoked")).toBool();
            QString displayName = item.value(QStringLiteral("name")).toString();
            if(owner) displayName += trUi(" (Owner)", "（主人）");
            auto *name = new QTableWidgetItem(displayName);
            name->setData(Qt::UserRole, item.value(QStringLiteral("id")).toString());
            name->setData(Qt::UserRole + 1, owner);
            name->setData(Qt::UserRole + 2, revoked);
            ownerDevices_->setItem(row, 0, name);
            ownerDevices_->setItem(row, 1, new QTableWidgetItem(item.value(QStringLiteral("id")).toString()));
            ownerDevices_->setItem(row, 2, new QTableWidgetItem(item.value(QStringLiteral("paired_via")).toString()));
            ownerDevices_->setItem(row, 3, new QTableWidgetItem(
                revoked ? trUi("Revoked", "已撤销") : trUi("Active", "有效")));
        }
        reply->deleteLater();
    });
}

void SettingsDialog::approveSelectedPairing() {
    const int row = pendingPairings_ ? pendingPairings_->currentRow() : -1;
    if(row < 0 || !pendingPairings_->item(row, 0)) {
        ownerStatus_->setText(trUi("Select a pending computer first.", "请先选择一台待审批电脑。"));
        return;
    }
    const QString approvalId = pendingPairings_->item(row, 0)->data(Qt::UserRole).toString();
    if(approvalId.isEmpty()) return;

    QJsonObject payload{{QStringLiteral("approval_id"), approvalId}};
    auto *reply = ownerNetwork_->post(ownerApiRequest(QStringLiteral("/owner/pairing/approve")),
                                      QJsonDocument(payload).toJson(QJsonDocument::Compact));
    ownerStatus_->setText(trUi("Approving selected computer…", "正在批准选中电脑…"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]{
        const QByteArray raw = reply->readAll();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(reply->error() != QNetworkReply::NoError || http < 200 || http >= 300) {
            ownerStatus_->setText(trUi("Approval failed: ", "批准失败：") + apiErrorText(reply, raw));
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(raw).object();
        const QString name = obj.value(QStringLiteral("device_name")).toString();
        ownerStatus_->setText(trUi("Approved ", "已批准 ") + name +
            trUi(". Waiting for that Tony to claim its device token…",
                 "。正在等待对方 Tony 领取设备令牌…"));
        reply->deleteLater();
        QTimer::singleShot(2200, this, &SettingsDialog::refreshOwnerPanel);
    });
}

void SettingsDialog::approveTypedPairingCode() {
    if(!pairingCodeInput_) return;
    QString code = pairingCodeInput_->text().trimmed().toUpper();
    code.remove(QRegularExpression(QStringLiteral("[^A-Z0-9]")));
    if(code.size() != 8) {
        ownerStatus_->setText(trUi("Enter an 8-character connection code.", "请输入 8 位连接码。"));
        return;
    }
    code.insert(4, QLatin1Char('-'));

    QJsonObject payload{{QStringLiteral("code"), code}};
    auto *reply = ownerNetwork_->post(ownerApiRequest(QStringLiteral("/owner/pairing/approve")),
                                      QJsonDocument(payload).toJson(QJsonDocument::Compact));
    ownerStatus_->setText(trUi("Approving connection code…", "正在批准连接码…"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]{
        const QByteArray raw = reply->readAll();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(reply->error() != QNetworkReply::NoError || http < 200 || http >= 300) {
            ownerStatus_->setText(trUi("Approval failed: ", "批准失败：") + apiErrorText(reply, raw));
            reply->deleteLater();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(raw).object();
        pairingCodeInput_->clear();
        ownerStatus_->setText(trUi("Approved ", "已批准 ") +
                              obj.value(QStringLiteral("device_name")).toString() +
                              trUi(". The friend's Tony can now claim its token.",
                                   "。朋友的 Tony 现在可以领取设备令牌。"));
        reply->deleteLater();
        QTimer::singleShot(2200, this, &SettingsDialog::refreshOwnerPanel);
    });
}

void SettingsDialog::revokeSelectedDevice() {
    const int row = ownerDevices_ ? ownerDevices_->currentRow() : -1;
    if(row < 0 || !ownerDevices_->item(row, 0)) {
        ownerStatus_->setText(trUi("Select a paired device first.", "请先选择一台已配对设备。"));
        return;
    }
    auto *item = ownerDevices_->item(row, 0);
    const QString deviceId = item->data(Qt::UserRole).toString();
    const bool owner = item->data(Qt::UserRole + 1).toBool();
    const bool revoked = item->data(Qt::UserRole + 2).toBool();
    if(owner) {
        ownerStatus_->setText(trUi("The owner computer cannot revoke itself here.", "主人电脑不能在这里撤销自己。"));
        return;
    }
    if(revoked) {
        ownerStatus_->setText(trUi("That device is already revoked.", "该设备已经被撤销。"));
        return;
    }
    if(QMessageBox::question(this, trUi("Revoke device", "撤销设备"),
                             trUi("Revoke this computer's Tony access? It will need to pair again.",
                                  "撤销这台电脑的 Tony 访问权限吗？之后需要重新配对。")) != QMessageBox::Yes)
        return;

    QJsonObject payload{{QStringLiteral("device_id"), deviceId}};
    auto *reply = ownerNetwork_->post(ownerApiRequest(QStringLiteral("/owner/devices/revoke")),
                                      QJsonDocument(payload).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]{
        const QByteArray raw = reply->readAll();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(reply->error() != QNetworkReply::NoError || http < 200 || http >= 300) {
            ownerStatus_->setText(trUi("Revoke failed: ", "撤销失败：") + apiErrorText(reply, raw));
            reply->deleteLater();
            return;
        }
        ownerStatus_->setText(trUi("Device access revoked.", "设备访问权限已撤销。"));
        reply->deleteLater();
        refreshOwnerPanel();
    });
}
