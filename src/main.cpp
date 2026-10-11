#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include "OverlayWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Capson");
    QCoreApplication::setApplicationName("Capson");
    QApplication::setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(QStringLiteral(":/app.ico")));

    OverlayWindow overlay;
    overlay.show();
    return app.exec();
}
