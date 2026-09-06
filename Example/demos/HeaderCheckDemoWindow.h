#ifndef HEADCHECKDEMWINDOW_H
#define HEADCHECKDEMWINDOW_H

#include "DemoWindowBase.h"

class AgGridModel;
class AgGridWidget;
class QPlainTextEdit;

/**
 * @brief 表头全选示例：数据列复选框在列头上增加全选 / 半选组件。
 */
class HeaderCheckDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit HeaderCheckDemoWindow(QWidget *parent = nullptr);

private:
    AgGridWidget *m_grid;
    AgGridModel *m_model;
    QPlainTextEdit *m_log;
};

#endif // HEADCHECKDEMWINDOW_H
