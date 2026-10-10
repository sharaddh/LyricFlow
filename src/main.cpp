#include <QApplication>
#include <QCoreApplication>
#include "OverlayWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("LyricFlow");
    QCoreApplication::setApplicationName("LyricFlow");
    QApplication::setQuitOnLastWindowClosed(false);

    OverlayWindow overlay;
    overlay.show();
    return app.exec();
}
