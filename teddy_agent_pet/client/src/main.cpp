#include <QApplication>
#include <QCoreApplication>
#include <QString>
#include "PetWindow.h"
#include "UpdateManager.h"

#ifndef TONY_APP_VERSION
#error "TONY_APP_VERSION must be provided by CMake"
#endif

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("TonyAgent");
    QCoreApplication::setApplicationName("Tony Desktop Pet");
    QCoreApplication::setApplicationVersion(QString::fromLatin1(TONY_APP_VERSION));
    app.setQuitOnLastWindowClosed(false);

    PetWindow pet;
    UpdateManager updater(&pet);
    pet.show();
    updater.scheduleAutomaticCheck();

    return app.exec();
}
