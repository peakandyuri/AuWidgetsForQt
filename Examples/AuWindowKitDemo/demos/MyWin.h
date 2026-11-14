#ifndef MYWIN_H
#define MYWIN_H

#include "AuWindow.h"
#include "ui_MyWinContent.h"

/**
 * @brief 业务窗体基类示例：统一定制 AuWindow 的外观与行为。
 *
 * 工程中的做法：业务窗体基类继承 AuWindow，在构造中集中设定主题、圆角、
 * 标题栏按钮与自定义组件；业务窗口再继承 MyWin，可直接 setupUi(this)。
 */
class MyWin : public AuWindow
{
    Q_OBJECT

public:
    explicit MyWin(QWidget *parent = nullptr);
};

/**
 * @brief 用 Qt Designer 设计、setupUi(this) 装载的 MyWin 子类。
 *
 * .ui 文件的根布局安装在窗体（this）上，AuWindow 会自动把该布局
 * 连同控件转移到标题栏下方的内容区。
 */
class FromUiWindow : public MyWin
{
    Q_OBJECT

public:
    explicit FromUiWindow(QWidget *parent = nullptr);

private:
    Ui::MyWinContent *ui = new Ui::MyWinContent();
};

#endif // MYWIN_H
