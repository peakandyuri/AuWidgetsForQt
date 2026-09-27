#ifndef SYNCDEMOWINDOW_H
#define SYNCDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuGridModel;
class AuGridSyncGroup;
class AuGridWidget;

/**
 * @brief 多表格同步示例：滚动与列宽在多个表格间互相同步（数据对比场景）。
 */
class SyncDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit SyncDemoWindow(QWidget *parent = nullptr);

private:
    AuGridWidget *m_gridA;
    AuGridWidget *m_gridB;
    AuGridModel *m_modelA;
    AuGridModel *m_modelB;
    AuGridSyncGroup *m_syncGroup;
};

#endif // SYNCDEMOWINDOW_H
