#include <QApplication>
#include <QCoreApplication>
#include "PetWindow.h"
#include "UpdateManager.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("TonyAgent");
    QCoreApplication::setApplicationName("Tony Desktop Pet");
    QCoreApplication::setApplicationVersion("0.8.4");
    app.setQuitOnLastWindowClosed(false);

    PetWindow pet;
    UpdateManager updater(&pet);
    pet.show();
    updater.scheduleAutomaticCheck();

    return app.exec();
}
