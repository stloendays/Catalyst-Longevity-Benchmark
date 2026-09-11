#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QKeySequence>
#include <QMouseEvent>
#include <QSettings>
#include <QShortcut>
#include <QTimer>
#include <QToolButton>

#include "AppLogger.h"
#include "PetWindow.h"
#include "SettingsDialog.h"
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
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("TonyAgent");
    QCoreApplication::setApplicationName("Tony Desktop Pet");
    QCoreApplication::setApplicationVersion(QStringLiteral(TONY_APP_VERSION));
    app.setQuitOnLastWindowClosed(false);

    AppLogger::install();
    qInfo().noquote() << "Tony Desktop Pet starting" << QCoreApplication::applicationVersion();

    PetWindow pet;
    PetActivityFilter activityFilter;
    pet.installEventFilter(&activityFilter);
    pet.show();

    UpdateManager updater(&app);
    SettingsDialog settingsDialog(&updater, &pet);
    settingsDialog.setModal(false);

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

    // A small, quiet settings affordance on the pet itself. It avoids another
    // tray icon and keeps the settings page discoverable without changing Tony's calm idle behavior.
    QToolButton settingsButton(&pet);
    settingsButton.setText(QStringLiteral("⚙"));
    settingsButton.setToolTip(QStringLiteral("Settings"));
    settingsButton.setFixedSize(26, 26);
    settingsButton.move(pet.width() - settingsButton.width() - 5, 5);
    settingsButton.setCursor(Qt::PointingHandCursor);
    settingsButton.setStyleSheet(QStringLiteral(
        "QToolButton { background: rgba(20,20,20,95); color: white; border: 0; border-radius: 13px; font-size: 14px; }"
        "QToolButton:hover { background: rgba(20,20,20,150); }"));
    settingsButton.show();

    const auto showSettings = [&settingsDialog]{
        AppLogger::recordOperatorEvent(QStringLiteral("settings_open"));
        settingsDialog.show();
        settingsDialog.raise();
        settingsDialog.activateWindow();
    };
    QObject::connect(&settingsButton, &QToolButton::clicked, &app, showSettings);

    auto *settingsShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+,")), &pet);
    settingsShortcut->setContext(Qt::ApplicationShortcut);
    QObject::connect(settingsShortcut, &QShortcut::activated, &app, showSettings);

    updater.scheduleStartupCheck();

    // Trusted-friend flow:
    // - first launch: automatically ask only for the reusable friend code;
    // - after pairing: the per-device token is protected by Windows DPAPI;
    // - later launches: PetWindow connects automatically and AgentClient reconnects on outages.
    QSettings settings;
    if(settings.value("agent/token").toString().trimmed().isEmpty()) {
        QTimer::singleShot(700, &pet, &PetWindow::configureConnection);
    }

    const int result = app.exec();
    qInfo() << "Tony Desktop Pet exiting" << result;
    return result;
}
