#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QIcon>
#include <QKeySequence>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QSettings>
#include <QShortcut>
#include <QTimer>

#include "AppLogger.h"
#include "PetCreatorDialog.h"
#include "PetWindow.h"
#include "SettingsDialog.h"
#include "TonyAutonomousCompanion.h"
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

    TonyAutonomousCompanion autonomy(&pet,&app);

    UpdateManager updater(&app);
    SettingsDialog settingsDialog(&updater, &pet);
    settingsDialog.setModal(false);
    PetCreatorDialog creatorDialog(&pet);
    creatorDialog.setModal(false);

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
