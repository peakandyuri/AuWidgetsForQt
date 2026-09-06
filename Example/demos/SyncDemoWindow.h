#ifndef SYNCDEMOWINDOW_H
#define SYNCDEMOWINDOW_H

#include "DemoWindowBase.h"

class AgGridModel;
class AgGridSyncGroup;
class AgGridWidget;

/**
 * @brief 多表格同步示例：滚动与列宽在多个表格间互相同步（数据对比场景）。
 */
class SyncDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit SyncDemoWindow(QWidget *parent = nullptr);

private:
    AgGridWidget *m_gridA;
    AgGridWidget *m_gridB;
    AgGridModel *m_modelA;
    AgGridModel *m_modelB;
    AgGridSyncGroup *m_syncGroup;
};

#endif // SYNCDEMOWINDOW_H
