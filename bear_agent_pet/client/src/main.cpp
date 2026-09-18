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
#include <QMenu>
#include <QMouseEvent>
#include <QSettings>
#include <QShortcut>
#include <QTimer>

#include "AppLogger.h"
#include "NewsCompanion.h"
#include "NewsSettingsDialog.h"
#include "PetCreatorDialog.h"
#include "PetWindow.h"
#include "SettingsDialog.h"
#include "TonyAutonomousCompanion.h"
#include "TonyQuietMode.h"
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

    QLocalServer singleInstanceServer;
    const QString instanceName = singleInstanceServerName();
    if(!singleInstanceServer.listen(instanceName)) {
        QLocalSocket existingInstance;
        existingInstance.connectToServer(instanceName, QIODevice::WriteOnly);
        if(existingInstance.waitForConnected(450)) {
            existingInstance.write("activate\n");
            existingInstance.flush();
            existingInstance.waitForBytesWritten(300);
            return 0;
        }

        // Recover from a stale local-server endpoint left by an unclean exit.
        QLocalServer::removeServer(instanceName);
        if(!singleInstanceServer.listen(instanceName))
            qWarning().noquote() << "Tony single-instance guard unavailable:" << instanceName;
    }

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

    auto *settingsShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+,")), &pet);
    settingsShortcut->setContext(Qt::ApplicationShortcut);
    QObject::connect(settingsShortcut, &QShortcut::activated, &app, showSettings);

    const auto uiIsChinese=[]{
        return QSettings().value(QStringLiteral("ui/language"),QStringLiteral("en")).toString()
            .startsWith(QStringLiteral("zh"),Qt::CaseInsensitive);
    };
    auto *trayMenu=new QMenu(&pet);

    auto *chatAction=trayMenu->addAction(QString());
    auto *hugAction=trayMenu->addAction(QString());
    auto *statusAction=trayMenu->addAction(QString());
    QObject::connect(chatAction,&QAction::triggered,&pet,&PetWindow::openChat);
    QObject::connect(hugAction,&QAction::triggered,&pet,&PetWindow::hug);
    QObject::connect(statusAction,&QAction::triggered,&pet,&PetWindow::showStatus);

    trayMenu->addSeparator();
    auto *quickReminder=trayMenu->addAction(QString());
    auto *openCreator=trayMenu->addAction(QString());
    auto *openSettings=trayMenu->addAction(QString());
    auto *helpAction=trayMenu->addAction(QString());
    QObject::connect(quickReminder,&QAction::triggered,&pet,&PetWindow::createQuickReminder);
    QObject::connect(openCreator,&QAction::triggered,&app,showCreator);
    QObject::connect(openSettings,&QAction::triggered,&app,showSettings);
    QObject::connect(helpAction,&QAction::triggered,&pet,&PetWindow::showWelcomeGuide);

    trayMenu->addSeparator();
    auto *newsMenu=trayMenu->addMenu(QString());
    auto *newsEnabled=newsMenu->addAction(QString());
    newsEnabled->setCheckable(true);
    newsEnabled->setChecked(news.enabled());
    auto *newsNow=newsMenu->addAction(QString());
    auto *newsOpen=newsMenu->addAction(QString());
    newsOpen->setEnabled(news.latestStoryUrl().isValid());

    newsMenu->addSeparator();
    auto *newsSourceMenu=newsMenu->addMenu(QString());
    const auto newsSources=news.sources();
    QVector<QAction*> newsSourceActions;
    newsSourceActions.reserve(newsSources.size());
    for(const auto &source : newsSources) {
        auto *action=newsSourceMenu->addAction(source.name);
        action->setCheckable(true);
        action->setChecked(news.sourceEnabled(source.id));
        QObject::connect(action,&QAction::toggled,&app,[&news,id=source.id](bool checked){
            news.setSourceEnabled(id,checked);
        });
        newsSourceActions.push_back(action);
    }

    auto *newsFrequencyMenu=newsMenu->addMenu(QString());
    auto *newsFrequencyGroup=new QActionGroup(newsFrequencyMenu);
    newsFrequencyGroup->setExclusive(true);
    const auto addNewsFrequency=[&](int minutes){
        auto *action=newsFrequencyMenu->addAction(QString());
        action->setCheckable(true);
        action->setData(minutes);
        action->setChecked(news.intervalMinutes()==minutes);
        newsFrequencyGroup->addAction(action);
        QObject::connect(action,&QAction::triggered,&app,[&news,minutes](bool checked){
            if(checked) news.setIntervalMinutes(minutes);
        });
        return action;
    };
    auto *newsEveryHour=addNewsFrequency(60);
    auto *newsEveryTwoHours=addNewsFrequency(120);
    auto *newsEveryFourHours=addNewsFrequency(240);

    QObject::connect(newsEnabled,&QAction::toggled,&app,[&news,&pet,uiIsChinese](bool checked){
        news.setEnabled(checked);
        const bool zh=uiIsChinese();
        pet.showAutonomyNotice(
            checked
                ? (zh ? QStringLiteral("联网新闻已开启。Tony 会避开夜间和你正在操作的时候，只偶尔分享没说过的新标题。")
                      : QStringLiteral("News Companion is on. Tony will avoid quiet hours and active moments, and only share unseen headlines occasionally."))
                : (zh ? QStringLiteral("联网新闻已暂停。")
                      : QStringLiteral("News Companion is paused.")));
    });
    QObject::connect(newsNow,&QAction::triggered,&app,[&news]{ news.fetchNow(true); });
    QObject::connect(newsOpen,&QAction::triggered,&news,&NewsCompanion::openLatestStory);
    QObject::connect(&news,&NewsCompanion::latestStoryChanged,&app,
                     [newsOpen](const QString &,const QString &,const QUrl &url){
        newsOpen->setEnabled(url.isValid());
    });

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
        chatAction->setText(zh ? QStringLiteral("和 Tony 聊天…") : QStringLiteral("Chat with Tony…"));
        hugAction->setText(zh ? QStringLiteral("抱抱 Tony") : QStringLiteral("Hug Tony"));
        statusAction->setText(zh ? QStringLiteral("Tony 现在怎么样？") : QStringLiteral("How is Tony feeling?"));
        quickReminder->setText(zh ? QStringLiteral("快速提醒…") : QStringLiteral("Quick Reminder…"));
        openCreator->setText(zh ? QStringLiteral("宠物与创作…") : QStringLiteral("Pets & Creator…"));
        openSettings->setText(zh ? QStringLiteral("Tony 设置…") : QStringLiteral("Tony Settings…"));
        helpAction->setText(zh ? QStringLiteral("使用帮助") : QStringLiteral("Help / controls"));
        newsMenu->setTitle(zh ? QStringLiteral("联网新闻") : QStringLiteral("News Companion"));
        newsEnabled->setText(zh ? QStringLiteral("自动播报新闻") : QStringLiteral("Automatic headlines"));
        newsNow->setText(zh ? QStringLiteral("现在说一条新闻") : QStringLiteral("Tell me one now"));
        newsOpen->setText(zh ? QStringLiteral("打开最近一条原文") : QStringLiteral("Open latest story"));
        newsSourceMenu->setTitle(zh ? QStringLiteral("新闻源") : QStringLiteral("Sources"));
        newsFrequencyMenu->setTitle(zh ? QStringLiteral("播报频率") : QStringLiteral("Frequency"));
        newsEveryHour->setText(zh ? QStringLiteral("每小时最多一条") : QStringLiteral("At most once per hour"));
        newsEveryTwoHours->setText(zh ? QStringLiteral("每两小时最多一条") : QStringLiteral("At most every 2 hours"));
        newsEveryFourHours->setText(zh ? QStringLiteral("每四小时最多一条") : QStringLiteral("At most every 4 hours"));
        autonomyMenu->setTitle(zh ? QStringLiteral("主动模式") : QStringLiteral("Autonomy"));
        autonomyOff->setText(zh ? QStringLiteral("关闭") : QStringLiteral("Off"));
        autonomyQuiet->setText(zh ? QStringLiteral("安静") : QStringLiteral("Quiet"));
        autonomyNormal->setText(zh ? QStringLiteral("正常") : QStringLiteral("Normal"));
        autonomyLively->setText(zh ? QStringLiteral("活泼") : QStringLiteral("Lively"));
        quitAction->setText(zh ? QStringLiteral("退出 Tony") : QStringLiteral("Quit Tony"));
    };
    refreshTrayLanguage();
    QObject::connect(&settingsDialog,&SettingsDialog::languageChanged,&app,
                     [refreshTrayLanguage](const QString &){ refreshTrayLanguage(); });

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
