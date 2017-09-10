#include <QtGui/QApplication>
#include "appcontroller.h"
#include "settings.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Settings::setDefaults();

    AppController app;
    app.show();

    return a.exec();
}
