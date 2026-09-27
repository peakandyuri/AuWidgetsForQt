#include "AuGridSyncGroup.h"

#include "AuGridWidget.h"
#include "AuGridModel.h"

AuGridSyncGroup::AuGridSyncGroup(QObject *parent)
    : QObject(parent)
    , m_syncEnabled(true)
    , m_features(SyncAll)
    , m_applying(false)
{
}

void AuGridSyncGroup::addGrid(AuGridWidget *grid)
{
    if (!grid || contains(grid))
        return;

    m_grids.append(QPointer<AuGridWidget>(grid));
    connectGrid(grid);
}

void AuGridSyncGroup::removeGrid(AuGridWidget *grid)
{
    if (!grid)
        return;

    disconnectGrid(grid);
    for (int i = m_grids.size() - 1; i >= 0; --i) {
        if (m_grids.at(i).data() == grid)
            m_grids.removeAt(i);
    }
}

void AuGridSyncGroup::clear()
{
    for (const auto &grid : m_grids) {
        if (grid.data())
            disconnectGrid(grid.data());
    }
    m_grids.clear();
}

int AuGridSyncGroup::gridCount() const
{
    return m_grids.size();
}

bool AuGridSyncGroup::contains(const AuGridWidget *grid) const
{
    if (!grid)
        return false;
    for (const auto &ptr : m_grids) {
        if (ptr.data() == grid)
            return true;
    }
    return false;
}

void AuGridSyncGroup::setSyncEnabled(bool enabled)
{
    m_syncEnabled = enabled;
}

bool AuGridSyncGroup::isSyncEnabled() const
{
    return m_syncEnabled;
}

void AuGridSyncGroup::setFeatures(SyncFeatures features)
{
    m_features = features;
}

AuGridSyncGroup::SyncFeatures AuGridSyncGroup::features() const
{
    return m_features;
}

void AuGridSyncGroup::syncFrom(AuGridWidget *source)
{
    if (!source || !m_syncEnabled || m_applying)
        return;

    m_applying = true;
    for (const auto &grid : m_grids) {
        AuGridWidget *target = grid.data();
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

void AuGridSyncGroup::connectGrid(AuGridWidget *grid)
{
    connect(grid, &AuGridWidget::verticalScrollChanged, this, [this](int value) {
        applyScroll(qobject_cast<AuGridWidget *>(sender()), value, true);
    });
    connect(grid, &AuGridWidget::horizontalScrollChanged, this, [this](int value) {
        applyScroll(qobject_cast<AuGridWidget *>(sender()), value, false);
    });
    connect(grid, &AuGridWidget::columnWidthsChanged, this, [this]() {
        applyColumnWidths(qobject_cast<AuGridWidget *>(sender()));
    });
    // 对象销毁时清理成员表（QPointer 已清空的无效条目顺带剔除）
    connect(grid, &QObject::destroyed, this, [this, grid]() {
        for (int i = m_grids.size() - 1; i >= 0; --i) {
            AuGridWidget *entry = m_grids.at(i).data();
            if (entry == grid || entry == nullptr)
                m_grids.removeAt(i);
        }
    });
}

void AuGridSyncGroup::disconnectGrid(AuGridWidget *grid)
{
    disconnect(grid, nullptr, this, nullptr);
}

void AuGridSyncGroup::applyScroll(AuGridWidget *source, int value, bool vertical)
{
    if (!source || !m_syncEnabled || m_applying || !(m_features & SyncScroll))
        return;

    // QScrollBar::setValue 值不变时不会再次发信号，配合 m_applying 防止回环
    m_applying = true;
    for (const auto &grid : m_grids) {
        AuGridWidget *target = grid.data();
        if (!target || target == source)
            continue;
        if (vertical)
            target->setVerticalOffset(value);
        else
            target->setHorizontalOffset(value);
    }
    m_applying = false;
}

void AuGridSyncGroup::applyColumnWidths(AuGridWidget *source)
{
    if (!source || !source->model() || !m_syncEnabled || m_applying || !(m_features & SyncColumnWidths))
        return;

    AuGridModel *sourceModel = source->model();
    const int sourceCount = sourceModel->columnCount();

    m_applying = true;
    for (const auto &grid : m_grids) {
        AuGridWidget *target = grid.data();
        AuGridModel *targetModel = target && target != source ? target->model() : nullptr;
        if (!targetModel)
            continue;

        const int count = qMin(sourceCount, targetModel->columnCount());
        for (int i = 0; i < count; ++i)
            targetModel->setColumnWidth(i, sourceModel->columnWidth(i));
    }
    m_applying = false;
}
