#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "AgWindow.h"

#include <QPushButton>

/**
 * @brief 演示主窗：AgWindow 基本用法 + 各特性入口。
 *
 * 覆盖：标题栏自定义组件三槽位（抢占 / 非抢占）、亮暗主题切换、
 * 圆角、按钮显隐、任务栏显隐、关闭拦截、Dialog 模态演示、
 * 继承定制（MyWin + .ui）演示。
 */
class MainWindow : public AgWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildContent();
    QWidget *buildWindowControlPage();   // 窗体自身特性
    QWidget *buildDialogPage();          // Dialog / 模态
    QWidget *buildInheritPage();         // 继承定制 + ui 文件

private slots:
    void switchGlobalTheme();            // 亮 / 暗全局样式切换
    void onCloseIntercepted();

private:
    QWidget *m_center = nullptr;
    QPushButton *m_themeButton = nullptr;
};

#endif // MAINWINDOW_H
