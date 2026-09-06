#ifndef SPANDEMOWINDOW_H
#define SPANDEMOWINDOW_H

#include "DemoWindowBase.h"

class AgGridModel;
class AgGridWidget;

/**
 * @brief 合并单元格示例：类似 QTableView::setSpan 的行列合并。
 */
class SpanDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit SpanDemoWindow(QWidget *parent = nullptr);

private:
    AgGridWidget *m_grid;
    AgGridModel *m_model;
};

#endif // SPANDEMOWINDOW_H
