#ifndef SPANDEMOWINDOW_H
#define SPANDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuGridModel;
class AuGridWidget;

/**
 * @brief 合并单元格示例：类似 QTableView::setSpan 的行列合并。
 */
class SpanDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit SpanDemoWindow(QWidget *parent = nullptr);

private:
    AuGridWidget *m_grid;
    AuGridModel *m_model;
};

#endif // SPANDEMOWINDOW_H
