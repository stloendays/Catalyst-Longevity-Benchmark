#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QIcon>
#include <QKeySequence>
#include <QLocale>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMenu>
#include <QMouseEvent>
#include <QSettings>
#include <QShortcut>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#include "AppLogger.h"
#include "NewsCompanion.h"
#include "NewsSettingsDialog.h"
#include "PetCreatorDialog.h"
#include "PetWindow.h"
#include "SettingsDialog.h"
#include "TonyAutonomousCompanion.h"
#include "TonyQuietMode.h"
#include "TodayDialog.h"
#include "UpdateManager.h"

#ifndef TONY_APP_VERSION
#define TONY_APP_VERSION "0.0.0"
#endif

namespace {
class PetActivityFilter final : public QObject {
public:
    bool eventFilter(QObject *watched, QEvent *event) override {
        Q_UNUSED(watched);

        switch(event->type()) {
        case QEvent::MouseButtonPress: {
            auto *mouse = static_cast<QMouseEvent*>(event);
            if(mouse->button() == Qt::LeftButton) {
                leftDown_ = true;
                dragged_ = false;
                pressGlobal_ = mouse->globalPosition().toPoint();
            }
            break;
        }
        case QEvent::MouseMove: {
            if(!leftDown_) break;
            auto *mouse = static_cast<QMouseEvent*>(event);
            if((mouse->globalPosition().toPoint() - pressGlobal_).manhattanLength() >= 6)
                dragged_ = true;
            break;
        }
        case QEvent::MouseButtonRelease: {
            auto *mouse = static_cast<QMouseEvent*>(event);
            if(mouse->button() == Qt::LeftButton && leftDown_) {
                AppLogger::recordOperatorEvent(
                    dragged_ ? QStringLiteral("pet_drag") : QStringLiteral("pet_click"));
                leftDown_ = false;
                dragged_ = false;
            }
            break;
        }
        case QEvent::MouseButtonDblClick:
            AppLogger::recordOperatorEvent(QStringLiteral("pet_double_click"));
            break;
        case QEvent::ContextMenu:
            AppLogger::recordOperatorEvent(QStringLiteral("pet_context_menu"));
            break;
        default:
            break;
        }
        return false;
    }

private:
    QPoint pressGlobal_;
    bool leftDown_{false};
    bool dragged_{false};
};

QString singleInstanceServerName() {
    const QByteArray digest = QCryptographicHash::hash(
        QDir::homePath().toUtf8(), QCryptographicHash::Sha256).toHex().left(12);
    return QStringLiteral("TonyDesktopPet-%1").arg(QString::fromLatin1(digest));
}

void applyPetCommandLine(const QStringList &arguments) {
    QSettings settings;
    const QString prefix = QStringLiteral("--pet-root=");
    for(const auto &argument : arguments) {
        if(argument == QStringLiteral("--pet-reset")) {
            settings.remove(QStringLiteral("pet/asset_root"));
            settings.remove(QStringLiteral("pet/active_id"));
            settings.remove(QStringLiteral("pet/active_name"));
            settings.remove(QStringLiteral("pet/active_version"));
            continue;
        }
        if(!argument.startsWith(prefix, Qt::CaseInsensitive)) continue;
        const QString raw = argument.mid(prefix.size()).trimmed();
        if(raw.isEmpty()) continue;
        settings.setValue(QStringLiteral("pet/asset_root"), QDir(raw).absolutePath());
    }
}
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("TonyAgent");
    QCoreApplication::setApplicationName("Tony Desktop Pet");
    QCoreApplication::setApplicationVersion(QStringLiteral(TONY_APP_VERSION));

    applyPetCommandLine(app.arguments());

    const QString instanceName = singleInstanceServerName();
    const QString lockPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
        + QStringLiteral("/") + instanceName + QStringLiteral(".lock");
    QLockFile singleInstanceLock(lockPath);
    singleInstanceLock.setStaleLockTime(0);

    if(!singleInstanceLock.tryLock(0)) {
        // The lock is the authoritative single-instance guard. The activation
        // socket may need a moment if the first Tony is still starting.
        for(int attempt = 0; attempt < 10; ++attempt) {
            QLocalSocket existingInstance;
            existingInstance.connectToServer(instanceName, QIODevice::WriteOnly);
            if(existingInstance.waitForConnected(180)) {
                existingInstance.write("activate\n");
                existingInstance.flush();
                existingInstance.waitForBytesWritten(250);
                break;
            }
            QThread::msleep(80);
        }
        return 0;
    }

    QLocalServer singleInstanceServer;
    // The lock proves this is the only Tony process, so a leftover local
    // socket endpoint can be removed safely after an unclean exit.
    QLocalServer::removeServer(instanceName);
    if(!singleInstanceServer.listen(instanceName))
        qWarning().noquote() << "Tony activation socket unavailable:" << instanceName;

    // Respect the saved interface language. On first launch, follow the OS locale
    // instead of silently forcing English every time Tony starts.
    QSettings startupSettings;
    if(!startupSettings.contains(QStringLiteral("ui/language"))) {
        const QString systemLocale = QLocale::system().name().toLower();
        startupSettings.setValue(
            QStringLiteral("ui/language"),
            systemLocale.startsWith(QStringLiteral("zh"))
                ? QStringLiteral("zh")
                : QStringLiteral("en"));
    }

    const QIcon appIcon(QCoreApplication::applicationDirPath()+QStringLiteral("/assets/tony-app.ico"));
    if(!appIcon.isNull()) app.setWindowIcon(appIcon);
    app.setQuitOnLastWindowClosed(false);

    AppLogger::install();
    qInfo().noquote() << "Tony Desktop Pet starting" << QCoreApplication::applicationVersion();

    PetWindow pet;
    QString petPackageError;
    if(!pet.applyActivePetPackage(&petPackageError) && !petPackageError.isEmpty())
        qWarning().noquote() << "Custom pet package was not activated:" << petPackageError;

    PetActivityFilter activityFilter;
    pet.installEventFilter(&activityFilter);
    pet.show();

    QObject::connect(&singleInstanceServer, &QLocalServer::newConnection, &app, [&]{
        while(auto *socket = singleInstanceServer.nextPendingConnection()) {
            socket->readAll();
            pet.show();
            pet.raise();
            pet.activateWindow();
            pet.openChat();
            socket->disconnectFromServer();
            socket->deleteLater();
        }
    });

    TonyAutonomousCompanion autonomy(&pet,&app);
    NewsCompanion news(&pet,&app);
    QObject::connect(&news, &NewsCompanion::headlineReady,
                     &pet, &PetWindow::announceNewsHeadline);

    UpdateManager updater(&app);
    SettingsDialog settingsDialog(&updater, &pet);
    settingsDialog.setModal(false);
    PetCreatorDialog creatorDialog(&pet);
    creatorDialog.setModal(false);
    NewsSettingsDialog newsDialog(&news, &pet);
    newsDialog.setModal(false);
    TodayDialog todayDialog(&pet, &news, &pet);
    todayDialog.setModal(false);

    QObject::connect(&settingsDialog, &SettingsDialog::languageChanged,
                     &pet, &PetWindow::applyUiLanguage);
    QObject::connect(&settingsDialog, &SettingsDialog::reconnectRequested,
                     &pet, [&pet, &settingsDialog]{
        settingsDialog.hide();
        AppLogger::recordOperatorEvent(QStringLiteral("pair_reconnect_open"));
        pet.configureConnection();
    });
    QObject::connect(&updater, &UpdateManager::updateFound,
                     &pet, &PetWindow::syncOperatorLogsForUpdate);
    QObject::connect(&updater, &UpdateManager::updateDownloaded,
                     &pet, &PetWindow::syncOperatorLogsForUpdate);

    const auto showSettings = [&settingsDialog]{
        AppLogger::recordOperatorEvent(QStringLiteral("settings_open"));
        settingsDialog.show();
        settingsDialog.raise();
        settingsDialog.activateWindow();
    };
    const auto showCreator = [&creatorDialog]{
        AppLogger::recordOperatorEvent(QStringLiteral("pet_creator_open"));
        creatorDialog.refresh();
        creatorDialog.show();
        creatorDialog.raise();
        creatorDialog.activateWindow();
    };
    const auto showNews = [&newsDialog]{
        AppLogger::recordOperatorEvent(QStringLiteral("news_settings_open"));
        newsDialog.refresh();
        newsDialog.show();
        newsDialog.raise();
        newsDialog.activateWindow();
    };
    const auto showToday = [&todayDialog]{
        AppLogger::recordOperatorEvent(QStringLiteral("today_hub_open"));
        todayDialog.refresh();
        todayDialog.show();
        todayDialog.raise();
        todayDialog.activateWindow();
    };

    auto *settingsShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+,")), &pet);
    settingsShortcut->setContext(Qt::ApplicationShortcut);
    QObject::connect(settingsShortcut, &QShortcut::activated, &app, showSettings);

    auto *todayShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+T")), &pet);
    todayShortcut->setContext(Qt::ApplicationShortcut);
    QObject::connect(todayShortcut, &QShortcut::activated, &app, showToday);

    const auto uiIsChinese=[]{
        return QSettings().value(QStringLiteral("ui/language"),QStringLiteral("en")).toString()
            .startsWith(QStringLiteral("zh"),Qt::CaseInsensitive);
    };
    auto *trayMenu=new QMenu(&pet);

    auto *todayAction=trayMenu->addAction(QString());
    auto *chatAction=trayMenu->addAction(QString());
    auto *hugAction=trayMenu->addAction(QString());
    auto *statusAction=trayMenu->addAction(QString());
    QObject::connect(todayAction,&QAction::triggered,&app,showToday);
    QObject::connect(chatAction,&QAction::triggered,&pet,&PetWindow::openChat);
    QObject::connect(hugAction,&QAction::triggered,&pet,&PetWindow::hug);
    QObject::connect(statusAction,&QAction::triggered,&pet,&PetWindow::showStatus);

    trayMenu->addSeparator();
    auto *quickReminder=trayMenu->addAction(QString());
    auto *openNews=trayMenu->addAction(QString());
    auto *openCreator=trayMenu->addAction(QString());
    auto *openSettings=trayMenu->addAction(QString());
    auto *helpAction=trayMenu->addAction(QString());
    QObject::connect(quickReminder,&QAction::triggered,&pet,&PetWindow::createQuickReminder);
    QObject::connect(openNews,&QAction::triggered,&app,showNews);
    QObject::connect(openCreator,&QAction::triggered,&app,showCreator);
    QObject::connect(openSettings,&QAction::triggered,&app,showSettings);
    QObject::connect(helpAction,&QAction::triggered,&pet,&PetWindow::showWelcomeGuide);

    trayMenu->addSeparator();
    auto *quietMenu=trayMenu->addMenu(QString());
    auto *quietOneHour=quietMenu->addAction(QString());
    auto *quietFourHours=quietMenu->addAction(QString());
    auto *quietUntilMorning=quietMenu->addAction(QString());
    quietMenu->addSeparator();
    auto *quietOff=quietMenu->addAction(QString());

    const auto announceQuiet=[&pet,uiIsChinese](const QString &kind){
        const bool zh=uiIsChinese();
        QString text;
        if(kind==QStringLiteral("off")) {
            TonyQuietMode::clear();
            text=zh ? QStringLiteral("免打扰已关闭。Tony 会恢复原来的主动模式和新闻设置。")
                    : QStringLiteral("Do Not Disturb is off. Tony will resume your previous autonomy and news settings.");
        } else if(kind==QStringLiteral("1h")) {
            TonyQuietMode::setForMinutes(60);
            text=zh ? QStringLiteral("好，接下来 1 小时我不主动打扰你。")
                    : QStringLiteral("Okay. I will stay quiet for the next hour.");
        } else if(kind==QStringLiteral("4h")) {
            TonyQuietMode::setForMinutes(240);
            text=zh ? QStringLiteral("好，接下来 4 小时我不主动打扰你。")
                    : QStringLiteral("Okay. I will stay quiet for the next 4 hours.");
        } else {
            TonyQuietMode::setUntilTomorrowMorning(8);
            text=zh ? QStringLiteral("好，我会安静到明早 8 点。")
                    : QStringLiteral("Okay. I will stay quiet until 8 AM.");
        }
        pet.showAutonomyNotice(text);
    };
    QObject::connect(quietOneHour,&QAction::triggered,&app,[&]{ announceQuiet(QStringLiteral("1h")); });
    QObject::connect(quietFourHours,&QAction::triggered,&app,[&]{ announceQuiet(QStringLiteral("4h")); });
    QObject::connect(quietUntilMorning,&QAction::triggered,&app,[&]{ announceQuiet(QStringLiteral("morning")); });
    QObject::connect(quietOff,&QAction::triggered,&app,[&]{ announceQuiet(QStringLiteral("off")); });

    trayMenu->addSeparator();
    auto *autonomyMenu=trayMenu->addMenu(QString());
    auto *autonomyGroup=new QActionGroup(autonomyMenu);
    autonomyGroup->setExclusive(true);
    const QString activeMode=autonomy.mode();
    const auto addAutonomyAction=[&](const QString &mode){
        auto *action=autonomyMenu->addAction(QString());
        action->setCheckable(true);
        action->setData(mode);
        action->setChecked(activeMode==mode);
        autonomyGroup->addAction(action);
        QObject::connect(action,&QAction::triggered,&app,[&autonomy,mode](bool checked){
            if(!checked) return;
            autonomy.setMode(mode);
            autonomy.announceMode();
        });
        return action;
    };
    auto *autonomyOff=addAutonomyAction(QStringLiteral("off"));
    auto *autonomyQuiet=addAutonomyAction(QStringLiteral("quiet"));
    auto *autonomyNormal=addAutonomyAction(QStringLiteral("normal"));
    auto *autonomyLively=addAutonomyAction(QStringLiteral("lively"));

    trayMenu->addSeparator();
    auto *quitAction=trayMenu->addAction(QString());
    QObject::connect(quitAction,&QAction::triggered,&app,&QApplication::quit);

    const auto refreshTrayLanguage=[=]{
        const bool zh=uiIsChinese();
        todayAction->setText(zh ? QStringLiteral("Tony 今日…") : QStringLiteral("Tony Today…"));
        chatAction->setText(zh ? QStringLiteral("和 Tony 聊天…") : QStringLiteral("Chat with Tony…"));
        hugAction->setText(zh ? QStringLiteral("抱抱 Tony") : QStringLiteral("Hug Tony"));
        statusAction->setText(zh ? QStringLiteral("Tony 现在怎么样？") : QStringLiteral("How is Tony feeling?"));
        quickReminder->setText(zh ? QStringLiteral("快速提醒…") : QStringLiteral("Quick Reminder…"));
        openNews->setText(zh ? QStringLiteral("联网新闻…") : QStringLiteral("News Companion…"));
        openCreator->setText(zh ? QStringLiteral("宠物与创作…") : QStringLiteral("Pets & Creator…"));
        openSettings->setText(zh ? QStringLiteral("Tony 设置…") : QStringLiteral("Tony Settings…"));
        helpAction->setText(zh ? QStringLiteral("使用帮助") : QStringLiteral("Help / controls"));
        quietMenu->setTitle(
            TonyQuietMode::isActive()
                ? (zh ? QStringLiteral("免打扰 · %1").arg(TonyQuietMode::remainingLabel(true))
                      : QStringLiteral("Do Not Disturb · %1").arg(TonyQuietMode::remainingLabel(false)))
                : (zh ? QStringLiteral("免打扰") : QStringLiteral("Do Not Disturb")));
        quietOneHour->setText(zh ? QStringLiteral("安静 1 小时") : QStringLiteral("Quiet for 1 hour"));
        quietFourHours->setText(zh ? QStringLiteral("安静 4 小时") : QStringLiteral("Quiet for 4 hours"));
        quietUntilMorning->setText(zh ? QStringLiteral("安静到早上 8 点") : QStringLiteral("Quiet until 8 AM"));
        quietOff->setText(zh ? QStringLiteral("恢复主动提醒") : QStringLiteral("Resume proactive features"));
        quietOff->setEnabled(TonyQuietMode::isActive());
        autonomyMenu->setTitle(zh ? QStringLiteral("主动模式") : QStringLiteral("Autonomy"));
        autonomyOff->setText(zh ? QStringLiteral("关闭") : QStringLiteral("Off"));
        autonomyQuiet->setText(zh ? QStringLiteral("安静") : QStringLiteral("Quiet"));
        autonomyNormal->setText(zh ? QStringLiteral("正常") : QStringLiteral("Normal"));
        autonomyLively->setText(zh ? QStringLiteral("活泼") : QStringLiteral("Lively"));
        quitAction->setText(zh ? QStringLiteral("退出 Tony") : QStringLiteral("Quit Tony"));
    };
    refreshTrayLanguage();
    QObject::connect(trayMenu,&QMenu::aboutToShow,&app,[&]{
        refreshTrayLanguage();
        newsDialog.refresh();
        todayDialog.refresh();
    });
    QObject::connect(&settingsDialog,&SettingsDialog::languageChanged,&app,
                     [&newsDialog,&todayDialog,refreshTrayLanguage](const QString &){
        refreshTrayLanguage();
        newsDialog.refresh();
        todayDialog.refresh();
    });

    pet.trayIcon()->setContextMenu(trayMenu);

    QObject::connect(&updater,&UpdateManager::updateDownloaded,&app,
                     [&pet,uiIsChinese](const QString &version){
        const bool zh=uiIsChinese();
        pet.trayIcon()->showMessage(
            zh ? QStringLiteral("Tony 更新已准备好") : QStringLiteral("Tony update ready"),
            zh ? QStringLiteral("Tony %1 已下载并校验。你可以在“Tony 设置 → 自动更新”里选择何时安装。").arg(version)
               : QStringLiteral("Tony %1 is downloaded and verified. Install it when convenient from Tony Settings → Automatic Updates.").arg(version),
            QSystemTrayIcon::Information,
            6500);
    });
    QObject::connect(pet.trayIcon(),&QSystemTrayIcon::messageClicked,&app,showSettings);

    updater.scheduleStartupCheck();

    QTimer::singleShot(900,&app,[&pet]{
        QSettings settings;
        if(settings.value(QStringLiteral("ux/welcome_seen"),false).toBool()) return;
        settings.setValue(QStringLiteral("ux/welcome_seen"),true);
        pet.showWelcomeGuide();
    });

    // First launch stays non-modal. An unpaired PetWindow automatically exposes
    // a short-lived device connection code after Tony is visibly on the desktop.
    // Settings/reconnect uses the same device-code flow; friend code is recovery-only.

    const int result = app.exec();
    qInfo() << "Tony Desktop Pet exiting" << result;
    return result;
}
