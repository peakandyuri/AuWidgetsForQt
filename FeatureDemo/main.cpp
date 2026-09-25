#include "MainWindow.h"

#include "AgDatePickerStyle.h"
#include "AgGridStyle.h"
#include "AgSelectStyle.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 全局样式：三个控件各设一套亮色主题，各演示页默认跟随全局；
    // "主题与尺寸" 演示页内再演示控件独立样式。
    AgGridStyle::setGlobalStyle(AgGridStyle::createLightStyle());
    AgSelectStyle::setGlobalStyle(AgSelectStyle::createLightStyle());
    AgDatePickerStyle::setGlobalStyle(AgDatePickerStyle::createLightStyle());

    MainWindow window;
    window.show();
    return app.exec();
}
