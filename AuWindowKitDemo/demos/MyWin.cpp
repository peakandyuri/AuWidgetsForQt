#include "MyWin.h"
#include "AuWindowStyle.h"

#include <QPushButton>

MyWin::MyWin(QWidget *parent)
    : AuWindow(parent)
{
    // 业务基类统一外观：所有继承 MyWin 的窗口自动获得
    setStyle(AuWindowStyle::createDarkStyle());
    setWindowRadius(8);
    setWindowTitle(QStringLiteral("MyWin 业务窗体"));
    setMinimumSize(420, 320);

    // 标题栏靠右放一个业务通用按钮（抢占鼠标，可点击）
    QPushButton *helpButton = new QPushButton(QStringLiteral("帮助"));
    helpButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:transparent; color:#ddd; border:1px solid #555;"
        "  border-radius:3px; padding:1px 10px; font-size:12px; }"
        "QPushButton:hover { background:#444; }"));
    addTitleBarWidget(helpButton, AuTitleBar::SlotBeforeButtons);
    connect(helpButton, &QPushButton::clicked, this, []() {
        qInfo("MyWin 通用帮助按钮被点击");
    });
}

FromUiWindow::FromUiWindow(QWidget *parent)
    : MyWin(parent)
{
    setWindowTitle(QStringLiteral("MyWin 子类（.ui 设计 + setupUi(this)）"));

    // Qt Designer 生成的 setupUi 将根布局安装在 this 上，
    // AuWindow 自动劫持该布局到标题栏下方内容区
    ui->setupUi(this);

    // setupUi 后仍可访问 ui 里的控件，与普通 QWidget 开发完全一致
    connect(ui->okButton, &QPushButton::clicked, this, [this]() {
        ui->outputLabel->setText(QStringLiteral("确定被点击：控件与信号全按普通 Qt 方式使用"));
    });
    connect(ui->cancelButton, &QPushButton::clicked, this, &FromUiWindow::close);
}
