#ifndef AUGRIDSYNCGROUP_H
#define AUGRIDSYNCGROUP_H

#include "AuGrid_global.h"

#include <QObject>
#include <QPointer>
#include <QList>

class AuGridWidget;

// 多表格视图同步：同组成员的滚动偏移与列宽互相跟随，
// 用于两个（或多个）相同数据表格的数据对比场景。

/**
 * @brief 多表格视图同步组：成员表格的滚动与列宽互相跟随。
 *
 * 典型场景：左右两个表格加载相同数据做逐行对比，滚动任一表格
 * 另一个跟随，调宽任一列另一边同步。
 *
 * 成员用 QPointer 持有，控件销毁时自动从组内失效。
 * 应用示例见 Example/mainwindow.cpp。
 *
 * @code
 *   AuGridSyncGroup *sync = new AuGridSyncGroup(this);
 *   sync->addGrid(gridA);
 *   sync->addGrid(gridB);
 *   sync->setFeatures(AuGridSyncGroup::SyncAll);
 *   sync->syncFrom(gridA);   // 初始对齐
 * @endcode
 */
class AUGRID_EXPORT AuGridSyncGroup : public QObject
{
    Q_OBJECT

public:
    /// 同步特性（可按位组合）。
    enum SyncFeature {
        NoSync = 0x0,
        SyncScroll = 0x1,        ///< 垂直/水平滚动偏移
        SyncColumnWidths = 0x2,  ///< 列宽（叶子列逐列拷贝）
        SyncAll = SyncScroll | SyncColumnWidths  ///< 全部特性
    };
    Q_FLAG(SyncFeature)
    Q_DECLARE_FLAGS(SyncFeatures, SyncFeature)

    explicit AuGridSyncGroup(QObject *parent = nullptr);

    /// 把表格加入同步组。
    void addGrid(AuGridWidget *grid);
    /// 把表格移出同步组。
    void removeGrid(AuGridWidget *grid);
    /// 清空全部成员。
    void clear();
    /// 当前成员数量。
    int gridCount() const;
    /// 是否包含某表格。
    bool contains(const AuGridWidget *grid) const;

    /// 同步总开关（暂停 / 恢复同步，不影响成员关系）。
    void setSyncEnabled(bool enabled);
    bool isSyncEnabled() const;

    /// 设置同步特性组合（滚动 / 列宽）。
    void setFeatures(SyncFeatures features);
    /// 当前同步特性。
    SyncFeatures features() const;

    // 一次性把 source 的当前状态（滚动偏移、全部列宽）推送给其他成员
    /// 一次性把 source 的滚动偏移与全部列宽推送给组内其他成员（初始对齐用）。
    void syncFrom(AuGridWidget *source);

private:
    /// 建立 / 断开与某成员的信号连接。
    void connectGrid(AuGridWidget *grid);
    void disconnectGrid(AuGridWidget *grid);
    /// 把 source 的滚动偏移应用到其他成员。
    void applyScroll(AuGridWidget *source, int value, bool vertical);
    /// 把 source 的列宽应用到其他成员。
    void applyColumnWidths(AuGridWidget *source);

    QList<QPointer<AuGridWidget>> m_grids;  ///< 成员列表（弱引用）
    bool m_syncEnabled;                     ///< 同步总开关
    SyncFeatures m_features;                ///< 同步特性
    bool m_applying;                        ///< 应用中标记（防止成员间互相触发死循环）
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AuGridSyncGroup::SyncFeatures)

#endif // AUGRIDSYNCGROUP_H
