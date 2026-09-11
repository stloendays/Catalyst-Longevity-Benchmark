#include <QApplication>
#include <QCoreApplication>
#include <QSettings>
#include <QTimer>
#include "PetWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("TonyAgent");
    QCoreApplication::setApplicationName("Tony Desktop Pet");
    app.setQuitOnLastWindowClosed(false);

    PetWindow pet;
    pet.show();

    // Trusted-friend flow:
    // - first launch: automatically ask only for the reusable friend code;
    // - after pairing: the per-device token is protected by Windows DPAPI;
    // - later launches: PetWindow connects automatically and AgentClient reconnects on outages.
    QSettings settings;
    if(settings.value("agent/token").toString().trimmed().isEmpty()) {
        QTimer::singleShot(700, &pet, &PetWindow::configureConnection);
    }

    return app.exec();
}
