// ============================================================================
// AuGridWidget 列实现：列停靠、列显隐动画、列宽调整与 AutoFit、
// 内置辅助列（行号 / 复选框 / 拖拽手柄）几何与显隐动画、
// 多级列树工具、任意层级列拖拽排序与重排动画。
// ============================================================================

#include "AuGridWidget.h"

#include <QCursor>
#include <QFontMetrics>
#include <QLabel>
#include <QVariantAnimation>
#include <algorithm>

void AuGridWidget::pinColumn(int column, AuColumnDef::PinMode mode)
{
    if (!m_model)
        return;
    m_model->setColumnPinMode(column, mode);
}

void AuGridWidget::pinColumn(const QString &field, AuColumnDef::PinMode mode)
{
    if (!m_model)
        return;
    m_model->setColumnPinMode(field, mode);
}

AuColumnDef::PinMode AuGridWidget::columnPinMode(int column) const
{
    if (!m_model)
        return AuColumnDef::PinMode::None;
    return m_model->columnPinMode(column);
}

void AuGridWidget::setColumnVisible(int column, bool visible)
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return;

    const QString field = m_model->fieldAtColumn(column);
    if (field.isEmpty())
        return;

    const bool hidden = m_hiddenFields.contains(field);
    if (hidden == !visible)
        return;

    animateColumnVisibility(column, column, visible);
}

void AuGridWidget::setColumnVisible(const QString &field, bool visible)
{
    if (!m_model || field.isEmpty())
        return;

    const QVector<QString> leafFields = m_model->leafFieldsForField(field);
    if (leafFields.isEmpty())
        return;

    int firstLeaf = INT_MAX;
    int lastLeaf = INT_MIN;
    for (const QString &leafField : leafFields) {
        const int col = m_model->columnIndexForField(leafField);
        if (col < 0)
            continue;
        const bool hidden = m_hiddenFields.contains(leafField);
        if (hidden == !visible)
            continue;
        firstLeaf = qMin(firstLeaf, col);
        lastLeaf = qMax(lastLeaf, col);
    }

    if (firstLeaf > lastLeaf)
        return;

    animateColumnVisibility(firstLeaf, lastLeaf, visible);
}

bool AuGridWidget::isColumnVisible(int column) const
{
    return !isColumnHidden(column);
}

bool AuGridWidget::isColumnHidden(int column) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return false;
    return m_hiddenFields.contains(m_model->fieldAtColumn(column));
}

void AuGridWidget::animateColumnVisibility(int firstLeaf, int lastLeaf, bool visible)
{
    if (!m_model || firstLeaf < 0 || firstLeaf > lastLeaf || lastLeaf >= m_model->columnCount())
        return;

    // 若已有同类动画，立即完成它
    finishColumnVisibilityAnimation();

    // 以目标列的 pinMode 判定区域（隐藏列的 m_columnRegions 是占位值，不能依赖）
    const AuColumnDef firstDef = m_model->columnDefAt(firstLeaf);
    const ColumnRegion region = firstDef.pinMode() == AuColumnDef::PinMode::Left ? ColumnRegion::LeftPinned
                                : firstDef.pinMode() == AuColumnDef::PinMode::Right ? ColumnRegion::RightPinned
                                                                                    : ColumnRegion::Body;
    const QVector<int> *regionList = (region == ColumnRegion::LeftPinned) ? &m_leftPinnedColumns
                                     : (region == ColumnRegion::RightPinned) ? &m_rightPinnedColumns
                                                                             : &m_bodyColumns;

    // 需要移动的列：同区域、位于目标之后的可见列
    QVector<int> shiftColumns;
    for (int col : *regionList) {
        if (col > lastLeaf && !isColumnHidden(col))
            shiftColumns.append(col);
    }

    m_columnVisAnimating = true;
    m_columnVisTargetFirst = firstLeaf;
    m_columnVisTargetLast = lastLeaf;
    m_columnVisHiding = !visible;
    m_columnVisProgress = 0.0;
    m_columnVisShiftEntries.clear();

    if (m_columnVisHiding) {
        // 隐藏：目标列保持宽度并淡出；右侧列滑向真实最终布局位置。
        // 终点不能简单取"当前位置 - 被隐藏宽度"：Flex/AutoFit 列会在隐藏后重新
        // 分配宽度（如 Demo 中 E 撑开把 F 顶回），产生先左移后右移的两段式跳动。
        // 与显示分支对称：先临时应用隐藏重建出最终布局取终点，再恢复现状。
        QVector<int> oldOffsets;
        oldOffsets.reserve(shiftColumns.size());
        for (int col : shiftColumns)
            oldOffsets.append(columnContentOffset(col));

        for (int col = firstLeaf; col <= lastLeaf; ++col) {
            const QString field = m_model->fieldAtColumn(col);
            if (!field.isEmpty())
                m_hiddenFields.insert(field);
        }
        rebuildColumnOffsets();

        QVector<int> finalOffsets;
        finalOffsets.reserve(shiftColumns.size());
        for (int col : shiftColumns)
            finalOffsets.append(columnContentOffset(col));

        for (int col = firstLeaf; col <= lastLeaf; ++col) {
            const QString field = m_model->fieldAtColumn(col);
            if (!field.isEmpty())
                m_hiddenFields.remove(field);
        }
        rebuildColumnOffsets();

        for (int i = 0; i < shiftColumns.size(); ++i) {
            ColumnDragAnimationEntry e;
            e.column = shiftColumns.at(i);
            e.startOffset = oldOffsets.at(i);
            e.endOffset = finalOffsets.at(i);
            m_columnVisShiftEntries.append(e);
        }
    } else {
        // 显示：先记录右侧列旧偏移，再显示并重建布局，然后右侧列从旧位置右移到新位置
        QVector<int> oldOffsets;
        oldOffsets.reserve(shiftColumns.size());
        for (int col : shiftColumns)
            oldOffsets.append(columnContentOffset(col));

        for (int col = firstLeaf; col <= lastLeaf; ++col) {
            const QString field = m_model->fieldAtColumn(col);
            if (!field.isEmpty())
                m_hiddenFields.remove(field);
        }
        rebuildColumnOffsets();

        for (int i = 0; i < shiftColumns.size(); ++i) {
            ColumnDragAnimationEntry e;
            e.column = shiftColumns.at(i);
            e.startOffset = oldOffsets.at(i);
            e.endOffset = columnContentOffset(shiftColumns.at(i));
            m_columnVisShiftEntries.append(e);
        }
    }

    invalidateRowHeights();
    updateScrollBars();

    m_columnVisAnimation = new QVariantAnimation(this);
    m_columnVisAnimation->setStartValue(0.0);
    m_columnVisAnimation->setEndValue(1.0);
    m_columnVisAnimation->setDuration(180);
    m_columnVisAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_columnVisAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_columnVisProgress = value.toReal();
        update();
    });
    connect(m_columnVisAnimation, &QVariantAnimation::finished, this, [this]() {
        finishColumnVisibilityAnimation();
    });

    update();
    m_columnVisAnimation->start();
}

void AuGridWidget::finishColumnVisibilityAnimation()
{
    if (m_columnVisAnimating && m_columnVisHiding) {
        // 隐藏动画结束（或被打断）：真正应用隐藏
        for (int col = m_columnVisTargetFirst; col <= m_columnVisTargetLast; ++col) {
            const QString field = m_model ? m_model->fieldAtColumn(col) : QString();
            if (!field.isEmpty())
                m_hiddenFields.insert(field);
        }
        if (m_model)
            rebuildColumnOffsets();
        invalidateRowHeights();
        updateScrollBars();
    }

    if (m_columnVisAnimation) {
        QVariantAnimation *anim = m_columnVisAnimation;
        m_columnVisAnimation = nullptr;
        QObject::disconnect(anim, nullptr, this, nullptr);
        anim->stop();
        anim->deleteLater();
    }

    m_columnVisAnimating = false;
    m_columnVisTargetFirst = -1;
    m_columnVisTargetLast = -1;
    m_columnVisHiding = false;
    m_columnVisProgress = 0.0;
    m_columnVisShiftEntries.clear();
    update();
}

qreal AuGridWidget::columnOpacity(int column) const
{
    if (m_columnVisTargetFirst < 0 || m_columnVisTargetLast < m_columnVisTargetFirst)
        return 1.0;
    if (column < m_columnVisTargetFirst || column > m_columnVisTargetLast)
        return 1.0;
    return m_columnVisHiding ? (1.0 - m_columnVisProgress) : m_columnVisProgress;
}

// ---------------------------------------------------------------------------
// 辅助列（行号 / 复选框 / 拖拽手柄）显隐动画：与列显隐动画同观感
// （180ms InOutCubic 缓动 + 淡入淡出）。显示路径先应用最终状态再让宽度从 0
// 插值到全宽（主体列随之右移）；隐藏路径动画结束后才真正应用隐藏（宽度从
// 全宽插值到 0，主体列左移），避免动画期间布局跳变。

void AuGridWidget::animateSpecialColumnVisibility(SpecialColumnKind kind, bool visible)
{
    finishSpecialColumnVisibilityAnimation();

    m_specialVisAnimating = true;
    m_specialVisHiding = !visible;
    m_specialVisProgress = 0.0;
    m_specialVisMask = 1u << kind;

    if (!m_specialVisHiding) {
        // 显示：立即应用最终状态并按最终布局重建列偏移 / 滚动条，
        // 动画期间宽度插值从 0 长到全宽
        applySpecialColumnVisibility(kind, visible);
        rebuildColumnOffsets();
        updateScrollBars();
    }

    m_specialVisAnimation = new QVariantAnimation(this);
    m_specialVisAnimation->setStartValue(0.0);
    m_specialVisAnimation->setEndValue(1.0);
    m_specialVisAnimation->setDuration(180);
    m_specialVisAnimation->setEasingCurve(QEasingCurve::InOutCubic);
    connect(m_specialVisAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_specialVisProgress = value.toReal();
        update();
    });
    connect(m_specialVisAnimation, &QVariantAnimation::finished, this, [this]() {
        finishSpecialColumnVisibilityAnimation();
    });

    update();
    m_specialVisAnimation->start();
}

void AuGridWidget::finishSpecialColumnVisibilityAnimation()
{
    if (m_specialVisAnimation) {
        QVariantAnimation *anim = m_specialVisAnimation;
        m_specialVisAnimation = nullptr;
        QObject::disconnect(anim, nullptr, this, nullptr);
        anim->stop();
        anim->deleteLater();
    }

    if (m_specialVisAnimating) {
        const unsigned mask = m_specialVisMask;
        const bool hiding = m_specialVisHiding;
        m_specialVisAnimating = false;
        m_specialVisHiding = false;
        m_specialVisProgress = 0.0;
        m_specialVisMask = 0;

        if (hiding) {
            // 隐藏动画结束（或被打断）：真正应用隐藏
            for (const unsigned bit : {1u, 2u, 4u}) {
                if (mask & bit)
                    applySpecialColumnVisibility(SpecialColumnKind(bit - 1), false);
            }
            rebuildColumnOffsets();
            updateScrollBars();
        }
    }

    update();
}

void AuGridWidget::applySpecialColumnVisibility(SpecialColumnKind kind, bool visible)
{
    switch (kind) {
    case SpecialRowNumber:
        m_showRowNumberColumn = visible;
        break;
    case SpecialCheckbox:
        m_showCheckboxColumn = visible;
        if (!visible) {
            m_checkedOriginalRows.clear();
            m_partialOriginalRows.clear();
        }
        break;
    case SpecialDragHandle:
        m_rowDragEnabled = visible;
        if (!visible)
            cancelRowDrag();
        break;
    }
}

qreal AuGridWidget::specialColumnOpacity(SpecialColumnKind kind) const
{
    if (!m_specialVisAnimating || !(m_specialVisMask & (1u << kind)))
        return 1.0;
    return m_specialVisHiding ? (1.0 - m_specialVisProgress) : m_specialVisProgress;
}

int AuGridWidget::specialColumnAnimatedWidth(SpecialColumnKind kind, int baseWidth) const
{
    if (!m_specialVisAnimating || baseWidth <= 0 || !(m_specialVisMask & (1u << kind)))
        return baseWidth;
    const qreal t = m_specialVisHiding ? (1.0 - m_specialVisProgress) : m_specialVisProgress;
    return qRound(baseWidth * t);
}

void AuGridWidget::setColumnWidth(int column, int width)
{
    if (!m_model)
        return;
    m_model->setColumnWidth(column, width);
}

void AuGridWidget::setColumnWidth(const QString &field, int width)
{
    if (!m_model)
        return;
    m_model->setColumnWidth(field, width);
}

int AuGridWidget::specialColumnPadding() const
{
    return m_style ? m_style->integer(AuGridStyle::CellHorizontalPadding, m_model) : 6;
}

int AuGridWidget::rowNumberColumnWidth() const
{
    if (!m_showRowNumberColumn || !m_style)
        return 0;

    const int padding = specialColumnPadding();
    if (!m_model || m_model->originalRowCount() == 0)
        return 2 * padding;

    QFont font = m_style->font(m_model);
    if (m_style->value(AuGridStyle::RowNumberFontBold, m_model).toBool())
        font.setBold(true);
    const QFontMetrics fm(font);
    const int textWidth = fm.horizontalAdvance(QString::number(m_model->originalRowCount()));
    return specialColumnAnimatedWidth(SpecialRowNumber, 2 * padding + textWidth);
}

bool AuGridWidget::isRowNumberColumn(int column) const
{
    return m_showRowNumberColumn && column == kRowNumberColumn;
}

QRect AuGridWidget::rowNumberCellRect(int row) const
{
    const int w = rowNumberColumnWidth();
    if (w <= 0)
        return QRect();
    return QRect(0, rowY(row), w, rowHeight(row));
}

int AuGridWidget::checkboxColumnX() const
{
    return rowNumberColumnWidth();
}

int AuGridWidget::dragHandleColumnX() const
{
    return rowNumberColumnWidth() + checkboxColumnWidth();
}

int AuGridWidget::fixedColumnsWidth() const
{
    return rowNumberColumnWidth() + checkboxColumnWidth() + dragHandleColumnWidth();
}

int AuGridWidget::checkboxColumnWidth() const
{
    if (!m_showCheckboxColumn || !m_style)
        return 0;
    const int size = m_style->integer(AuGridStyle::CheckboxSize, m_model);
    const int padding = specialColumnPadding();
    const int rightPadding = m_rowDragEnabled ? padding / 2 : padding;
    return specialColumnAnimatedWidth(SpecialCheckbox, padding + size + rightPadding);
}

bool AuGridWidget::isCheckboxColumn(int column) const
{
    return m_showCheckboxColumn && column == kCheckboxColumn;
}

QRect AuGridWidget::checkboxHeaderRect() const
{
    const int w = checkboxColumnWidth();
    if (w <= 0)
        return QRect();
    return QRect(checkboxColumnX(), 0, w, headerHeight());
}

QRect AuGridWidget::checkboxCellRect(int row) const
{
    const int w = checkboxColumnWidth();
    if (w <= 0)
        return QRect();
    return QRect(checkboxColumnX(), rowY(row), w, rowHeight(row));
}

int AuGridWidget::dragHandleColumnWidth() const
{
    if (!m_rowDragEnabled || !m_style)
        return 0;
    const int dotSize = m_style->integer(AuGridStyle::DragHandleDotSize, m_model);
    const int iconWidth = dotSize * 3 + (3 - 1) * dotSize;
    const int padding = specialColumnPadding();
    const int leftPadding = m_showCheckboxColumn ? padding / 2 : padding;
    return specialColumnAnimatedWidth(SpecialDragHandle, leftPadding + iconWidth + padding);
}

bool AuGridWidget::isDragHandleColumn(int column) const
{
    return m_rowDragEnabled && column == kDragHandleColumn;
}

QRect AuGridWidget::dragHandleCellRect(int row) const
{
    const int w = dragHandleColumnWidth();
    if (w <= 0)
        return QRect();
    return QRect(dragHandleColumnX(), rowY(row), w, rowHeight(row));
}

int AuGridWidget::resizeHandleAt(const QPoint &pos) const
{
    if (!m_model || m_model->columnCount() == 0)
        return -1;

    const int levelHeight = headerLevelHeight();
    const int bottomHeaderTop = headerHeight() - levelHeight;
    if (pos.y() < bottomHeaderTop || pos.y() > headerHeight())
        return -1;

    const auto checkBorder = [&](const QVector<int> &columns) {
        for (int i = 0; i < columns.size(); ++i) {
            const int col = columns.at(i);
            if (!m_model->columnDefAt(col).isResizable())
                continue;
            const int borderX = columnX(col) + columnWidth(col);
            if (qAbs(pos.x() - borderX) <= kResizeHandleTolerance)
                return col;
        }
        return -1;
    };

    int result = checkBorder(m_leftPinnedColumns);
    if (result >= 0)
        return result;

    result = checkBorder(m_bodyColumns);
    if (result >= 0)
        return result;

    return checkBorder(m_rightPinnedColumns);
}

void AuGridWidget::startColumnResize(int column, int x)
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return;

    m_isResizing = true;
    m_resizeColumn = column;
    m_resizeStartX = x;
    m_resizeStartWidth = m_columnOffsets.at(column + 1) - m_columnOffsets.at(column);
    setCursor(Qt::SplitHCursor);
}

void AuGridWidget::updateColumnResize(int x)
{
    if (!m_isResizing || !m_model)
        return;

    const int delta = x - m_resizeStartX;
    m_model->setColumnWidth(m_resizeColumn, m_resizeStartWidth + delta);
}

void AuGridWidget::endColumnResize()
{
    m_isResizing = false;
    m_resizeColumn = -1;
    setCursor(Qt::ArrowCursor);
}

int AuGridWidget::clampColumnWidth(int column, int width) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return width;

    const AuColumnDef def = m_model->columnDefAt(column);
    int result = qMax(def.minWidth(), width);
    if (def.maxWidth() > 0)
        result = qMin(def.maxWidth(), result);
    return result;
}

int AuGridWidget::measureAutoFitWidth(int column)
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return 0;

    if (isColumnHidden(column))
        return 0;

    const AuColumnDef def = m_model->columnDefAt(column);
    int maxWidth = 0;

    // 内置复选框列：宽度按复选框尺寸加内边距计算，不测量文本
    if (def.isCheckbox()) {
        const int checkboxSize = m_style->integer(AuGridStyle::CheckboxSize, m_model);
        const int hPadding = m_style->integer(AuGridStyle::CellHorizontalPadding, m_model, -1, column);
        return clampColumnWidth(column, checkboxSize + 2 * hPadding);
    }

    // 表头宽度
    const QFont headerFont = m_style->value(AuGridStyle::HeaderFont, m_model, -1, column).value<QFont>();
    const QFontMetrics headerFm(headerFont);
    const int headerWidth = headerFm.horizontalAdvance(def.headerName()) + 12; // 左右各 6px 内边距
    maxWidth = qMax(maxWidth, headerWidth);

    // 可视行单元格宽度
    const bool treeFirst = m_model->isTreeMode() && column == treeColumn();
    const QPair<int, int> rowRange = visibleRowRange();
    for (int row = rowRange.first; row <= rowRange.second; ++row) {
        // 被合并覆盖的单元格数据不显示，不参与列宽测量
        const CellSpan span = spanAt(row, column);
        if (span.isValid() && (row != span.anchorRow || column != span.anchorColumn))
            continue;
        const QString text = m_model->cellValue(row, column).toString();
        // 使用与渲染一致的行内字体和内边距，避免按默认字体测量导致大字体行被截断
        const QFont cellFont = m_style->font(m_model, row, column);
        const QFontMetrics cellFm(cellFont);
        const int hPadding = m_style->integer(AuGridStyle::CellHorizontalPadding, m_model, row, column);
        // 树形首列：文本前还有缩进 + 箭头占位
        int treeLeading = 0;
        if (treeFirst) {
            const int originalRow = m_model->originalRowForVisibleRow(row);
            if (originalRow >= 0)
                treeLeading = treeLeadingWidth(m_model->rowDepth(originalRow));
        }
        const int cellWidth = cellFm.horizontalAdvance(text) + 2 * hPadding + 2 + treeLeading; // +2 补偿加粗字体笔画悬出与 1px 边框
        maxWidth = qMax(maxWidth, cellWidth);
    }

    m_autoFitDirty[column] = false;
    return clampColumnWidth(column, maxWidth);
}

void AuGridWidget::invalidateAutoFit(int column)
{
    if (column >= 0 && column < m_autoFitDirty.size())
        m_autoFitDirty[column] = true;
}

void AuGridWidget::fitColumnToContent(int column)
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return;

    if (!m_model->columnDefAt(column).isResizable())
        return;

    const int fitted = measureAutoFitWidth(column);
    m_model->setColumnWidth(column, fitted);
}

void AuGridWidget::refreshAutoFitColumns()
{
    if (!m_model || m_refreshingAutoFitColumns)
        return;
    // 行/列拖拽过程中不重建列偏移，避免列宽跳变干扰拖拽动画
    if (m_isRowDragging || m_isColumnDragging)
        return;

    m_refreshingAutoFitColumns = true;

    bool changed = false;
    const int columnCount = m_model->columnCount();
    for (int col = 0; col < columnCount; ++col) {
        if (m_model->columnDefAt(col).widthMode() != AuColumnDef::ColumnWidthMode::AutoFit)
            continue;
        if (col >= m_autoFitWidths.size())
            continue;

        // 按当前可视内容重新测量；只增不减，避免滚动/编辑时列宽来回抖动
        const int measured = measureAutoFitWidth(col);
        if (measured > m_autoFitWidths.at(col)) {
            m_autoFitWidths[col] = measured;
            changed = true;
        }
    }

    if (changed) {
        rebuildColumnOffsets();
        updateScrollBars();
        update();
    }

    m_refreshingAutoFitColumns = false;
}

void AuGridWidget::autoSizeColumn(int column)
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return;

    invalidateAutoFit(column);
    rebuildColumnOffsets();
    updateScrollBars();
    update();
}

void AuGridWidget::autoSizeAllColumns()
{
    m_autoFitDirty.fill(true);
    rebuildColumnOffsets();
    updateScrollBars();
    update();
}

void AuGridWidget::setRowNumberColumnVisible(bool visible)
{
    if (m_showRowNumberColumn == visible)
        return;

    // 带显隐动画（与列显隐观感一致）；最终状态由动画路径应用
    animateSpecialColumnVisibility(SpecialRowNumber, visible);
}

bool AuGridWidget::isRowNumberColumnVisible() const
{
    return m_showRowNumberColumn;
}

void AuGridWidget::setCheckboxColumnVisible(bool visible)
{
    if (m_showCheckboxColumn == visible)
        return;

    // 带显隐动画（与列显隐观感一致）；勾选集合在隐藏动画结束时随最终状态清空
    animateSpecialColumnVisibility(SpecialCheckbox, visible);
}

bool AuGridWidget::isCheckboxColumnVisible() const
{
    return m_showCheckboxColumn;
}

int AuGridWidget::topLevelLeafCount(const AuColumnDef &def) const
{
    if (!def.isGroup())
        return 1;
    int count = 0;
    for (const AuColumnDef &child : def.children())
        count += topLevelLeafCount(child);
    return count;
}

QVector<int> AuGridWidget::leafPath(int leafColumn) const
{
    QVector<int> path;
    if (!m_model || leafColumn < 0 || leafColumn >= m_model->columnCount())
        return path;

    int cursor = 0;
    std::function<bool(const QVector<AuColumnDef> &)> walk = [&](const QVector<AuColumnDef> &nodes) -> bool {
        for (int i = 0; i < nodes.size(); ++i) {
            const AuColumnDef &def = nodes.at(i);
            path.append(i);
            if (def.isGroup()) {
                if (walk(def.children()))
                    return true;
            } else {
                if (cursor == leafColumn)
                    return true;
                ++cursor;
            }
            path.removeLast();
        }
        return false;
    };

    walk(m_model->topLevelColumnDefs());
    return path;
}

const AuColumnDef *AuGridWidget::nodeAtPath(const QVector<int> &path) const
{
    if (!m_model)
        return nullptr;

    const QVector<AuColumnDef> *container = &m_model->topLevelColumnDefs();
    const AuColumnDef *node = nullptr;
    for (int idx : path) {
        if (!container || idx < 0 || idx >= container->size())
            return nullptr;
        node = &container->at(idx);
        if (node->isGroup())
            container = &node->children();
        else
            container = nullptr;
    }
    return node;
}

int AuGridWidget::firstLeafOfPath(const QVector<int> &path) const
{
    if (!m_model)
        return 0;

    int first = 0;
    const QVector<AuColumnDef> *container = &m_model->topLevelColumnDefs();
    for (int idx : path) {
        if (!container || idx < 0 || idx >= container->size())
            return first;
        for (int i = 0; i < idx; ++i)
            first += topLevelLeafCount(container->at(i));
        container = &container->at(idx).children();
    }
    return first;
}

int AuGridWidget::leafCountOfPath(const QVector<int> &path) const
{
    if (!m_model)
        return 0;
    if (path.isEmpty())
        return m_model->columnCount();
    const AuColumnDef *node = nodeAtPath(path);
    return node ? topLevelLeafCount(*node) : 0;
}

int AuGridWidget::firstLeafOfTopLevelIndex(int topLevelIndex) const
{
    if (!m_model || topLevelIndex < 0 || topLevelIndex >= m_model->topLevelColumnDefs().size())
        return -1;

    const QVector<AuColumnDef> &top = m_model->topLevelColumnDefs();
    int seen = 0;
    for (int i = 0; i < topLevelIndex; ++i)
        seen += topLevelLeafCount(top.at(i));
    return seen;
}

int AuGridWidget::topLevelColumnIndexAt(int x) const
{
    if (!m_model)
        return -1;

    const QVector<AuColumnDef> &top = m_model->topLevelColumnDefs();
    int seen = 0;
    for (int i = 0; i < top.size(); ++i) {
        const int leaves = topLevelLeafCount(top.at(i));
        if (leaves <= 0) {
            continue;
        }
        const int firstLeaf = seen;
        const int lastLeaf = seen + leaves - 1;
        const int left = baseColumnX(firstLeaf);
        const int right = baseColumnX(lastLeaf) + columnWidth(lastLeaf);
        if (x >= left && x < right)
            return i;
        seen += leaves;
    }
    return -1;
}

AuGridWidget::ColumnRegion AuGridWidget::topLevelColumnRegion(int topLevelIndex) const
{
    if (!m_model || topLevelIndex < 0 || topLevelIndex >= m_model->topLevelColumnDefs().size())
        return ColumnRegion::Body;

    const QVector<AuColumnDef> &top = m_model->topLevelColumnDefs();
    int seen = 0;
    for (int i = 0; i < topLevelIndex; ++i)
        seen += topLevelLeafCount(top.at(i));

    const int firstLeaf = seen;
    if (firstLeaf < 0 || firstLeaf >= m_model->columnCount())
        return ColumnRegion::Body;
    return columnRegion(firstLeaf);
}

int AuGridWidget::columnContentOffset(int column) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return 0;

    const ColumnRegion region = columnRegion(column);
    const int x = columnX(column);
    if (region == ColumnRegion::Body)
        return x + m_xOffset;
    return x;
}

static bool reorderChildInTree(QVector<AuColumnDef> &nodes, const QVector<int> &path,
                                int oldIndex, int targetIndex)
{
    if (path.isEmpty()) {
        if (oldIndex < 0 || oldIndex >= nodes.size())
            return false;
        AuColumnDef dragged = nodes.takeAt(oldIndex);
        nodes.insert(qBound(0, targetIndex, nodes.size()), dragged);
        return true;
    }

    const int idx = path.first();
    if (idx < 0 || idx >= nodes.size())
        return false;

    QVector<AuColumnDef> children = nodes[idx].children();
    const bool ok = reorderChildInTree(children, path.mid(1), oldIndex, targetIndex);
    if (ok)
        nodes[idx].setChildren(children);
    return ok;
}

void AuGridWidget::startColumnDrag(const QVector<int> &parentPath, int index, const QPoint &globalPos)
{
    if (!m_model || index < 0)
        return;

    if (m_activeEditor)
        closeEditor(false);

    const bool topLevel = parentPath.isEmpty();

    int representativeLeaf = -1;
    if (topLevel) {
        if (index >= m_model->topLevelColumnDefs().size())
            return;
        representativeLeaf = firstLeafOfTopLevelIndex(index);
    } else {
        const AuColumnDef *parent = nodeAtPath(parentPath);
        if (!parent || !parent->isGroup() || index >= parent->children().size())
            return;
        int first = firstLeafOfPath(parentPath);
        const QVector<AuColumnDef> &children = parent->children();
        for (int i = 0; i < index; ++i)
            first += topLevelLeafCount(children.at(i));
        representativeLeaf = first;
    }

    if (representativeLeaf < 0 || representativeLeaf >= m_model->columnCount())
        return;

    m_isColumnDragging = true;
    m_columnDragParentPath = parentPath;
    m_columnDragStartColumn = index;
    m_draggingTopLevelColumns.clear();
    m_draggingTopLevelColumns.append(index);
    m_columnDragTargetIndex = -1;
    m_columnDragLastX = globalPos.x();
    m_columnDragDirection = 0;
    m_columnDragRegion = columnRegion(representativeLeaf);

    m_columnDragRegionFirst = -1;
    m_columnDragRegionLast = -1;
    if (topLevel) {
        const int topCount = m_model->topLevelColumnDefs().size();
        for (int i = 0; i < topCount; ++i) {
            if (topLevelColumnRegion(i) == m_columnDragRegion) {
                if (m_columnDragRegionFirst < 0)
                    m_columnDragRegionFirst = i;
                m_columnDragRegionLast = i;
            }
        }
        if (m_columnDragRegionFirst < 0) {
            m_columnDragRegionFirst = index;
            m_columnDragRegionLast = index;
        }
    }

    m_columnDragParentLeafStart = topLevel ? 0 : firstLeafOfPath(parentPath);
    m_columnDragParentLeafEnd = m_columnDragParentLeafStart
        + (topLevel ? m_model->columnCount() : leafCountOfPath(parentPath));

    clearHoverCell();

    ensureDragFloatWidget();
    updateColumnDragFloatWidgetText();
    m_dragFloatWidget->adjustSize();
    m_dragFloatWidget->show();
    m_dragFloatWidget->raise();
    updateColumnDragFloatWidgetPosition(globalPos);

    update();
}

void AuGridWidget::updateColumnDrag(const QPoint &globalPos)
{
    if (!m_isColumnDragging || !m_model)
        return;

    updateColumnDragFloatWidgetPosition(globalPos);

    const int dx = globalPos.x() - m_columnDragLastX;
    m_columnDragLastX = globalPos.x();
    if (dx < 0)
        m_columnDragDirection = -1;
    else if (dx > 0)
        m_columnDragDirection = 1;

    const int targetIndex = targetColumnIndexForDrag(mapFromGlobal(globalPos));
    if (targetIndex < 0)
        return;

    if (targetIndex == m_columnDragTargetIndex)
        return;
    m_columnDragTargetIndex = targetIndex;

    const int oldIndex = m_draggingTopLevelColumns.isEmpty() ? -1 : m_draggingTopLevelColumns.first();
    if (oldIndex < 0)
        return;

    if (targetIndex == oldIndex) {
        finishColumnReorderAnimation();
        return;
    }

    finishColumnReorderAnimation();
    startColumnReorderAnimation();

    const QVector<int> oldAutoFitWidths = m_autoFitWidths;
    QHash<QString, int> fieldToOldIndex;
    for (int i = 0; i < m_columnDragAnimationOldFields.size(); ++i)
        fieldToOldIndex.insert(m_columnDragAnimationOldFields.at(i), i);

    QVector<AuColumnDef> defs = m_model->topLevelColumnDefs();
    reorderChildInTree(defs, m_columnDragParentPath, oldIndex, targetIndex);
    m_model->setColumnDefs(defs);

    m_autoFitWidths.resize(m_model->columnCount());
    m_autoFitDirty.resize(m_model->columnCount());
    m_autoFitDirty.fill(true);
    for (int i = 0; i < m_model->columnCount(); ++i) {
        const QString field = m_model->fieldAtColumn(i);
        const int oldLeafIndex = fieldToOldIndex.value(field, -1);
        if (oldLeafIndex >= 0 && oldLeafIndex < oldAutoFitWidths.size()) {
            m_autoFitWidths[i] = oldAutoFitWidths.at(oldLeafIndex);
            m_autoFitDirty[i] = false;
        }
    }

    rebuildColumnOffsets();

    m_draggingTopLevelColumns.clear();
    m_draggingTopLevelColumns.append(targetIndex);

    if (m_selectedColumn >= 0 && m_selectedColumn < m_columnDragAnimationOldFields.size()) {
        const QString selectedField = m_columnDragAnimationOldFields.at(m_selectedColumn);
        const int newSelectedColumn = m_model->columnIndexForField(selectedField);
        if (newSelectedColumn >= 0)
            m_selectedColumn = newSelectedColumn;
    }

    for (int i = 0; i < m_model->columnCount(); ++i) {
        const QString field = m_model->fieldAtColumn(i);
        const int oldIndex = fieldToOldIndex.value(field, -1);
        if (oldIndex < 0 || oldIndex == i)
            continue;
        if (i >= m_columnOffsets.size() - 1)
            continue;
        ColumnDragAnimationEntry entry;
        entry.column = i;
        entry.startOffset = m_columnDragAnimationOldOffsets.at(oldIndex);
        entry.endOffset = columnContentOffset(i);
        m_columnDragAnimationEntries.append(entry);
    }

    if (!m_columnDragAnimationEntries.isEmpty()) {
        m_columnDragOffsetAnimation = new QVariantAnimation(this);
        m_columnDragOffsetAnimation->setStartValue(0.0);
        m_columnDragOffsetAnimation->setEndValue(1.0);
        m_columnDragOffsetAnimation->setDuration(200);
        m_columnDragOffsetAnimation->setEasingCurve(QEasingCurve::InOutSine);
        connect(m_columnDragOffsetAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            applyColumnReorderAnimation(value.toReal());
        });
        connect(m_columnDragOffsetAnimation, &QVariantAnimation::finished, this, &AuGridWidget::finishColumnReorderAnimation);
        m_columnDragOffsetAnimation->start();
    }

    update();
}

void AuGridWidget::endColumnDrag()
{
    if (!m_isColumnDragging)
        return;

    m_isColumnDragging = false;
    m_columnDragStartColumn = -1;
    m_draggingTopLevelColumns.clear();
    m_columnDragTargetIndex = -1;
    if (m_dragFloatWidget)
        m_dragFloatWidget->hide();

    finishColumnReorderAnimation();
    updateScrollBars();
    clearHoverCell();
    update();
}

void AuGridWidget::cancelColumnDrag()
{
    if (!m_isColumnDragging)
        return;

    m_isColumnDragging = false;
    m_columnDragStartColumn = -1;
    m_draggingTopLevelColumns.clear();
    m_columnDragTargetIndex = -1;
    if (m_dragFloatWidget)
        m_dragFloatWidget->hide();

    finishColumnReorderAnimation();
    clearHoverCell();
    update();
}

int AuGridWidget::targetColumnIndexForDrag(const QPoint &pos) const
{
    if (!m_model)
        return -1;

    const int oldIndex = m_draggingTopLevelColumns.isEmpty() ? -1 : m_draggingTopLevelColumns.first();
    if (oldIndex < 0)
        return -1;

    if (m_columnDragParentPath.isEmpty()) {
        const int topIdx = topLevelColumnIndexAt(pos.x());
        int t;
        if (topIdx >= 0 && topLevelColumnRegion(topIdx) == m_columnDragRegion) {
            if (m_columnDragDirection < 0)
                t = topIdx;       // 向左拖：插入到目标列之前
            else
                t = topIdx + 1;   // 向右拖：插入到目标列之后
        } else {
            int left = INT_MAX;
            int right = INT_MIN;
            const QVector<AuColumnDef> &top = m_model->topLevelColumnDefs();
            for (int i = m_columnDragRegionFirst; i <= m_columnDragRegionLast && i < top.size(); ++i) {
                const int fl = firstLeafOfTopLevelIndex(i);
                const int leaves = topLevelLeafCount(top.at(i));
                const int ll = fl + leaves - 1;
                if (fl < 0 || ll < 0)
                    continue;
                left = qMin(left, baseColumnX(fl));
                right = qMax(right, baseColumnX(ll) + columnWidth(ll));
            }
            if (left == INT_MAX)
                return oldIndex;
            if (pos.x() < left)
                t = m_columnDragRegionFirst;
            else
                t = m_columnDragRegionLast + 1;
        }
        t = qBound(m_columnDragRegionFirst, t, m_columnDragRegionLast + 1);

        if (t <= oldIndex)
            return t;
        if (t > oldIndex + 1)
            return t - 1;
        return oldIndex;
    }

    // 任意层级分组：只允许在父分组内部交换
    const AuColumnDef *parent = nodeAtPath(m_columnDragParentPath);
    if (!parent || !parent->isGroup())
        return -1;
    const QVector<AuColumnDef> &children = parent->children();
    const int childCount = children.size();
    if (childCount == 0)
        return -1;

    const int firstLeaf = m_columnDragParentLeafStart;
    const int lastLeaf = m_columnDragParentLeafEnd - 1;
    const int leaf = headerColumnAt(pos.x());

    int t;
    if (leaf >= firstLeaf && leaf <= lastLeaf) {
        int h = -1;
        int acc = firstLeaf;
        for (int i = 0; i < childCount; ++i) {
            const int childLeaves = topLevelLeafCount(children.at(i));
            if (leaf >= acc && leaf < acc + childLeaves) {
                h = i;
                break;
            }
            acc += childLeaves;
        }
        if (h < 0)
            h = childCount - 1;
        if (m_columnDragDirection < 0)
            t = h;       // 向左拖：插入到目标列之前
        else
            t = h + 1;   // 向右拖：插入到目标列之后
    } else {
        const int firstX = baseColumnX(firstLeaf);
        const int lastRight = baseColumnX(lastLeaf) + columnWidth(lastLeaf);
        if (pos.x() < firstX)
            t = 0;
        else if (pos.x() >= lastRight)
            t = childCount;
        else
            t = oldIndex;
    }
    t = qBound(0, t, childCount);

    if (t <= oldIndex)
        return t;
    if (t > oldIndex + 1)
        return t - 1;
    return oldIndex;
}

void AuGridWidget::updateColumnDragFloatWidgetPosition(const QPoint &globalPos)
{
    if (!m_dragFloatWidget)
        return;

    const QRect globalRect = QRect(mapToGlobal(QPoint(0, 0)), size());
    const int headerTop = globalRect.top();
    const int maxY = globalRect.bottom() - m_dragFloatWidget->height();
    QPoint target = globalPos - QPoint(m_dragFloatWidget->width() / 2, m_dragFloatWidget->height() / 2);
    target.setX(qMax(globalRect.left(), qMin(globalRect.right() - m_dragFloatWidget->width(), target.x())));
    target.setY(qMax(headerTop, qMin(maxY, target.y())));
    m_dragFloatWidget->move(mapFromGlobal(target));
}

void AuGridWidget::updateColumnDragFloatWidgetText()
{
    if (!m_dragFloatWidget || !m_model || m_columnDragStartColumn < 0)
        return;

    QString headerName;
    if (m_columnDragParentPath.isEmpty()) {
        if (m_columnDragStartColumn < m_model->topLevelColumnDefs().size())
            headerName = m_model->topLevelColumnDefs().at(m_columnDragStartColumn).headerName();
    } else {
        const AuColumnDef *parent = nodeAtPath(m_columnDragParentPath);
        if (parent && parent->isGroup() && m_columnDragStartColumn < parent->children().size())
            headerName = parent->children().at(m_columnDragStartColumn).headerName();
    }

    m_dragFloatWidget->setText(headerName);
    m_dragFloatWidget->adjustSize();
}

void AuGridWidget::startColumnReorderAnimation()
{
    m_columnDragAnimationEntries.clear();
    m_columnDragAnimationOldOrder.clear();
    m_columnDragAnimationOldOffsets.clear();
    m_columnDragAnimationOldFields.clear();

    if (!m_model || m_model->columnCount() == 0)
        return;

    const int count = m_model->columnCount();
    m_columnDragAnimationOldOrder.reserve(count);
    m_columnDragAnimationOldOffsets.reserve(count);
    m_columnDragAnimationOldFields.reserve(count);
    for (int i = 0; i < count; ++i) {
        m_columnDragAnimationOldOrder.append(i);
        m_columnDragAnimationOldOffsets.append(columnContentOffset(i));
        m_columnDragAnimationOldFields.append(m_model->fieldAtColumn(i));
    }
}

void AuGridWidget::applyColumnReorderAnimation(qreal progress)
{
    m_columnDragAnimationProgress = qBound(0.0, progress, 1.0);
    update();
}

void AuGridWidget::finishColumnReorderAnimation()
{
    // 同 finishColumnVisibilityAnimation：可能在自身 finished 槽内被调用，用 deleteLater
    if (m_columnDragOffsetAnimation) {
        QVariantAnimation *anim = m_columnDragOffsetAnimation;
        m_columnDragOffsetAnimation = nullptr;
        QObject::disconnect(anim, nullptr, this, nullptr);
        anim->stop();
        anim->deleteLater();
    }
    m_columnDragAnimationEntries.clear();
    m_columnDragAnimationOldOrder.clear();
    m_columnDragAnimationOldOffsets.clear();
    m_columnDragAnimationOldFields.clear();
    m_columnDragAnimationProgress = 0.0;
    update();
}

