#include "AgGridSyncGroup.h"

#include "AgGridWidget.h"
#include "AgGridModel.h"

AgGridSyncGroup::AgGridSyncGroup(QObject *parent)
    : QObject(parent)
    , m_syncEnabled(true)
    , m_features(SyncAll)
    , m_applying(false)
{
}

void AgGridSyncGroup::addGrid(AgGridWidget *grid)
{
    if (!grid || contains(grid))
        return;

    m_grids.append(QPointer<AgGridWidget>(grid));
    connectGrid(grid);
}

void AgGridSyncGroup::removeGrid(AgGridWidget *grid)
{
    if (!grid)
        return;

    disconnectGrid(grid);
    for (int i = m_grids.size() - 1; i >= 0; --i) {
        if (m_grids.at(i).data() == grid)
            m_grids.removeAt(i);
    }
}

void AgGridSyncGroup::clear()
{
    for (const auto &grid : m_grids) {
        if (grid.data())
            disconnectGrid(grid.data());
    }
    m_grids.clear();
}

int AgGridSyncGroup::gridCount() const
{
    return m_grids.size();
}

bool AgGridSyncGroup::contains(const AgGridWidget *grid) const
{
    if (!grid)
        return false;
    for (const auto &ptr : m_grids) {
        if (ptr.data() == grid)
            return true;
    }
    return false;
}

void AgGridSyncGroup::setSyncEnabled(bool enabled)
{
    m_syncEnabled = enabled;
}

bool AgGridSyncGroup::isSyncEnabled() const
{
    return m_syncEnabled;
}

void AgGridSyncGroup::setFeatures(SyncFeatures features)
{
    m_features = features;
}

AgGridSyncGroup::SyncFeatures AgGridSyncGroup::features() const
{
    return m_features;
}

void AgGridSyncGroup::syncFrom(AgGridWidget *source)
{
    if (!source || !m_syncEnabled || m_applying)
        return;

    m_applying = true;
    for (const auto &grid : m_grids) {
        AgGridWidget *target = grid.data();
        if (!target || target == source)
            continue;
        if (m_features & SyncScroll) {
            target->setVerticalOffset(source->verticalOffset());
            target->setHorizontalOffset(source->horizontalOffset());
        }
    }
    if (m_features & SyncColumnWidths)
        applyColumnWidths(source);
    m_applying = false;
}

void AgGridSyncGroup::connectGrid(AgGridWidget *grid)
{
    connect(grid, &AgGridWidget::verticalScrollChanged, this, [this](int value) {
        applyScroll(qobject_cast<AgGridWidget *>(sender()), value, true);
    });
    connect(grid, &AgGridWidget::horizontalScrollChanged, this, [this](int value) {
        applyScroll(qobject_cast<AgGridWidget *>(sender()), value, false);
    });
    connect(grid, &AgGridWidget::columnWidthsChanged, this, [this]() {
        applyColumnWidths(qobject_cast<AgGridWidget *>(sender()));
    });
    // 对象销毁时清理成员表（QPointer 已清空的无效条目顺带剔除）
    connect(grid, &QObject::destroyed, this, [this, grid]() {
        for (int i = m_grids.size() - 1; i >= 0; --i) {
            AgGridWidget *entry = m_grids.at(i).data();
            if (entry == grid || entry == nullptr)
                m_grids.removeAt(i);
        }
    });
}

void AgGridSyncGroup::disconnectGrid(AgGridWidget *grid)
{
    disconnect(grid, nullptr, this, nullptr);
}

void AgGridSyncGroup::applyScroll(AgGridWidget *source, int value, bool vertical)
{
    if (!source || !m_syncEnabled || m_applying || !(m_features & SyncScroll))
        return;

    // QScrollBar::setValue 值不变时不会再次发信号，配合 m_applying 防止回环
    m_applying = true;
    for (const auto &grid : m_grids) {
        AgGridWidget *target = grid.data();
        if (!target || target == source)
            continue;
        if (vertical)
            target->setVerticalOffset(value);
        else
            target->setHorizontalOffset(value);
    }
    m_applying = false;
}

void AgGridSyncGroup::applyColumnWidths(AgGridWidget *source)
{
    if (!source || !source->model() || !m_syncEnabled || m_applying || !(m_features & SyncColumnWidths))
        return;

    AgGridModel *sourceModel = source->model();
    const int sourceCount = sourceModel->columnCount();

    m_applying = true;
    for (const auto &grid : m_grids) {
        AgGridWidget *target = grid.data();
        AgGridModel *targetModel = target && target != source ? target->model() : nullptr;
        if (!targetModel)
            continue;

        const int count = qMin(sourceCount, targetModel->columnCount());
        for (int i = 0; i < count; ++i)
            targetModel->setColumnWidth(i, sourceModel->columnWidth(i));
    }
    m_applying = false;
}
