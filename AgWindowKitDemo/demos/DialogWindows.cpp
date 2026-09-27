#include "DialogWindows.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

DialogWindow::DialogWindow(AgWindow::Modality modality, QWidget *parent)
    : AgWindow(parent)
{
    setDialogMode(true);          // Qt::Dialog 形态：居中于父窗口 + 系统对话框所有权
    setModality(modality);        // 模态范围
    setWindowTitle(QStringLiteral("AgWindow 对话框"));
    setWindowRadius(8);
    setButtonVisible(AgTitleBar::ButtonMaximize, false);   // 对话框通常不给最大化
    setButtonVisible(AgTitleBar::ButtonMinimize, false);
    setMinimumSize(380, 220);
    resize(420, 240);

    QWidget *content = new QWidget;
    QVBoxLayout *lay = new QVBoxLayout(content);
    lay->setContentsMargins(24, 20, 24, 16);
    lay->setSpacing(16);

    m_message = new QLabel(QStringLiteral("这是 setDialogMode(true) + exec() 的模态对话框。\n"
                                           "Windows 上模态时点击父窗口会触发系统提示音，"
                                           "父窗口标题栏闪烁（操作系统行为）。"));
    m_message->setWordWrap(true);
    lay->addWidget(m_message);
    lay->addStretch(1);

    QHBoxLayout *buttonRow = new QHBoxLayout;
    QPushButton *cancel = new QPushButton(QStringLiteral("取消"));
    QPushButton *ok = new QPushButton(QStringLiteral("确定"));
    connect(cancel, &QPushButton::clicked, this, &DialogWindow::reject);
    connect(ok, &QPushButton::clicked, this, &DialogWindow::accept);
    buttonRow->addStretch(1);
    buttonRow->addWidget(cancel);
    buttonRow->addWidget(ok);
    lay->addItem(buttonRow);

    setCentralWidget(content);
}
