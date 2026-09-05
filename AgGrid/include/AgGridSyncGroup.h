#ifndef AGGRIDSYNCGROUP_H
#define AGGRIDSYNCGROUP_H

#include "AgGrid_global.h"

#include <QObject>
#include <QPointer>
#include <QList>

class AgGridWidget;

// 多表格视图同步：同组成员的滚动偏移与列宽互相跟随，
// 用于两个（或多个）相同数据表格的数据对比场景。
class AGGRID_EXPORT AgGridSyncGroup : public QObject
{
    Q_OBJECT

public:
    enum SyncFeature {
        NoSync = 0x0,
        SyncScroll = 0x1,        // 垂直/水平滚动偏移
        SyncColumnWidths = 0x2,  // 列宽（叶子列逐列拷贝）
        SyncAll = SyncScroll | SyncColumnWidths
    };
    Q_FLAG(SyncFeature)
    Q_DECLARE_FLAGS(SyncFeatures, SyncFeature)

    explicit AgGridSyncGroup(QObject *parent = nullptr);

    void addGrid(AgGridWidget *grid);
    void removeGrid(AgGridWidget *grid);
    void clear();
    int gridCount() const;
    bool contains(const AgGridWidget *grid) const;

    void setSyncEnabled(bool enabled);
    bool isSyncEnabled() const;

    void setFeatures(SyncFeatures features);
    SyncFeatures features() const;

    // 一次性把 source 的当前状态（滚动偏移、全部列宽）推送给其他成员
    void syncFrom(AgGridWidget *source);

private:
    void connectGrid(AgGridWidget *grid);
    void disconnectGrid(AgGridWidget *grid);
    void applyScroll(AgGridWidget *source, int value, bool vertical);
    void applyColumnWidths(AgGridWidget *source);

    QList<QPointer<AgGridWidget>> m_grids;
    bool m_syncEnabled;
    SyncFeatures m_features;
    bool m_applying;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AgGridSyncGroup::SyncFeatures)

#endif // AGGRIDSYNCGROUP_H
