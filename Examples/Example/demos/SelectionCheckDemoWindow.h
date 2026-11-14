#ifndef SELECTIONCHECKDEMOWINDOW_H
#define SELECTIONCHECKDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuGridModel;
class AuGridWidget;
class QPlainTextEdit;

/**
 * @brief 选中与勾选示例：行号列、复选框列、行可选回调、选中 API 与勾选信号。
 */
class SelectionCheckDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit SelectionCheckDemoWindow(QWidget *parent = nullptr);

private:
    AuGridWidget *m_grid;
    AuGridModel *m_model;
    QPlainTextEdit *m_log;
};

#endif // SELECTIONCHECKDEMOWINDOW_H
