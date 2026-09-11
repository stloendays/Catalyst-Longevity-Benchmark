#include <QApplication>
#include <QCoreApplication>
#include "PetWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("TonyAgent");
    QCoreApplication::setApplicationName("Tony Desktop Pet");
    app.setQuitOnLastWindowClosed(false);
    PetWindow pet;
    pet.show();
    return app.exec();
}
