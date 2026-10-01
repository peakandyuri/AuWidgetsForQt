// ============================================================================
// AuGridWidget 行实现：复选框勾选（含树形级联三态）、树形展开 / 折叠
// （含动画）、排序 / 过滤、行拖拽重排（浮动提示 / 自动滚动 / 重排动画）。
// ============================================================================

#include "AuGridWidget.h"
#include "AuScrollBar.h"

#include <QCursor>
#include <QDebug>
#include <QLabel>
#include <QTimer>
#include <QVariantAnimation>

void AuGridWidget::toggleCheckboxHeader()
{
    if (!m_model)
        return;

    // 树形模式：按原始行全集判断全选 / 清空
    if (m_model->isTreeMode()) {
        const int checkableCount = checkableOriginalRowCount();
        if (checkableCount == 0)
            return;
        if (checkedCheckableOriginalCount() == checkableCount)
            clearAllCheckedRows();
        else
            selectAllRows();
        return;
    }

    const int checkableCount = checkableRowCount();
    if (checkableCount == 0)
        return;

    if (checkedRows().size() == checkableCount)
        clearAllCheckedRows();
    else
        selectAllRows();
}

void AuGridWidget::toggleCheckboxCell(int row)
{
    if (row < 0 || !m_model || row >= m_model->rowCount())
        return;

    if (!isRowCheckable(row))
        return;

    setRowChecked(row, !isRowChecked(row));
}

void AuGridWidget::toggleSort(int column)
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return;

    int newColumn = -1;
    Qt::SortOrder newOrder = Qt::AscendingOrder;

    if (m_sortedColumn == column) {
        if (m_sortOrder == Qt::AscendingOrder) {
            newColumn = column;
            newOrder = Qt::DescendingOrder;
        }
    } else {
        newColumn = column;
    }

    if (newColumn < 0) {
        clearSort();
    } else {
        m_sortRules.clear();
        m_sortRules << AuSortRule{newColumn, newOrder};
        m_model->sort(m_sortRules);
        m_sortedColumn = newColumn;
        m_sortOrder = newOrder;
        update();
        emit sortChanged(m_sortedColumn, m_sortOrder);
    }
}

bool AuGridWidget::isRowChecked(int row) const
{
    if (row < 0 || !m_model || row >= m_model->rowCount())
        return false;
    const int originalRow = m_model->originalRowForVisibleRow(row);
    return originalRow >= 0 && m_checkedOriginalRows.contains(originalRow);
}

void AuGridWidget::setRowChecked(int row, bool checked)
{
    if (row < 0 || !m_model || row >= m_model->rowCount())
        return;

    if (checked && !isRowCheckable(row))
        return;

    const int originalRow = m_model->originalRowForVisibleRow(row);
    if (originalRow < 0)
        return;

    // 树形模式：勾选父行级联全部子行，祖先按子行聚合出三态
    if (m_model->isTreeMode()) {
        const Qt::CheckState state = rowCheckStateOriginal(originalRow);
        if (state == Qt::Checked && checked)
            return;
        if (state == Qt::Unchecked && !checked)
            return;
        cascadeRowCheck(originalRow, checked);
        update();
        emit rowCheckedChanged(row, checked);
        emit checkedStateChanged();
        return;
    }

    const bool already = m_checkedOriginalRows.contains(originalRow);
    if (already == checked)
        return;

    if (checked)
        m_checkedOriginalRows.insert(originalRow);
    else
        m_checkedOriginalRows.remove(originalRow);

    update();
    emit rowCheckedChanged(row, checked);
    emit checkedStateChanged();
}

bool AuGridWidget::isRowPartiallyChecked(int row) const
{
    if (row < 0 || !m_model || !m_model->isTreeMode() || row >= m_model->rowCount())
        return false;
    const int originalRow = m_model->originalRowForVisibleRow(row);
    return originalRow >= 0 && m_partialOriginalRows.contains(originalRow);
}

bool AuGridWidget::isRowCheckable(int row) const
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return false;
    if (m_rowCheckableCallback)
        return m_rowCheckableCallback(m_model, row);
    return true;
}

void AuGridWidget::setRowCheckableCallback(const std::function<bool(const AuGridModel *, int)> &callback)
{
    m_rowCheckableCallback = callback;

    QSet<int> removedOriginal;
    if (m_model && m_model->isTreeMode()) {
        // 树形模式按原始行全量裁决（折叠不可见的行也参与）
        for (int originalRow = 0; originalRow < m_model->originalRowCount(); ++originalRow) {
            if (!isRowCheckableOriginal(originalRow)) {
                if (m_checkedOriginalRows.remove(originalRow))
                    removedOriginal.insert(originalRow);
                m_partialOriginalRows.remove(originalRow);
            }
        }
        recomputeAllCheckStates();
    } else {
        for (int row = 0; row < m_model->rowCount(); ++row) {
            if (!isRowCheckable(row)) {
                const int originalRow = m_model->originalRowForVisibleRow(row);
                if (originalRow >= 0 && m_checkedOriginalRows.remove(originalRow))
                    removedOriginal.insert(originalRow);
            }
        }
    }

    update();
    if (!removedOriginal.isEmpty())
        emit checkedStateChanged();
}

int AuGridWidget::checkableRowCount() const
{
    if (!m_model)
        return 0;
    if (!m_rowCheckableCallback)
        return m_model->rowCount();

    int count = 0;
    const int total = m_model->rowCount();
    for (int i = 0; i < total; ++i) {
        if (isRowCheckable(i))
            ++count;
    }
    return count;
}

void AuGridWidget::selectAllRows()
{
    if (!m_model)
        return;

    // 树形模式：按原始行全量勾选（折叠不可见的行同样勾选）
    if (m_model->isTreeMode()) {
        const int total = m_model->originalRowCount();
        const int checkableCount = checkableOriginalRowCount();
        if (total == 0 || checkableCount == 0 || m_checkedOriginalRows.size() == checkableCount)
            return;
        m_checkedOriginalRows.clear();
        m_partialOriginalRows.clear();
        for (int i = 0; i < total; ++i) {
            if (isRowCheckableOriginal(i))
                m_checkedOriginalRows.insert(i);
        }
        update();
        emit checkedStateChanged();
        return;
    }

    const int count = m_model->rowCount();
    const int checkableCount = checkableRowCount();
    if (count == 0 || checkableCount == 0 || checkedRows().size() == checkableCount)
        return;

    m_checkedOriginalRows.clear();
    for (int i = 0; i < count; ++i) {
        if (isRowCheckable(i)) {
            const int originalRow = m_model->originalRowForVisibleRow(i);
            if (originalRow >= 0)
                m_checkedOriginalRows.insert(originalRow);
        }
    }

    update();
    emit checkedStateChanged();
}

void AuGridWidget::clearAllCheckedRows()
{
    if (m_checkedOriginalRows.isEmpty() && m_partialOriginalRows.isEmpty())
        return;

    m_checkedOriginalRows.clear();
    m_partialOriginalRows.clear();
    update();
    emit checkedStateChanged();
}

QSet<int> AuGridWidget::checkedRows() const
{
    QSet<int> result;
    if (!m_model)
        return result;

    const int count = m_model->rowCount();
    for (int row = 0; row < count; ++row) {
        const int originalRow = m_model->originalRowForVisibleRow(row);
        if (originalRow >= 0 && m_checkedOriginalRows.contains(originalRow))
            result.insert(row);
    }
    return result;
}

QSet<int> AuGridWidget::checkedOriginalRows() const
{
    return m_checkedOriginalRows;
}

// ---- 树形表格 ----

bool AuGridWidget::isTreeMode() const
{
    return m_model && m_model->isTreeMode();
}

int AuGridWidget::treeColumn() const
{
    if (!m_model || !m_model->isTreeMode())
        return -1;
    const int count = m_model->columnCount();
    for (int col = 0; col < count; ++col) {
        if (!isColumnHidden(col))
            return col;
    }
    return -1;
}

int AuGridWidget::treeLeadingWidth(int depth) const
{
    const int indentWidth = m_style->integer(AuGridStyle::TreeIndentWidth, m_model);
    const int expanderSize = m_style->integer(AuGridStyle::TreeExpanderSize, m_model);
    // 与 AuDefaultCellRenderer::paint 的占位公式保持一致
    return depth * indentWidth + expanderSize + qMax(4, expanderSize / 4);
}

QRect AuGridWidget::treeExpanderRect(int row) const
{
    if (!isTreeMode() || row < 0 || !m_model || row >= m_model->rowCount())
        return QRect();

    const int col = treeColumn();
    if (col < 0)
        return QRect();

    const int originalRow = m_model->originalRowForVisibleRow(row);
    if (originalRow < 0 || !m_model->rowHasChildren(originalRow))
        return QRect();

    const QRect crect = cellRect(row, col);
    if (!crect.isValid())
        return QRect();

    // 箭头盒几何与 AuDefaultCellRenderer::paint 保持一致：
    // x = 格左 + 水平内边距 + depth * 缩进宽，垂直居中，边长 TreeExpanderSize
    const int hPadding = m_style->integer(AuGridStyle::CellHorizontalPadding, m_model, row, col);
    const int indentWidth = m_style->integer(AuGridStyle::TreeIndentWidth, m_model, row, col);
    const int expanderSize = m_style->integer(AuGridStyle::TreeExpanderSize, m_model, row, col);
    const int depth = m_model->rowDepth(originalRow);
    const int x = crect.left() + hPadding + depth * indentWidth;
    const int y = crect.center().y() - expanderSize / 2;
    return QRect(x, y, expanderSize, expanderSize);
}

void AuGridWidget::toggleRowExpanded(int row)
{
    if (!m_model || !m_model->isTreeMode() || row < 0 || row >= m_model->rowCount())
        return;
    const int originalRow = m_model->originalRowForVisibleRow(row);
    if (originalRow < 0 || !m_model->rowHasChildren(originalRow))
        return;
    setRowExpandedAnimated(row, !m_model->isRowExpanded(originalRow));
}

void AuGridWidget::setRowExpanded(int row, bool expanded)
{
    if (!m_model || !m_model->isTreeMode() || row < 0 || row >= m_model->rowCount())
        return;
    setRowExpandedAnimated(row, expanded);
}

void AuGridWidget::setRowExpandedAnimated(int row, bool expanded)
{
    if (!m_model || !m_model->isTreeMode()
        || row < 0 || row >= m_model->rowCount())
        return;
    const int originalRow = m_model->originalRowForVisibleRow(row);
    if (originalRow < 0 || !m_model->rowHasChildren(originalRow))
        return;
    if (m_model->isRowExpanded(originalRow) == expanded)
        return;

    // 上一次动画尚未结束：立即结算（折叠路径在此应用模型），避免快照错乱
    finishTreeAnimation();

    // 快照动画前的可见顺序（original 行号按可见顺序）与行偏移
    if (m_rowOffsetsDirty)
        rebuildRowOffsets();
    QVector<int> oldOrder;
    QVector<int> oldOffsets;
    oldOrder.reserve(m_model->rowCount());
    oldOffsets.reserve(m_model->rowCount());
    for (int v = 0; v < m_model->rowCount(); ++v) {
        oldOrder.append(m_model->originalRowForVisibleRow(v));
        oldOffsets.append(v < m_rowOffsets.size() ? m_rowOffsets.at(v) : 0);
    }

    m_treeAnimationEntries.clear();
    m_treeAnimationFadeRows.clear();
    m_treeAnimationMoveRows.clear();
    m_treeAnimationExpanding = expanded;
    m_treeAnimationPendingCollapse = false;
    m_treeAnimationPendingRow = -1;

    if (expanded) {
        // 展开：先应用模型变化，子行成为真实可见行，可直接淡入
        m_model->setRowExpanded(originalRow, true);
        if (m_rowOffsetsDirty)
            rebuildRowOffsets();

        // 遍历新可见行：动画前不存在的 = 渐显子行；存在且位移的 = 平移兄弟行
        for (int newRow = 0; newRow < m_model->rowCount(); ++newRow) {
            const int original = m_model->originalRowForVisibleRow(newRow);
            if (original < 0)
                continue;
            const int oldVisible = oldOrder.indexOf(original);
            if (oldVisible < 0) {
                m_treeAnimationFadeRows.insert(newRow);
                continue;
            }
            if (oldVisible == newRow)
                continue;
            TreeRowAnimationEntry entry;
            entry.visibleRow = newRow;
            entry.startOffset = (oldVisible < oldOffsets.size()) ? oldOffsets.at(oldVisible) : 0;
            entry.endOffset = (newRow < m_rowOffsets.size()) ? m_rowOffsets.at(newRow) : entry.startOffset;
            m_treeAnimationEntries.append(entry);
            m_treeAnimationMoveRows.insert(newRow);
        }
    } else {
        // 折叠：模型变化延迟到动画结束，期间子行仍是真实可见行，可渐隐
        // 子树在可见行上是连续区段 [row + 1, lastDescendant]
        const int anchorDepth = m_model->rowDepth(originalRow);
        int lastDescendant = row;
        for (int r = row + 1; r < m_model->rowCount(); ++r) {
            const int orow = m_model->originalRowForVisibleRow(r);
            if (orow < 0 || m_model->rowDepth(orow) <= anchorDepth)
                break;
            lastDescendant = r;
        }

        if (lastDescendant == row) {
            // 子树当前不可见（已折叠/被过滤）：无动画，直接折叠
            m_model->setRowExpanded(originalRow, false);
            return;
        }

        const int removedHeight = m_rowOffsets.at(lastDescendant + 1)
                                - m_rowOffsets.at(row + 1);

        // 子树行渐隐（保持原位）
        for (int r = row + 1; r <= lastDescendant; ++r)
            m_treeAnimationFadeRows.insert(r);
        // 其后的行整体上移“被移除的子树高度”
        for (int r = lastDescendant + 1; r < m_model->rowCount(); ++r) {
            TreeRowAnimationEntry entry;
            entry.visibleRow = r;
            entry.startOffset = m_rowOffsets.at(r);
            entry.endOffset = m_rowOffsets.at(r) - removedHeight;
            m_treeAnimationEntries.append(entry);
            m_treeAnimationMoveRows.insert(r);
        }
        m_treeAnimationPendingCollapse = true;
        m_treeAnimationPendingRow = originalRow;
    }

    if (m_treeAnimationEntries.isEmpty() && m_treeAnimationFadeRows.isEmpty())
        return;   // 无可动画内容（模型状态已就绪）

    m_treeAnimation = new QVariantAnimation(this);
    m_treeAnimation->setStartValue(0.0);
    m_treeAnimation->setEndValue(1.0);
    m_treeAnimation->setDuration(200);
    m_treeAnimation->setEasingCurve(QEasingCurve::InOutSine);
    connect(m_treeAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        applyTreeRowAnimation(value.toReal());
    });
    connect(m_treeAnimation, &QVariantAnimation::finished, this, &AuGridWidget::finishTreeAnimation);
    m_treeAnimation->start();
}

void AuGridWidget::applyTreeRowAnimation(qreal progress)
{
    m_treeAnimationProgress = qBound(0.0, progress, 1.0);
    repaint();
}

void AuGridWidget::finishTreeAnimation()
{
    if (m_treeAnimation) {
        m_treeAnimation->stop();
        delete m_treeAnimation;
        m_treeAnimation = nullptr;
    }
    const bool pendingCollapse = m_treeAnimationPendingCollapse;
    const int pendingRow = m_treeAnimationPendingRow;
    m_treeAnimationEntries.clear();
    m_treeAnimationFadeRows.clear();
    m_treeAnimationMoveRows.clear();
    m_treeAnimationPendingCollapse = false;
    m_treeAnimationPendingRow = -1;
    m_treeAnimationProgress = 0.0;

    // 折叠动画结束时应用模型折叠：最终偏移与动画末帧一致，无跳变
    if (pendingCollapse && m_model && m_model->isTreeMode()
        && pendingRow >= 0 && m_model->isRowExpanded(pendingRow)) {
        m_model->setRowExpanded(pendingRow, false);
    }
}

qreal AuGridWidget::rowAnimationOpacity(int row) const
{
    if (m_treeAnimationFadeRows.isEmpty() || !m_treeAnimationFadeRows.contains(row))
        return 1.0;
    return m_treeAnimationExpanding ? m_treeAnimationProgress
                                    : 1.0 - m_treeAnimationProgress;
}

bool AuGridWidget::treeLayerAcceptRow(int row) const
{
    switch (m_treeLayerFilter) {
    case 1:     // 仅渐变子行
        return m_treeAnimationFadeRows.contains(row);
    case 2:     // 仅平移行
        return m_treeAnimationMoveRows.contains(row);
    case 3:     // 仅静态行（既不渐变也不平移）
        return !m_treeAnimationFadeRows.contains(row)
            && !m_treeAnimationMoveRows.contains(row);
    default:
        return true;
    }
}

bool AuGridWidget::isRowExpanded(int row) const
{
    if (!m_model || !m_model->isTreeMode() || row < 0 || row >= m_model->rowCount())
        return false;
    const int originalRow = m_model->originalRowForVisibleRow(row);
    return originalRow >= 0 && m_model->isRowExpanded(originalRow);
}

bool AuGridWidget::rowHasChildren(int row) const
{
    if (!m_model || !m_model->isTreeMode() || row < 0 || row >= m_model->rowCount())
        return false;
    const int originalRow = m_model->originalRowForVisibleRow(row);
    return originalRow >= 0 && m_model->rowHasChildren(originalRow);
}

int AuGridWidget::rowDepth(int row) const
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return 0;
    const int originalRow = m_model->originalRowForVisibleRow(row);
    return originalRow >= 0 ? m_model->rowDepth(originalRow) : 0;
}

void AuGridWidget::expandAll()
{
    finishTreeAnimation();   // 结算进行中的展开动画，避免偏移快照失效
    if (m_model)
        m_model->expandAll();
}

void AuGridWidget::collapseAll()
{
    finishTreeAnimation();
    if (m_model)
        m_model->collapseAll();
}

void AuGridWidget::expandToDepth(int depth)
{
    finishTreeAnimation();
    if (m_model)
        m_model->expandToDepth(depth);
}

void AuGridWidget::collapseToDepth(int depth)
{
    finishTreeAnimation();
    if (m_model)
        m_model->collapseToDepth(depth);
}

// ---- 复选框列（树形级联三态） ----

QVector<int> AuGridWidget::subtreeRows(int originalRow) const
{
    QVector<int> result;
    if (!m_model || !m_model->isTreeMode()
        || originalRow < 0 || originalRow >= m_model->originalRowCount())
        return result;

    QVector<int> stack;
    stack.append(originalRow);
    while (!stack.isEmpty()) {
        const int current = stack.takeLast();
        result.append(current);
        const QVector<int> children = m_model->rowChildren(current);
        for (int child : children)
            stack.append(child);
    }
    return result;
}

bool AuGridWidget::isRowCheckableOriginal(int originalRow) const
{
    if (!m_model || originalRow < 0 || originalRow >= m_model->originalRowCount())
        return false;
    if (!m_rowCheckableCallback)
        return true;
    // 树形模式回调接收原始行号：级联需要覆盖折叠不可见的行
    return m_rowCheckableCallback(m_model, originalRow);
}

int AuGridWidget::checkableOriginalRowCount() const
{
    if (!m_model || !m_model->isTreeMode())
        return 0;
    int count = 0;
    const int total = m_model->originalRowCount();
    for (int i = 0; i < total; ++i) {
        if (isRowCheckableOriginal(i))
            ++count;
    }
    return count;
}

int AuGridWidget::checkedCheckableOriginalCount() const
{
    if (!m_model || !m_model->isTreeMode())
        return 0;
    int count = 0;
    for (int originalRow : m_checkedOriginalRows) {
        if (isRowCheckableOriginal(originalRow))
            ++count;
    }
    return count;
}

Qt::CheckState AuGridWidget::rowCheckStateOriginal(int originalRow) const
{
    if (m_checkedOriginalRows.contains(originalRow))
        return Qt::Checked;
    if (m_partialOriginalRows.contains(originalRow))
        return Qt::PartiallyChecked;
    return Qt::Unchecked;
}

void AuGridWidget::cascadeRowCheck(int originalRow, bool checked)
{
    const QVector<int> rows = subtreeRows(originalRow);
    for (int r : rows) {
        if (!isRowCheckableOriginal(r))
            continue;
        if (checked)
            m_checkedOriginalRows.insert(r);
        else
            m_checkedOriginalRows.remove(r);
        m_partialOriginalRows.remove(r);
    }
    recomputeAncestorCheckStates(originalRow);
}

void AuGridWidget::recomputeAncestorCheckStates(int originalRow)
{
    if (!m_model || !m_model->isTreeMode())
        return;

    // 自底向上聚合祖先链：有可勾选后代时祖先状态完全由后代聚合决定
    // （全勾 = Checked / 混合 = PartiallyChecked / 全不勾 = Unchecked）
    int current = m_model->rowParent(originalRow);
    while (current >= 0) {
        int checkedCount = 0;
        int checkableCount = 0;
        const QVector<int> rows = subtreeRows(current);
        for (int r : rows) {
            if (r == current || !isRowCheckableOriginal(r))
                continue;
            ++checkableCount;
            if (m_checkedOriginalRows.contains(r))
                ++checkedCount;
        }

        m_partialOriginalRows.remove(current);
        if (checkableCount > 0) {
            m_checkedOriginalRows.remove(current);
            if (checkedCount == checkableCount) {
                m_checkedOriginalRows.insert(current);
            } else if (checkedCount > 0) {
                m_partialOriginalRows.insert(current);
            }
        }

        current = m_model->rowParent(current);
    }
}

void AuGridWidget::recomputeAllCheckStates()
{
    m_partialOriginalRows.clear();
    if (!m_model || !m_model->isTreeMode())
        return;

    // 深度优先铺平保证父行索引小于子行索引，自底向上遍历即可逐层聚合
    for (int i = m_model->originalRowCount() - 1; i >= 0; --i) {
        if (m_model->rowChildren(i).isEmpty())
            continue;

        int checkedCount = 0;
        int checkableCount = 0;
        const QVector<int> rows = subtreeRows(i);
        for (int r : rows) {
            if (r == i || !isRowCheckableOriginal(r))
                continue;
            ++checkableCount;
            if (m_checkedOriginalRows.contains(r))
                ++checkedCount;
        }

        m_partialOriginalRows.remove(i);
        if (checkableCount > 0) {
            m_checkedOriginalRows.remove(i);
            if (checkedCount == checkableCount) {
                m_checkedOriginalRows.insert(i);
            } else if (checkedCount > 0) {
                m_partialOriginalRows.insert(i);
            }
        }
    }
}

int AuGridWidget::sortedColumn() const
{
    return m_sortedColumn;
}

Qt::SortOrder AuGridWidget::sortOrder() const
{
    return m_sortOrder;
}

void AuGridWidget::setSortingEnabled(bool enabled)
{
    if (m_sortingEnabled == enabled)
        return;
    m_sortingEnabled = enabled;
}

bool AuGridWidget::isSortingEnabled() const
{
    return m_sortingEnabled;
}

void AuGridWidget::setSortRules(const QList<AuSortRule> &rules)
{
    if (!m_model)
        return;

    m_sortRules = rules;
    if (m_sortRules.isEmpty()) {
        clearSort();
        return;
    }

    m_model->sort(m_sortRules);
    m_sortedColumn = m_sortRules.first().column;
    m_sortOrder = m_sortRules.first().order;
    update();
    emit sortChanged(m_sortedColumn, m_sortOrder);
}

QList<AuSortRule> AuGridWidget::sortRules() const
{
    return m_sortRules;
}

void AuGridWidget::clearSort()
{
    if (!m_model)
        return;

    m_model->clearSort();
    m_sortedColumn = -1;
    m_sortOrder = Qt::AscendingOrder;
    m_sortRules.clear();
    update();
    emit sortChanged(-1, Qt::AscendingOrder);
}

void AuGridWidget::setSortComparator(const AuRowComparator &comparator)
{
    if (!m_model)
        return;

    m_model->setSortComparator(comparator);
    // 自定义比较函数不指向具体列：仅当当前无排序列指示时清掉残留的表头箭头
    if (m_sortedColumn >= 0) {
        m_sortedColumn = -1;
        m_sortOrder = Qt::AscendingOrder;
        update();
        emit sortChanged(-1, Qt::AscendingOrder);
    }
}

void AuGridWidget::clearSortComparator()
{
    if (m_model)
        m_model->clearSortComparator();
}

void AuGridWidget::setFilterCallback(const std::function<bool(const QVariantMap &row)> &callback)
{
    if (!m_model)
        return;

    m_model->setFilterCallback(callback);
}

void AuGridWidget::clearFilter()
{
    if (!m_model)
        return;

    m_model->clearFilterCallback();
}

bool AuGridWidget::isFiltered() const
{
    return m_model && m_model->hasFilterCallback();
}

void AuGridWidget::setRowDragEnabled(bool enabled)
{
    // 树形模式行序由父子结构决定，扁平拖拽重排会破坏层级
    if (enabled && m_model && m_model->isTreeMode()) {
        qWarning() << "AuGridWidget: row drag is not supported in tree mode.";
        return;
    }

    if (m_rowDragEnabled == enabled)
        return;
    // 带显隐动画（与复选框 / 行号列观感一致）；最终状态由动画路径应用
    animateSpecialColumnVisibility(SpecialDragHandle, enabled);
}

bool AuGridWidget::isRowDragEnabled() const
{
    return m_rowDragEnabled;
}

void AuGridWidget::setDragPreviewTextCallback(const std::function<QString(const AuGridModel *, const QVector<int> &)> &callback)
{
    m_dragPreviewTextCallback = callback;
}

void AuGridWidget::startRowDrag(int visibleRow, const QPoint &globalPos)
{
    if (!m_model || visibleRow < 0 || visibleRow >= m_model->rowCount())
        return;

    QSet<int> checked = m_checkedOriginalRows;
    QVector<int> dragging;
    if (!checked.isEmpty()) {
        for (int v = 0; v < m_model->rowCount(); ++v) {
            const int original = m_model->originalRowForVisibleRow(v);
            if (checked.contains(original))
                dragging.append(original);
        }
    } else {
        dragging.append(m_model->originalRowForVisibleRow(visibleRow));
    }

    if (dragging.isEmpty())
        return;

    m_isRowDragging = true;
    m_dragStartPos = globalPos;
    m_dragStartVisibleRow = visibleRow;
    m_draggingOriginalRows = dragging;
    m_dragTargetVisibleIndex = -1;
    m_rowDragLastY = globalPos.y();
    m_rowDragDirection = 0;
    setCursor(Qt::ArrowCursor);
    clearHoverCell();

    ensureDragFloatWidget();
    updateDragFloatWidgetText();
    m_dragFloatWidget->adjustSize();
    m_dragFloatWidget->show();
    m_dragFloatWidget->raise();
    updateDragFloatWidgetPosition(globalPos);

    startAutoScroll();
    update();
}

void AuGridWidget::updateRowDrag(const QPoint &globalPos)
{
    if (!m_isRowDragging || !m_model)
        return;

    updateDragFloatWidgetPosition(globalPos);

    const int dy = globalPos.y() - m_rowDragLastY;
    m_rowDragLastY = globalPos.y();
    if (dy < 0)
        m_rowDragDirection = -1;
    else if (dy > 0)
        m_rowDragDirection = 1;

    const QPoint localPos = mapFromGlobal(globalPos);
    const int targetVisibleIndex = targetVisibleIndexForDrag(localPos);

    if (targetVisibleIndex == m_dragTargetVisibleIndex)
        return;
    m_dragTargetVisibleIndex = targetVisibleIndex;

    const int K = m_draggingOriginalRows.size();
    const int reducedSize = qMax(0, m_model->rowCount() - K);
    const int targetOriginalIndex = qBound(0, targetVisibleIndex, reducedSize);

    const QVector<int> oldDragging = m_draggingOriginalRows;

    // 提前构建集合与排序，用于后续选中行跟随映射
    QSet<int> draggedSet;
    for (int original : oldDragging)
        draggedSet.insert(original);
    QVector<int> draggedSorted = oldDragging;
    std::sort(draggedSorted.begin(), draggedSorted.end());

    // 记录动画起始偏移
    finishDragReorderAnimation();
    startDragReorderAnimation();

    m_model->moveRows(oldDragging, targetOriginalIndex);
    updateCheckedRowsAfterMove(oldDragging, targetOriginalIndex);

    // 多选选中集合随行移动重映射（moveRows 会重编号原始行号）
    if (!m_selectedOriginalRows.isEmpty()) {
        const int K = oldDragging.size();
        QSet<int> remapped;
        for (int oldIdx : m_selectedOriginalRows) {
            int newIdx;
            if (draggedSet.contains(oldIdx)) {
                newIdx = targetOriginalIndex + draggedSorted.indexOf(oldIdx);
            } else {
                int removedBefore = 0;
                for (int d : draggedSorted) {
                    if (d < oldIdx)
                        ++removedBefore;
                }
                newIdx = oldIdx - removedBefore;
                if (newIdx >= targetOriginalIndex)
                    newIdx += K;
            }
            remapped.insert(newIdx);
        }
        m_selectedOriginalRows = remapped;
    }

    // 记录动画结束偏移并启动插值动画
    if (m_rowOffsetsDirty)
        rebuildRowOffsets();

    // 根据移动操作计算每个新可见行对应的内容来自哪个旧可见行
    QVector<int> newOrder = m_dragAnimationOldOrder;
    {
        // 移除被拖动的 original 行
        QVector<int> draggedSorted = oldDragging;
        std::sort(draggedSorted.begin(), draggedSorted.end());
        for (int i = draggedSorted.size() - 1; i >= 0; --i) {
            const int idx = newOrder.indexOf(draggedSorted.at(i));
            if (idx >= 0)
                newOrder.removeAt(idx);
        }
        // 在 targetOriginalIndex 处插入
        for (int i = 0; i < draggedSorted.size(); ++i)
            newOrder.insert(targetOriginalIndex + i, draggedSorted.at(i));
    }

    for (int newVisible = 0; newVisible < newOrder.size(); ++newVisible) {
        const int original = newOrder.at(newVisible);
        const int oldVisible = m_dragAnimationOldOrder.indexOf(original);
        if (oldVisible < 0 || oldVisible == newVisible)
            continue;
        if (newVisible >= m_rowOffsets.size())
            continue;
        RowDragAnimationEntry entry;
        entry.visibleRow = newVisible;
        entry.startOffset = (oldVisible < m_dragAnimationOldOffsets.size()) ? m_dragAnimationOldOffsets.at(oldVisible) : 0;
        entry.endOffset = m_rowOffsets.at(newVisible);
        m_dragAnimationEntries.append(entry);
    }

    if (!m_dragAnimationEntries.isEmpty()) {
        m_dragOffsetAnimation = new QVariantAnimation(this);
        m_dragOffsetAnimation->setStartValue(0.0);
        m_dragOffsetAnimation->setEndValue(1.0);
        m_dragOffsetAnimation->setDuration(200);
        m_dragOffsetAnimation->setEasingCurve(QEasingCurve::InOutSine);
        connect(m_dragOffsetAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            applyDragReorderAnimation(value.toReal());
        });
        connect(m_dragOffsetAnimation, &QVariantAnimation::finished, this, &AuGridWidget::finishDragReorderAnimation);
        m_dragOffsetAnimation->start();
    }

    m_draggingOriginalRows.clear();
    for (int i = 0; i < K; ++i)
        m_draggingOriginalRows.append(targetOriginalIndex + i);

    // 修正选中行跟随：用移动前的 originalRow 映射到移动后的 originalRow
    if (m_selectedOriginalRow >= 0) {
        const int oldSelectedOriginal = m_selectedOriginalRow;
        int newSelectedOriginal;
        if (draggedSet.contains(oldSelectedOriginal)) {
            const int posInBlock = draggedSorted.indexOf(oldSelectedOriginal);
            newSelectedOriginal = targetOriginalIndex + posInBlock;
        } else {
            int removedBefore = 0;
            for (int d : draggedSorted) {
                if (d < oldSelectedOriginal)
                    ++removedBefore;
            }
            newSelectedOriginal = oldSelectedOriginal - removedBefore;
            if (newSelectedOriginal >= targetOriginalIndex)
                newSelectedOriginal += K;
        }
        m_selectedOriginalRow = newSelectedOriginal;
        const int newVisible = m_model->visibleRowForOriginalRow(newSelectedOriginal);
        if (newVisible != m_selectedRow) {
            m_selectedRow = newVisible;
            emit selectionChanged();
        }
    }

    emit rowsDragged(m_draggingOriginalRows, targetOriginalIndex);
    update();
}

void AuGridWidget::endRowDrag()
{
    if (!m_isRowDragging)
        return;

    stopAutoScroll();
    if (m_dragFloatWidget)
        m_dragFloatWidget->hide();

    m_isRowDragging = false;
    m_dragStartVisibleRow = -1;
    m_draggingOriginalRows.clear();
    m_dragTargetVisibleIndex = -1;
    finishDragReorderAnimation();
    setCursor(Qt::ArrowCursor);
    clearHoverCell();
    update();
}

void AuGridWidget::cancelRowDrag()
{
    if (!m_isRowDragging)
        return;

    stopAutoScroll();
    if (m_dragFloatWidget)
        m_dragFloatWidget->hide();

    m_isRowDragging = false;
    m_dragStartVisibleRow = -1;
    m_draggingOriginalRows.clear();
    m_dragTargetVisibleIndex = -1;
    finishDragReorderAnimation();
    setCursor(Qt::ArrowCursor);
    clearHoverCell();
    update();
}

void AuGridWidget::startDragReorderAnimation()
{
    m_dragAnimationEntries.clear();
    m_dragAnimationOldOrder.clear();
    m_dragAnimationOldOffsets.clear();
    if (!m_model || m_model->rowCount() == 0)
        return;

    if (m_rowOffsetsDirty)
        rebuildRowOffsets();

    const int count = m_model->rowCount();
    m_dragAnimationOldOrder.reserve(count);
    m_dragAnimationOldOffsets.reserve(count);
    for (int v = 0; v < count; ++v) {
        m_dragAnimationOldOrder.append(m_model->originalRowForVisibleRow(v));
        m_dragAnimationOldOffsets.append((v < m_rowOffsets.size()) ? m_rowOffsets.at(v) : 0);
    }
}

void AuGridWidget::applyDragReorderAnimation(qreal progress)
{
    m_dragAnimationProgress = qBound(0.0, progress, 1.0);
    repaint();
}

void AuGridWidget::finishDragReorderAnimation()
{
    if (m_dragOffsetAnimation) {
        m_dragOffsetAnimation->stop();
        delete m_dragOffsetAnimation;
        m_dragOffsetAnimation = nullptr;
    }
    m_dragAnimationEntries.clear();
    m_dragAnimationOldOrder.clear();
    m_dragAnimationOldOffsets.clear();
    m_dragAnimationProgress = 0.0;
    captureLayoutSnapshot();   // 行拖拽会重编号原始行，动画基线随之刷新
    update();
}

int AuGridWidget::targetVisibleIndexForDrag(const QPoint &pos) const
{
    if (!m_model || m_model->rowCount() == 0)
        return 0;

    const int bodyTop = headerHeight();
    int contentY = pos.y() - bodyTop + m_yOffset;

    if (m_rowOffsetsDirty)
        rebuildRowOffsets();

    auto it = std::upper_bound(m_rowOffsets.begin(), m_rowOffsets.end(), contentY);
    int visibleRow = int(it - m_rowOffsets.begin()) - 1;
    if (visibleRow < 0)
        visibleRow = 0;
    else if (visibleRow >= m_model->rowCount())
        visibleRow = m_model->rowCount() - 1;

    // 先找到被拖动行的连续可见块
    QSet<int> draggedSet;
    for (int original : m_draggingOriginalRows)
        draggedSet.insert(original);
    int blockStart = -1;
    int blockEnd = -1;
    for (int v = 0; v < m_model->rowCount(); ++v) {
        const int original = m_model->originalRowForVisibleRow(v);
        if (draggedSet.contains(original)) {
            if (blockStart < 0)
                blockStart = v;
            blockEnd = v;
        }
    }
    if (blockStart < 0)
        return 0;

    const int K = blockEnd - blockStart + 1;

    // 只要拖到其它行上就触发移动，无需超过该行一半；用真实拖动方向决定插入到目标行前还是后。
    int targetVisibleIndex;
    if (m_rowDragDirection < 0)
        targetVisibleIndex = visibleRow;       // 向上拖：插入到目标行之前
    else
        targetVisibleIndex = visibleRow + 1;   // 向下拖：插入到目标行之后

    const int rowBottom = (visibleRow + 1 < m_rowOffsets.size())
                              ? m_rowOffsets.at(visibleRow + 1)
                              : totalContentHeight();
    if (visibleRow == m_model->rowCount() - 1 && contentY >= rowBottom)
        targetVisibleIndex = m_model->rowCount();

    // 换算为移除拖动块之后的插入下标
    if (targetVisibleIndex <= blockStart)
        return targetVisibleIndex;
    if (targetVisibleIndex > blockEnd + 1)
        return targetVisibleIndex - K;
    return blockStart;
}

void AuGridWidget::ensureDragFloatWidget()
{
    if (m_dragFloatWidget)
        return;
    // 用子控件覆盖层而非顶层 Qt::Tool 原生窗口：部分 Linux 窗口管理器会给顶层窗口
    // 再描一圈方形装饰边框（Wayland 下顶层窗口也无法自由定位移动），子控件则无此问题
    m_dragFloatWidget = new QLabel(this);
    m_dragFloatWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_dragFloatWidget->setStyleSheet(QStringLiteral("QLabel { background-color: rgba(240, 240, 240, 230); border: 1px solid gray; border-radius: 4px; padding: 4px 8px; }"));
}

void AuGridWidget::updateDragFloatWidget(const QPoint &globalPos)
{
    updateDragFloatWidgetPosition(globalPos);
    updateDragFloatWidgetText();
}

void AuGridWidget::updateDragFloatWidgetPosition(const QPoint &globalPos)
{
    if (!m_dragFloatWidget)
        return;

    const QRect globalRect = QRect(mapToGlobal(QPoint(0, 0)), size());
    const int bodyTop = globalRect.top() + headerHeight();
    const int bodyBottom = globalRect.bottom();

    QPoint target = globalPos - QPoint(m_dragFloatWidget->width() / 2, m_dragFloatWidget->height() / 2);
    const int maxX = globalRect.right() - m_dragFloatWidget->width();
    const int maxY = bodyBottom - m_dragFloatWidget->height();
    target.setX(qMax(globalRect.left(), qMin(maxX, target.x())));
    target.setY(qMax(bodyTop, qMin(maxY, target.y())));

    m_dragFloatWidget->move(mapFromGlobal(target));
}

void AuGridWidget::updateDragFloatWidgetText()
{
    if (!m_dragFloatWidget || !m_model)
        return;

    QString text;
    if (m_dragPreviewTextCallback) {
        text = m_dragPreviewTextCallback(m_model, m_draggingOriginalRows);
    } else if (m_draggingOriginalRows.size() == 1) {
        const int originalRow = m_draggingOriginalRows.first();
        const int visibleRow = m_model->visibleRowForOriginalRow(originalRow);
        if (visibleRow >= 0 && m_model->columnCount() > 0) {
            const QString field = m_model->fieldAtColumn(0);
            text = m_model->cellValue(visibleRow, field).toString();
        }
    } else {
        text = QStringLiteral("%1 Rows").arg(m_draggingOriginalRows.size());
    }

    m_dragFloatWidget->setText(text);
    m_dragFloatWidget->adjustSize();
    // 高度与当前尺寸模式的默认行高保持一致（排除 WordWrap 撑高的情况）：
    // 小 / 中 / 大档位下浮层依次增高，观感与被拖起的行一致
    m_dragFloatWidget->setFont(m_style->font(m_model));
    m_dragFloatWidget->setFixedHeight(qMax(rowHeight(), 16));
}

void AuGridWidget::startAutoScroll()
{
    if (m_autoScrollTimer)
        m_autoScrollTimer->start();
}

void AuGridWidget::stopAutoScroll()
{
    if (m_autoScrollTimer)
        m_autoScrollTimer->stop();
}

void AuGridWidget::onAutoScrollTimeout()
{
    if (!m_isRowDragging || !m_model)
        return;

    const QPoint globalPos = QCursor::pos();
    const QPoint localPos = mapFromGlobal(globalPos);
    const int margin = 30;

    int delta = 0;
    if (localPos.y() < headerHeight() + margin)
        delta = -qMax(1, (headerHeight() + margin - localPos.y()) / 5);
    else if (localPos.y() > height() - margin)
        delta = qMax(1, (localPos.y() - (height() - margin)) / 5);

    if (delta != 0) {
        const int maxOffset = qMax(0, totalContentHeight() - bodyHeight());
        const int newOffset = qBound(0, m_yOffset + delta, maxOffset);
        if (newOffset != m_yOffset) {
            m_yOffset = newOffset;
            m_vScrollBar->setValue(m_yOffset);
            updateRowDrag(globalPos);
        }
    }
}

void AuGridWidget::updateCheckedRowsAfterMove(const QVector<int> &draggedOriginalRows, int targetOriginalIndex)
{
    if (m_checkedOriginalRows.isEmpty())
        return;

    const int K = draggedOriginalRows.size();
    const QSet<int> oldChecked = m_checkedOriginalRows;
    QSet<int> draggedSet;
    for (int original : draggedOriginalRows)
        draggedSet.insert(original);

    QVector<int> draggedSorted = draggedOriginalRows;
    std::sort(draggedSorted.begin(), draggedSorted.end());

    m_checkedOriginalRows.clear();

    for (int oldIdx : oldChecked) {
        int newIdx;
        if (draggedSet.contains(oldIdx)) {
            const int posInBlock = draggedSorted.indexOf(oldIdx);
            newIdx = targetOriginalIndex + posInBlock;
        } else {
            int removedBefore = 0;
            for (int d : draggedSorted) {
                if (d < oldIdx)
                    ++removedBefore;
            }
            newIdx = oldIdx - removedBefore;
            if (newIdx >= targetOriginalIndex)
                newIdx += K;
        }
        m_checkedOriginalRows.insert(newIdx);
    }
}

