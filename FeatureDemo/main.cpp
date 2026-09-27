#include "MainWindow.h"

#include "AuDatePickerStyle.h"
#include "AuGridStyle.h"
#include "AuSelectStyle.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 全局样式：三个控件各设一套亮色主题，各演示页默认跟随全局；
    // "主题与尺寸" 演示页内再演示控件独立样式。
    AuGridStyle::setGlobalStyle(AuGridStyle::createLightStyle());
    AuSelectStyle::setGlobalStyle(AuSelectStyle::createLightStyle());
    AuDatePickerStyle::setGlobalStyle(AuDatePickerStyle::createLightStyle());

    MainWindow window;
    window.show();
    return app.exec();
}
