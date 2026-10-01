// ============================================================================
// AuGridWidget 交互实现：鼠标 / 滚轮 / 键盘 / 焦点 / 右键菜单事件，
// 单元格与表头悬停跟踪，行选中与键盘导航，滚动定位，tooltip 与复制。
// ============================================================================

#include "AuGridWidget.h"
#include "AuScrollBar.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QCursor>
#include <QDebug>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QWheelEvent>

int AuGridWidget::selectedRow() const
{
    return m_selectedRow;
}

int AuGridWidget::selectedColumn() const
{
    return m_selectedColumn;
}

void AuGridWidget::mousePressEvent(QMouseEvent *event)
{
    // 加载遮罩显示期间屏蔽交互
    if (m_loading) {
        event->accept();
        return;
    }

    if (!isInsideRoundedRect(event->pos())) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (!m_model)
        return;

    if (event->button() == Qt::LeftButton && event->pos().y() <= headerHeight()) {
        const int resizeCol = resizeHandleAt(event->pos());
        if (resizeCol >= 0) {
            startColumnResize(resizeCol, event->pos().x());
            return;
        }

        const int cbHeaderStart = checkboxColumnX();
        if (m_showCheckboxColumn && event->pos().x() >= cbHeaderStart && event->pos().x() < cbHeaderStart + checkboxColumnWidth()) {
            toggleCheckboxHeader();
            event->accept();
            return;
        }

        // 自定义列头渲染器按钮命中：记录待点击，不进入列拖拽/排序
        int hLevel = 0;
        QPoint hPos;
        const int hLeaf = headerLeafAt(event->pos(), &hLevel, &hPos);
        if (hLeaf >= 0) {
            QSharedPointer<AuHeaderRenderer> hr = headerRendererForColumn(hLeaf);
            if (hr && hr->hitTest(hLeaf, hLevel, headerNodeRect(hLeaf, hLevel), hPos)) {
                m_headerRenderClickPending = true;
                event->accept();
                return;
            }
        }

        // 列拖拽：按点击的表头深度与列位置命中具体表头节点（叶子或任意层级分组）
        const int leaf = headerColumnAt(event->pos().x());
        if (leaf >= 0) {
            const QVector<int> path = leafPath(leaf);
            if (!path.isEmpty()) {
                const int levelHeight = headerLevelHeight();
                const int depth = qBound(0, event->pos().y() / levelHeight, m_model->headerLevelCount() - 1);
                const int leafDepth = path.size() - 1;
                const int hitDepth = qMin(depth, leafDepth);

                QVector<int> parentPath;
                for (int d = 0; d < hitDepth; ++d)
                    parentPath.append(path.at(d));
                const int index = path.at(hitDepth);

                m_isPotentialColumnDrag = true;
                m_potentialColumnDragColumn = (hitDepth == leafDepth) ? leaf : -1;
                m_potentialColumnDragStartPos = event->globalPos();
                m_columnDragParentPath = parentPath;
                m_columnDragStartColumn = index;
                event->accept();
                return;
            }
        }
    }

    const QPoint cell = cellAtPosition(event->pos());
    if (cell.y() < 0 || (cell.x() < 0 && !isCheckboxColumn(cell.x()) && !isDragHandleColumn(cell.x())))
        return;

    const bool ctrlPressed = (event->modifiers() & Qt::ControlModifier) != 0;

    // 树形展开箭头：点击切换展开态（不改选中、不触发 cellClicked）
    if (event->button() == Qt::LeftButton && isTreeMode() && cell.x() == treeColumn()) {
        const QRect expanderRect = treeExpanderRect(cell.y());
        if (expanderRect.isValid() && expanderRect.contains(event->pos())) {
            toggleRowExpanded(cell.y());
            event->accept();
            return;
        }
    }

    // 复选框列：切换勾选状态并按选中模式更新行选中
    if (isCheckboxColumn(cell.x())) {
        toggleCheckboxCell(cell.y());

        const bool changed = applyCheckboxClickSelection(cell.y(), -1, false);
        update();
        if (changed)
            emit selectionChanged();
        event->accept();
        return;
    }

    // 行拖拽手柄列：开始拖拽
    if (m_rowDragEnabled && isDragHandleColumn(cell.x()) && event->button() == Qt::LeftButton) {
        startRowDrag(cell.y(), event->globalPos());
        event->accept();
        return;
    }

    // 内置复选框单元格：可编辑列左键点击直接切换勾选状态
    if (cell.x() >= 0 && cell.x() < m_model->columnCount()
        && m_model->columnDefAt(cell.x()).isCheckbox()
        && event->button() == Qt::LeftButton
        && isCellEditable(cell.y(), cell.x())) {
        if (m_activeEditor && !tryFinishCurrentEdit())
            return;
        toggleCheckboxCellValue(cell.y(), cell.x());

        // 不可聚焦格不抢占锚点列（withColumn=false），行选中仍生效
        const bool changed = applyCheckboxClickSelection(cell.y(), cell.x(),
                                                         isCellFocusable(cell.y(), cell.x()));
        update();
        emit cellClicked(cell.y(), cell.x());
        if (changed)
            emit selectionChanged();
        event->accept();
        return;
    }

    // 如果当前有编辑器，先尝试结束；若校验要求保持编辑，则禁止切换单元格
    if (m_activeEditor && !tryFinishCurrentEdit())
        return;

    // 不可聚焦的单元格（如放操作按钮的自定义列）：点击仍触发行选中与 cellClicked，
    // 但锚点（焦点框）不落到该格——保持原锚点列
    const bool changed = applyRowSelectionClick(cell.y(), cell.x(), ctrlPressed,
                                                isCellFocusable(cell.y(), cell.x()));
    update();
    emit cellClicked(cell.y(), cell.x());
    if (changed)
        emit selectionChanged();
}

void AuGridWidget::mouseMoveEvent(QMouseEvent *event)
{
    // 加载遮罩显示期间屏蔽交互：清除悬停高亮与光标，避免透过半透明蒙层显现
    if (m_loading) {
        if (!m_isResizing)
            setCursor(Qt::ArrowCursor);
        clearHoverCell();
        clearHeaderHover();
        event->accept();
        return;
    }

    if (m_isRowDragging) {
        updateRowDrag(event->globalPos());
        return;
    }

    if (m_isPotentialColumnDrag) {
        if ((event->globalPos() - m_potentialColumnDragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_isPotentialColumnDrag = false;
            startColumnDrag(m_columnDragParentPath, m_columnDragStartColumn, event->globalPos());
        }
        return;
    }

    if (m_isColumnDragging) {
        updateColumnDrag(event->globalPos());
        return;
    }

    if (m_isResizing) {
        updateColumnResize(event->pos().x());
        return;
    }

    if (!isInsideRoundedRect(event->pos())) {
        setCursor(Qt::ArrowCursor);
        clearHoverCell();
        return;
    }

    if (!m_model) {
        clearHoverCell();
        return;
    }

    if (event->pos().y() <= headerHeight() && resizeHandleAt(event->pos()) >= 0) {
        setCursor(Qt::SplitHCursor);
        clearHoverCell();
        return;
    }

    if (event->pos().y() <= headerHeight()) {
        clearHoverCell();
        updateHeaderHover(event->pos());
        return;
    }

    clearHeaderHover();
    updateHoverCell(event->pos());
}

void AuGridWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_headerRenderClickPending && event->button() == Qt::LeftButton) {
        m_headerRenderClickPending = false;
        int hLevel = 0;
        QPoint hPos;
        const int hLeaf = headerLeafAt(event->pos(), &hLevel, &hPos);
        if (hLeaf >= 0) {
            QSharedPointer<AuHeaderRenderer> hr = headerRendererForColumn(hLeaf);
            if (hr && hr->hitTest(hLeaf, hLevel, headerNodeRect(hLeaf, hLevel), hPos))
                hr->headerMouseClick(hLeaf, hLevel, headerNodeRect(hLeaf, hLevel), hPos);
        }
        event->accept();
        return;
    }

    if (m_isResizing && event->button() == Qt::LeftButton)
        endColumnResize();

    if (m_isPotentialColumnDrag && event->button() == Qt::LeftButton) {
        m_isPotentialColumnDrag = false;
        const int column = m_potentialColumnDragColumn;
        m_potentialColumnDragColumn = -1;
        if (m_sortingEnabled && m_model && column >= 0 && m_model->columnDefAt(column).isSortable()) {
            const int levelHeight = headerLevelHeight();
            const int leafHeaderTop = headerHeight() - levelHeight;
            if (event->pos().y() >= leafHeaderTop)
                toggleSort(column);
        }
        event->accept();
        return;
    }

    if (m_isRowDragging && event->button() == Qt::LeftButton)
        endRowDrag();

    if (m_isColumnDragging && event->button() == Qt::LeftButton)
        endColumnDrag();
}

void AuGridWidget::leaveEvent(QEvent * /*event*/)
{
    if (m_isRowDragging)
        return;
    if (!m_isResizing)
        setCursor(Qt::ArrowCursor);
    clearHoverCell();
    clearHeaderHover();
}

QRect AuGridWidget::headerNodeRect(int column, int level) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return QRect();
    const int depth = level - 1;
    const int levelHeight = headerLevelHeight();
    const int x = columnX(column);
    const int w = columnWidth(column);
    const int y = depth * levelHeight;
    const int h = (m_model->headerLevelCount() - depth) * levelHeight;
    return QRect(x, y, w, h);
}

int AuGridWidget::headerLeafAt(const QPoint &pos, int *level, QPoint *headerPos) const
{
    if (!m_model || pos.y() < 0 || pos.y() >= headerHeight())
        return -1;

    const int leaf = headerColumnAt(pos.x());
    if (leaf < 0)
        return -1;

    const QVector<int> path = leafPath(leaf);
    if (path.isEmpty())
        return -1;

    const int leafDepth = path.size() - 1;
    const int clickedDepth = qBound(0, pos.y() / headerLevelHeight(), m_model->headerLevelCount() - 1);
    if (clickedDepth < leafDepth)
        return -1; // 命中祖先分组表头

    const int lvl = leafDepth + 1;
    if (level)
        *level = lvl;
    if (headerPos)
        *headerPos = pos - headerNodeRect(leaf, lvl).topLeft();
    return leaf;
}

void AuGridWidget::updateHeaderHover(const QPoint &pos)
{
    int level = 0;
    QPoint headerPos;
    const int column = headerLeafAt(pos, &level, &headerPos);
    if (column < 0) {
        clearHeaderHover();
        setCursor(Qt::ArrowCursor);
        return;
    }

    QSharedPointer<AuHeaderRenderer> renderer = headerRendererForColumn(column);
    if (!renderer) {
        clearHeaderHover();
        setCursor(Qt::ArrowCursor);
        return;
    }

    const QRect rect = headerNodeRect(column, level);

    if (m_hoverHeaderColumn == column && m_hoverHeaderLevel == level) {
        renderer->headerMouseMove(column, level, rect, headerPos);
        m_hoverHeaderPos = headerPos;
    } else {
        notifyHeaderRendererLeave();
        m_hoverHeaderColumn = column;
        m_hoverHeaderLevel = level;
        m_hoverHeaderPos = headerPos;
        notifyHeaderRendererEnter(column, level, rect, headerPos);
    }

    setCursor(renderer->wantsHoverCursor(column, level, rect, headerPos)
              ? Qt::PointingHandCursor
              : Qt::ArrowCursor);
    update(QRect(0, 0, width(), headerHeight()));
}

void AuGridWidget::clearHeaderHover()
{
    if (m_hoverHeaderColumn < 0)
        return;
    notifyHeaderRendererLeave();
    m_hoverHeaderColumn = -1;
    m_hoverHeaderLevel = 0;
    m_hoverHeaderPos = QPoint(-1, -1);
    update(QRect(0, 0, width(), headerHeight()));
}

void AuGridWidget::notifyHeaderRendererEnter(int column, int level, const QRect &rect, const QPoint &headerPos)
{
    if (column < 0)
        return;
    QSharedPointer<AuHeaderRenderer> renderer = headerRendererForColumn(column);
    if (renderer)
        renderer->headerMouseEnter(column, level, rect, headerPos);
}

void AuGridWidget::notifyHeaderRendererLeave()
{
    if (m_hoverHeaderColumn < 0)
        return;
    QSharedPointer<AuHeaderRenderer> renderer = headerRendererForColumn(m_hoverHeaderColumn);
    if (renderer)
        renderer->headerMouseLeave(m_hoverHeaderColumn, m_hoverHeaderLevel);
}

bool AuGridWidget::isRowDragging(int visibleRow) const
{
    if (!m_isRowDragging || !m_model || visibleRow < 0 || visibleRow >= m_model->rowCount())
        return false;
    return m_draggingOriginalRows.contains(m_model->originalRowForVisibleRow(visibleRow));
}

void AuGridWidget::updateHoverVisual()
{
    // hover 变化整体重绘 body 区：合并单元格的锚点按整个合并区域填充 hover 背景，
    // 任何局部行/单元格刷新都会让被覆盖行切片残留旧像素（视觉上逐一出现、与整行高亮不同步）。
    // 可视行经过虚拟化后数量有限，整体重绘代价可忽略。
    update(QRect(0, headerHeight(), viewportWidthForColumns(), bodyHeight()));
}

void AuGridWidget::updateHoverCell(const QPoint &pos)
{
    m_hoverPos = pos;

    const QPoint cell = cellAtPosition(pos);
    const bool valid = cell.y() >= 0 && (cell.x() >= 0 || isCheckboxColumn(cell.x()) || isDragHandleColumn(cell.x()));

    if (!valid) {
        clearHoverCell();
        setCursor(Qt::ArrowCursor);
        return;
    }

    // 拖拽手柄列：手型光标
    if (isDragHandleColumn(cell.x())) {
        if (m_hoverRow != cell.y() || m_hoverColumn != cell.x()) {
            notifyRendererLeave(m_hoverRow, m_hoverColumn);
            m_hoverRow = cell.y();
            m_hoverColumn = cell.x();
            updateHoverVisual();
        }
        setCursor(Qt::OpenHandCursor);
        return;
    }

    const QRect crect = cellRect(cell.y(), cell.x());
    const QPoint cellPos = pos - crect.topLeft();
    QSharedPointer<AuCellRenderer> renderer = rendererForColumn(cell.x());

    if (m_hoverRow == cell.y() && m_hoverColumn == cell.x()) {
        renderer->cellMouseMove(cell.y(), cell.x(), cellPos);
        setCursor(renderer->wantsHoverCursor(cell.y(), cell.x(), cellPos)
                  ? Qt::PointingHandCursor
                  : Qt::ArrowCursor);
        applyTreeExpanderCursor(cell.x(), cell.y(), pos);
        return;
    }

    notifyRendererLeave(m_hoverRow, m_hoverColumn);

    m_hoverRow = cell.y();
    m_hoverColumn = cell.x();

    notifyRendererEnter(m_hoverRow, m_hoverColumn, cellPos);

    updateHoverVisual();

    emit cellHovered(m_hoverRow, m_hoverColumn);
    setCursor(renderer->wantsHoverCursor(m_hoverRow, m_hoverColumn, cellPos)
              ? Qt::PointingHandCursor
              : Qt::ArrowCursor);
    applyTreeExpanderCursor(cell.x(), cell.y(), pos);
}

/// 树形展开箭头热区：手型光标（优先于渲染器的光标判定）。
void AuGridWidget::applyTreeExpanderCursor(int column, int row, const QPoint &pos)
{
    if (!isTreeMode() || column != treeColumn())
        return;
    const QRect expanderRect = treeExpanderRect(row);
    if (expanderRect.isValid() && expanderRect.contains(pos))
        setCursor(Qt::PointingHandCursor);
}

void AuGridWidget::clearHoverCell()
{
    if (m_hoverRow < 0)
        return;

    notifyRendererLeave(m_hoverRow, m_hoverColumn);
    m_hoverRow = -1;
    m_hoverColumn = -1;
    m_hoverPos = QPoint(-1, -1);
    updateHoverVisual();
}

void AuGridWidget::notifyRendererEnter(int row, int column, const QPoint &cellPos)
{
    if (row < 0 || column < 0 || isCheckboxColumn(column))
        return;
    rendererForColumn(column)->cellMouseEnter(row, column, cellPos);
}

void AuGridWidget::notifyRendererLeave(int row, int column)
{
    if (row < 0 || column < 0 || isCheckboxColumn(column))
        return;
    rendererForColumn(column)->cellMouseLeave(row, column);
}

void AuGridWidget::focusInEvent(QFocusEvent *event)
{
    QWidget::focusInEvent(event);
    update();
}

void AuGridWidget::focusOutEvent(QFocusEvent *event)
{
    QWidget::focusOutEvent(event);
    update();
}

void AuGridWidget::contextMenuEvent(QContextMenuEvent *event)
{
    // 加载遮罩显示期间屏蔽交互
    if (m_loading) {
        event->accept();
        return;
    }

    if (!m_model) {
        QWidget::contextMenuEvent(event);
        return;
    }

    int headerColumn = -1;
    int headerLevel = 0;
    if (headerHitAt(event->pos(), headerColumn, headerLevel)) {
        emit headerRightClicked(headerColumn, headerLevel, event->globalPos());
        event->accept();
        return;
    }

    const QPoint cell = cellAtPosition(event->pos());
    if (cell.y() >= 0) {
        emit cellRightClicked(cell.y(), cell.x(), event->globalPos());
        event->accept();
        return;
    }

    QWidget::contextMenuEvent(event);
}

void AuGridWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    // 加载遮罩显示期间屏蔽交互
    if (m_loading) {
        event->accept();
        return;
    }

    if (!isInsideRoundedRect(event->pos())) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const int resizeCol = resizeHandleAt(event->pos());
        if (resizeCol >= 0) {
            fitColumnToContent(resizeCol);
            event->accept();
            return;
        }
    }

    const QPoint cell = cellAtPosition(event->pos());
    if (cell.x() < 0 || cell.y() < 0)
        return;

    if (isCheckboxColumn(cell.x()))
        return;

    if (cell.x() >= 0 && cell.x() < m_model->columnCount()
        && m_model->columnDefAt(cell.x()).isCheckbox())
        return;

    // 树形首列双击：切换展开 / 折叠（不进入编辑）
    if (isTreeMode() && cell.x() == treeColumn()) {
        const int originalRow = m_model->originalRowForVisibleRow(cell.y());
        if (originalRow >= 0 && m_model->rowHasChildren(originalRow)) {
            toggleRowExpanded(cell.y());
            event->accept();
            return;
        }
    }

    if (m_activeEditor && !tryFinishCurrentEdit())
        return;

    emit cellDoubleClicked(cell.y(), cell.x());
    editCell(cell.y(), cell.x());
}

void AuGridWidget::wheelEvent(QWheelEvent *event)
{
    // 加载遮罩显示期间屏蔽交互
    if (m_loading) {
        event->accept();
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    if (!isInsideRoundedRect(event->position().toPoint())) {
#else
    if (!isInsideRoundedRect(event->pos())) {
#endif
        QWidget::wheelEvent(event);
        return;
    }

    if (!m_model)
        return;

    const int deltaY = event->angleDelta().y();
    const int deltaX = event->angleDelta().x();
    const bool shift = event->modifiers() & Qt::ShiftModifier;

    // Shift + 垂直滚轮 = 水平滚动
    if (shift && deltaY != 0 && m_hScrollBarVisible) {
        const int step = rowHeight() * 3;
        m_hScrollBar->setValue(m_hScrollBar->value() - deltaY / 120 * step);
        updateEditorGeometry();
        event->accept();
        return;
    }

    if (deltaY != 0 && m_vScrollBarVisible) {
        const int step = rowHeight() * 3;
        m_vScrollBar->setValue(m_vScrollBar->value() - deltaY / 120 * step);
        updateEditorGeometry();
        event->accept();
    } else if (deltaX != 0 && m_hScrollBarVisible) {
        const int step = rowHeight() * 3;
        m_hScrollBar->setValue(m_hScrollBar->value() - deltaX / 120 * step);
        updateEditorGeometry();
        event->accept();
    }
}

int AuGridWidget::headerColumnAt(int x) const
{
    if (!m_model || m_model->columnCount() == 0 || m_columnOffsets.isEmpty())
        return -1;

    if (m_showRowNumberColumn && x >= 0 && x < rowNumberColumnWidth())
        return -1;

    const int cbStart = checkboxColumnX();
    if (m_showCheckboxColumn && x >= cbStart && x < cbStart + checkboxColumnWidth())
        return -1;

    const int dhStart = dragHandleColumnX();
    const int dhEnd = dhStart + dragHandleColumnWidth();
    if (m_rowDragEnabled && x >= dhStart && x < dhEnd)
        return -1;

    for (int col : m_leftPinnedColumns) {
        const int colX = columnX(col);
        const int w = columnWidth(col);
        if (x >= colX && x < colX + w)
            return col;
    }

    for (int col : m_rightPinnedColumns) {
        const int colX = columnX(col);
        const int w = columnWidth(col);
        if (x >= colX && x < colX + w)
            return col;
    }

    const int fixedWidth = fixedColumnsWidth();
    const int absoluteX = x + m_xOffset - fixedWidth;
    for (int col : m_bodyColumns) {
        if (absoluteX >= m_columnOffsets.at(col) && absoluteX < m_columnOffsets.at(col + 1))
            return col;
    }

    return -1;
}

bool AuGridWidget::headerHitAt(const QPoint &pos, int &column, int &level) const
{
    if (!m_model || pos.y() < 0 || pos.y() >= headerHeight())
        return false;

    if (m_showRowNumberColumn && pos.x() < rowNumberColumnWidth())
        return false;
    const int cbStart = checkboxColumnX();
    if (m_showCheckboxColumn && pos.x() >= cbStart && pos.x() < cbStart + checkboxColumnWidth())
        return false;
    const int dhStart = dragHandleColumnX();
    const int dhEnd = dhStart + dragHandleColumnWidth();
    if (m_rowDragEnabled && pos.x() >= dhStart && pos.x() < dhEnd)
        return false;

    const int leaf = headerColumnAt(pos.x());
    if (leaf < 0)
        return false;

    const QVector<int> path = leafPath(leaf);
    if (path.isEmpty())
        return false;

    const int depth = qBound(0, pos.y() / headerLevelHeight(), m_model->headerLevelCount() - 1);
    const int leafDepth = path.size() - 1;
    const int hitDepth = qMin(depth, leafDepth);

    QVector<int> nodePath;
    for (int d = 0; d <= hitDepth; ++d)
        nodePath.append(path.at(d));

    column = firstLeafOfPath(nodePath);
    level = hitDepth + 1;
    return true;
}

bool AuGridWidget::isCellFocusable(int row, int column) const
{
    // 辅助列（行号 / 复选框 / 拖拽手柄）不参与该控制：其点击路径本就不落锚点列
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return true;

    if (!m_model->columnDefAt(column).isFocusable())
        return false;

    if (m_cellFocusableCallback)
        return m_cellFocusableCallback(m_model, row, column);

    return true;
}

void AuGridWidget::setCellFocusableCallback(const std::function<bool(const AuGridModel *, int, int)> &callback)
{
    m_cellFocusableCallback = callback;
    update();
}

int AuGridWidget::nearestFocusableColumn(int row, int column) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return -1;

    // 就近向两侧搜索第一个可落锚点的列（跳过隐藏 / 合并覆盖 / 不可聚焦）；
    // 两侧等距时优先取右侧（与向右导航的自然延伸一致）
    const int colCount = m_model->columnCount();
    for (int dist = 1; dist < colCount; ++dist) {
        const int left = column - dist;
        const int right = column + dist;
        const bool leftOk = left >= 0 && !isColumnHidden(left)
                            && !isSpanCoveredCell(row, left) && isCellFocusable(row, left);
        const bool rightOk = right < colCount && !isColumnHidden(right)
                             && !isSpanCoveredCell(row, right) && isCellFocusable(row, right);
        if (rightOk)
            return right;
        if (leftOk)
            return left;
    }
    return -1;
}

void AuGridWidget::setCellTooltipTextCallback(const std::function<QString(const AuGridModel *, int, int, const QString &)> &callback)
{
    m_cellTooltipTextCallback = callback;
}

QString AuGridWidget::cellTooltipText(const QPoint &pos) const
{
    if (!m_model || m_loading || m_activeEditor)
        return QString();

    // 悬停在滚动条等子控件上时不显示（childAt 无子控件时返回 nullptr）
    if (childAt(pos))
        return QString();

    // cellAtPosition 约定：x = 列（内置辅助列为负值编号），y = 行
    const QPoint cell = cellAtPosition(pos);
    const int row = cell.y();
    const int column = cell.x();
    if (row < 0 || column < 0)
        return QString();

    const AuColumnDef def = m_model->columnDefAt(column);
    // 仅 NoWrap / NoWrapTruncate 存在"内容超出列宽被裁剪"的语义：
    // WordWrap 自动换行完整展示，NoWrapOverflow 有意溢出到相邻单元格，均不弹
    if (!def.isTooltipEnabled() || def.isCheckbox()
        || (def.textWrapMode() != AuColumnDef::TextWrapMode::NoWrap
            && def.textWrapMode() != AuColumnDef::TextWrapMode::NoWrapTruncate))
        return QString();

    const QString text = m_model->cellValue(row, column).toString();
    if (text.isEmpty())
        return QString();

    // 与默认渲染器的文本矩形保持同一几何约定：左右内边距 + 树形首列缩进/箭头占位。
    // cellRect 对合并单元格返回整块合并矩形，与锚点按合并区域绘制的宽度一致。
    const QRect rect = cellRect(row, column);
    const int hPadding = m_style->integer(AuGridStyle::CellHorizontalPadding, m_model, row, column);
    int treeLeading = 0;
    const int treeCol = isTreeMode() ? treeColumn() : -1;
    if (column == treeCol) {
        const int originalRow = m_model->originalRowForVisibleRow(row);
        if (originalRow >= 0)
            treeLeading = treeLeadingWidth(m_model->rowDepth(originalRow));
    }

    QFont font = m_style->font(m_model, row, column);
    if (font == QFont())
        font = this->font();
    if (QFontMetrics(font).horizontalAdvance(text) <= rect.width() - 2 * hPadding - treeLeading)
        return QString();

    if (m_cellTooltipTextCallback)
        return m_cellTooltipTextCallback(m_model, row, column, text);
    return text;
}

void AuGridWidget::copySelectedCell()
{
    if (!m_model || m_selectedRow < 0 || m_selectedColumn < 0)
        return;

    QSharedPointer<AuCellRenderer> renderer = rendererForColumn(m_selectedColumn);
    if (!renderer)
        return;

    const AuColumnDef def = m_model->columnDefAt(m_selectedColumn);
    const QVariant value = m_model->cellValue(m_selectedRow, m_selectedColumn);
    const QVariant copyValue = renderer->copyContent(value, def, m_selectedRow, m_selectedColumn, m_model);

    if (copyValue.isValid()) {
        QApplication::clipboard()->setText(copyValue.toString());
    }
}

void AuGridWidget::moveSelection(int dRow, int dColumn)
{
    if (!m_model || m_model->rowCount() == 0 || m_model->columnCount() == 0)
        return;
    if (m_selectionMode == SelectionMode::None)
        return;   // 不可选中模式：无选中光标，键盘导航无效

    int row = qMax(0, m_selectedRow);
    int column = qMax(0, m_selectedColumn);
    row += dRow;
    column += dColumn;
    row = qBound(0, row, m_model->rowCount() - 1);
    column = qBound(0, column, m_model->columnCount() - 1);

    // 水平方向移动时跳过隐藏列与不可聚焦列（如放操作按钮的自定义列）
    if (dColumn != 0) {
        while (column >= 0 && column < m_model->columnCount()
               && (isColumnHidden(column) || !isCellFocusable(row, column)))
            column += dColumn;
        if (column < 0 || column >= m_model->columnCount())
            return;
    } else if (!isCellFocusable(row, column)) {
        // 垂直移动落在不可聚焦列（动态回调生效后锚点列可能失焦）：横向就近跳出
        const int fallback = nearestFocusableColumn(row, column);
        if (fallback < 0)
            return;
        column = fallback;
    }

    // 与隐藏列同样处理：导航不落入被合并覆盖的单元格（锚点本身可选）
    if (dRow != 0) {
        while (row >= 0 && row < m_model->rowCount() && isSpanCoveredCell(row, column))
            row += dRow;
    }
    if (dColumn != 0) {
        while (column >= 0 && column < m_model->columnCount()
               && (isSpanCoveredCell(row, column) || !isCellFocusable(row, column)))
            column += dColumn;
    }
    if (row < 0 || row >= m_model->rowCount() || column < 0 || column >= m_model->columnCount())
        return;

    if (row == m_selectedRow && column == m_selectedColumn)
        return;

    m_selectedRow = row;
    m_selectedColumn = column;
    m_selectedOriginalRow = m_model->originalRowForVisibleRow(row);
    ensureCellVisible(row, column);
    update();
    emit selectionChanged();
}

void AuGridWidget::ensureCellVisible(int row, int column)
{
    scrollToCell(row, column, ScrollHint::EnsureVisible);
}

int AuGridWidget::targetVerticalOffset(int row, ScrollHint hint, int spanBottomRow) const
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return m_vScrollBar->value();

    const int contentHeight = totalContentHeight();
    const int bodyH = bodyHeight();
    const int maxOffset = qMax(0, contentHeight - bodyH);
    if (m_rowOffsetsDirty)
        rebuildRowOffsets();
    // 合并单元格按整个区域计算可视范围
    const int lastRow = (spanBottomRow > row) ? spanBottomRow : row;
    const int rowTop = m_rowOffsets.at(row);
    const int rowBottom = rowTop + (m_rowOffsets.at(lastRow + 1) - m_rowOffsets.at(row));
    const int rowH = rowBottom - rowTop;
    const int current = m_vScrollBar->value();

    int target = current;
    switch (hint) {
    case ScrollHint::EnsureVisible:
        if (rowTop < current)
            target = rowTop;
        else if (rowBottom > current + bodyH)
            target = rowBottom - bodyH;
        break;
    case ScrollHint::PositionTop:
        target = rowTop;
        break;
    case ScrollHint::PositionCenter:
        target = rowTop - (bodyH - rowH) / 2;
        break;
    case ScrollHint::PositionBottom:
        target = rowBottom - bodyH;
        break;
    }

    return qBound(0, target, maxOffset);
}

int AuGridWidget::targetHorizontalOffset(int column, ScrollHint hint, int spanRightColumn) const
{
    if (!m_model || m_columnOffsets.isEmpty() || column < 0 || column >= m_model->columnCount())
        return m_hScrollBar->value();

    const int contentWidth = totalContentWidth();
    const int viewportW = viewportWidthForColumns();
    const int fixedWidth = fixedColumnsWidth() + m_leftPinnedWidth + m_rightPinnedWidth;
    const int bodyViewportW = qMax(0, viewportW - fixedWidth);
    // 与滚动量程一致：body 区内容宽 = 全部列宽 - 停靠列宽
    const int bodyContentWidth = qMax(0, contentWidth - m_leftPinnedWidth - m_rightPinnedWidth);
    const int maxOffset = qMax(0, bodyContentWidth - bodyViewportW);
    // 合并单元格按整个区域计算可视范围
    const int rightColumn = (spanRightColumn > column) ? spanRightColumn : column;
    // body 列的滚动偏移相对 body 区起点（全局偏移需扣除其前面的左停靠列宽）
    const int colLeft = m_columnOffsets.at(column) - m_leftPinnedWidth;
    const int colRight = m_columnOffsets.at(rightColumn + 1) - m_leftPinnedWidth;
    const int colWidth = colRight - colLeft;
    const int current = m_hScrollBar->value();

    int target = current;
    switch (hint) {
    case ScrollHint::EnsureVisible:
        if (colLeft < current)
            target = colLeft;
        else if (colRight > current + bodyViewportW)
            target = colRight - bodyViewportW;
        break;
    case ScrollHint::PositionTop:
        target = colLeft;
        break;
    case ScrollHint::PositionCenter:
        target = colLeft - (bodyViewportW - colWidth) / 2;
        break;
    case ScrollHint::PositionBottom:
        target = colRight - bodyViewportW;
        break;
    }

    return qBound(0, target, maxOffset);
}

void AuGridWidget::scrollToRow(int row, ScrollHint hint)
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return;

    m_vScrollBar->setValue(targetVerticalOffset(row, hint));
    updateScrollBars();
    update();
}

void AuGridWidget::scrollToColumn(int column, ScrollHint hint)
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return;

    m_hScrollBar->setValue(targetHorizontalOffset(column, hint));
    updateScrollBars();
    update();
}

void AuGridWidget::scrollToCell(int row, int column, ScrollHint hint)
{
    if (!m_model || row < 0 || row >= m_model->rowCount()
        || column < 0 || column >= m_model->columnCount())
        return;

    // 合并单元格按整个合并区域滚动定位
    const CellSpan span = spanAt(row, column);
    if (span.isValid()) {
        row = span.anchorRow;
        column = span.anchorColumn;
    }
    const int spanBottom = span.isValid() ? span.bottomRow() : -1;
    const int spanRight = span.isValid() ? span.rightColumn() : -1;

    // 先处理水平方向，让滚动条显隐稳定
    m_hScrollBar->setValue(targetHorizontalOffset(column, hint, spanRight));
    updateScrollBars();

    // 水平滚动条显隐会改变 bodyHeight，因此重新计算垂直方向
    m_vScrollBar->setValue(targetVerticalOffset(row, hint, spanBottom));
    updateScrollBars();
    update();
}

int AuGridWidget::verticalOffset() const
{
    return m_vScrollBar->value();
}

int AuGridWidget::horizontalOffset() const
{
    return m_hScrollBar->value();
}

void AuGridWidget::setVerticalOffset(int value)
{
    m_vScrollBar->setValue(value);
}

void AuGridWidget::setHorizontalOffset(int value)
{
    m_hScrollBar->setValue(value);
}

void AuGridWidget::setSelectionMode(SelectionMode mode)
{
    if (m_selectionMode == mode)
        return;

    m_selectionMode = mode;

    bool changed = false;
    if (mode == SelectionMode::None) {
        // 不可选中：清空全部选中
        changed = (m_selectedRow >= 0 || m_selectedColumn >= 0 || !m_selectedOriginalRows.isEmpty());
        m_selectedRow = -1;
        m_selectedColumn = -1;
        m_selectedOriginalRow = -1;
        m_selectedOriginalRows.clear();
    } else if (mode == SelectionMode::Single) {
        // 单选：仅保留最后点击的锚点行
        QSet<int> keep;
        if (m_selectedRow >= 0 && m_selectedOriginalRow >= 0)
            keep.insert(m_selectedOriginalRow);
        changed = (keep != m_selectedOriginalRows);
        m_selectedOriginalRows = keep;
    } else if (m_selectedRow >= 0 && m_selectedOriginalRow >= 0
               && !m_selectedOriginalRows.contains(m_selectedOriginalRow)) {
        // 多选：锚点行并入选中集合
        m_selectedOriginalRows.insert(m_selectedOriginalRow);
        changed = true;
    }

    update();
    if (changed)
        emit selectionChanged();
}

AuGridWidget::SelectionMode AuGridWidget::selectionMode() const
{
    return m_selectionMode;
}

void AuGridWidget::selectRow(int row)
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return;
    if (m_selectionMode == SelectionMode::None)
        return;

    setFocus();

    if (m_selectionMode == SelectionMode::Multiple) {
        const int original = m_model->originalRowForVisibleRow(row);
        if (m_selectedOriginalRows.size() == 1 && m_selectedOriginalRows.contains(original)
            && m_selectedRow == row) {
            ensureCellVisible(row, m_selectedColumn >= 0 ? m_selectedColumn : 0);
            return;
        }
        m_selectedOriginalRows.clear();
        m_selectedOriginalRows.insert(original);
        m_selectedRow = row;
        m_selectedOriginalRow = original;
        if (m_selectedColumn < 0)
            m_selectedColumn = 0;
        ensureCellVisible(m_selectedRow, m_selectedColumn);
        update();
        emit selectionChanged();
        return;
    }

    if (row == m_selectedRow && m_selectedColumn >= 0) {
        ensureCellVisible(row, m_selectedColumn);
        return;
    }

    m_selectedRow = row;
    m_selectedOriginalRow = m_model->originalRowForVisibleRow(row);
    if (m_selectedColumn < 0)
        m_selectedColumn = 0;

    ensureCellVisible(m_selectedRow, m_selectedColumn);
    update();
    emit selectionChanged();
}

void AuGridWidget::selectCell(int row, int column)
{
    if (!m_model || row < 0 || row >= m_model->rowCount()
        || column < 0 || column >= m_model->columnCount())
        return;

    // 选中合并单元格的被覆盖格时，落到合并锚点上
    const CellSpan span = spanAt(row, column);
    if (span.isValid()) {
        row = span.anchorRow;
        column = span.anchorColumn;
    }

    if (m_selectionMode == SelectionMode::None)
        return;

    // 不可聚焦的单元格：不把锚点（焦点框）落到该格，退化为按行选中（保留当前锚点列）
    if (!isCellFocusable(row, column)) {
        selectRow(row);
        return;
    }

    setFocus();

    if (m_selectionMode == SelectionMode::Multiple) {
        const int original = m_model->originalRowForVisibleRow(row);
        if (m_selectedOriginalRows.size() == 1 && m_selectedOriginalRows.contains(original)
            && m_selectedRow == row && m_selectedColumn == column) {
            ensureCellVisible(row, column);
            return;
        }
        m_selectedOriginalRows.clear();
        m_selectedOriginalRows.insert(original);
        m_selectedRow = row;
        m_selectedColumn = column;
        m_selectedOriginalRow = original;
        ensureCellVisible(row, column);
        update();
        emit selectionChanged();
        return;
    }

    if (row == m_selectedRow && column == m_selectedColumn) {
        ensureCellVisible(row, column);
        return;
    }

    m_selectedRow = row;
    m_selectedColumn = column;
    m_selectedOriginalRow = m_model->originalRowForVisibleRow(row);

    ensureCellVisible(row, column);
    update();
    emit selectionChanged();
}

void AuGridWidget::selectRows(const QSet<int> &rows)
{
    if (!m_model || rows.isEmpty())
        return;
    if (m_selectionMode == SelectionMode::None)
        return;

    if (m_selectionMode == SelectionMode::Single) {
        int best = -1;
        for (int row : rows) {
            if (row >= 0 && row < m_model->rowCount() && (best < 0 || row < best))
                best = row;
        }
        if (best >= 0)
            selectRow(best);
        return;
    }

    // 多选：按可见行号收集原始行；锚点取最小选中行，不改变滚动位置
    QSet<int> originals;
    int anchor = -1;
    for (int row : rows) {
        if (row < 0 || row >= m_model->rowCount())
            continue;
        originals.insert(m_model->originalRowForVisibleRow(row));
        if (anchor < 0 || row < anchor)
            anchor = row;
    }

    const QSet<int> oldRows = m_selectedOriginalRows;
    const int oldAnchorRow = m_selectedRow;

    m_selectedOriginalRows = originals;
    if (anchor >= 0) {
        m_selectedRow = anchor;
        m_selectedOriginalRow = m_model->originalRowForVisibleRow(anchor);
        if (m_selectedColumn < 0)
            m_selectedColumn = 0;
    } else {
        m_selectedRow = -1;
        m_selectedOriginalRow = -1;
    }

    update();
    if (m_selectedOriginalRows != oldRows || m_selectedRow != oldAnchorRow)
        emit selectionChanged();
}

void AuGridWidget::clearSelection()
{
    if (m_selectedRow < 0 && m_selectedColumn < 0 && m_selectedOriginalRows.isEmpty())
        return;

    m_selectedRow = -1;
    m_selectedColumn = -1;
    m_selectedOriginalRow = -1;
    m_selectedOriginalRows.clear();
    update();
    emit selectionChanged();
}

QSet<int> AuGridWidget::selectedRows() const
{
    if (!m_model || m_selectionMode == SelectionMode::None)
        return QSet<int>();

    if (m_selectionMode == SelectionMode::Single) {
        if (m_selectedRow >= 0)
            return QSet<int>{m_selectedRow};
        return QSet<int>();
    }

    QSet<int> rows;
    for (int original : m_selectedOriginalRows) {
        const int visible = m_model->visibleRowForOriginalRow(original);
        if (visible >= 0)
            rows.insert(visible);
    }
    return rows;
}

QSet<int> AuGridWidget::selectedOriginalRows() const
{
    if (m_selectionMode == SelectionMode::None)
        return QSet<int>();

    if (m_selectionMode == SelectionMode::Single) {
        if (m_selectedOriginalRow >= 0)
            return QSet<int>{m_selectedOriginalRow};
        return QSet<int>();
    }

    return m_selectedOriginalRows;
}

bool AuGridWidget::isRowSelected(int row) const
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return false;
    if (m_selectionMode == SelectionMode::None)
        return false;

    if (m_selectionMode == SelectionMode::Single)
        return row == m_selectedRow;

    const int original = m_model->originalRowForVisibleRow(row);
    return original >= 0 && m_selectedOriginalRows.contains(original);
}

void AuGridWidget::syncSelectionAfterLayoutChange()
{
    if (!m_model)
        return;

    // 多选集合按原始行存储：排序 / 过滤 / 树折叠不改变原始行身份，无需重映射；
    // 仅剔除已越出模型范围的行（行数据被移除 / 模型重载）。
    if (m_selectionMode == SelectionMode::Multiple && !m_selectedOriginalRows.isEmpty()) {
        const int originalCount = m_model->originalRowCount();
        QSet<int> pruned = m_selectedOriginalRows;
        for (int original : m_selectedOriginalRows) {
            if (original < 0 || original >= originalCount)
                pruned.remove(original);
        }
        if (pruned.size() != m_selectedOriginalRows.size()) {
            m_selectedOriginalRows = pruned;
            emit selectionChanged();
        }
    }

    if (m_selectedOriginalRow < 0) {
        m_selectedRow = -1;
        return;
    }

    const int newVisibleRow = m_model->visibleRowForOriginalRow(m_selectedOriginalRow);
    if (newVisibleRow >= 0) {
        m_selectedRow = newVisibleRow;
    } else {
        m_selectedRow = -1;
        m_selectedColumn = -1;
        m_selectedOriginalRow = -1;
        emit selectionChanged();
    }
}

bool AuGridWidget::applyRowSelectionClick(int row, int column, bool ctrl, bool withColumn)
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return false;
    if (m_selectionMode == SelectionMode::None)
        return false;

    const int original = m_model->originalRowForVisibleRow(row);
    if (original < 0)
        return false;

    if (m_selectionMode == SelectionMode::Single) {
        if (ctrl && row == m_selectedRow) {
            // Ctrl+点击已选中行：取消选中（单选模式的鼠标取消手势）
            m_selectedRow = -1;
            m_selectedColumn = -1;
            m_selectedOriginalRow = -1;
            return true;
        }
        const bool changed = (m_selectedRow != row
                              || (withColumn && m_selectedColumn != column));
        m_selectedRow = row;
        m_selectedOriginalRow = original;
        if (withColumn)
            m_selectedColumn = column;
        return changed;
    }

    // 多选：点击与 Ctrl+点击均为切换该行选中——点击未选中行即选中（其它已选行保持），
    // 再点击同一行取消；两种方式（纯点击累积 / Ctrl+点击累积）并存
    if (m_selectedOriginalRows.contains(original))
        m_selectedOriginalRows.remove(original);
    else
        m_selectedOriginalRows.insert(original);

    m_selectedRow = row;
    m_selectedOriginalRow = original;
    if (withColumn)
        m_selectedColumn = column;
    return true;
}

bool AuGridWidget::applyCheckboxClickSelection(int row, int column, bool withColumn)
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return false;
    if (m_selectionMode == SelectionMode::None)
        return false;

    const int original = m_model->originalRowForVisibleRow(row);
    if (original < 0)
        return false;

    if (m_selectionMode == SelectionMode::Single) {
        const bool changed = (m_selectedRow != row || (withColumn && m_selectedColumn != column));
        m_selectedRow = row;
        m_selectedOriginalRow = original;
        if (withColumn)
            m_selectedColumn = column;
        return changed;
    }

    // 多选：复选框交互语义为切换加入 / 移出（与 Ctrl+点击一致）
    if (m_selectedOriginalRows.contains(original))
        m_selectedOriginalRows.remove(original);
    else
        m_selectedOriginalRows.insert(original);

    m_selectedRow = row;
    m_selectedOriginalRow = original;
    if (withColumn)
        m_selectedColumn = column;
    return true;
}

void AuGridWidget::keyPressEvent(QKeyEvent *event)
{
    // 加载遮罩显示期间屏蔽交互
    if (m_loading) {
        event->accept();
        return;
    }

    if (!m_model) {
        QWidget::keyPressEvent(event);
        return;
    }

    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_C) {
        copySelectedCell();
        event->accept();
        return;
    }

    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_F2:
        if (m_selectedRow >= 0 && m_selectedColumn >= 0) {
            if (m_selectedColumn < m_model->columnCount()
                && m_model->columnDefAt(m_selectedColumn).isCheckbox()) {
                if (isCellEditable(m_selectedRow, m_selectedColumn))
                    toggleCheckboxCellValue(m_selectedRow, m_selectedColumn);
            } else {
                editCell(m_selectedRow, m_selectedColumn);
            }
        }
        event->accept();
        return;
    case Qt::Key_Escape:
        if (m_activeEditor) {
            clearEditError();
            closeEditor(false);
        }
        event->accept();
        return;
    case Qt::Key_Tab:
        if (m_activeEditor && !tryFinishCurrentEdit()) {
            event->accept();
            return;
        }
        if (m_selectedRow >= 0 && m_selectedColumn >= 0) {
            int row = m_selectedRow;
            int column = m_selectedColumn;
            if (findNextEditableCell(row, column, 1))
                editCell(row, column);
        }
        event->accept();
        return;
    case Qt::Key_Backtab:
        if (m_activeEditor && !tryFinishCurrentEdit()) {
            event->accept();
            return;
        }
        if (m_selectedRow >= 0 && m_selectedColumn >= 0) {
            int row = m_selectedRow;
            int column = m_selectedColumn;
            if (findNextEditableCell(row, column, -1))
                editCell(row, column);
        }
        event->accept();
        return;
    case Qt::Key_Left:
    case Qt::Key_Right:
        if (m_activeEditor && !tryFinishCurrentEdit()) {
            event->accept();
            return;
        }
        // 树形模式：选中树列时 Left / Right 折叠展开（QTreeWidget 风格）
        if (isTreeMode() && m_selectedRow >= 0 && m_selectedColumn == treeColumn()) {
            const int originalRow = m_model->originalRowForVisibleRow(m_selectedRow);
            if (originalRow >= 0 && event->key() == Qt::Key_Right
                && m_model->rowHasChildren(originalRow) && !m_model->isRowExpanded(originalRow)) {
                setRowExpandedAnimated(m_selectedRow, true);
                event->accept();
                return;
            }
            if (originalRow >= 0 && event->key() == Qt::Key_Right
                && m_model->isRowExpanded(originalRow) && m_model->rowHasChildren(originalRow)) {
                // 已展开：选中第一个可见子行
                const QVector<int> children = m_model->rowChildren(originalRow);
                if (!children.isEmpty()) {
                    const int childVisible = m_model->visibleRowForOriginalRow(children.first());
                    if (childVisible >= 0) {
                        m_selectedRow = childVisible;
                        m_selectedOriginalRow = children.first();
                        ensureCellVisible(childVisible, m_selectedColumn);
                        update();
                        emit selectionChanged();
                        event->accept();
                        return;
                    }
                }
            }
            if (originalRow >= 0 && event->key() == Qt::Key_Left) {
                if (m_model->isRowExpanded(originalRow) && m_model->rowHasChildren(originalRow)) {
                    setRowExpandedAnimated(m_selectedRow, false);
                    event->accept();
                    return;
                }
                // 已折叠 / 叶子行：选中父行
                const int parentOriginal = m_model->rowParent(originalRow);
                if (parentOriginal >= 0) {
                    const int parentVisible = m_model->visibleRowForOriginalRow(parentOriginal);
                    if (parentVisible >= 0) {
                        m_selectedRow = parentVisible;
                        m_selectedOriginalRow = parentOriginal;
                        ensureCellVisible(parentVisible, m_selectedColumn);
                        update();
                        emit selectionChanged();
                        event->accept();
                        return;
                    }
                }
            }
        }
        moveSelection(0, event->key() == Qt::Key_Left ? -1 : 1);
        event->accept();
        return;
    case Qt::Key_Up:
        if (!m_activeEditor || tryFinishCurrentEdit())
            moveSelection(-1, 0);
        event->accept();
        return;
    case Qt::Key_Down:
        if (!m_activeEditor || tryFinishCurrentEdit())
            moveSelection(1, 0);
        event->accept();
        return;
    case Qt::Key_Space:
        if (m_selectedRow >= 0 && m_selectedColumn >= 0
            && m_selectedColumn < m_model->columnCount()
            && m_model->columnDefAt(m_selectedColumn).isCheckbox()
            && isCellEditable(m_selectedRow, m_selectedColumn)) {
            toggleCheckboxCellValue(m_selectedRow, m_selectedColumn);
            event->accept();
            return;
        }
        // 非复选框列：空格作为普通字符，落入 default 的“按键即编辑”逻辑
        /* fall through */
    default:
        if (m_selectedRow >= 0 && m_selectedColumn >= 0
            && m_selectedColumn < m_model->columnCount()
            && m_model->columnDefAt(m_selectedColumn).isCheckbox()) {
            event->accept();
            return;
        }
        if (!m_activeEditor && m_selectedRow >= 0 && m_selectedColumn >= 0
            && isCellEditable(m_selectedRow, m_selectedColumn)
            && !event->text().isEmpty()
            && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))) {
            const QString text = event->text();
            if (!text.isEmpty() && text.at(0).isPrint()) {
                if (editCell(m_selectedRow, m_selectedColumn, text))
                    event->accept();
                return;
            }
        }
        break;
    }

    QWidget::keyPressEvent(event);
}

bool AuGridWidget::event(QEvent *event)
{
    // 列 tooltipEnabled=true 且内容超出列宽的单元格：悬停由 Qt 按系统延迟
    // 派发 ToolTip 事件，此处统一经 cellTooltipText 裁决后交给 QToolTip 显示
    // （QToolTip 自身即为无边框置顶窗口，定位与点击/按键/失焦自动隐藏跨平台一致）。
    if (event->type() == QEvent::ToolTip) {
        const QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
        const QString text = cellTooltipText(helpEvent->pos());
        if (!text.isEmpty())
            QToolTip::showText(helpEvent->globalPos(), text, this);
        return true;
    }
    return QWidget::event(event);
}

bool AuGridWidget::focusNextPrevChild(bool next)
{
    // 与 QAbstractItemView 相同的模式：Tab / Backtab 由表格自管——提交当前编辑并
    // 跳到下一个 / 上一个可编辑格。不重写时，编辑提交（尤其经复合编辑器的主动提交
    // 路径）之后按 Tab，Qt 默认 Tab 链可能把焦点移出表格（落到布局中其它控件或
    // 残留的编辑器子控件上），键盘编辑链断裂。
    // 无下一可编辑格时同样消费按键：焦点留在表格内（与 keyPressEvent 的 Tab 语义一致）。
    if (m_activeEditor && !tryFinishCurrentEdit())
        return true;   // 校验要求保持编辑：吃掉 Tab

    if (m_model && m_selectedRow >= 0 && m_selectedColumn >= 0) {
        int row = m_selectedRow;
        int column = m_selectedColumn;
        if (findNextEditableCell(row, column, next ? 1 : -1)) {
            editCell(row, column);
            return true;
        }
    }

    // 无模型 / 无选中 / 无下一格：焦点留在表格内
    return true;
}

