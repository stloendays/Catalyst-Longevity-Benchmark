#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QIcon>
#include <QKeySequence>
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

    // Tony's character voice is English-only. Pin the shared runtime language
    // before PetWindow, the local router, autonomous speech and Agent initialize.
    QSettings().setValue(QStringLiteral("ui/language"), QStringLiteral("en"));

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

    const bool zhUi=QSettings().value(QStringLiteral("ui/language"),QStringLiteral("en")).toString()
                        .startsWith(QStringLiteral("zh"),Qt::CaseInsensitive);
    auto *trayMenu=new QMenu(&pet);
    auto *openSettings=trayMenu->addAction(zhUi ? QStringLiteral("Tony 设置…") : QStringLiteral("Tony Settings…"));
    QObject::connect(openSettings,&QAction::triggered,&app,showSettings);

    auto *openCreator=trayMenu->addAction(zhUi ? QStringLiteral("宠物与创作…") : QStringLiteral("Pets & Creator…"));
    QObject::connect(openCreator,&QAction::triggered,&app,showCreator);

    auto *quickReminder=trayMenu->addAction(zhUi ? QStringLiteral("快速提醒…") : QStringLiteral("Quick Reminder…"));
    QObject::connect(quickReminder,&QAction::triggered,&pet,&PetWindow::createQuickReminder);

    auto *autonomyMenu=trayMenu->addMenu(zhUi ? QStringLiteral("主动模式") : QStringLiteral("Autonomy"));
    auto *autonomyGroup=new QActionGroup(autonomyMenu);
    autonomyGroup->setExclusive(true);
    const QString activeMode=autonomy.mode();
    const auto addAutonomyAction=[&](const QString &mode,const QString &en,const QString &zh){
        auto *action=autonomyMenu->addAction(zhUi ? zh : en);
        action->setCheckable(true);
        action->setData(mode);
        action->setChecked(activeMode==mode);
        autonomyGroup->addAction(action);
        QObject::connect(action,&QAction::triggered,&app,[&autonomy,mode](bool checked){
            if(!checked) return;
            autonomy.setMode(mode);
            autonomy.announceMode();
        });
    };
    addAutonomyAction(QStringLiteral("off"),QStringLiteral("Off"),QStringLiteral("关闭"));
    addAutonomyAction(QStringLiteral("quiet"),QStringLiteral("Quiet"),QStringLiteral("安静"));
    addAutonomyAction(QStringLiteral("normal"),QStringLiteral("Normal"),QStringLiteral("正常"));
    addAutonomyAction(QStringLiteral("lively"),QStringLiteral("Lively"),QStringLiteral("活泼"));

    trayMenu->addSeparator();
    auto *quitAction=trayMenu->addAction(zhUi ? QStringLiteral("退出 Tony") : QStringLiteral("Quit Tony"));
    QObject::connect(quitAction,&QAction::triggered,&app,&QApplication::quit);
    pet.trayIcon()->setContextMenu(trayMenu);

    updater.scheduleStartupCheck();

    // First launch stays non-modal. An unpaired PetWindow automatically exposes
    // a short-lived device connection code after Tony is visibly on the desktop.
    // Settings/reconnect uses the same device-code flow; friend code is recovery-only.

    const int result = app.exec();
    qInfo() << "Tony Desktop Pet exiting" << result;
    return result;
}
