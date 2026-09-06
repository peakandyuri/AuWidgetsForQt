#ifndef COLUMNDEMOWINDOW_H
#define COLUMNDEMOWINDOW_H

#include "DemoWindowBase.h"

class AgGridModel;
class AgGridWidget;

/**
 * @brief 列操作示例：列停靠、列显隐（含分组列）、列拖拽重排。
 */
class ColumnDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit ColumnDemoWindow(QWidget *parent = nullptr);

private:
    AgGridWidget *m_grid;
    AgGridModel *m_model;
};

#endif // COLUMNDEMOWINDOW_H
