#ifndef DIALOGWINDOWS_H
#define DIALOGWINDOWS_H

#include "AuWindow.h"

class QLabel;

/**
 * @brief 演示用对话框：Dialog 模式 + exec 模态。
 *
 * setDialogMode(true) 使窗体挂 Qt::Dialog 标志（作为父窗口的对话框：
 * 居中于父窗口、Windows 上与父窗口建立系统对话框所有权关系），
 * setModality() 决定阻塞范围，exec() 进入模态事件循环。
 * exec() 期间回车触发 setDefault(true) 的默认按钮，Esc 触发 reject()。
 */
class DialogWindow : public AuWindow
{
    Q_OBJECT

public:
    explicit DialogWindow(AuWindow::Modality modality, QWidget *parent = nullptr);

private:
    QLabel *m_message = nullptr;
};

#endif // DIALOGWINDOWS_H
