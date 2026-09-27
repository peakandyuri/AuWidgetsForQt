#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/app.svg"));

    MainWindow win;
    win.resize(960, 640);
    win.show();

    return app.exec();
}
