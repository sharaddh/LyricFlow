#include <QApplication>
#include <QCoreApplication>
#include "OverlayWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Capson");
    QCoreApplication::setApplicationName("Capson");
    QApplication::setQuitOnLastWindowClosed(false);

    OverlayWindow overlay;
    overlay.show();
    return app.exec();
}
