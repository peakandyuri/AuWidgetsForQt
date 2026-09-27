#include "AuGridWidget.h"
#include "AuScrollBar.h"
#include "AuDefaultCellRenderer.h"
#include "AuCheckBoxCellRenderer.h"
#include "AuDefaultCellEditor.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QPalette>
#include <QApplication>
#include <QToolTip>
#include <QHelpEvent>
#include <QClipboard>
#include <QLabel>
#include <QTimer>
#include <QCursor>
#include <QDebug>
#include <QVariantAnimation>
#include <algorithm>
#include <limits>

constexpr int AuGridWidget::kResizeHandleTolerance;

// OutCubic 缓动：快出缓停（数据变化动画各阶段使用，接近 web 端过渡手感）
static qreal auOutCubic(qreal t)
{
    t = qBound(0.0, t, 1.0);
    const qreal u = 1.0 - t;
    return 1.0 - u * u * u;
}

/// 判断 widget 是否位于编辑器子树内。与 QWidget::isAncestorOf 不同：这里跨越
/// 顶层窗口边界（编辑器弹出的下拉面板 / QComboBox 弹层等都是挂在编辑器下的
/// 子顶级窗口，其 parentWidget() 链仍指向编辑器）。
static bool auInEditorSubtree(const QWidget *editor, const QWidget *widget)
{
    while (widget) {
        if (widget == editor)
            return true;
        widget = widget->parentWidget();
    }
    return false;
}

/// 编辑器自有弹层（QComboBox 弹层 / AuSelect、AuDatePicker 的面板）是否处于打开状态。
/// Linux/Wayland 上点击这类弹层窗口会把激活与键盘焦点交给弹层（其内部通常没有
/// 可聚焦控件，焦点表现为空），且激活面板的同时宿主窗口失活——这些都不是真正的
/// “焦点离开编辑器”，不能据此提交销毁编辑器（Windows 由 MA_NOACTIVATE 天然规避）。
static bool auEditorPopupActive(const QWidget *editor)
{
    const QWidget *popup = QApplication::activePopupWidget();
    if (popup && auInEditorSubtree(editor, popup))
        return true;
    const QWidget *active = QApplication::activeWindow();
    return active && active != editor->window() && auInEditorSubtree(editor, active);
}

AuGridWidget::AuGridWidget(QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
    , m_xOffset(0)
    , m_yOffset(0)
    , m_selectedRow(-1)
    , m_selectedColumn(-1)
    , m_hoverRow(-1)
    , m_hoverColumn(-1)
    , m_hoverPos(-1, -1)
    , m_hoverHeaderColumn(-1)
    , m_hoverHeaderLevel(0)
    , m_headerRenderClickPending(false)
    , m_visibleRowBuffer(3)
    , m_rowOffsetsDirty(true)
    , m_leftPinnedWidth(0)
    , m_rightPinnedWidth(0)
    , m_showRowNumberColumn(false)
    , m_showCheckboxColumn(false)
    , m_verticalGridLinesVisible(true)
    , m_sortedColumn(-1)
    , m_sortOrder(Qt::AscendingOrder)
    , m_sortingEnabled(false)
    , m_selectedOriginalRow(-1)
    , m_selectionMode(SelectionMode::Single)
    , m_hScrollBar(new AuScrollBar(Qt::Horizontal, this))
    , m_vScrollBar(new AuScrollBar(Qt::Vertical, this))
    , m_hScrollBarVisible(false)
    , m_vScrollBarVisible(false)
    , m_style(AuGridStyle::globalStyle())
    , m_ownStyle(false)
    , m_explicitStyle(false)
    , m_sizeMode(AuGridStyle::SizeMode::Small)
    , m_styleClonedForSizeMode(false)
    , m_defaultCellRenderer(new AuDefaultCellRenderer())
    , m_checkboxCellRenderer(new AuCheckBoxCellRenderer())
    , m_defaultCellEditor(new AuDefaultCellEditor())
    , m_defaultMultiLineCellEditor(new AuMultiLineCellEditor())
    , m_activeEditor(nullptr)
    , m_editRow(-1)
    , m_editColumn(-1)
    , m_committing(false)
    , m_editErrorTip(nullptr)
    , m_isResizing(false)
    , m_resizeColumn(-1)
    , m_resizeStartX(0)
    , m_resizeStartWidth(0)
    , m_rowDragEnabled(false)
    , m_isRowDragging(false)
    , m_dragStartVisibleRow(-1)
    , m_dragTargetVisibleIndex(-1)
    , m_rowDragLastY(0)
    , m_rowDragDirection(0)
    , m_dragFloatWidget(nullptr)
    , m_autoScrollTimer(nullptr)
    , m_dragAnimationProgress(0.0)
    , m_dragOffsetAnimation(nullptr)
    , m_treeLayerFilter(0)
    , m_treeAnimationExpanding(false)
    , m_treeAnimationPendingCollapse(false)
    , m_treeAnimationPendingRow(-1)
    , m_treeAnimationProgress(0.0)
    , m_treeAnimation(nullptr)
    , m_isColumnDragging(false)
    , m_isPotentialColumnDrag(false)
    , m_potentialColumnDragColumn(-1)
    , m_columnDragStartColumn(-1)
    , m_columnDragParentLeafStart(-1)
    , m_columnDragParentLeafEnd(-1)
    , m_columnDragTargetIndex(-1)
    , m_columnDragLastX(0)
    , m_columnDragDirection(0)
    , m_columnDragRegion(ColumnRegion::Body)
    , m_columnDragRegionFirst(-1)
    , m_columnDragRegionLast(-1)
    , m_columnDragAnimationProgress(0.0)
    , m_columnDragOffsetAnimation(nullptr)
    , m_columnVisAnimating(false)
    , m_columnVisTargetFirst(-1)
    , m_columnVisTargetLast(-1)
    , m_columnVisHiding(false)
    , m_columnVisProgress(0.0)
    , m_columnVisAnimation(nullptr)
    , m_specialVisAnimating(false)
    , m_specialVisHiding(false)
    , m_specialVisProgress(0.0)
    , m_specialVisMask(0)
    , m_specialVisAnimation(nullptr)
    , m_loading(false)
    , m_loadingOpacity(0.0)
    , m_loadingSpinnerAngle(0)
    , m_loadingTimer(nullptr)
    , m_loadingAnimation(nullptr)
    , m_dataAnimationEnabled(true)
    , m_sortAnimationDuration(100)
    , m_resetAnimationDuration(200)
    , m_suppressBottomLineForCapture(false)
    , m_dataAnimationProgress(0.0)
    , m_dataAnimation(nullptr)
    , m_dataResetPending(false)
    , m_dataResetAnimating(false)
    , m_dataResetProgress(0.0)
    , m_dataResetSlideHeight(0)
    , m_dataResetAnimation(nullptr)
    , m_rowRemovePending(false)
{
    AuGridStyle::registerGlobalStyleObserver(this);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAutoFillBackground(false);

    m_autoScrollTimer = new QTimer(this);
    m_autoScrollTimer->setInterval(50);
    connect(m_autoScrollTimer, &QTimer::timeout, this, &AuGridWidget::onAutoScrollTimeout);

    // 加载遮罩：圆环旋转定时器（仅遮罩可见期间运行）+ 淡入/淡出动画
    m_loadingTimer = new QTimer(this);
    m_loadingTimer->setInterval(16);
    connect(m_loadingTimer, &QTimer::timeout, this, [this]() {
        m_loadingSpinnerAngle = (m_loadingSpinnerAngle + 360 - 6) % 360;
        update();
    });

    m_loadingAnimation = new QVariantAnimation(this);
    connect(m_loadingAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        const qreal opacity = qBound(0.0, value.toReal(), 1.0);
        m_loadingOpacity = opacity;
        if (opacity > 0.0 && !m_loadingTimer->isActive())
            m_loadingTimer->start();
        else if (opacity <= 0.0 && m_loadingTimer->isActive())
            m_loadingTimer->stop();
        // 滚动条随蒙层淡入淡出；过渡两端（蒙层全隐 / 全显）交回 updateScrollBars 恢复真实显隐
        applyLoadingFadeToScrollBars(opacity);
        update();
    });

    m_hScrollBar->hide();
    m_vScrollBar->hide();

    connect(m_hScrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        m_xOffset = value;
        QToolTip::hideText();   // 滚动后 tooltip 指向的行列已移动，直接隐藏
        updateEditorGeometry();
        update();
        emit horizontalScrollChanged(value);
    });
    connect(m_vScrollBar, &QScrollBar::valueChanged, this, [this](int value) {
        m_yOffset = value;
        QToolTip::hideText();
        refreshAutoFitColumns();
        updateEditorGeometry();
        update();
        emit verticalScrollChanged(value);
    });

    // 复合编辑器的失焦提交：编辑器内部子控件（如 AuSelect / AuDatePicker 的行编辑框）
    // 持有焦点时，焦点移出编辑器子树只会向子控件发 FocusOut，编辑器本体收不到，
    // eventFilter 的失焦提交路径感知不到——这里按全局焦点变化补一条等价裁决。
    // 焦点在编辑器子树内部挪动（展开面板把焦点交给内部行编辑框）不触发提交；
    // 焦点移入编辑器自己的弹层窗口（含 Qt::Popup 下拉 / Qt::Tool 面板，跨窗口
    // 判定见 auInEditorSubtree），或弹层被点击激活导致焦点短暂为空时同样不提交。
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *old, QWidget *now) {
        if (!m_activeEditor || m_committing)
            return;
        const bool fromEditor = old && (old == m_activeEditor
                                        || m_activeEditor->isAncestorOf(old));
        const bool toEditor = now && (now == m_activeEditor
                                      || m_activeEditor->isAncestorOf(now));
        if (fromEditor && !toEditor) {
            if (auEditorPopupActive(m_activeEditor))
                return;
            tryFinishCurrentEdit(true);
        }
    });

    applyStyleToScrollBars();
}

AuGridWidget::~AuGridWidget()
{
    AuGridStyle::unregisterGlobalStyleObserver(this);
    finishSpecialColumnVisibilityAnimation();
    if (m_ownStyle)
        delete m_style;
    delete m_dragFloatWidget;
    delete m_editErrorTip;
}

void AuGridWidget::setModel(AuGridModel *model)
{
    if (m_model == model)
        return;

    disconnectModel();
    closeEditor(false);
    cancelRowDrag();
    cancelColumnDrag();
    finishTreeAnimation();
    finishColumnVisibilityAnimation();
    finishSpecialColumnVisibilityAnimation();
    finishSortMoveAnimation();
    finishDataResetAnimation();
    m_dataResetPending = false;
    m_dataResetSnapshot = QImage();
    m_model = model;
    connectModel();
    m_xOffset = 0;
    m_yOffset = 0;
    m_autoFitWidths.clear();
    m_autoFitDirty.clear();
    m_hiddenFields.clear();
    // 合并区域随旧模型失效，换模型时整体清除
    m_spans.clear();
    m_spanCovered.clear();
    rebuildColumnOffsets();
    invalidateRowHeights();

    m_selectedRow = -1;
    m_selectedColumn = -1;
    m_selectedOriginalRow = -1;
    m_selectedOriginalRows.clear();
    m_hoverRow = -1;
    m_hoverColumn = -1;
    m_editRow = -1;
    m_editColumn = -1;
    m_activeEditor = nullptr;
    m_checkedOriginalRows.clear();
    captureLayoutSnapshot();   // 建立布局基线：下一次排序 / 过滤据此播放位移动画
    updateScrollBars();
    update();
}

AuGridModel *AuGridWidget::model() const
{
    return m_model;
}

int AuGridWidget::headerHeight() const
{
    if (!m_model)
        return m_style->integer(AuGridStyle::HeaderHeight, m_model);
    return m_model->headerLevelCount() * m_style->integer(AuGridStyle::HeaderHeight, m_model);
}

int AuGridWidget::headerLevelHeight() const
{
    return m_style->integer(AuGridStyle::HeaderHeight, m_model);
}

int AuGridWidget::rowHeight() const
{
    return m_style->integer(AuGridStyle::RowHeight, m_model);
}

int AuGridWidget::rowHeight(int row) const
{
    if (!m_model || row < 0 || row >= m_model->rowCount())
        return rowHeight();

    if (m_rowOffsetsDirty)
        rebuildRowOffsets();

    if (row < m_rowHeights.size())
        return m_rowHeights.at(row);
    return rowHeight();
}

void AuGridWidget::invalidateRowHeights()
{
    m_rowOffsetsDirty = true;
    m_rowHeights.clear();
    m_rowOffsets.clear();
}

void AuGridWidget::invalidateRowHeight(int /*row*/)
{
    m_rowOffsetsDirty = true;
}

void AuGridWidget::rebuildRowOffsets() const
{
    m_rowHeights.clear();
    m_rowOffsets.clear();
    m_rowOffsetsDirty = false;

    if (!m_model)
        return;

    const int count = m_model->rowCount();
    if (count == 0)
        return;

    const int globalHeight = rowHeight();
    m_rowHeights.fill(globalHeight, count);

    const int columnCount = m_model->columnCount();
    for (int col = 0; col < columnCount; ++col) {
        if (isColumnHidden(col))
            continue;
        const AuColumnDef def = m_model->columnDefAt(col);
        QSharedPointer<AuCellRenderer> renderer = rendererForColumn(col);
        const int colWidth = columnWidth(col);

        for (int row = 0; row < count; ++row) {
            // 合并单元格：被覆盖格不参与行高测量；锚点按合并区域总宽测量
            const CellSpan span = spanAt(row, col);
            if (span.isValid()) {
                if (row != span.anchorRow || col != span.anchorColumn)
                    continue;
            }
            const int measureWidth = span.isValid() ? spanWidth(span) : colWidth;
            const QRect measureRect(0, 0, measureWidth, globalHeight);

            // 树形首列：测量时扣减缩进 + 箭头占位（与绘制一致）
            AuCellRendererContext measureCtx;
            if (m_model->isTreeMode() && col == treeColumn()) {
                const int originalRow = m_model->originalRowForVisibleRow(row);
                if (originalRow >= 0) {
                    measureCtx.treeRow = true;
                    measureCtx.treeDepth = m_model->rowDepth(originalRow);
                }
            }

            const int pref = renderer->preferredHeight(measureRect,
                                                         m_model->cellValue(row, col),
                                                         def, row, col,
                                                         m_model, m_style, measureCtx);
            if (pref > m_rowHeights.at(row))
                m_rowHeights[row] = pref;
        }
    }

    m_rowOffsets.resize(count + 1);
    m_rowOffsets[0] = 0;
    for (int i = 0; i < count; ++i)
        m_rowOffsets[i + 1] = m_rowOffsets[i] + m_rowHeights.at(i);
}

int AuGridWidget::selectedRow() const
{
    return m_selectedRow;
}

int AuGridWidget::selectedColumn() const
{
    return m_selectedColumn;
}

int AuGridWidget::visibleRowBuffer() const
{
    return m_visibleRowBuffer;
}

void AuGridWidget::setVisibleRowBuffer(int buffer)
{
    if (buffer < 0)
        buffer = 0;
    if (m_visibleRowBuffer == buffer)
        return;
    m_visibleRowBuffer = buffer;
    update();
}

void AuGridWidget::setSizeMode(SizeMode mode)
{
    if (m_sizeMode == mode && m_styleClonedForSizeMode)
        return;

    m_sizeMode = mode;
    m_styleClonedForSizeMode = true;

    if (!m_ownStyle) {
        AuGridStyle *newStyle = m_style->clone();
        setStyle(newStyle);
    }

    if (m_style->sizeMode() == mode)
        return;

    m_style->setSizeMode(mode);
    applyStyleToScrollBars();
    m_autoFitDirty.fill(true);
    rebuildColumnOffsets();
    invalidateRowHeights();
    updateScrollBars();
    update();
}

AuGridWidget::SizeMode AuGridWidget::sizeMode() const
{
    return m_sizeMode;
}

void AuGridWidget::setStyle(AuGridStyle *style)
{
    if (style) {
        if (m_style == style && m_explicitStyle)
            return;
        if (m_ownStyle)
            delete m_style;
        m_style = style;
        m_ownStyle = true;
        m_explicitStyle = true;
    } else {
        if (!m_explicitStyle)
            return;
        if (m_ownStyle)
            delete m_style;
        m_style = AuGridStyle::globalStyle();
        m_ownStyle = false;
        m_explicitStyle = false;
    }

    applyStyleToScrollBars();
    m_autoFitDirty.fill(true);
    invalidateRowHeights();
    updateScrollBars();
    update();
}

void AuGridWidget::onGlobalStyleChanged()
{
    if (m_explicitStyle && !m_styleClonedForSizeMode)
        return;

    if (m_styleClonedForSizeMode) {
        AuGridStyle *newStyle = AuGridStyle::globalStyle()->clone();
        newStyle->setSizeMode(m_sizeMode);
        setStyle(newStyle);
        m_styleClonedForSizeMode = true;
        return;
    }

    m_style = AuGridStyle::globalStyle();
    applyStyleToScrollBars();
    m_autoFitDirty.fill(true);
    invalidateRowHeights();
    updateScrollBars();
    update();
}

void AuGridWidget::registerCellRenderer(const QString &name, QSharedPointer<AuCellRenderer> renderer)
{
    if (name.isEmpty() || !renderer)
        return;
    m_cellRenderers.insert(name, renderer);
    m_autoFitDirty.fill(true);
    update();
}

void AuGridWidget::registerCellEditor(const QString &name, QSharedPointer<AuCellEditor> editor)
{
    if (name.isEmpty() || !editor)
        return;
    m_cellEditors.insert(name, editor);
}

void AuGridWidget::registerHeaderRenderer(const QString &name, QSharedPointer<AuHeaderRenderer> renderer)
{
    if (name.isEmpty() || !renderer)
        return;
    m_headerRenderers.insert(name, renderer);
    update();
}

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

AuGridStyle *AuGridWidget::style() const
{
    return m_style;
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

void AuGridWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const int radius = cornerRadius();
    const QRect panel = contentPanelRect();

    // 1. 先填充整个 widget 背景（滚动条区域也会用到）
    drawBackground(&painter);

    // 2. 绘制圆角表格面板背景（需要反锯齿）
    if (radius > 0) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(panelBackgroundColor());
        painter.drawRoundedRect(panel, radius, radius);
        painter.restore();
    }

    // 3. 在面板区域内裁剪并绘制表格内容
    painter.save();
    if (radius > 0) {
        painter.setRenderHint(QPainter::Antialiasing, true);
        // 内容裁剪区对齐边框内沿：边框占最外圈 1px（外径 radius、内径 radius-1），
        // 与网页 border-radius 的内圆角一致，避免表头/单元格边框超出圆角外边框。
        QPainterPath contentClip;
        const qreal innerRadius = qMax(0, radius - 1);
        contentClip.addRoundedRect(QRectF(panel).adjusted(1, 1, -1, -1), innerRadius, innerRadius);
        painter.setClipPath(contentClip, Qt::ReplaceClip);
    } else {
        painter.setClipRect(panel, Qt::ReplaceClip);
    }

    // 表格内部线条关闭反锯齿，保持 1px 清晰
    painter.setRenderHint(QPainter::Antialiasing, false);

    const QPair<int, int> bodyColRange = visibleColumnRange();
    QPair<int, int> rowRange = visibleRowRange();
    // 排序 / 过滤位移动画：退场行的新可见索引在视口之外，把绘制范围扩展到这些行，
    // 使它们能按插值位置画出“移出视口”的过程（进场行本就在可视范围内）
    if (!m_dataAnimationEntries.isEmpty() && m_model) {
        for (const DataMoveAnimationEntry &entry : m_dataAnimationEntries) {
            rowRange.first = qMin(rowRange.first, entry.visibleRow);
            rowRange.second = qMax(rowRange.second, entry.visibleRow);
        }
        rowRange.first = qMax(0, rowRange.first);
        rowRange.second = qMin(m_model->rowCount() - 1, rowRange.second);
    }

    drawHeader(&painter);

    // 一个区域的行内容（背景之外的单元格 + 网格线）
    auto drawRegionRows = [&](int firstColumn, int lastColumn) {
        if (firstColumn > lastColumn)
            return;
        drawRows(&painter, rowRange.first, rowRange.second, firstColumn, lastColumn);
        drawGridLines(&painter, firstColumn, lastColumn, rowRange.first, rowRange.second);
    };
    auto drawPinnedRegions = [&]() {
        if (!m_leftPinnedColumns.isEmpty())
            drawRegionRows(m_leftPinnedColumns.first(), m_leftPinnedColumns.last());
        if (!m_rightPinnedColumns.isEmpty())
            drawRegionRows(m_rightPinnedColumns.first(), m_rightPinnedColumns.last());
    };

    // 树形展开 / 折叠动画期间分三层绘制：
    //   静态行 → 渐变子行（半透明）→ 平移兄弟行（不透明顶层），
    // 平移行盖在渐变子行之上，避免移动中单元格的边框 / 内容与子行互相穿透。
    const bool treeAnimActive = !m_treeAnimationEntries.isEmpty()
                              || !m_treeAnimationFadeRows.isEmpty();

    if (!treeAnimActive) {
        drawRowBackgrounds(&painter, rowRange.first, rowRange.second);
        drawRegionRows(bodyColRange.first, bodyColRange.second);
        drawPinnedRegions();
    } else {
        const int layers[3] = {3, 1, 2};   // 静态行 → 渐变子行 → 平移行
        for (const int layer : layers) {
            m_treeLayerFilter = layer;
            drawRowBackgrounds(&painter, rowRange.first, rowRange.second);
            drawRegionRows(bodyColRange.first, bodyColRange.second);
            drawPinnedRegions();
        }
        m_treeLayerFilter = 0;
    }

    // 绘制行号列单元格
    if (m_showRowNumberColumn) {
        const qreal columnFade = specialColumnOpacity(SpecialRowNumber);
        painter.save();
        painter.setClipRect(0, headerHeight(), rowNumberColumnWidth(), bodyHeight(), Qt::IntersectClip);
        for (int row = rowRange.first; row <= rowRange.second; ++row) {
            const qreal rowOpacity = rowAnimationOpacity(row) * columnFade;
            if (rowOpacity < 1.0) {
                painter.save();
                painter.setOpacity(rowOpacity);
            }
            drawRowNumberCell(&painter, row);
            if (rowOpacity < 1.0)
                painter.restore();
        }
        painter.restore();

        const QColor gridLineColor = m_style->color(AuGridStyle::GridLineColor, m_model);
        if (gridLineColor.isValid()) {
            painter.save();
            const int rnW = rowNumberColumnWidth();
            // 裁剪含右边界像素：drawRect 的右边线落在 x = rnW 处，
            // 数据列竖线隐藏时该边线由本循环独立绘制
            painter.setClipRect(0, headerHeight(), rnW + 1, bodyHeight(), Qt::IntersectClip);
            painter.setPen(gridLineColor);
            // 与 drawGridLines 一致：重设动画期间末行底边线不绘制（见 drawGridLines 内注释）
            const bool suppressLastRowBottom = m_dataResetAnimating || m_suppressBottomLineForCapture;
            const int lastDataRow = m_model->rowCount() - 1;
            for (int row = rowRange.first; row <= rowRange.second; ++row) {
                const int y = rowY(row);
                const int h = rowHeight(row);
                const qreal rowOpacity = rowAnimationOpacity(row) * columnFade;
                if (rowOpacity < 1.0) {
                    painter.save();
                    painter.setOpacity(rowOpacity);
                }
                // 行号列始终画完整边框，不受列边框开关影响；动画期间末行只画上 / 左 / 右
                if (suppressLastRowBottom && row == lastDataRow) {
                    painter.drawLine(0, y, rnW, y);
                    painter.drawLine(0, y, 0, y + h);
                    painter.drawLine(rnW, y, rnW, y + h);
                } else {
                    painter.drawRect(0, y, rnW, h);
                }
                if (rowOpacity < 1.0)
                    painter.restore();
            }
            painter.restore();
        }
    }

    // 绘制复选框列 + 行拖拽手柄列（合并为一个固定列单元格，中间不再画分隔边框）
    if (m_showCheckboxColumn || m_rowDragEnabled) {
        const int fixedX = rowNumberColumnWidth();
        const int fixedW = checkboxColumnWidth() + dragHandleColumnWidth();
        const qreal checkboxFade = specialColumnOpacity(SpecialCheckbox);
        const qreal dragFade = specialColumnOpacity(SpecialDragHandle);

        painter.save();
        painter.setClipRect(fixedX, headerHeight(), fixedW, bodyHeight(), Qt::IntersectClip);
        for (int row = rowRange.first; row <= rowRange.second; ++row) {
            const qreal rowOpacity = rowAnimationOpacity(row);
            if (m_showCheckboxColumn) {
                const qreal opacity = rowOpacity * checkboxFade;
                if (opacity < 1.0) {
                    painter.save();
                    painter.setOpacity(opacity);
                }
                drawCheckboxCell(&painter, row);
                if (opacity < 1.0)
                    painter.restore();
            }
            if (m_rowDragEnabled) {
                const qreal opacity = rowOpacity * dragFade;
                if (opacity < 1.0) {
                    painter.save();
                    painter.setOpacity(opacity);
                }
                drawDragHandleCell(&painter, row);
                if (opacity < 1.0)
                    painter.restore();
            }
        }
        painter.restore();

        // 合并列的边框：仅四周画一次，复选框与拖拽手柄之间不再有分隔线
        const QColor gridLineColor = m_style->color(AuGridStyle::GridLineColor, m_model);
        if (gridLineColor.isValid()) {
            painter.save();
            // 裁剪含右边界像素（同行号列：保证右边线在数据列竖线隐藏时仍由本循环绘制）
            painter.setClipRect(fixedX, headerHeight(), fixedW + 1, bodyHeight(), Qt::IntersectClip);
            painter.setPen(gridLineColor);
            // 与 drawGridLines 一致：重设动画期间末行底边线不绘制（见 drawGridLines 内注释）
            const bool suppressLastRowBottom = m_dataResetAnimating || m_suppressBottomLineForCapture;
            const int lastDataRow = m_model->rowCount() - 1;
            for (int row = rowRange.first; row <= rowRange.second; ++row) {
                const int y = rowY(row);
                const int h = rowHeight(row);
                const qreal rowOpacity = rowAnimationOpacity(row);
                if (rowOpacity < 1.0) {
                    painter.save();
                    painter.setOpacity(rowOpacity);
                }
                // 复选框 / 拖拽手柄列始终画完整边框，不受列边框开关影响；
                // 动画期间末行只画上 / 左 / 右
                if (suppressLastRowBottom && row == lastDataRow) {
                    painter.drawLine(fixedX, y, fixedX + fixedW, y);
                    painter.drawLine(fixedX, y, fixedX, y + h);
                    painter.drawLine(fixedX + fixedW, y, fixedX + fixedW, y + h);
                } else {
                    painter.drawRect(fixedX, y, fixedW, h);
                }
                if (rowOpacity < 1.0)
                    painter.restore();
            }
            painter.restore();
        }
    }

    // 绘制被拖拽行的半透明覆盖层，使拖动行与未拖动行区分
    if (m_isRowDragging) {
        painter.save();
        painter.setPen(Qt::NoPen);
        const QColor overlayColor = m_style->color(AuGridStyle::DraggedRowOverlayColor, m_model);
        painter.setBrush(overlayColor.isValid() ? overlayColor : QColor(255, 255, 255, 102));
        const int viewportW = viewportWidthForColumns();
        for (int row = rowRange.first; row <= rowRange.second; ++row) {
            if (isRowDragging(row)) {
                const int y = rowY(row);
                const int h = rowHeight(row);
                painter.drawRect(0, y, viewportW, h);
            }
        }
        painter.restore();
    }

    drawCellFocus(&painter);
    drawRegionSeparators(&painter);

    // 数据行未填满可视区时，在最后一条数据行底部画一条贯穿线，避免底部看起来缺边框。
    // 重设数据切换动画期间（及为其捕获快照时）不画：内容整体平移时，这条静止的
    // 贯穿线与内容自身的底部边框分离，会在空视口中单独晃动；动画结束后再补画。
    if (m_model && m_model->rowCount() > 0 && !m_vScrollBarVisible
        && !m_dataResetAnimating && !m_suppressBottomLineForCapture) {
        const QColor gridLineColor = m_style->color(AuGridStyle::GridLineColor, m_model);
        if (gridLineColor.isValid()) {
            const int contentBottom = headerHeight() + totalContentHeight() - m_yOffset;
            painter.save();
            painter.setPen(gridLineColor);
            painter.drawLine(0, contentBottom, viewportWidthForColumns(), contentBottom);
            painter.restore();
        }
    }

    // 重设数据切换动画（两段式）：前半程旧内容快照匀速整体上移离场（视口渐空），
    // 后半程快照已完全离场，新内容自视口上方下移涌入（见 rowY 的偏移逻辑）
    if (m_dataResetAnimating && !m_dataResetSnapshot.isNull()) {
        const qreal qe = (m_dataResetProgress < 0.5)
                             ? m_dataResetProgress * 2.0   // 离场匀速，全程可见
                             : 1.0;                        // 后半程快照已完全移出视口
        const int y = headerHeight() - qRound(m_dataResetSlideHeight * qe);
        if (y + m_dataResetSnapshot.height() > headerHeight()) {
            painter.save();
            painter.setClipRect(0, headerHeight(), width(), bodyHeight());
            painter.drawImage(QPoint(0, y), m_dataResetSnapshot);
            painter.restore();
        }
    }

    // 行删除消失动画：被删行的像素幽灵带原位淡出并轻微上漂，
    // 下方行经排序位移动画上滑补位（与排序动画共用进度与缓动，绘制在其之上）
    if (!m_removedRowGhosts.isEmpty()) {
        const qreal ghostOpacity = qBound(0.0, 1.0 - m_dataAnimationProgress, 1.0);
        if (ghostOpacity > 0.0) {
            painter.save();
            painter.setClipRect(0, headerHeight(), width(), bodyHeight(), Qt::IntersectClip);
            painter.setOpacity(ghostOpacity);
            for (const RemovedRowGhost &ghost : m_removedRowGhosts) {
                const int drift = qRound(ghost.height * 0.3 * m_dataAnimationProgress);
                painter.drawImage(QPoint(0, ghost.y - drift), ghost.image);
            }
            painter.restore();
        }
    }

    painter.restore();

    // 4. 绘制面板边框：用“外轮廓 - 内轮廓”的环形填充代替画笔描边。
    // 描边在开反锯齿时会把 1px 线摊到两侧（显得发虚、圆角处偏粗），
    // 关闭反锯齿则会让圆角变成锯齿台阶；环形填充的直边恰好落在整数
    // 像素上（实色 1px），圆角由反锯齿平滑过渡，观感与网页 border-radius 一致。
    const QColor borderColor = m_style->color(AuGridStyle::BorderColor, m_model);
    if (borderColor.isValid()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(borderColor);
        const QRectF innerPanel = QRectF(panel).adjusted(1, 1, -1, -1);
        QPainterPath borderPath;
        borderPath.setFillRule(Qt::OddEvenFill);
        if (radius > 0) {
            borderPath.addRoundedRect(QRectF(panel), radius, radius);
            borderPath.addRoundedRect(innerPanel, radius - 1, radius - 1);
        } else {
            borderPath.addRect(QRectF(panel));
            borderPath.addRect(innerPanel);
        }
        painter.drawPath(borderPath);
        painter.restore();
    }

    // 5. 加载遮罩：最后绘制，盖住表格内容（编辑器由 setLoading 关闭；
    //    滚动条是子控件盖不住，随蒙层反向淡变，见 applyLoadingFadeToScrollBars）
    if (m_loadingOpacity > 0.0)
        drawLoadingOverlay(&painter);
}

void AuGridWidget::drawLoadingOverlay(QPainter *painter)
{
    if (m_loadingOpacity <= 0.0 || !m_style)
        return;

    const QRect panel = contentPanelRect();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setClipPath(roundedClipPath(), Qt::ReplaceClip);
    painter->setOpacity(m_loadingOpacity);

    // 半透明蒙层（覆盖表头与全部内容区）
    const QColor overlayColor = m_style->color(AuGridStyle::LoadingOverlayColor, m_model);
    if (overlayColor.isValid()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(overlayColor);
        painter->drawRect(panel);
    }

    // 圆环 + 文案整体在面板内居中
    QString text = m_loadingText;
    if (text.isNull())
        text = m_style->value(AuGridStyle::LoadingText, m_model).toString();

    const QColor spinnerColor = m_style->color(AuGridStyle::LoadingSpinnerColor, m_model);
    const QColor textColor = m_style->color(AuGridStyle::LoadingTextColor, m_model);
    const int spinnerSize = qMax(12, m_style->integer(AuGridStyle::LoadingSpinnerSize, m_model));
    const QFont textFont = m_style->font(m_model);
    const QFontMetrics fm(textFont);
    const int textGap = 12;

    qreal blockHeight = spinnerSize;
    if (!text.isEmpty())
        blockHeight += textGap + fm.height();
    const qreal blockTop = panel.center().y() - blockHeight / 2.0;

    if (spinnerColor.isValid()) {
        const qreal penWidth = qMax(2.0, spinnerSize / 7.0);
        QRectF spinnerRect(panel.center().x() - spinnerSize / 2.0, blockTop, spinnerSize, spinnerSize);
        // 内缩半个线宽，避免圆弧溢出圆环外接矩形
        spinnerRect.adjust(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0);

        QPen pen(spinnerColor);
        pen.setWidthF(penWidth);
        pen.setCapStyle(Qt::RoundCap);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawArc(spinnerRect, m_loadingSpinnerAngle * 16, 100 * 16);
    }

    if (!text.isEmpty()) {
        painter->setFont(textFont);
        painter->setPen(textColor.isValid() ? textColor : QColor(80, 80, 80));
        const QRectF textRect(panel.left(), blockTop + spinnerSize + textGap,
                              panel.width(), fm.height());
        painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, text);
    }

    painter->restore();
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

QSharedPointer<AuCellRenderer> AuGridWidget::rendererForColumn(int column) const
{
    QSharedPointer<AuCellRenderer> renderer = m_defaultCellRenderer;
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return renderer;

    const AuColumnDef def = m_model->columnDefAt(column);
    if (def.isCheckbox())
        return m_checkboxCellRenderer;

    const QString rendererName = def.cellRenderer();
    if (!rendererName.isEmpty()) {
        auto it = m_cellRenderers.find(rendererName);
        if (it != m_cellRenderers.end())
            renderer = it.value();
    }
    return renderer;
}

QSharedPointer<AuHeaderRenderer> AuGridWidget::headerRendererForColumn(int column) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return QSharedPointer<AuHeaderRenderer>();
    const QString name = m_model->columnDefAt(column).headerRenderer();
    if (name.isEmpty())
        return QSharedPointer<AuHeaderRenderer>();
    return m_headerRenderers.value(name);
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

void AuGridWidget::resizeEvent(QResizeEvent * /*event*/)
{
    rebuildColumnOffsets();
    updateScrollBars();
    captureLayoutSnapshot();   // Flex 列宽随视口变化，动画基线的行偏移保持新鲜
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

void AuGridWidget::connectModel()
{
    if (!m_model)
        return;

    connect(m_model, &AuGridModel::dataChanged, this, [this](int topRow, int leftColumn, int bottomRow, int rightColumn) {
        for (int col = leftColumn; col <= rightColumn; ++col)
            invalidateAutoFit(col);
        refreshAutoFitColumns();
        invalidateRowHeights();
        if (m_activeEditor && m_editRow >= topRow && m_editRow <= bottomRow
            && m_editColumn >= leftColumn && m_editColumn <= rightColumn) {
            const QVariant value = m_model->cellValue(m_editRow, m_editColumn);
            const AuColumnDef def = m_model->columnDefAt(m_editColumn);
            QSharedPointer<AuCellEditor> editor = m_defaultCellEditor;
            const QString editorName = def.cellEditor();
            if (!editorName.isEmpty()) {
                auto it = m_cellEditors.find(editorName);
                if (it != m_cellEditors.end())
                    editor = it.value();
            }
            editor->setEditorValue(m_activeEditor, value);
        }
        update();
    });
    connect(m_model, &AuGridModel::rowsAboutToReset, this, [this]() {
        // 数据整体重设前捕获旧内容像素快照：重设动画 = 快照上移出场 + 新内容下移入场
        m_dataResetPending = false;
        if (!m_dataAnimationEnabled || m_loading || !isVisible() || bodyHeight() <= 0)
            return;
        if (!m_model || m_model->rowCount() <= 0)
            return;
        finishSortMoveAnimation();
        finishDataResetAnimation();
        // 快照中不携带底部边框线（与动画期间的绘制规则一致）：
        // 旧内容上移出场时，其底部边框会在逐渐变空的视口中单独滑动，非常出戏。
        // grab() 对可见控件直接从后备缓冲区取像素（其中是带线的旧画面），
        // 需先 repaint() 强制同步重绘一次，让压制标志真正作用到画面上；
        // 另外抓取矩形去掉最底部 1px——那里是面板外边框，烘焙进快照后同样会
        // 随旧内容上移、在空视口中形成一条晃动的灰线（用户反馈的正是它）
        m_suppressBottomLineForCapture = true;
        repaint(QRect(0, headerHeight(), width(), bodyHeight()));
        m_dataResetSnapshot = grab(QRect(0, headerHeight(), width(),
                                         qMax(0, bodyHeight() - 1))).toImage();
        m_suppressBottomLineForCapture = false;
        m_dataResetPending = !m_dataResetSnapshot.isNull();
    });
    connect(m_model, &AuGridModel::rowsAboutToBeRemoved, this, [this](const QVector<int> &originalRows) {
        // 删除落地前旧数据 / 旧布局仍完整：捕获被删可见行的像素带供消失动画使用
        captureRemovedRowGhosts(originalRows);
    });
    connect(m_model, &AuGridModel::rowsRemoved, this, [this](const QVector<int> & /*originalRows*/) {
        // 原始行号按位置存储，删除会使后续行整体前移，保留的选择/勾选必然错位，
        // 行为与 rowsChanged（数据替换）一致：整体清空并通知
        const bool hadChecks = !m_checkedOriginalRows.isEmpty() || !m_partialOriginalRows.isEmpty();
        const bool hadSelection = (m_selectedRow >= 0 || !m_selectedOriginalRows.isEmpty());
        m_checkedOriginalRows.clear();
        m_partialOriginalRows.clear();
        m_selectedRow = -1;
        m_selectedOriginalRow = -1;
        m_selectedOriginalRows.clear();
        if (!m_isRowDragging && !m_isColumnDragging) {
            if (hadChecks)
                emit checkedStateChanged();
            if (hadSelection)
                emit selectionChanged();
        }
    });
    connect(m_model, &AuGridModel::rowsChanged, this, [this]() {
        const bool hadChecks = !m_checkedOriginalRows.isEmpty();
        const bool hadSelection = (m_selectedRow >= 0 || !m_selectedOriginalRows.isEmpty());
        // 行/列拖拽过程中由拖拽逻辑自行维护勾选与选择状态，不要清空；也避免自动重绘干扰拖拽动画
        if (!m_isRowDragging && !m_isColumnDragging) {
            m_checkedOriginalRows.clear();
            m_partialOriginalRows.clear();
            m_selectedOriginalRow = -1;
            m_selectedOriginalRows.clear();
            captureLayoutSnapshot();   // 数据已替换：位移动画基线随之重置
        }
        pruneSpans();
        invalidateRowHeights();
        if (!m_isRowDragging && !m_isColumnDragging) {
            updateScrollBars();
            update();
        }
        if (hadChecks && !m_isRowDragging && !m_isColumnDragging)
            emit checkedStateChanged();
        if (hadSelection && !m_isRowDragging && !m_isColumnDragging)
            emit selectionChanged();
    });
    connect(m_model, &AuGridModel::columnsChanged, this, [this]() {
        // 列拖拽过程中由拖拽逻辑自行重建列偏移，避免此处清空 AutoFit 导致列宽跳变
        if (!m_isColumnDragging) {
            m_autoFitWidths.clear();
            m_autoFitDirty.clear();
            pruneSpans();
            rebuildColumnOffsets();
            invalidateRowHeights();
            updateScrollBars();
            update();
            captureLayoutSnapshot();
        }
    });
    connect(m_model, &AuGridModel::columnWidthsChanged, this, [this]() {
        rebuildColumnOffsets();
        invalidateRowHeights();
        updateEditorGeometry();   // 列宽变化时打开中的编辑器跟随单元格移动
        updateScrollBars();
        update();
        emit columnWidthsChanged();
        captureLayoutSnapshot();
    });
    connect(m_model, &AuGridModel::layoutChanged, this, [this]() {
        QToolTip::hideText();   // 排序/过滤/移动行后悬停处内容已变化，隐藏失效的 tooltip
        if (m_activeEditor)
            closeEditor(false);
        // 合并区域按可见行坐标存储；排序/过滤/移动行会改变可见行对应的数据，
        // 原合并位置不再可靠，直接清空（行为与按数据行实现的方案不同，需在文档中说明）
        if (!m_isRowDragging && !m_isColumnDragging)
            clearSpans();
        // 行/列拖拽过程中由拖拽逻辑自行维护选择状态，避免在模型移动后立即触发重绘，干扰拖拽动画
        if (!m_isRowDragging && !m_isColumnDragging)
            syncSelectionAfterLayoutChange();
        // 行/列拖拽过程中列宽不应因 model 的 layoutChanged 重新计算，避免列宽跳变
        if (!m_isRowDragging && !m_isColumnDragging) {
            m_autoFitWidths.clear();
            m_autoFitDirty.clear();
            rebuildColumnOffsets();
        }
        invalidateRowHeights();
        if (!m_isRowDragging && !m_isColumnDragging) {
            updateScrollBars();
            update();
            // 数据变化动画：重设数据 → 快照切换动画；删除行 → 专用消失动画；
            // 其余（排序 / 过滤）→ 行位移动画。之后都刷新布局基线，供下一次布局变化使用。
            if (m_dataResetPending)
                startDataResetAnimation();
            else if (m_rowRemovePending)
                startRowRemoveAnimation();
            else
                startSortMoveAnimation();
            captureLayoutSnapshot();
        } else {
            m_rowRemovePending = false;   // 拖拽期间不播动画：丢弃待演状态，避免污染下次布局变化
        }
    });
    connect(m_model, &AuGridModel::filterChanged, this, [this]() { emit filterChanged(); });
    // 树形展开态变化：转发为可见行号信号（折叠后行可能不可见，返回 -1）
    connect(m_model, &AuGridModel::rowExpandedChanged, this, [this](int originalRow, bool expanded) {
        const int visibleRow = m_model->visibleRowForOriginalRow(originalRow);
        emit rowExpandedChanged(visibleRow, expanded);
    });
}

void AuGridWidget::disconnectModel()
{
    if (m_model)
        disconnect(m_model, nullptr, this, nullptr);
}

void AuGridWidget::updateScrollRange()
{
    updateScrollBars();
}

void AuGridWidget::updateScrollBars()
{
    if (!m_model) {
        m_hScrollBar->hide();
        m_vScrollBar->hide();
        m_hScrollBarVisible = false;
        m_vScrollBarVisible = false;
        return;
    }

    // 加载遮罩完全显示期间强制隐藏滚动条（子控件会盖在蒙层上）；
    // 淡入 / 淡出过渡中按内容正常显隐，透明度由 applyLoadingFadeToScrollBars 随蒙层淡变
    if (m_loading && m_loadingOpacity >= 1.0) {
        m_hScrollBar->hide();
        m_vScrollBar->hide();
        m_hScrollBarVisible = false;
        m_vScrollBarVisible = false;
        return;
    }

    const bool hBarWasVisible = m_hScrollBarVisible;
    const bool vBarWasVisible = m_vScrollBarVisible;

    updateScrollBarsCompute();

    // 滚动条显隐变化后，Flex 列的剩余空间计算依据变了，需要重新分配列宽。
    if (hBarWasVisible != m_hScrollBarVisible || vBarWasVisible != m_vScrollBarVisible) {
        rebuildColumnOffsets();
        updateScrollBarsCompute();
    }

    const bool visibilityChanged = (hBarWasVisible != m_hScrollBarVisible)
                                   || (vBarWasVisible != m_vScrollBarVisible);
    if (visibilityChanged) {
        if (QWidget *win = window())
            win->repaint();
        else
            repaint();
    }
}

void AuGridWidget::updateScrollBarsCompute()
{
    const int scrollBarExtent = m_style->integer(AuGridStyle::ScrollBarExtent, m_model);
    const int fixedWidth = fixedColumnsWidth() + m_leftPinnedWidth + m_rightPinnedWidth;
    const int viewportWidth = width();
    const int viewportHeight = qMax(0, height() - headerHeight());
    const int contentWidth = totalContentWidth();
    const int contentHeight = totalContentHeight();
    // 横向滚动只作用于 body 区列：偏移表含左 / 右停靠列的全局偏移，
    // 量程需扣除停靠列宽，否则会过量滚动、在末尾露出一条空白列
    const int bodyContentWidth = qMax(0, contentWidth - m_leftPinnedWidth - m_rightPinnedWidth);

    const bool needHorizontal = bodyContentWidth > qMax(0, viewportWidth - fixedWidth);
    const bool needVertical = contentHeight > viewportHeight;

    const int hBarHeight = needHorizontal ? scrollBarExtent : 0;
    const int vBarWidth = needVertical ? scrollBarExtent : 0;

    const int availableWidth = qMax(0, width() - vBarWidth);
    const int availableHeight = qMax(0, height() - headerHeight() - hBarHeight);
    const int bodyVisibleWidth = qMax(0, availableWidth - fixedWidth);

    // 重新判断，减去滚动条占用的空间后是否仍然需要另一个滚动条
    const bool finalNeedHorizontal = bodyContentWidth > bodyVisibleWidth;
    const bool finalNeedVertical = contentHeight > availableHeight;

    const int finalHBarHeight = finalNeedHorizontal ? scrollBarExtent : 0;
    const int finalVBarWidth = finalNeedVertical ? scrollBarExtent : 0;

    const int finalAvailableWidth = qMax(0, width() - finalVBarWidth);
    const int finalBodyVisibleWidth = qMax(0, finalAvailableWidth - fixedWidth);

    const int hBarX = 0;
    const int hBarY = height() - finalHBarHeight;
    const int hBarWidth = qMax(0, width() - finalVBarWidth);
    const int vBarX = width() - finalVBarWidth;
    const int vBarY = headerHeight();
    const int vBarHeight = qMax(0, height() - headerHeight() - finalHBarHeight);

    if (finalNeedHorizontal) {
        m_hScrollBar->setRange(0, bodyContentWidth - finalBodyVisibleWidth);
        m_hScrollBar->setPageStep(finalBodyVisibleWidth);
        m_hScrollBar->setSingleStep(rowHeight());
        m_hScrollBar->setGeometry(hBarX, hBarY, hBarWidth, finalHBarHeight);
        m_hScrollBar->show();
        m_hScrollBar->raise();
        m_hScrollBarVisible = true;
    } else {
        m_hScrollBar->hide();
        m_hScrollBar->setRange(0, 0);
        m_hScrollBarVisible = false;
    }

    if (finalNeedVertical) {
        m_vScrollBar->setRange(0, contentHeight - availableHeight);
        m_vScrollBar->setPageStep(availableHeight);
        m_vScrollBar->setSingleStep(rowHeight());
        m_vScrollBar->setGeometry(vBarX, vBarY, finalVBarWidth, vBarHeight);
        m_vScrollBar->show();
        m_vScrollBar->raise();
        m_vScrollBarVisible = true;
    } else {
        m_vScrollBar->hide();
        m_vScrollBar->setRange(0, 0);
        m_vScrollBarVisible = false;
    }

    m_xOffset = m_hScrollBar->value();
    m_yOffset = m_vScrollBar->value();

    updateRoundedMask();
}

void AuGridWidget::updateRoundedMask()
{
    // 滚动条保持在表格外部，widget 本身不需要异形 mask
    clearMask();
}

void AuGridWidget::rebuildColumnOffsets()
{
    // 旧列宽表留作比较：Flex 列宽随视口尺寸 / 滚动条显隐 / 列显隐变化后，
    // 依赖列宽的行高测量（WordWrap preferredHeight）需一并失效重算
    const QVector<int> oldOffsets = m_columnOffsets;

    m_columnOffsets.clear();
    m_columnRegions.clear();
    m_leftPinnedColumns.clear();
    m_bodyColumns.clear();
    m_rightPinnedColumns.clear();
    m_leftPinnedWidth = 0;
    m_rightPinnedWidth = 0;

    if (!m_model) {
        m_columnOffsets.append(0);
        return;
    }

    const int columnCount = m_model->columnCount();
    if (m_autoFitWidths.size() != columnCount) {
        m_autoFitWidths.resize(columnCount);
        m_autoFitDirty.resize(columnCount);
        m_autoFitDirty.fill(true);
    }

    QVector<int> resolvedWidths(columnCount);

    // 第一遍：解析 Fixed 和 AutoFit 列；Flex 列暂记为 0 并累计权重
    int fixedAutoWidth = 0;
    int totalFlexWeight = 0;
    for (int i = 0; i < columnCount; ++i) {
        if (isColumnHidden(i)) {
            resolvedWidths[i] = 0;
            continue;
        }
        const AuColumnDef def = m_model->columnDefAt(i);
        int width = 0;
        switch (def.widthMode()) {
        case AuColumnDef::ColumnWidthMode::Fixed:
            width = def.width();
            break;
        case AuColumnDef::ColumnWidthMode::AutoFit:
            if (m_autoFitDirty.at(i))
                m_autoFitWidths[i] = measureAutoFitWidth(i);
            width = m_autoFitWidths.at(i);
            break;
        case AuColumnDef::ColumnWidthMode::Flex:
            width = 0;
            totalFlexWeight += def.flexWeight();
            break;
        }
        width = clampColumnWidth(i, width);
        resolvedWidths[i] = width;
        if (def.widthMode() != AuColumnDef::ColumnWidthMode::Flex)
            fixedAutoWidth += width;
    }

    // 临时全局偏移，用于计算列宽
    int offset = 0;
    m_columnOffsets.append(offset);
    for (int i = 0; i < columnCount; ++i) {
        offset += resolvedWidths.at(i);
        m_columnOffsets.append(offset);
    }

    // 按停靠状态分区
    m_columnRegions.resize(columnCount);
    for (int i = 0; i < columnCount; ++i) {
        if (isColumnHidden(i)) {
            m_columnRegions[i] = ColumnRegion::Body;
            continue;
        }
        const AuColumnDef def = m_model->columnDefAt(i);
        if (def.pinMode() == AuColumnDef::PinMode::Left)
            m_columnRegions[i] = ColumnRegion::LeftPinned;
        else if (def.pinMode() == AuColumnDef::PinMode::Right)
            m_columnRegions[i] = ColumnRegion::RightPinned;
        else
            m_columnRegions[i] = ColumnRegion::Body;
    }

    // 记录各区域列索引与总宽
    for (int i = 0; i < columnCount; ++i) {
        if (isColumnHidden(i))
            continue;
        const int width = resolvedWidths.at(i);
        const ColumnRegion region = m_columnRegions.at(i);
        if (region == ColumnRegion::LeftPinned) {
            m_leftPinnedColumns.append(i);
            m_leftPinnedWidth += width;
        } else if (region == ColumnRegion::RightPinned) {
            m_rightPinnedColumns.append(i);
            m_rightPinnedWidth += width;
        } else {
            m_bodyColumns.append(i);
        }
    }

    // 第二遍：分配 Flex 列的剩余空间
    if (totalFlexWeight > 0) {
        const int viewportW = viewportWidthForColumns();
        const int fixedWidth = fixedColumnsWidth();
        const int remaining = viewportW - fixedWidth - fixedAutoWidth;

        QVector<int> flexIndices;
        for (int i = 0; i < columnCount; ++i) {
            if (isColumnHidden(i))
                continue;
            if (m_model->columnDefAt(i).widthMode() == AuColumnDef::ColumnWidthMode::Flex)
                flexIndices.append(i);
        }

        if (remaining > 0) {
            int allocated = 0;
            for (int idx = 0; idx < flexIndices.size(); ++idx) {
                const int col = flexIndices.at(idx);
                const AuColumnDef def = m_model->columnDefAt(col);
                const int weight = def.flexWeight();

                int share;
                if (idx == flexIndices.size() - 1) {
                    share = remaining - allocated;
                } else {
                    share = static_cast<int>(static_cast<qreal>(remaining) * weight / totalFlexWeight);
                }

                share = clampColumnWidth(col, share);
                resolvedWidths[col] = share;
                allocated += share;
            }

            // 若因 maxWidth 裁剪导致未分完，将剩余像素依次分给未达上限的 Flex 列
            int leftover = remaining - allocated;
            while (leftover > 0) {
                bool progressed = false;
                for (int col : flexIndices) {
                    if (leftover <= 0)
                        break;
                    const AuColumnDef def = m_model->columnDefAt(col);
                    const int max = def.maxWidth();
                    if (max > 0 && resolvedWidths.at(col) >= max)
                        continue;
                    resolvedWidths[col]++;
                    leftover--;
                    progressed = true;
                }
                if (!progressed)
                    break;
            }
        } else {
            for (int col : flexIndices)
                resolvedWidths[col] = clampColumnWidth(col, m_model->columnDefAt(col).minWidth());
        }

        // 重新计算全局偏移
        m_columnOffsets.clear();
        offset = 0;
        m_columnOffsets.append(offset);
        for (int i = 0; i < columnCount; ++i) {
            offset += resolvedWidths.at(i);
            m_columnOffsets.append(offset);
        }
    }

    if (m_columnOffsets != oldOffsets)
        invalidateRowHeights();   // 列宽实际变化：行高测量结果失效（惰性重算）
}

int AuGridWidget::totalContentWidth() const
{
    if (m_columnOffsets.isEmpty())
        return 0;
    return m_columnOffsets.last();
}

int AuGridWidget::totalContentHeight() const
{
    if (!m_model || m_model->rowCount() == 0)
        return 0;
    if (m_rowOffsetsDirty)
        rebuildRowOffsets();
    return m_rowOffsets.isEmpty() ? 0 : m_rowOffsets.last();
}

int AuGridWidget::bodyHeight() const
{
    return qMax(0, height() - headerHeight() - (m_hScrollBarVisible ? m_style->integer(AuGridStyle::ScrollBarExtent, m_model) : 0));
}

QPoint AuGridWidget::cellAtPosition(const QPoint &pos) const
{
    if (!m_model || m_columnOffsets.isEmpty())
        return QPoint(-1, -1);

    int row = -1;
    if (pos.y() >= headerHeight() && m_model->rowCount() > 0) {
        if (m_rowOffsetsDirty)
            rebuildRowOffsets();
        const int contentY = pos.y() - headerHeight() + m_yOffset;
        auto it = std::upper_bound(m_rowOffsets.begin(), m_rowOffsets.end(), contentY);
        row = int(it - m_rowOffsets.begin()) - 1;
        if (row < 0)
            row = 0;
        else if (row >= m_model->rowCount())
            row = -1;
    }

    // 优先判断是否点击行号列
    if (m_showRowNumberColumn && pos.x() >= 0 && pos.x() < rowNumberColumnWidth())
        return QPoint(kRowNumberColumn, row);

    // 判断是否点击复选框列
    const int cbStart = checkboxColumnX();
    if (m_showCheckboxColumn && pos.x() >= cbStart && pos.x() < cbStart + checkboxColumnWidth())
        return QPoint(kCheckboxColumn, row);

    // 再判断是否点击行拖拽手柄列
    const int dhStart = dragHandleColumnX();
    const int dhEnd = dhStart + dragHandleColumnWidth();
    if (m_rowDragEnabled && pos.x() >= dhStart && pos.x() < dhEnd)
        return QPoint(kDragHandleColumn, row);

    int column = -1;

    // 优先判断是否在 sticky 的停靠列上
    for (int col : m_leftPinnedColumns) {
        const int x = columnX(col);
        const int w = columnWidth(col);
        if (pos.x() >= x && pos.x() < x + w) {
            column = col;
            break;
        }
    }
    if (column < 0) {
        for (int col : m_rightPinnedColumns) {
            const int x = columnX(col);
            const int w = columnWidth(col);
            if (pos.x() >= x && pos.x() < x + w) {
                column = col;
                break;
            }
        }
    }

    // 否则按自然位置查找 body 列（m_columnOffsets 已有序，用二分查找）
    if (column < 0) {
        const int fixedWidth = fixedColumnsWidth();
        const int absoluteX = pos.x() + m_xOffset - fixedWidth;
        if (absoluteX >= 0 && absoluteX < totalContentWidth()) {
            auto it = std::upper_bound(m_columnOffsets.begin(), m_columnOffsets.end(), absoluteX);
            const int col = int(it - m_columnOffsets.begin()) - 1;
            if (col >= 0 && col < m_model->columnCount()
                && columnRegion(col) == ColumnRegion::Body) {
                column = col;
            }
        }
    }

    // 命中合并单元格的被覆盖区域时，定位到合并锚点（与 QTableView 一致）
    if (row >= 0 && column >= 0) {
        const CellSpan span = spanAt(row, column);
        if (span.isValid()) {
            row = span.anchorRow;
            column = span.anchorColumn;
        }
    }

    return QPoint(column, row);
}

qint64 AuGridWidget::spanCellKey(int row, int column)
{
    return (static_cast<qint64>(row) << 32) | static_cast<quint32>(column);
}

AuGridWidget::CellSpan AuGridWidget::effectiveSpan(const CellSpan &span) const
{
    CellSpan invalid;
    if (!m_model || !span.isValid())
        return invalid;

    const int rows = m_model->rowCount();
    const int cols = m_model->columnCount();
    CellSpan eff = span;
    eff.rowCount = qMin(eff.rowCount, rows - eff.anchorRow);
    eff.columnCount = qMin(eff.columnCount, cols - eff.anchorColumn);
    if (eff.rowCount < 1 || eff.columnCount < 1)
        return invalid;

    // 合并区域不跨停靠区域：从锚点向右延伸，遇到与锚点不同区域的可见列即截断；
    // 隐藏列宽为 0，跳过后继续延伸
    const ColumnRegion region = columnRegion(eff.anchorColumn);
    int lastColumn = eff.anchorColumn;
    for (int c = eff.anchorColumn + 1; c <= eff.rightColumn(); ++c) {
        if (isColumnHidden(c))
            continue;
        if (columnRegion(c) != region)
            break;
        lastColumn = c;
    }
    eff.columnCount = lastColumn - eff.anchorColumn + 1;
    return eff;
}

AuGridWidget::CellSpan AuGridWidget::spanAt(int row, int column) const
{
    CellSpan invalid;
    if (m_spans.isEmpty() || row < 0 || column < 0)
        return invalid;

    const qint64 key = spanCellKey(row, column);
    CellSpan raw;
    const qint64 anchorKey = m_spanCovered.value(key, -1);
    if (anchorKey >= 0)
        raw = m_spans.value(anchorKey);
    else if (m_spans.contains(key))
        raw = m_spans.value(key);
    else
        return invalid;

    // 存储范围可能因行/列裁剪或停靠变化超出有效范围，裁剪后需仍包含该单元格
    const CellSpan eff = effectiveSpan(raw);
    if (!eff.isValid() || !eff.contains(row, column))
        return invalid;
    return eff;
}

bool AuGridWidget::isSpanCoveredCell(int row, int column) const
{
    const CellSpan span = spanAt(row, column);
    return span.isValid() && (row != span.anchorRow || column != span.anchorColumn);
}

int AuGridWidget::spanWidth(const CellSpan &span) const
{
    int width = 0;
    for (int c = span.anchorColumn; c <= span.rightColumn(); ++c)
        width += columnWidth(c);
    return width;
}

QRect AuGridWidget::spanRect(const CellSpan &span) const
{
    if (!span.isValid())
        return QRect();
    int height = 0;
    for (int r = span.anchorRow; r <= span.bottomRow(); ++r)
        height += rowHeight(r);
    return QRect(columnX(span.anchorColumn), rowY(span.anchorRow), spanWidth(span), height);
}

void AuGridWidget::removeSpan(qint64 anchorKey)
{
    if (m_spans.remove(anchorKey) == 0)
        return;
    for (auto it = m_spanCovered.begin(); it != m_spanCovered.end();) {
        if (it.value() == anchorKey)
            it = m_spanCovered.erase(it);
        else
            ++it;
    }
}

void AuGridWidget::removeOverlappingSpans(int topRow, int leftColumn, int bottomRow, int rightColumn)
{
    if (m_spans.isEmpty())
        return;

    QVector<qint64> overlapped;
    for (auto it = m_spans.cbegin(); it != m_spans.cend(); ++it) {
        const CellSpan &s = it.value();
        if (s.anchorRow <= bottomRow && s.bottomRow() >= topRow
            && s.anchorColumn <= rightColumn && s.rightColumn() >= leftColumn)
            overlapped.append(it.key());
    }
    for (qint64 key : overlapped)
        removeSpan(key);
}

void AuGridWidget::pruneSpans()
{
    if (m_spans.isEmpty() || !m_model)
        return;

    const int rows = m_model->rowCount();
    const int cols = m_model->columnCount();

    QVector<qint64> stale;
    for (auto it = m_spans.cbegin(); it != m_spans.cend(); ++it) {
        if (it.value().anchorRow >= rows || it.value().anchorColumn >= cols)
            stale.append(it.key());
    }
    for (qint64 key : stale)
        removeSpan(key);
}

void AuGridWidget::setSpan(int row, int column, int rowSpanCount, int columnSpanCount)
{
    if (!m_model || row < 0 || column < 0 || rowSpanCount <= 0 || columnSpanCount <= 0)
        return;

    const int maxRows = m_model->rowCount() - row;
    const int maxCols = m_model->columnCount() - column;
    if (maxRows <= 0 || maxCols <= 0)
        return;
    rowSpanCount = qMin(rowSpanCount, maxRows);
    columnSpanCount = qMin(columnSpanCount, maxCols);

    // 新合并替换区域内重叠的旧合并；1x1 等价于清除该处合并
    removeOverlappingSpans(row, column, row + rowSpanCount - 1, column + columnSpanCount - 1);

    if (rowSpanCount > 1 || columnSpanCount > 1) {
        CellSpan span;
        span.anchorRow = row;
        span.anchorColumn = column;
        span.rowCount = rowSpanCount;
        span.columnCount = columnSpanCount;
        const qint64 anchorKey = spanCellKey(row, column);
        m_spans.insert(anchorKey, span);
        for (int r = row; r < row + rowSpanCount; ++r) {
            for (int c = column; c < column + columnSpanCount; ++c) {
                if (r == row && c == column)
                    continue;
                m_spanCovered.insert(spanCellKey(r, c), anchorKey);
            }
        }
    }

    invalidateRowHeights();
    updateScrollBars();
    update();
}

void AuGridWidget::clearSpan(int row, int column)
{
    if (m_spans.isEmpty() || row < 0 || column < 0)
        return;

    const CellSpan span = spanAt(row, column);
    if (!span.isValid())
        return;

    removeSpan(spanCellKey(span.anchorRow, span.anchorColumn));
    invalidateRowHeights();
    updateScrollBars();
    update();
}

void AuGridWidget::clearSpans()
{
    if (m_spans.isEmpty())
        return;

    m_spans.clear();
    m_spanCovered.clear();
    invalidateRowHeights();
    updateScrollBars();
    update();
}

int AuGridWidget::rowSpan(int row, int column) const
{
    const CellSpan span = spanAt(row, column);
    return span.isValid() ? span.rowCount : 1;
}

int AuGridWidget::columnSpan(int row, int column) const
{
    const CellSpan span = spanAt(row, column);
    return span.isValid() ? span.columnCount : 1;
}

bool AuGridWidget::hasSpans() const
{
    return !m_spans.isEmpty();
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

void AuGridWidget::drawRowNumberCell(QPainter *painter, int row)
{
    const QRect rect = rowNumberCellRect(row);
    if (rect.isEmpty() || !m_model)
        return;

    if (row < 0 || row >= m_model->rowCount())
        return;

    QFont font = m_style->font(m_model);
    if (m_style->value(AuGridStyle::RowNumberFontBold, m_model).toBool())
        font.setBold(true);

    painter->save();
    painter->setFont(font);
    const QColor fgColor = m_style->color(AuGridStyle::RowNumberForegroundColor, m_model);
    painter->setPen(fgColor.isValid() ? fgColor : palette().text().color());
    const int padding = specialColumnPadding();
    const QRect textRect = rect.adjusted(padding, 0, -padding, 0);
    // 行号 = 可见行号 + 1：排序 / 过滤 / 拖拽重排后跟随可见顺序重新编号
    painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, QString::number(row + 1));
    painter->restore();
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

void AuGridWidget::drawDragHandleCell(QPainter *painter, int row)
{
    const QRect rect = dragHandleCellRect(row);
    if (rect.isEmpty())
        return;

    const bool hovered = (m_hoverRow == row && m_hoverColumn == kDragHandleColumn);
    const QColor bgColor = hovered
        ? m_style->color(AuGridStyle::DragHandleHoveredBackgroundColor, m_model)
        : m_style->color(AuGridStyle::DragHandleBackgroundColor, m_model);
    if (bgColor.isValid())
        painter->fillRect(rect, bgColor);

    const int dotSize = m_style->integer(AuGridStyle::DragHandleDotSize, m_model);
    const QColor dotColor = hovered
        ? m_style->color(AuGridStyle::DragHandleHoveredIconColor, m_model)
        : m_style->color(AuGridStyle::DragHandleIconColor, m_model);
    if (!dotColor.isValid() || dotSize <= 0)
        return;

    constexpr int cols = 3;
    constexpr int rows = 2;
    const int spacing = dotSize;
    const int totalH = rows * dotSize + (rows - 1) * spacing;
    const int padding = specialColumnPadding();
    const int leftPadding = m_showCheckboxColumn ? padding / 2 : padding;
    const int startX = rect.left() + leftPadding;
    const int startY = rect.top() + (rect.height() - totalH) / 2;

    painter->setBrush(dotColor);
    painter->setPen(Qt::NoPen);
    for (int r = 0; r < rows; ++r) {
        for (int c = 0; c < cols; ++c) {
            const int x = startX + c * (dotSize + spacing);
            const int y = startY + r * (dotSize + spacing);
            painter->drawEllipse(x, y, dotSize, dotSize);
        }
    }
}

QRect AuGridWidget::cellRect(int row, int column) const
{
    if (isRowNumberColumn(column))
        return rowNumberCellRect(row);
    if (isCheckboxColumn(column))
        return checkboxCellRect(row);
    if (isDragHandleColumn(column))
        return dragHandleCellRect(row);
    if (m_columnOffsets.isEmpty() || column < 0 || column >= m_model->columnCount())
        return QRect();
    // 合并单元格（锚点或被覆盖单元格）返回整个合并区域的矩形
    const CellSpan span = spanAt(row, column);
    if (span.isValid())
        return spanRect(span);
    return QRect(columnX(column), rowY(row),
                 m_columnOffsets.at(column + 1) - m_columnOffsets.at(column), rowHeight(row));
}

int AuGridWidget::columnX(int column) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return 0;

    const ColumnRegion region = columnRegion(column);

    // 列拖拽排序动画：使用 content offset 插值，body 列再减去当前水平滚动偏移
    for (const ColumnDragAnimationEntry &entry : m_columnDragAnimationEntries) {
        if (entry.column == column) {
            const int offset = entry.startOffset
                               + qRound((entry.endOffset - entry.startOffset) * m_columnDragAnimationProgress);
            if (region == ColumnRegion::Body)
                return offset - m_xOffset;
            return offset;
        }
    }

    // 列隐藏/显示动画：右侧列的位置插值
    for (const ColumnDragAnimationEntry &entry : m_columnVisShiftEntries) {
        if (entry.column == column) {
            const int offset = entry.startOffset
                               + qRound((entry.endOffset - entry.startOffset) * m_columnVisProgress);
            if (region == ColumnRegion::Body)
                return offset - m_xOffset;
            return offset;
        }
    }

    return baseColumnX(column);
}

int AuGridWidget::baseColumnX(int column) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return 0;

    const ColumnRegion region = columnRegion(column);
    const int fixedWidth = fixedColumnsWidth();
    if (region == ColumnRegion::Body)
        return fixedWidth + m_columnOffsets.at(column) - m_xOffset;

    if (region == ColumnRegion::LeftPinned) {
        // 右停靠区的左边界（按右区未夹紧位置计算，避免两个区域互相引用）
        int rightLimit = std::numeric_limits<int>::max();
        if (!m_rightPinnedColumns.isEmpty()) {
            int nextLeft = viewportWidthForColumns();
            for (int i = m_rightPinnedColumns.size() - 1; i >= 0; --i) {
                const int col = m_rightPinnedColumns.at(i);
                const int naturalRight = fixedWidth + m_columnOffsets.at(col + 1) - m_xOffset;
                nextLeft = qMin(nextLeft, naturalRight) - columnWidth(col);
            }
            rightLimit = nextLeft;
        }
        // 让位右移时不得越过右停靠区；预留本列右侧其余左停靠列的宽度，保证整组不重叠
        int totalLeftWidth = 0;
        for (int col : m_leftPinnedColumns)
            totalLeftWidth += columnWidth(col);
        int prevRight = fixedWidth;
        int widthsBefore = 0;
        for (int idx = 0; idx < m_leftPinnedColumns.size(); ++idx) {
            const int col = m_leftPinnedColumns.at(idx);
            const int naturalX = fixedWidth + m_columnOffsets.at(col) - m_xOffset;
            const int maxX = rightLimit - (totalLeftWidth - widthsBefore - columnWidth(col));
            const int x = qMax(prevRight, qMin(naturalX, maxX));
            if (col == column)
                return x;
            widthsBefore += columnWidth(col);
            prevRight = x + columnWidth(col);
        }
        return 0;
    }

    if (region == ColumnRegion::RightPinned) {
        // 左停靠区的右边缘（按左区未夹紧位置计算）
        int prevRight = fixedWidth;
        for (int col : m_leftPinnedColumns) {
            const int naturalX = fixedWidth + m_columnOffsets.at(col) - m_xOffset;
            prevRight = qMax(prevRight, naturalX) + columnWidth(col);
        }
        const int leftLimit = prevRight;

        // 先按让位规则求最左列位置，得出整组需右移的量，保持右停靠组内部不重叠
        int nextLeft = viewportWidthForColumns();
        int leftmostX = nextLeft;
        for (int i = m_rightPinnedColumns.size() - 1; i >= 0; --i) {
            const int col = m_rightPinnedColumns.at(i);
            const int naturalRight = fixedWidth + m_columnOffsets.at(col + 1) - m_xOffset;
            leftmostX = qMin(nextLeft, naturalRight) - columnWidth(col);
            nextLeft = leftmostX;
        }
        int shift = qMax(0, leftLimit - leftmostX);

        nextLeft = viewportWidthForColumns();
        for (int i = m_rightPinnedColumns.size() - 1; i >= 0; --i) {
            const int col = m_rightPinnedColumns.at(i);
            const int naturalRight = fixedWidth + m_columnOffsets.at(col + 1) - m_xOffset;
            const int width = columnWidth(col);
            const int right = qMin(nextLeft, naturalRight);
            const int x = right - width + shift;
            if (col == column)
                return x;
            nextLeft = x - shift;
        }
        return 0;
    }

    return 0;
}

int AuGridWidget::columnWidth(int column) const
{
    if (isRowNumberColumn(column))
        return rowNumberColumnWidth();
    if (isCheckboxColumn(column))
        return checkboxColumnWidth();
    if (isDragHandleColumn(column))
        return dragHandleColumnWidth();
    if (column < 0 || column >= m_columnOffsets.size() - 1)
        return 0;
    return m_columnOffsets.at(column + 1) - m_columnOffsets.at(column);
}

int AuGridWidget::rowY(int row) const
{
    if (m_rowOffsetsDirty)
        rebuildRowOffsets();

    int offset = m_rowOffsets.at(row);
    // 树形展开 / 折叠动画：受影响行按进度在旧 / 新偏移间插值
    for (const TreeRowAnimationEntry &entry : m_treeAnimationEntries) {
        if (entry.visibleRow == row) {
            offset = entry.startOffset
                     + qRound((entry.endOffset - entry.startOffset) * m_treeAnimationProgress);
            break;
        }
    }
    if (!m_dragAnimationEntries.isEmpty()) {
        for (const RowDragAnimationEntry &entry : m_dragAnimationEntries) {
            if (entry.visibleRow == row) {
                offset = entry.startOffset + qRound((entry.endOffset - entry.startOffset) * m_dragAnimationProgress);
                break;
            }
        }
    }
    if (!m_dataAnimationEntries.isEmpty()) {
        // 排序 / 过滤位移动画：行按旧布局位置 → 新布局位置插值
        for (const DataMoveAnimationEntry &entry : m_dataAnimationEntries) {
            if (entry.visibleRow == row) {
                offset = entry.startOffset
                         + qRound((entry.endOffset - entry.startOffset) * m_dataAnimationProgress);
                break;
            }
        }
    }
    if (m_dataResetAnimating) {
        // 重设数据切换（两段式）：前半程旧内容（快照）上移离场、新内容停在视口下方；
        // 后半程视口留空后，新内容自视口上方下移涌入
        if (m_dataResetProgress < 0.5) {
            offset += m_dataResetSlideHeight;
        } else {
            const qreal qe = auOutCubic((m_dataResetProgress - 0.5) * 2.0);
            offset -= qRound(m_dataResetSlideHeight * (1.0 - qe));
        }
    }
    return headerHeight() + offset - m_yOffset;
}

AuGridWidget::ColumnRegion AuGridWidget::columnRegion(int column) const
{
    if (column < 0 || column >= m_columnRegions.size())
        return ColumnRegion::Body;
    return m_columnRegions.at(column);
}

QPair<int, int> AuGridWidget::visibleColumnRange() const
{
    if (m_bodyColumns.isEmpty())
        return qMakePair(0, -1);

    int firstBodyIdx = -1;
    int lastBodyIdx = -1;

    // body 列按自然位置绘制（sticky 停靠列绘制在其上层，遮挡由绘制顺序保证），
    // 这里只做保守裁剪：自然跨度与视口完全不相交的列才剔除。
    // 旧实现假设左停靠组始终覆盖内容坐标 [0, 左停靠总宽]；当被停靠列位于叶子序列
    // 中部且未发生 sticky 位移时（如仅停靠中间列），该假设不成立，会把停靠列之前、
    // 实际未被遮挡的 body 列误判为视口外而漏绘（表现为该列内容消失）。
    const int fixedWidth = fixedColumnsWidth();
    const int viewportW = viewportWidthForColumns();
    // 内容坐标：widget x = fixedWidth + 内容x - m_xOffset，body 裁剪区为 [fixedWidth, viewportW]
    const int viewLeft = m_xOffset;
    const int viewRight = m_xOffset + viewportW - fixedWidth;

    for (int i = 0; i < m_bodyColumns.size(); ++i) {
        const int col = m_bodyColumns.at(i);
        if (m_columnOffsets.at(col + 1) > viewLeft) {
            firstBodyIdx = i;
            break;
        }
    }
    if (firstBodyIdx < 0)
        return qMakePair(0, -1);

    for (int i = m_bodyColumns.size() - 1; i >= firstBodyIdx; --i) {
        const int col = m_bodyColumns.at(i);
        if (m_columnOffsets.at(col) < viewRight) {
            lastBodyIdx = i;
            break;
        }
    }
    if (lastBodyIdx < 0)
        return qMakePair(0, -1);

    return qMakePair(m_bodyColumns.at(firstBodyIdx), m_bodyColumns.at(lastBodyIdx));
}

QPair<int, int> AuGridWidget::visibleRowRange() const
{
    if (!m_model || m_model->rowCount() == 0)
        return qMakePair(0, -1);

    if (m_rowOffsetsDirty)
        rebuildRowOffsets();

    const int bodyH = bodyHeight();
    const int viewBottom = m_yOffset + bodyH;
    const int rowCount = m_model->rowCount();

    auto firstIt = std::upper_bound(m_rowOffsets.begin(), m_rowOffsets.end(), m_yOffset);
    int firstRow = int(firstIt - m_rowOffsets.begin()) - 1;
    firstRow = qMax(0, firstRow - m_visibleRowBuffer);

    auto lastIt = std::upper_bound(m_rowOffsets.begin(), m_rowOffsets.end(), viewBottom);
    int lastRow = int(lastIt - m_rowOffsets.begin()) - 1;
    lastRow = qMin(rowCount - 1, lastRow + m_visibleRowBuffer);

    return qMakePair(firstRow, lastRow);
}

int AuGridWidget::viewportWidthForColumns() const
{
    return qMax(0, width() - (m_vScrollBarVisible ? m_style->integer(AuGridStyle::ScrollBarExtent, m_model) : 0));
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

int AuGridWidget::cornerRadius() const
{
    if (!m_style)
        return 0;
    const int radius = m_style->integer(AuGridStyle::CornerRadius, m_model);
    const QRect panel = contentPanelRect();
    const int maxRadius = qMin(panel.width(), panel.height()) / 2;
    return qBound(0, radius, maxRadius);
}

QRect AuGridWidget::contentPanelRect() const
{
    return rect();
}

QPainterPath AuGridWidget::roundedClipPath() const
{
    QPainterPath path;
    const int radius = cornerRadius();
    const QRect panel = contentPanelRect();
    if (radius > 0)
        path.addRoundedRect(panel, radius, radius);
    else
        path.addRect(panel);
    return path;
}

bool AuGridWidget::isInsideRoundedRect(const QPoint &pos) const
{
    const int radius = cornerRadius();
    if (radius <= 0)
        return contentPanelRect().contains(pos);
    return roundedClipPath().contains(pos);
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

void AuGridWidget::applyStyleToScrollBars()
{
    m_hScrollBar->setSliderColor(m_style->color(AuGridStyle::ScrollBarSliderColor, m_model));
    m_hScrollBar->setBackgroundColor(m_style->color(AuGridStyle::ScrollBarTrackBackgroundColor, m_model));
    m_vScrollBar->setSliderColor(m_style->color(AuGridStyle::ScrollBarSliderColor, m_model));
    m_vScrollBar->setBackgroundColor(m_style->color(AuGridStyle::ScrollBarTrackBackgroundColor, m_model));
    m_vScrollBar->setSeparatorColor(m_style->color(AuGridStyle::GridLineColor, m_model));

    // 横向滚动条两端只留与纵向滚动条一致的小边距（2px）。
    // 滑块是胶囊形（端部全圆角），贴近平板圆角区域也不会突兀；
    // 之前按圆角半径缩进（默认 8px），导致滑块两端出现明显空隙。
    m_hScrollBar->setTrackMargin(2);
}

void AuGridWidget::drawBackground(QPainter *painter)
{
    // 非圆角模式下，手动填充内容区背景
    if (cornerRadius() == 0)
        painter->fillRect(contentPanelRect(), panelBackgroundColor());
}

QColor AuGridWidget::panelBackgroundColor() const
{
    QColor color = m_style->color(AuGridStyle::RowBackgroundColor, m_model);
    if (!color.isValid())
        color = palette().base().color();
    return color;
}

void AuGridWidget::drawCheckbox(QPainter *painter, const QRect &box, bool checked, bool partial, bool enabled) const
{
    QColor checkedColor = m_style->color(AuGridStyle::CheckboxCheckedColor, m_model);
    QColor uncheckedColor = m_style->color(AuGridStyle::CheckboxUncheckedColor, m_model);
    QColor borderColor = m_style->color(AuGridStyle::CheckboxBorderColor, m_model);
    QColor checkmarkColor = m_style->color(AuGridStyle::CheckboxCheckmarkColor, m_model);

    if (!enabled) {
        checkedColor = QColor(200, 200, 200);
        uncheckedColor = QColor(230, 230, 230);
        borderColor = QColor(180, 180, 180);
        checkmarkColor = QColor(120, 120, 120);
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const int radius = 3;
    if (checked || partial) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(checkedColor.isValid() ? checkedColor : QColor(0, 120, 215));
        painter->drawRoundedRect(box, radius, radius);

        painter->setPen(QPen(checkmarkColor.isValid() ? checkmarkColor : Qt::white, 2));
        if (partial) {
            const int lineY = box.center().y();
            painter->drawLine(box.left() + 4, lineY, box.right() - 4, lineY);
        } else {
            QPainterPath path;
            path.moveTo(box.left() + 4, box.center().y());
            path.lineTo(box.center().x() - 1, box.bottom() - 4);
            path.lineTo(box.right() - 4, box.top() + 4);
            painter->drawPath(path);
        }
    } else {
        if (uncheckedColor.isValid()) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(uncheckedColor);
            painter->drawRoundedRect(box, radius, radius);
        }
        if (borderColor.isValid()) {
            painter->setPen(QPen(borderColor, 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(box.adjusted(1, 1, -1, -1), radius, radius);
        }
    }

    painter->restore();
}

void AuGridWidget::drawCheckboxHeader(QPainter *painter)
{
    const QRect rect = checkboxHeaderRect();
    if (!rect.isValid())
        return;

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(m_style->color(AuGridStyle::HeaderBackgroundColor, m_model));
    painter->drawRect(rect);
    painter->restore();

    const int size = m_style->integer(AuGridStyle::CheckboxSize, m_model);
    const int padding = specialColumnPadding();
    const int x = rect.left() + padding;
    const int y = rect.top() + (rect.height() - size) / 2;

    // 树形模式：表头三态按原始行全集聚合（折叠不可见的行也计入；
    // 聚合勾选可能包含不可勾选的祖先行，统计时需按可勾选行过滤）
    int checkableCount;
    int checkedCount;
    if (m_model && m_model->isTreeMode()) {
        checkableCount = checkableOriginalRowCount();
        checkedCount = checkedCheckableOriginalCount();
    } else {
        checkableCount = checkableRowCount();
        checkedCount = checkedRows().size();
    }
    const bool allChecked = checkableCount > 0 && checkedCount == checkableCount;
    const bool partial = !allChecked && checkedCount > 0;
    drawCheckbox(painter, QRect(x, y, size, size), allChecked, partial);
}

void AuGridWidget::drawCheckboxCell(QPainter *painter, int row)
{
    const QRect rect = checkboxCellRect(row);
    if (!rect.isValid())
        return;

    const int size = m_style->integer(AuGridStyle::CheckboxSize, m_model);
    const int padding = specialColumnPadding();
    const int x = rect.left() + padding;
    const int y = rect.top() + (rect.height() - size) / 2;

    const bool enabled = isRowCheckable(row);
    drawCheckbox(painter, QRect(x, y, size, size), isRowChecked(row), isRowPartiallyChecked(row), enabled);
}

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

void AuGridWidget::drawSortIndicator(QPainter *painter, const QRect &rect, Qt::SortOrder order) const
{
    const qreal size = 8;
    const qreal x = rect.right() - size - 6;
    const qreal y = rect.top() + (rect.height() - size) / 2.0;

    // 浮点坐标 + 反锯齿：底边落在整数像素行保持锐利，两条斜边平滑无锯齿
    QPolygonF triangle;
    if (order == Qt::AscendingOrder) {
        triangle << QPointF(x, y + size)
                 << QPointF(x + size, y + size)
                 << QPointF(x + size / 2.0, y);
    } else {
        triangle << QPointF(x, y)
                 << QPointF(x + size, y)
                 << QPointF(x + size / 2.0, y + size);
    }

    QColor color = m_style->color(AuGridStyle::SortIndicatorColor, m_model);
    if (!color.isValid())
        color = m_style->color(AuGridStyle::HeaderForegroundColor, m_model);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPolygon(triangle);
    painter->restore();
}

void AuGridWidget::drawHeader(QPainter *painter)
{
    if (!m_model || m_model->topLevelColumnDefs().isEmpty())
        return;

    const int viewportW = viewportWidthForColumns();
    const int panelW = contentPanelRect().width();
    const int vBarWidth = m_vScrollBarVisible ? m_style->integer(AuGridStyle::ScrollBarExtent, m_model) : 0;

    // 先填充整个表头底色（包含滚动条占位列）
    painter->save();
    painter->setClipRect(0, 0, panelW, headerHeight(), Qt::IntersectClip);
    painter->fillRect(0, 0, panelW, headerHeight(),
                     m_style->color(AuGridStyle::HeaderBackgroundColor, m_model));
    painter->restore();

    const int fixedWidth = fixedColumnsWidth();

    // body 区表头：裁剪掉复选框列与拖拽手柄列区域，避免横向滚动时覆盖
    painter->save();
    painter->setClipRect(fixedWidth, 0, viewportW - fixedWidth, headerHeight(), Qt::IntersectClip);
    int leafIndex = 0;
    drawHeaderRecursive(painter, m_model->topLevelColumnDefs(), 0, leafIndex, ColumnRegion::Body);
    painter->restore();

    // 左侧停靠列表头
    if (!m_leftPinnedColumns.isEmpty()) {
        painter->save();
        painter->setClipRect(0, 0, viewportW, headerHeight(), Qt::IntersectClip);
        leafIndex = 0;
        drawHeaderRecursive(painter, m_model->topLevelColumnDefs(), 0, leafIndex, ColumnRegion::LeftPinned);
        painter->restore();
    }

    // 右侧停靠列表头
    if (!m_rightPinnedColumns.isEmpty()) {
        painter->save();
        painter->setClipRect(0, 0, viewportW, headerHeight(), Qt::IntersectClip);
        leafIndex = 0;
        drawHeaderRecursive(painter, m_model->topLevelColumnDefs(), 0, leafIndex, ColumnRegion::RightPinned);
        painter->restore();
    }

    // 复选框列表头：最后绘制，使其覆盖可能滚动过来的 body 列头
    if (m_showCheckboxColumn)
        drawCheckboxHeader(painter);

    // 右侧滚动条占位列：与滚动条等宽的无内容表头
    if (vBarWidth > 0) {
        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_style->color(AuGridStyle::HeaderBackgroundColor, m_model));
        painter->drawRect(viewportW, 0, vBarWidth, headerHeight());
        painter->restore();
    }

    // 在表头最底部绘制一条贯穿的边框线，从表格左边缘到右边缘，
    // 覆盖复选框列和滚动条占位列（这些列没有叶子列头绘制底边）。
    // 与叶子列头“独占”底边对齐（y = headerHeight），与 body 首行顶边重叠仍为 1px。
    const QColor headerBorderColor = m_style->color(AuGridStyle::HeaderBorderColor, m_model);
    if (headerBorderColor.isValid()) {
        painter->save();
        painter->setPen(headerBorderColor);
        const int bottomY = headerHeight();
        painter->drawLine(0, bottomY, panelW, bottomY);
        painter->restore();
    }
}

void AuGridWidget::drawHeaderRecursive(QPainter *painter, const QVector<AuColumnDef> &defs, int depth, int &leafIndex, ColumnRegion region)
{
    for (const AuColumnDef &def : defs) {
        const int leafStart = leafIndex;

        if (def.isGroup()) {
            drawHeaderRecursive(painter, def.children(), depth + 1, leafIndex, region);
            // 分组内所有叶子都被隐藏时，该分组表头不再绘制
            bool anyVisible = false;
            for (int i = leafStart; i < leafIndex; ++i) {
                if (!isColumnHidden(i)) {
                    anyVisible = true;
                    break;
                }
            }
            if (anyVisible)
                drawHeaderNode(painter, def, depth, leafStart, leafIndex, region);
        } else {
            leafIndex++;
            if (!isColumnHidden(leafStart))
                drawHeaderNode(painter, def, depth, leafStart, leafIndex, region);
        }
    }
}

void AuGridWidget::drawHeaderNode(QPainter *painter, const AuColumnDef &def, int depth, int leafStart, int leafEnd, ColumnRegion region)
{
    const int levelHeight = headerLevelHeight();

    int firstInRegion = -1;
    int lastInRegion = -1;
    for (int i = leafStart; i < leafEnd; ++i) {
        if (isColumnHidden(i))
            continue;
        if (columnRegion(i) == region) {
            if (firstInRegion < 0)
                firstInRegion = i;
            lastInRegion = i;
        }
    }
    if (firstInRegion < 0)
        return;

    // 列拖拽在某个分组内部交换时，该分组及其所有祖先分组表头的边界应保持固定，不能跟随子列动画移动。
    const bool fixedGroupHeader = def.isGroup()
                                  && m_isColumnDragging
                                  && m_columnDragParentLeafStart >= 0
                                  && m_columnDragParentLeafEnd >= 0
                                  && leafStart <= m_columnDragParentLeafStart
                                  && leafEnd >= m_columnDragParentLeafEnd;

    const int x = fixedGroupHeader ? baseColumnX(firstInRegion)
                                   : columnX(firstInRegion);
    const int right = fixedGroupHeader ? (baseColumnX(lastInRegion) + columnWidth(lastInRegion))
                                       : (columnX(lastInRegion) + columnWidth(lastInRegion));
    const int width = right - x;
    const int y = depth * levelHeight;
    const int height = def.isGroup() ? levelHeight
                                      : (m_model->headerLevelCount() - depth) * levelHeight;

    const QRect rect(x, y, width, height);
    const QColor bgColor = m_style->color(AuGridStyle::HeaderBackgroundColor, m_model);
    const QColor borderColor = m_style->color(AuGridStyle::HeaderBorderColor, m_model);

    QColor textColor;
    if (def.hasHeaderForegroundColor())
        textColor = def.headerForegroundColor();
    else
        textColor = m_style->color(AuGridStyle::HeaderForegroundColor, m_model, -1, leafStart);

    Qt::Alignment alignment;
    if (def.hasHeaderAlignment())
        alignment = def.headerAlignment();
    else
        alignment = static_cast<Qt::Alignment>(m_style->value(AuGridStyle::HeaderAlignment, m_model, -1, leafStart).toInt());
    if (alignment == Qt::Alignment())
        alignment = def.isGroup() ? Qt::AlignCenter : def.alignment();
    alignment |= Qt::AlignVCenter;

    const QFont oldFont = painter->font();
    if (def.hasHeaderFont()) {
        painter->setFont(def.headerFont());
    } else {
        const QFont headerFont = m_style->value(AuGridStyle::HeaderFont, m_model, -1, leafStart).value<QFont>();
        if (headerFont != QFont())
            painter->setFont(headerFont);
    }

    const qreal opacity = columnOpacity(leafStart);
    const bool fade = opacity < 1.0;
    if (fade) {
        painter->save();
        painter->setOpacity(opacity);
    }

    painter->fillRect(rect, bgColor);
    painter->setPen(borderColor);
    // 每个列头绘制完整边框；相邻列头共享边界线，视觉上保持 1px，
    // 且列拖拽/排序动画中每个列头都自带完整边框，不会因共享边缺失而掉线。
    painter->drawRect(rect);

    const QSharedPointer<AuHeaderRenderer> headerRenderer =
        def.isGroup() ? QSharedPointer<AuHeaderRenderer>() : headerRendererForColumn(leafStart);
    if (headerRenderer) {
        AuHeaderRendererContext ctx;
        ctx.hovered = (leafStart == m_hoverHeaderColumn && (depth + 1) == m_hoverHeaderLevel);
        ctx.column = leafStart;
        ctx.level = depth + 1;
        ctx.isGroup = false;
        ctx.sorted = (leafStart == m_sortedColumn);
        ctx.sortOrder = m_sortOrder;
        ctx.headerPos = ctx.hovered ? m_hoverHeaderPos : QPoint(-1, -1);
        headerRenderer->paint(painter, rect, def, leafStart, depth + 1, m_model, m_style, textColor, ctx);
    } else {
        painter->setPen(textColor);
        QRect textRect = rect.adjusted(6, 0, -6, 0);
        if (!def.isGroup() && leafStart == m_sortedColumn)
            textRect.adjust(0, 0, -16, 0);
        painter->drawText(textRect, alignment, def.headerName());

        if (!def.isGroup() && leafStart == m_sortedColumn)
            drawSortIndicator(painter, rect, m_sortOrder);
    }

    painter->setFont(oldFont);
    if (fade)
        painter->restore();
}

void AuGridWidget::drawRowBackgrounds(QPainter *painter, int firstRow, int lastRow)
{
    if (!m_model || firstRow > lastRow)
        return;

    const int viewportW = viewportWidthForColumns();
    const int bodyH = bodyHeight();
    const QRect clipRect(0, headerHeight(), width(), bodyH);
    painter->save();
    painter->setClipRect(clipRect, Qt::IntersectClip);

    // 先铺满 body 区底色（包含滚动条占用的右侧区域），避免滚动条隐藏时露出旧像素。
    // 平移行图层不铺全局底色（会盖住下层渐变子行），改由平移行逐行铺不透明底。
    const QColor rowBackgroundColor = m_style->color(AuGridStyle::RowBackgroundColor, m_model);
    if (m_treeLayerFilter == 0 || m_treeLayerFilter == 3)
        painter->fillRect(clipRect, rowBackgroundColor.isValid() ? rowBackgroundColor : palette().base());

    const QColor selectedColor = m_style->color(AuGridStyle::SelectedRowBackgroundColor, m_model);
    const QColor alternateColor = m_style->color(AuGridStyle::AlternateRowBackgroundColor, m_model);
    const QColor hoverColor = m_style->color(AuGridStyle::HoverRowBackgroundColor, m_model);

    for (int row = firstRow; row <= lastRow; ++row) {
        if (!treeLayerAcceptRow(row))
            continue;
        const int y = rowY(row);
        const int h = rowHeight(row);
        // 树形展开 / 折叠动画：渐显 / 渐隐子行的行背景随进度变透明；
        // 平移行图层保持完全不透明（覆盖下层渐变子行，避免单元格边框穿透）
        const qreal rowOpacity = (m_treeLayerFilter == 2) ? 1.0 : rowAnimationOpacity(row);
        if (rowOpacity < 1.0) {
            painter->save();
            painter->setOpacity(rowOpacity);
        }
        if (m_treeLayerFilter == 2)
            painter->fillRect(0, y, viewportW, h,
                              rowBackgroundColor.isValid() ? rowBackgroundColor : palette().base());
        const bool rowSelected = isRowSelected(row);
        if (rowSelected && selectedColor.isValid())
            painter->fillRect(0, y, viewportW, h, selectedColor);
        else if (alternateColor.isValid() && row % 2 == 1)
            painter->fillRect(0, y, viewportW, h, alternateColor);

        if (row == m_hoverRow && !rowSelected && hoverColor.isValid())
            painter->fillRect(0, y, viewportW, h, hoverColor);
        if (rowOpacity < 1.0)
            painter->restore();
    }

    painter->restore();
}

void AuGridWidget::drawRows(QPainter *painter, int firstRow, int lastRow, int firstColumn, int lastColumn)
{
    if (!m_model || firstRow > lastRow || firstColumn > lastColumn)
        return;

    const int viewportW = viewportWidthForColumns();
    const int bodyH = bodyHeight();
    const ColumnRegion region = columnRegion(firstColumn);
    const int fixedWidth = fixedColumnsWidth();
    const QRect clipRect(region == ColumnRegion::Body ? fixedWidth : 0,
                         headerHeight(),
                         region == ColumnRegion::Body ? viewportW - fixedWidth : viewportW,
                         bodyH);
    painter->save();
    painter->setClipRect(clipRect, Qt::IntersectClip);

    const QColor defaultTextColor = palette().text().color();
    const QFont defaultFont = painter->font();

    const bool isPinnedRegion = (region == ColumnRegion::LeftPinned || region == ColumnRegion::RightPinned);
    const QColor selectedColor = m_style->color(AuGridStyle::SelectedRowBackgroundColor, m_model);
    const QColor alternateColor = m_style->color(AuGridStyle::AlternateRowBackgroundColor, m_model);
    const QColor hoverColor = m_style->color(AuGridStyle::HoverRowBackgroundColor, m_model);
    const int treeCol = isTreeMode() ? treeColumn() : -1;

    // 合并单元格锚点可能在可视范围外（区域被部分滚动出），把锚点并入绘制范围
    int paintFirstRow = firstRow;
    int paintFirstColumn = firstColumn;
    if (!m_spans.isEmpty()) {
        for (const CellSpan &s : m_spans) {
            const CellSpan e = effectiveSpan(s);
            if (!e.isValid() || columnRegion(e.anchorColumn) != region)
                continue;
            if (e.bottomRow() < firstRow || e.anchorRow > lastRow)
                continue;
            if (e.rightColumn() < firstColumn || e.anchorColumn > lastColumn)
                continue;
            paintFirstRow = qMin(paintFirstRow, e.anchorRow);
            paintFirstColumn = qMin(paintFirstColumn, e.anchorColumn);
        }
    }

    for (int row = paintFirstRow; row <= lastRow; ++row) {
        if (!treeLayerAcceptRow(row))
            continue;
        const int y = rowY(row);
        const int h = rowHeight(row);

        for (int col = paintFirstColumn; col <= lastColumn; ++col) {
            if (columnRegion(col) != region)
                continue;

            // 被合并覆盖的单元格不绘制任何内容
            const CellSpan span = spanAt(row, col);
            if (span.isValid() && (row != span.anchorRow || col != span.anchorColumn))
                continue;

            const AuColumnDef def = m_model->columnDefAt(col);
            const QRect rect = span.isValid() ? spanRect(span)
                : QRect(columnX(col), y, m_columnOffsets.at(col + 1) - m_columnOffsets.at(col), h);
            const QVariant value = m_model->cellValue(row, col);

            // 列显隐动画透明度 × 树形展开 / 折叠动画的行透明度
            const qreal opacity = columnOpacity(col) * rowAnimationOpacity(row);
            const bool fade = opacity < 1.0;
            if (fade) {
                painter->save();
                painter->setOpacity(opacity);
            }

            // 停靠列逐格铺背景；body 区合并单元格也需铺锚点行背景，避免露出斑马纹
            if (isPinnedRegion || span.isValid()) {
                const bool rowSelected = isRowSelected(row);
                QColor bg;
                if (rowSelected && selectedColor.isValid())
                    bg = selectedColor;
                else if (alternateColor.isValid() && row % 2 == 1)
                    bg = alternateColor;
                else {
                    const QColor rowBg = m_style->color(AuGridStyle::RowBackgroundColor, m_model);
                    bg = rowBg.isValid() ? rowBg : palette().base().color();
                }
                if (row == m_hoverRow && !rowSelected && hoverColor.isValid())
                    bg = hoverColor;
                painter->fillRect(rect, bg);
            }

            QSharedPointer<AuCellRenderer> renderer = rendererForColumn(col);

            painter->setPen(defaultTextColor);
            painter->setFont(defaultFont);

            AuCellRendererContext ctx;
            ctx.hovered = (row == m_hoverRow && col == m_hoverColumn);
            ctx.selected = (row == m_selectedRow && col == m_selectedColumn);
            ctx.alignment = def.alignment() | Qt::AlignVCenter;
            const QVariant alignVar = m_style->value(AuGridStyle::CellTextAlignment, m_model, row, col);
            if (alignVar.isValid() && alignVar.toInt() != 0)
                ctx.alignment = static_cast<Qt::Alignment>(alignVar.toInt()) | Qt::AlignVCenter;

            // 树形首列：层级缩进 + 展开箭头上下文
            if (col == treeCol) {
                const int originalRow = m_model->originalRowForVisibleRow(row);
                if (originalRow >= 0) {
                    ctx.treeRow = true;
                    ctx.treeDepth = m_model->rowDepth(originalRow);
                    ctx.treeHasChildren = m_model->rowHasChildren(originalRow);
                    ctx.treeExpanded = m_model->isRowExpanded(originalRow);
                    ctx.treeExpanderHovered = ctx.treeHasChildren
                        && row == m_hoverRow && col == m_hoverColumn
                        && treeExpanderRect(row).contains(m_hoverPos);
                }
            }

            renderer->paint(painter, rect, value, def, row, col, m_model, m_style, defaultTextColor, ctx);

            if (fade)
                painter->restore();
        }
    }

    painter->setFont(defaultFont);
    painter->restore();
}

void AuGridWidget::drawGridLines(QPainter *painter, int firstColumn, int lastColumn, int firstRow, int lastRow)
{
    if (!m_model || firstColumn > lastColumn || firstRow > lastRow)
        return;

    const ColumnRegion region = columnRegion(firstColumn);

    const int viewportW = viewportWidthForColumns();
    const int scrollBarExtent = m_style->integer(AuGridStyle::ScrollBarExtent, m_model);
    const QColor gridLineColor = m_style->color(AuGridStyle::GridLineColor, m_model);
    painter->setPen(gridLineColor);

    const int bodyTop = headerHeight();
    const int bodyBottom = qMin(bodyTop + totalContentHeight(), height() - (m_hScrollBarVisible ? scrollBarExtent : 0));

    const int fixedWidth = fixedColumnsWidth();
    const QRect clipRect(region == ColumnRegion::Body ? fixedWidth : 0,
                         bodyTop,
                         region == ColumnRegion::Body ? viewportW - fixedWidth : viewportW,
                         bodyBottom - bodyTop);
    painter->save();
    painter->setClipRect(clipRect, Qt::IntersectClip);

    if (m_rowOffsetsDirty)
        rebuildRowOffsets();

    // 合并单元格锚点可能在可视范围外（区域被部分滚动出），把锚点并入绘制范围
    int paintFirstRow = firstRow;
    int paintFirstColumn = firstColumn;
    if (!m_spans.isEmpty()) {
        for (const CellSpan &s : m_spans) {
            const CellSpan e = effectiveSpan(s);
            if (!e.isValid() || columnRegion(e.anchorColumn) != region)
                continue;
            if (e.bottomRow() < firstRow || e.anchorRow > lastRow)
                continue;
            if (e.rightColumn() < firstColumn || e.anchorColumn > lastColumn)
                continue;
            paintFirstRow = qMin(paintFirstRow, e.anchorRow);
            paintFirstColumn = qMin(paintFirstColumn, e.anchorColumn);
        }
    }

    // 每个单元格绘制完整边框；相邻单元格共享边界线，视觉上保持 1px，
    // 动画中每个单元格自带完整边框，避免掉线。
    // 隐藏左右（列方向）边框时只画上下两条横线（行分隔线保留）。
    const bool drawVertical = m_verticalGridLinesVisible;
    // 隐藏列方向边框且 body 内容未铺满区域宽度时，在内容右边缘补一条竖线，
    // 否则最右侧列的右侧缺边框观感不佳。x 取 body 区最后一列的右边缘（随滚动移动），
    // 始终描住“当前最右侧列”的右边界，而非停留在某个固定列上。
    int bodyContentRightX = -1;
    if (!drawVertical && region == ColumnRegion::Body && !m_bodyColumns.isEmpty()
        && m_columnOffsets.size() == m_model->columnCount() + 1) {
        const int lastBodyCol = m_bodyColumns.last();
        const int contentRight = fixedWidth + m_columnOffsets.at(lastBodyCol + 1) - m_xOffset;
        if (contentRight < viewportW)
            bodyContentRightX = contentRight;
    }
    // 重设数据切换动画期间（及为其捕获快照时），最后一行的底边线不绘制：
    // 内容整体平移时，这条线会在逐渐变空的视口中单独滑动，非常出戏；
    // 动画结束（或快照捕获完毕回到稳态）后按最终布局补画。
    const bool suppressLastRowBottom = m_dataResetAnimating || m_suppressBottomLineForCapture;
    const int lastDataRow = m_model->rowCount() - 1;
    for (int row = paintFirstRow; row <= lastRow; ++row) {
        if (!treeLayerAcceptRow(row))
            continue;
        const int y = rowY(row);
        const int h = rowHeight(row);
        // 渐变子行的边框随动画进度淡入 / 淡出（平移行图层保持不透明）
        const qreal rowOpacity = (m_treeLayerFilter == 2) ? 1.0 : rowAnimationOpacity(row);
        if (rowOpacity < 1.0) {
            painter->save();
            painter->setOpacity(rowOpacity);
        }
        // 末行且压制底边线时：竖线可见时画上 / 左 / 右三边，竖线隐藏时只画顶边
        const bool skipRowBottom = suppressLastRowBottom && row == lastDataRow;
        for (int col = paintFirstColumn; col <= lastColumn; ++col) {
            if (columnRegion(col) != region)
                continue;

            // 合并单元格：被覆盖格不画内框，锚点画整个合并区域的外框
            const CellSpan span = spanAt(row, col);
            if (span.isValid()) {
                if (row != span.anchorRow || col != span.anchorColumn)
                    continue;
                const QRect spanArea = spanRect(span);
                const qreal spanOpacity = columnOpacity(col);
                if (spanOpacity < 1.0) {
                    painter->save();
                    painter->setOpacity(spanOpacity);
                }
                if (drawVertical) {
                    if (skipRowBottom) {
                        painter->drawLine(spanArea.left(), spanArea.top(), spanArea.right(), spanArea.top());
                        painter->drawLine(spanArea.left(), spanArea.top(), spanArea.left(), spanArea.bottom());
                        painter->drawLine(spanArea.right(), spanArea.top(), spanArea.right(), spanArea.bottom());
                    } else {
                        painter->drawRect(spanArea);
                    }
                } else {
                    painter->drawLine(spanArea.left(), spanArea.top(), spanArea.right(), spanArea.top());
                    painter->drawLine(spanArea.left(), spanArea.bottom(), spanArea.right(), spanArea.bottom());
                }
                if (spanOpacity < 1.0)
                    painter->restore();
                continue;
            }

            const int x = columnX(col);
            const int w = columnWidth(col);
            const qreal opacity = columnOpacity(col);
            const bool fade = opacity < 1.0;
            if (fade) {
                painter->save();
                painter->setOpacity(opacity);
            }
            if (drawVertical) {
                if (skipRowBottom) {
                    painter->drawLine(x, y, x + w, y);
                    painter->drawLine(x, y, x, y + h);
                    painter->drawLine(x + w, y, x + w, y + h);
                } else {
                    painter->drawRect(x, y, w, h);
                }
            } else {
                painter->drawLine(x, y, x + w, y);
                if (!skipRowBottom)
                    painter->drawLine(x, y + h, x + w, y + h);
            }
            if (fade)
                painter->restore();
        }
        if (bodyContentRightX >= 0)
            painter->drawLine(bodyContentRightX, y, bodyContentRightX, y + h);
        if (rowOpacity < 1.0)
            painter->restore();
    }

    painter->restore();
}

void AuGridWidget::drawCellFocus(QPainter *painter)
{
    if (!m_model || !hasFocus())
        return;

    if (m_selectedRow < 0 || m_selectedColumn < 0)
        return;

    if (m_activeEditor && m_editRow == m_selectedRow && m_editColumn == m_selectedColumn)
        return;

    // 锚点落在不可聚焦格上（动态回调生效后可能出现）时不绘制焦点框；
    // 锚点会在下一次导航时自动跳到就近可聚焦列
    if (!isCellFocusable(m_selectedRow, m_selectedColumn))
        return;

    const QColor borderColor = m_style->color(AuGridStyle::FocusedCellBorderColor, m_model, m_selectedRow, m_selectedColumn);
    const int borderWidth = m_style->integer(AuGridStyle::FocusedCellBorderWidth, m_model, m_selectedRow, m_selectedColumn);
    if (!borderColor.isValid() || borderWidth <= 0)
        return;

    const QRect rect = cellRect(m_selectedRow, m_selectedColumn);
    if (rect.isEmpty())
        return;

    const int viewportW = viewportWidthForColumns();
    const int scrollBarExtent = m_style->integer(AuGridStyle::ScrollBarExtent, m_model);
    const int bodyTop = headerHeight();
    const int bodyBottom = qMin(bodyTop + totalContentHeight(), height() - (m_hScrollBarVisible ? scrollBarExtent : 0));
    const QRect clipRect(0, bodyTop, viewportW, bodyBottom - bodyTop);
    painter->save();
    painter->setClipRect(clipRect, Qt::IntersectClip);

    // body 单元格（含合并区域）滚动到停靠列下方时，边框不能画到固定列/停靠列上；
    // 边界与 drawRegionSeparators 的分隔线一致（仅统计实际发生 sticky 位移的停靠列）。
    // 停靠列单元格位置固定，不会滑入其他区域，无需收缩。
    if (columnRegion(m_selectedColumn) == ColumnRegion::Body) {
        QRect drawClip = clipRect;
        drawClip.setLeft(qMax(drawClip.left(), stickyLeftBoundaryX()));
        drawClip.setRight(qMin(drawClip.right(), stickyRightBoundaryX() - 1));
        painter->setClipRect(drawClip, Qt::IntersectClip);
    }

    QPen pen(borderColor);
    pen.setWidth(borderWidth);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect.adjusted(borderWidth / 2, borderWidth / 2, -borderWidth / 2, -borderWidth / 2));

    painter->restore();
}

int AuGridWidget::stickyLeftBoundaryX() const
{
    // 左侧 sticky 区域的右边界：固定列与实际发生 sticky 位移的左停靠列中最大右边缘
    int boundary = fixedColumnsWidth();
    for (int col : m_leftPinnedColumns) {
        const int stickyX = columnX(col);
        const int naturalX = m_columnOffsets.at(col) - m_xOffset;
        if (stickyX > naturalX)
            boundary = qMax(boundary, stickyX + columnWidth(col));
    }
    return boundary;
}

int AuGridWidget::stickyRightBoundaryX() const
{
    // 右侧 sticky 区域的左边界：实际发生 sticky 位移的右停靠列中最小左边缘
    const int viewportW = viewportWidthForColumns();
    int boundary = viewportW;
    for (int col : m_rightPinnedColumns) {
        const int stickyX = columnX(col);
        const int naturalX = m_columnOffsets.at(col) - m_xOffset;
        if (stickyX < naturalX)
            boundary = qMin(boundary, stickyX);
    }
    return boundary;
}

void AuGridWidget::drawRegionSeparators(QPainter *painter)
{
    if (!m_model)
        return;

    const int viewportW = viewportWidthForColumns();
    const int scrollBarExtent = m_style->integer(AuGridStyle::ScrollBarExtent, m_model);
    const int bodyTop = headerHeight();
    const int bodyBottom = qMin(bodyTop + totalContentHeight(), height() - (m_hScrollBarVisible ? scrollBarExtent : 0));

    const QColor separatorColor = m_style->color(AuGridStyle::HeaderBorderColor, m_model);
    if (!separatorColor.isValid())
        return;

    QPen pen(separatorColor);
    pen.setWidth(1);
    painter->setPen(pen);

    // 停靠区与 body 区的交界竖线始终绘制（无论停靠列是否发生 sticky 位移，
    // 也不受列边框开关影响）；同侧多列停靠之间的分隔随列边框开关。
    int leftSeparatorX = -1;
    if (fixedColumnsWidth() > 0)
        leftSeparatorX = stickyLeftBoundaryX();
    for (int i = m_leftPinnedColumns.size() - 1; i >= 0; --i) {
        const int col = m_leftPinnedColumns.at(i);
        if (!isColumnHidden(col)) {
            leftSeparatorX = qMax(leftSeparatorX, columnX(col) + columnWidth(col));
            break;
        }
    }
    if (leftSeparatorX > 0)
        painter->drawLine(leftSeparatorX, 0, leftSeparatorX, bodyBottom);

    int rightSeparatorX = stickyRightBoundaryX();
    for (int col : m_rightPinnedColumns) {
        if (!isColumnHidden(col)) {
            rightSeparatorX = qMin(rightSeparatorX, columnX(col));
            break;
        }
    }
    if (rightSeparatorX < viewportW)
        painter->drawLine(rightSeparatorX, 0, rightSeparatorX, bodyBottom);
}

bool AuGridWidget::editCell(int row, int column)
{
    return editCell(row, column, QString());
}

bool AuGridWidget::editCell(int row, int column, const QString &initialText)
{

    if (!m_model || row < 0 || row >= m_model->rowCount()
        || column < 0 || column >= m_model->columnCount())
        return false;

    // 编辑合并单元格时始终编辑锚点数据
    const CellSpan span = spanAt(row, column);
    if (span.isValid()) {
        row = span.anchorRow;
        column = span.anchorColumn;
    }

    if (!isCellEditable(row, column))
        return false;

    // 内置复选框列通过点击/空格切换，不使用文本编辑器
    if (m_model->columnDefAt(column).isCheckbox())
        return false;

    // 如果当前有编辑器，先尝试结束当前编辑
    if (m_activeEditor && !tryFinishCurrentEdit())
        return false;

    if (m_activeEditor)
        closeEditor(false);

    const AuColumnDef def = m_model->columnDefAt(column);
    QSharedPointer<AuCellEditor> editor = m_defaultCellEditor;
    const QString editorName = def.cellEditor();
    if (!editorName.isEmpty()) {
        auto it = m_cellEditors.find(editorName);
        if (it != m_cellEditors.end())
            editor = it.value();
    } else if (def.textWrapMode() == AuColumnDef::TextWrapMode::WordWrap) {
        editor = m_defaultMultiLineCellEditor;
    }

    QWidget *editorWidget = editor->createEditor(this);
    if (!editorWidget)
        return false;

    editor->setEditorValue(editorWidget, m_model->cellValue(row, column));
    if (!initialText.isEmpty())
        editor->setInitialTypedText(editorWidget, initialText);

    editorWidget->installEventFilter(this);
    editorWidget->setAutoFillBackground(true);
    editorWidget->setAttribute(Qt::WA_Hover, true); // 校验失败时悬停编辑框可重新唤起错误提示
    QPalette pal = editorWidget->palette();
    const QColor bgColor = m_style->color(AuGridStyle::CellBackgroundColor, m_model, row, column);
    pal.setColor(QPalette::Base, bgColor.isValid() ? bgColor : palette().base().color());
    editorWidget->setPalette(pal);

    m_activeEditor = editorWidget;
    m_activeEditorImpl = editor;
    m_editRow = row;
    m_editColumn = column;

    ensureCellVisible(row, column);
    updateEditorGeometry();
    editorWidget->setFocus();
    editorWidget->show();

    const bool selectionDidChange = (m_selectedRow != row || m_selectedColumn != column);
    m_selectedRow = row;
    m_selectedColumn = column;
    m_selectedOriginalRow = m_model->originalRowForVisibleRow(row);
    emit cellEditingStarted(row, column);
    if (selectionDidChange)
        emit selectionChanged();
    return true;
}

bool AuGridWidget::isEditing() const
{
    return m_activeEditor != nullptr;
}

void AuGridWidget::closeEditor(bool commit)
{
    if (!m_activeEditor)
        return;

    // 编辑结束即清除错误提示（编辑器红框 + 提示浮层）
    clearEditError();

    QWidget *editor = m_activeEditor;
    const int row = m_editRow;
    const int column = m_editColumn;
    QSharedPointer<AuCellEditor> editorImpl = m_activeEditorImpl;

    m_committing = true;
    m_activeEditor = nullptr;
    m_activeEditorImpl.reset();
    m_editRow = -1;
    m_editColumn = -1;

    editor->removeEventFilter(this);

    if (commit) {
        const QString field = m_model ? m_model->fieldAtColumn(column) : QString();
        if (!field.isEmpty() && editorImpl) {
            const QVariant value = editorImpl->editorValue(editor);
            if (m_model->setCellValue(row, field, value))
                emit cellValueChanged(row, column, value);
        }
    }

    emit cellEditingFinished(row, column);
    editor->deleteLater();
    // 编辑器子树仍持有焦点时（如 AuSelect / AuDatePicker 选择后主动提交），
    // 把焦点还给表格：否则编辑器销毁后焦点悬空，Tab / Enter / 方向键等键盘导航失效
    QWidget *focused = QApplication::focusWidget();
    if (focused && (focused == editor || editor->isAncestorOf(focused)))
        setFocus();
    m_committing = false;
    update();
}

void AuGridWidget::setCellEditValidator(QSharedPointer<AuCellEditValidator> validator)
{
    m_cellEditValidator = validator;
}

void AuGridWidget::setCellEditableCallback(const std::function<bool(const AuGridModel *, int, int)> &callback)
{
    m_cellEditableCallback = callback;
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

bool AuGridWidget::isCellEditable(int row, int column) const
{
    if (!m_model || row < 0 || row >= m_model->rowCount()
        || column < 0 || column >= m_model->columnCount())
        return false;

    if (!m_model->columnDefAt(column).isEditable())
        return false;

    if (m_cellEditableCallback)
        return m_cellEditableCallback(m_model, row, column);

    return true;
}

void AuGridWidget::toggleCheckboxCellValue(int row, int column)
{
    if (!m_model || row < 0 || row >= m_model->rowCount()
        || column < 0 || column >= m_model->columnCount())
        return;

    if (!m_model->columnDefAt(column).isCheckbox())
        return;
    if (!isCellEditable(row, column))
        return;

    const bool next = !m_model->cellValue(row, column).toBool();
    if (m_model->setCellValue(row, column, next))
        emit cellCheckStateChanged(row, column, next);
}

bool AuGridWidget::tryFinishCurrentEdit(bool focusOut)
{
    if (!m_activeEditor || !m_model)
        return true;

    clearEditError();

    const int row = m_editRow;
    const int column = m_editColumn;
    QSharedPointer<AuCellEditor> editorImpl = editorImplForColumn(column);

    const QVariant oldValue = m_model->cellValue(row, column);
    const QVariant newValue = editorImpl->editorValue(m_activeEditor);

    if (m_cellEditValidator) {
        QString errorMessage;
        const AuEditValidationResult result = m_cellEditValidator->validate(
            m_model, row, column, oldValue, newValue, errorMessage);

        switch (result) {
        case AuEditValidationResult::Accept:
            closeEditor(true);
            return true;
        case AuEditValidationResult::Reject:
            closeEditor(false);
            return true;
        case AuEditValidationResult::RejectKeepEditing:
            if (focusOut) {
                // 失焦时校验不通过直接取消
                closeEditor(false);
                return true;
            }
            showEditError(errorMessage);
            ensureCellVisible(row, column);
            updateEditorGeometry();
            if (m_activeEditor)
                m_activeEditor->setFocus();
            return false;
        }
    }

    closeEditor(true);
    return true;
}

void AuGridWidget::showEditError(const QString &message)
{
    if (!m_activeEditor)
        return;

    m_activeEditor->setStyleSheet("border: 2px solid red;");

    // 常驻错误提示浮层：紧贴编辑框正下方（间隙 2px），持续显示到校验通过
    //（替代 QToolTip：只弹一次、约 20px 悬浮间距、超时自动消失的观感都不符合需求）
    if (!m_editErrorTip) {
        m_editErrorTip = new QLabel(this);
        m_editErrorTip->setTextFormat(Qt::PlainText);
        m_editErrorTip->setStyleSheet(QStringLiteral(
            "QLabel { background-color: #fff1f0; color: #cf1322;"
            " border: 1px solid #ffa39e; border-radius: 4px; padding: 3px 8px; }"));
    }
    m_editErrorTip->setText(message);
    m_editErrorTip->adjustSize();
    updateEditErrorTipGeometry();
    m_editErrorTip->show();
    m_editErrorTip->raise();
}

void AuGridWidget::clearEditError()
{
    // 编辑器已被 closeEditor 清空时也要把残留的提示浮层收掉
    if (m_editErrorTip && m_editErrorTip->isVisible())
        m_editErrorTip->hide();

    if (!m_activeEditor)
        return;

    m_activeEditor->setStyleSheet(QString());
}

void AuGridWidget::refreshEditError()
{
    // 校验失败期间（提示浮层可见）按键时即时复查：
    // 通过则清除提示，不通过则按最新输入刷新提示内容与位置
    if (!m_editErrorTip || !m_editErrorTip->isVisible())
        return;
    if (!m_activeEditor || !m_model || !m_cellEditValidator) {
        clearEditError();
        return;
    }

    const int row = m_editRow;
    const int column = m_editColumn;
    const QSharedPointer<AuCellEditor> editorImpl = editorImplForColumn(column);
    if (!editorImpl) {
        clearEditError();
        return;
    }

    const QVariant oldValue = m_model->cellValue(row, column);
    const QVariant newValue = editorImpl->editorValue(m_activeEditor);
    QString errorMessage;
    const AuEditValidationResult result = m_cellEditValidator->validate(
        m_model, row, column, oldValue, newValue, errorMessage);

    if (result == AuEditValidationResult::RejectKeepEditing) {
        if (!errorMessage.isEmpty())
            m_editErrorTip->setText(errorMessage);
        m_editErrorTip->adjustSize();
        updateEditErrorTipGeometry();
        m_editErrorTip->raise();
    } else {
        clearEditError();
    }
}

void AuGridWidget::updateEditErrorTipGeometry()
{
    if (!m_editErrorTip || !m_activeEditor)
        return;

    const QRect editorRect = m_activeEditor->geometry();
    const int gap = 2;
    const int scrollBarExtent = m_style ? m_style->integer(AuGridStyle::ScrollBarExtent, m_model) : 0;
    const int bottomLimit = height() - (m_hScrollBarVisible ? scrollBarExtent : 0);

    // 优先贴编辑框下方；编辑框贴近表格底部放不下时翻转到上方
    int y = editorRect.bottom() + 1 + gap;
    if (y + m_editErrorTip->height() > bottomLimit)
        y = editorRect.top() - gap - m_editErrorTip->height();
    const int x = qBound(0, editorRect.left(), qMax(0, width() - m_editErrorTip->width()));
    m_editErrorTip->setGeometry(x, y, m_editErrorTip->width(), m_editErrorTip->height());
}

QSharedPointer<AuCellEditor> AuGridWidget::editorImplForColumn(int column) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return m_defaultCellEditor;

    const QString editorName = m_model->columnDefAt(column).cellEditor();
    if (!editorName.isEmpty()) {
        auto it = m_cellEditors.find(editorName);
        if (it != m_cellEditors.end())
            return it.value();
    }
    return m_defaultCellEditor;
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

void AuGridWidget::updateEditorGeometry()
{
    if (!m_activeEditor || !m_model)
        return;

    QRect rect = cellRect(m_editRow, m_editColumn);
    const int bodyTop = headerHeight();
    const int bodyBottom = height() - (m_hScrollBarVisible ? m_style->integer(AuGridStyle::ScrollBarExtent, m_model) : 0);
    const int viewportW = viewportWidthForColumns();

    const bool visible = !(rect.isEmpty() || rect.bottom() <= bodyTop || rect.top() >= bodyBottom
                           || rect.right() <= 0 || rect.left() >= viewportW);

    if (visible) {
        // 裁剪到可视区域
        if (rect.top() < bodyTop)
            rect.setTop(bodyTop);
        if (rect.bottom() > bodyBottom)
            rect.setBottom(bodyBottom);

        // body 单元格（含合并区域）不能覆盖到固定列/停靠列上
        if (columnRegion(m_editColumn) == ColumnRegion::Body) {
            if (rect.left() < stickyLeftBoundaryX())
                rect.setLeft(stickyLeftBoundaryX());
            if (rect.right() > stickyRightBoundaryX() - 1)
                rect.setRight(stickyRightBoundaryX() - 1);
        } else {
            if (rect.left() < 0)
                rect.setLeft(0);
            if (rect.right() > viewportW)
                rect.setRight(viewportW);
        }

        // 四边各留 1px 给单元格边框线：编辑框若完全覆盖单元格，会把与上一行 /
        // 下一行之间的分隔线一并盖住，视觉上与相邻行粘连（尤其无边框的文本编辑框）
        rect.adjust(1, 1, -1, -1);

        if (rect.width() > 0 && rect.height() > 0) {
            m_activeEditor->setGeometry(rect);
            if (!m_activeEditor->isVisible())
                m_activeEditor->show();
        } else if (m_activeEditor->isVisible()) {
            m_activeEditor->hide();
        }
    } else {
        if (m_activeEditor->isVisible())
            m_activeEditor->hide();
    }

    // 编辑器几何跟随（滚动 / 布局变化）时，错误提示浮层保持贴住编辑框
    if (m_editErrorTip && m_editErrorTip->isVisible()) {
        updateEditErrorTipGeometry();
        m_editErrorTip->raise();
    }
}

bool AuGridWidget::findNextEditableCell(int &row, int &column, int dColumn) const
{
    if (!m_model || m_model->rowCount() == 0 || m_model->columnCount() == 0 || qAbs(dColumn) != 1)
        return false;

    const int colCount = m_model->columnCount();
    const int rowCount = m_model->rowCount();

    int r = row;
    int c = column + dColumn;

    while (r >= 0 && r < rowCount) {
        while (c >= 0 && c < colCount) {
            if (!isColumnHidden(c) && !isSpanCoveredCell(r, c)
                && isCellFocusable(r, c)
                && isCellEditable(r, c) && !m_model->columnDefAt(c).isCheckbox()) {
                row = r;
                column = c;
                return true;
            }
            c += dColumn;
        }
        r += dColumn;
        c = (dColumn > 0) ? 0 : colCount - 1;
    }
    return false;
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

void AuGridWidget::setVerticalGridLinesVisible(bool visible)
{
    if (m_verticalGridLinesVisible == visible)
        return;
    m_verticalGridLinesVisible = visible;
    update();
}

bool AuGridWidget::isVerticalGridLinesVisible() const
{
    return m_verticalGridLinesVisible;
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

void AuGridWidget::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;

    if (loading) {
        // 蒙层盖不住子控件，先收掉：编辑器（数据即将刷新，放弃未提交修改）、
        // 进行中的拖拽、展开动画与滚动条
        if (isEditing())
            closeEditor(false);
        cancelRowDrag();
        cancelColumnDrag();
        finishTreeAnimation();
        clearHoverCell();
        clearHeaderHover();
        // 淡入过渡中滚动条仍短暂显示（随蒙层淡出），禁用交互保证遮罩屏蔽行为不变
        m_hScrollBar->setEnabled(false);
        m_vScrollBar->setEnabled(false);
    } else {
        m_hScrollBar->setEnabled(true);
        m_vScrollBar->setEnabled(true);
    }

    // 滚动条显隐与列布局在过渡开始时就位（此时蒙层几乎不透明，布局变化不可见），
    // 之后透明度随蒙层淡变，避免淡出结束时滚动条 / 列宽突然变化造成闪烁
    updateScrollBars();

    m_loadingAnimation->stop();
    m_loadingAnimation->setDuration(loading ? 200 : 160);
    m_loadingAnimation->setStartValue(m_loadingOpacity);
    m_loadingAnimation->setEndValue(loading ? 1.0 : 0.0);
    m_loadingAnimation->start();
    update();
}

void AuGridWidget::applyLoadingFadeToScrollBars(qreal maskOpacity)
{
    // 滚动条是子控件，蒙层盖不住；淡入 / 淡出过渡中让其透明度与蒙层反向同步淡变，
    // 稳态（蒙层全显 / 全隐）时交回 updateScrollBars 按内容与加载状态决定真实显隐
    const qreal barOpacity = 1.0 - maskOpacity;
    m_hScrollBar->setContentOpacity(barOpacity);
    m_vScrollBar->setContentOpacity(barOpacity);
    if (maskOpacity <= 0.0 || maskOpacity >= 1.0)
        updateScrollBars();
}

bool AuGridWidget::isLoading() const
{
    return m_loading;
}

void AuGridWidget::setLoadingText(const QString &text)
{
    if (m_loadingText == text)
        return;
    m_loadingText = text;
    if (m_loadingOpacity > 0.0)
        update();
}

QString AuGridWidget::loadingText() const
{
    return m_loadingText;
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

bool AuGridWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_activeEditor)
        return false;

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_activeEditorImpl && m_activeEditorImpl->wantsReturnForNewLine())
                return false;
            if (tryFinishCurrentEdit())
                setFocus();
            return true;
        case Qt::Key_Escape:
            clearEditError();
            closeEditor(false);
            setFocus();
            return true;
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            keyPressEvent(keyEvent);
            return true;
        default:
            // 校验失败期间按键：即时复查，通过则清除提示，不通过则刷新内容位置
            refreshEditError();
            break;
        }
    } else if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove) {
        // 悬停编辑框时确保错误提示可见并贴住编辑框下方
        if (m_editErrorTip && m_editErrorTip->isVisible()) {
            updateEditErrorTipGeometry();
            m_editErrorTip->raise();
        }
    } else if (event->type() == QEvent::FocusOut) {
        if (m_committing)
            return false;
        // 焦点仍在编辑器自身或其子控件内部时不算失焦：复合编辑器（如 AuSelect /
        // AuDatePicker 展开面板时把焦点移入内部行编辑框）内部挪焦点会向编辑器
        // 本体发 FocusOut——若按失焦提交，面板刚弹出编辑器就被提交销毁，
        // 表现为“面板只剩文本框”
        QWidget *focus = QApplication::focusWidget();
        if (focus && (focus == m_activeEditor || m_activeEditor->isAncestorOf(focus)))
            return false;
        // 编辑器自有弹层（QComboBox 弹层 / AuSelect、AuDatePicker 面板）被点击
        // 或激活时，Linux/Wayland 会把焦点交给弹层窗口或短暂置空，不算失焦，
        // 见 auEditorPopupActive
        if (QApplication::activePopupWidget() || auEditorPopupActive(m_activeEditor))
            return false;
        tryFinishCurrentEdit(true);
        return false;
    }

    return false;
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

void AuGridWidget::setDataAnimationEnabled(bool enabled)
{
    if (m_dataAnimationEnabled == enabled)
        return;
    m_dataAnimationEnabled = enabled;
    if (!enabled) {
        finishSortMoveAnimation();
        finishDataResetAnimation();
        m_dataResetPending = false;
        m_dataResetSnapshot = QImage();
    }
}

bool AuGridWidget::isDataAnimationEnabled() const
{
    return m_dataAnimationEnabled;
}

void AuGridWidget::setSortAnimationDuration(int ms)
{
    m_sortAnimationDuration = qMax(0, ms);
    if (m_sortAnimationDuration <= 0)
        finishSortMoveAnimation();
}

int AuGridWidget::sortAnimationDuration() const
{
    return m_sortAnimationDuration;
}

void AuGridWidget::setDataResetAnimationDuration(int ms)
{
    m_resetAnimationDuration = qMax(0, ms);
    if (m_resetAnimationDuration <= 0) {
        finishDataResetAnimation();
        m_dataResetPending = false;
        m_dataResetSnapshot = QImage();
    }
}

int AuGridWidget::dataResetAnimationDuration() const
{
    return m_resetAnimationDuration;
}

void AuGridWidget::captureLayoutSnapshot()
{
    if (!m_model) {
        m_animLastOrder.clear();
        m_animLastOffsets.clear();
        return;
    }

    if (m_rowOffsetsDirty)
        rebuildRowOffsets();

    const int count = m_model->rowCount();
    m_animLastOrder.resize(count);
    for (int i = 0; i < count; ++i)
        m_animLastOrder[i] = m_model->originalRowForVisibleRow(i);
    m_animLastOffsets = m_rowOffsets;
}

void AuGridWidget::captureRemovedRowGhosts(const QVector<int> &originalRows)
{
    m_removedRowGhosts.clear();
    m_removedOldVis.clear();
    m_rowRemovePending = false;
    if (!m_dataAnimationEnabled || m_loading || !isVisible() || bodyHeight() <= 0)
        return;
    if (isTreeMode())
        return;   // 树形模式不支持按行删除（模型层同样忽略），无消失动画
    if (m_animLastOrder.isEmpty() || m_animLastOffsets.size() != m_animLastOrder.size() + 1)
        return;

    // 与进行中的排序 / 位移动画互斥：先落地为终态，再捕获本次删除的幽灵带
    finishSortMoveAnimation();

    const int bodyTop = headerHeight();
    const int bodyHeightPx = bodyHeight();
    // 行偏移表为 body 相对坐标：视口可见带 = [m_yOffset, m_yOffset + bodyHeight)
    const int viewTop = m_yOffset;
    const int viewBottom = m_yOffset + bodyHeightPx;

    // 旧可见行号的完整被删集合（含视口外）：新→旧位置映射必须跳过全部被删行
    QVector<RemovedRowGhost> ghosts;
    for (int original : originalRows) {
        const int oldVis = m_animLastOrder.indexOf(original);
        if (oldVis < 0)
            continue;   // 行不可见（被过滤 / 折叠）：无外观可捕获，映射同样跳过
        m_removedOldVis.append(oldVis);
        const int y = m_animLastOffsets.at(oldVis);
        const int h = m_animLastOffsets.at(oldVis + 1) - y;
        if (y + h <= viewTop || y >= viewBottom)
            continue;   // 完全在视口外：无需幽灵，但参与位置映射
        RemovedRowGhost ghost;
        ghost.y = y;
        ghost.height = h;
        ghosts.append(ghost);
    }
    if (m_removedOldVis.isEmpty())
        return;

    std::sort(m_removedOldVis.begin(), m_removedOldVis.end());
    if (!ghosts.isEmpty()) {
        // 只截内容区宽度：grab 会把子控件（右侧竖向滚动条）一并渲染进来，
        // 若按整宽快照，每条幽灵带右缘都会带上一段滚动条像素，多行删除时
        // 叠在实时滚动条上呈现为阶梯状的粗竖线残影
        const int contentW = viewportWidthForColumns();
        if (contentW <= 0) {
            m_removedRowGhosts.clear();
            return;
        }
        // 整个 body 一次快照再裁出行带（此刻模型旧数据仍完整，grab 即旧行内容）
        const QImage bodySnapshot = grab(QRect(0, bodyTop, contentW, bodyHeightPx)).toImage();
        if (bodySnapshot.isNull()) {
            m_removedRowGhosts.clear();
            return;
        }
        const qreal dpr = bodySnapshot.devicePixelRatio() > 0 ? bodySnapshot.devicePixelRatio() : 1.0;
        for (RemovedRowGhost &ghost : ghosts) {
            // 快照第 p 像素行对应 body 坐标 viewTop + p；裁出可见带并记录其视口绘制 y
            const int bandTop = qMax(viewTop, ghost.y);
            const int cropTop = bandTop - viewTop;
            const int cropH = qMin(viewBottom, ghost.y + ghost.height) - bandTop;
            // copy(x, y, w, h)：行带是横向整条、纵向裁剪，勿将纵向偏移误传给 x
            ghost.image = bodySnapshot.copy(0, qRound(cropTop * dpr),
                                            qRound(contentW * dpr), qRound(cropH * dpr));
            ghost.y = bodyTop + cropTop;   // 视口坐标（裁剪过的带从可见处起画）
        }
        m_removedRowGhosts = ghosts;
    }
    m_rowRemovePending = true;
}

void AuGridWidget::startRowRemoveAnimation()
{
    m_rowRemovePending = false;
    if (!m_dataAnimationEnabled || m_loading || !isVisible())
        return;
    if (m_rowOffsetsDirty)
        rebuildRowOffsets();   // 删除后的新布局行偏移

    // 新可见行序列 = 旧可见行序列剔除被删行：由此得到每行旧 y → 新 y 的上滑映射。
    // 不能走排序 FLIP：删除使原始行号前移，按身份映射会把不同数据行错配到一起。
    const int oldCount = m_animLastOrder.size();
    const int newCount = m_model ? m_model->rowCount() : 0;
    if (oldCount == 0 || m_animLastOffsets.size() != oldCount + 1)
        return;

    m_dataAnimationEntries.clear();
    const int removedCount = m_removedOldVis.size();
    // 行偏移为 body 相对坐标：视口带 = [m_yOffset, m_yOffset + bodyHeight)
    const int viewTop = m_yOffset;
    const int viewBottom = m_yOffset + bodyHeight();
    const int bandMargin = rowHeight() * 4;   // 只动画视口邻域的行，远处行直接就位
    int removedIdx = 0;
    int newVis = 0;
    for (int oldVis = 0; oldVis < oldCount && newVis < newCount; ++oldVis) {
        if (removedIdx < removedCount && m_removedOldVis.at(removedIdx) == oldVis) {
            ++removedIdx;
            continue;   // 该旧行被删除：无新载体，由幽灵带负责淡出
        }
        const int oldY = m_animLastOffsets.at(oldVis);
        const int newY = m_rowOffsets.at(newVis);
        const bool oldNear = (oldY >= viewTop - bandMargin && oldY <= viewBottom + bandMargin);
        const bool newNear = (newY >= viewTop - bandMargin && newY <= viewBottom + bandMargin);
        if (oldY != newY && (oldNear || newNear)) {
            DataMoveAnimationEntry e;
            e.visibleRow = newVis;
            e.startOffset = oldY;
            e.endOffset = newY;
            m_dataAnimationEntries.append(e);
        }
        ++newVis;
    }

    if (m_dataAnimationEntries.isEmpty() && m_removedRowGhosts.isEmpty())
        return;
    if (m_sortAnimationDuration <= 0) {
        finishSortMoveAnimation();   // 时长为 0：不播动画，直接呈现终态
        return;
    }

    m_dataAnimationProgress = 0.0;
    m_dataAnimation = new QVariantAnimation(this);
    m_dataAnimation->setStartValue(0.0);
    m_dataAnimation->setEndValue(1.0);
    m_dataAnimation->setDuration(m_sortAnimationDuration);
    m_dataAnimation->setEasingCurve(QEasingCurve::OutCubic);   // 与排序动画一致的快出缓停手感
    connect(m_dataAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_dataAnimationProgress = qBound(0.0, value.toReal(), 1.0);
        update();
    });
    connect(m_dataAnimation, &QVariantAnimation::finished, this, [this]() {
        finishSortMoveAnimation();
    });
    m_dataAnimation->start();
}

void AuGridWidget::startSortMoveAnimation()
{
    finishSortMoveAnimation();
    if (!m_dataAnimationEnabled || !m_model || m_loading || !isVisible())
        return;
    if (isTreeMode())
        return;   // 树形结构变化由展开 / 折叠动画负责，避免两套动画叠加

    const int oldCount = m_animLastOrder.size();
    const int newCount = m_model->rowCount();
    if (oldCount == 0 || newCount == 0 || m_animLastOffsets.size() != oldCount + 1)
        return;

    // 行序未变化则无位移可言
    bool orderChanged = (newCount != oldCount);
    for (int i = 0; !orderChanged && i < newCount; ++i) {
        if (m_animLastOrder.at(i) != m_model->originalRowForVisibleRow(i))
            orderChanged = true;
    }
    if (!orderChanged)
        return;

    if (m_rowOffsetsDirty)
        rebuildRowOffsets();   // 新布局的行高 / 偏移（WordWrap 行高随数据位置变化）

    QHash<int, int> oldVisibleOf;
    oldVisibleOf.reserve(oldCount * 2);
    for (int i = 0; i < oldCount; ++i)
        oldVisibleOf.insert(m_animLastOrder.at(i), i);

    const int bodyTop = headerHeight();
    const int bodyBottom = bodyTop + bodyHeight();
    const int outsideTop = bodyTop - rowHeight();   // 视口上沿外一行（完全不可见处）
    const int outsideBottom = bodyBottom;
    const int rowH = rowHeight();

    // 待入队的退场 / 入场行：visibleRow = 新可见行号，inBandY = 视口带内的真实位置，
    // outY = 视口带外的真实位置（用于排序定队列次序）
    struct TrainItem { int visibleRow; int inBandY; int outY; };
    QVector<TrainItem> exitUp, exitDown, enterFromTop, enterFromBottom;
    m_dataAnimationEntries.clear();

    const int totalOriginals = qMax(oldCount, m_model->originalRowCount());
    for (int original = 0; original < totalOriginals; ++original) {
        const int newVis = (original < m_model->originalRowCount())
                               ? m_model->visibleRowForOriginalRow(original)
                               : -1;
        if (newVis < 0)
            continue;   // 移出可见集合（被过滤等）：无绘制载体，瞬时消失

        const int newY = m_rowOffsets.at(newVis);
        const int oldVis = oldVisibleOf.value(original, -1);
        const int oldY = (oldVis >= 0) ? m_animLastOffsets.at(oldVis) : -1;

        const bool oldInBand = (oldVis >= 0 && oldY >= outsideTop && oldY <= outsideBottom);
        const bool newInBand = (newY >= outsideTop && newY <= outsideBottom);

        if (oldInBand && newInBand) {
            // 新旧位置都在视口带内：直接从旧位置滑到新位置
            if (oldY == newY)
                continue;
            DataMoveAnimationEntry e;
            e.visibleRow = newVis;
            e.startOffset = oldY;
            e.endOffset = newY;
            m_dataAnimationEntries.append(e);
        } else if (oldInBand) {
            // 可视行移出视口：向新位置方向退场（队列在下方统一排定）
            if (newY < bodyTop)
                exitUp.append({ newVis, oldY, newY });
            else
                exitDown.append({ newVis, oldY, newY });
        } else if (newInBand) {
            // 视口外的行变为可见：从旧位置所在方向入场（oldY < 0 = 新增行，自下方进入）
            if (oldVis >= 0 && oldY < bodyTop)
                enterFromTop.append({ newVis, newY, oldY });
            else
                enterFromBottom.append({ newVis, newY, oldVis >= 0 ? oldY : outsideBottom });
        }
        // 起止都在视口带外（跨视口移动 / 始终不可见）：不参与动画，保持不可见
    }

    // 退场 / 入场按“距视口边缘的远近”排成间距为一行的队列：
    // 离边缘最近的行贴边出入，其余依次向视口外排开——保持行间相对间距，
    // 避免多行同时叠在同一个边缘点上形成色块。
    const auto byOutDesc = [](const TrainItem &a, const TrainItem &b) { return a.outY > b.outY; };
    const auto byOutAsc = [](const TrainItem &a, const TrainItem &b) { return a.outY < b.outY; };

    std::sort(exitUp.begin(), exitUp.end(), byOutDesc);
    for (int k = 0; k < exitUp.size(); ++k) {
        DataMoveAnimationEntry e;
        e.visibleRow = exitUp.at(k).visibleRow;
        e.startOffset = exitUp.at(k).inBandY;
        e.endOffset = outsideTop - k * rowH;
        m_dataAnimationEntries.append(e);
    }
    std::sort(exitDown.begin(), exitDown.end(), byOutAsc);
    for (int k = 0; k < exitDown.size(); ++k) {
        DataMoveAnimationEntry e;
        e.visibleRow = exitDown.at(k).visibleRow;
        e.startOffset = exitDown.at(k).inBandY;
        e.endOffset = outsideBottom + k * rowH;
        m_dataAnimationEntries.append(e);
    }
    std::sort(enterFromTop.begin(), enterFromTop.end(), byOutDesc);
    for (int k = 0; k < enterFromTop.size(); ++k) {
        DataMoveAnimationEntry e;
        e.visibleRow = enterFromTop.at(k).visibleRow;
        e.startOffset = outsideTop - k * rowH;
        e.endOffset = enterFromTop.at(k).inBandY;
        m_dataAnimationEntries.append(e);
    }
    std::sort(enterFromBottom.begin(), enterFromBottom.end(), byOutAsc);
    for (int k = 0; k < enterFromBottom.size(); ++k) {
        DataMoveAnimationEntry e;
        e.visibleRow = enterFromBottom.at(k).visibleRow;
        e.startOffset = outsideBottom + k * rowH;
        e.endOffset = enterFromBottom.at(k).inBandY;
        m_dataAnimationEntries.append(e);
    }

    if (m_dataAnimationEntries.isEmpty() && m_removedRowGhosts.isEmpty())
        return;
    if (m_sortAnimationDuration <= 0) {
        finishSortMoveAnimation();   // 时长为 0：不播动画，清理条目直接呈现终态
        return;
    }

    m_dataAnimationProgress = 0.0;
    m_dataAnimation = new QVariantAnimation(this);
    m_dataAnimation->setStartValue(0.0);
    m_dataAnimation->setEndValue(1.0);
    m_dataAnimation->setDuration(m_sortAnimationDuration);
    m_dataAnimation->setEasingCurve(QEasingCurve::OutCubic);   // 快出缓停，接近 web 端手感
    connect(m_dataAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_dataAnimationProgress = qBound(0.0, value.toReal(), 1.0);
        update();
    });
    connect(m_dataAnimation, &QVariantAnimation::finished, this, [this]() {
        finishSortMoveAnimation();
    });
    m_dataAnimation->start();
}

void AuGridWidget::finishSortMoveAnimation()
{
    if (m_dataAnimation) {
        m_dataAnimation->stop();
        delete m_dataAnimation;
        m_dataAnimation = nullptr;
    }
    if (m_dataAnimationEntries.isEmpty() && m_removedRowGhosts.isEmpty()
        && m_dataAnimationProgress == 0.0)
        return;
    m_dataAnimationEntries.clear();
    m_removedRowGhosts.clear();
    m_removedOldVis.clear();
    m_dataAnimationProgress = 0.0;
    update();
}

void AuGridWidget::startDataResetAnimation()
{
    m_dataResetPending = false;
    if (!m_dataAnimationEnabled || m_loading || !isVisible() || m_dataResetSnapshot.isNull())
        return;
    if (!m_model || m_model->rowCount() <= 0)
        return;
    if (m_resetAnimationDuration <= 0) {
        m_dataResetSnapshot = QImage();
        return;   // 时长为 0：不播动画，直接呈现新数据
    }

    finishSortMoveAnimation();
    finishDataResetAnimation();

    m_dataResetSlideHeight = bodyHeight();
    m_dataResetProgress = 0.0;
    m_dataResetAnimating = true;
    m_dataResetAnimation = new QVariantAnimation(this);
    m_dataResetAnimation->setStartValue(0.0);
    m_dataResetAnimation->setEndValue(1.0);
    m_dataResetAnimation->setDuration(m_resetAnimationDuration);
    // 总时长前半为离场、后半为入场，两段各自在 rowY / 绘制处施加 OutCubic 缓动，
    // 动画驱动本身保持线性
    m_dataResetAnimation->setEasingCurve(QEasingCurve::Linear);
    connect(m_dataResetAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_dataResetProgress = qBound(0.0, value.toReal(), 1.0);
        update();
    });
    connect(m_dataResetAnimation, &QVariantAnimation::finished, this, [this]() {
        finishDataResetAnimation();
    });
    m_dataResetAnimation->start();
}

void AuGridWidget::finishDataResetAnimation()
{
    if (m_dataResetAnimation) {
        m_dataResetAnimation->stop();
        delete m_dataResetAnimation;
        m_dataResetAnimation = nullptr;
    }
    if (!m_dataResetAnimating)
        return;
    m_dataResetAnimating = false;
    m_dataResetProgress = 0.0;
    m_dataResetSlideHeight = 0;
    m_dataResetSnapshot = QImage();
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
    if (m_columnDragOffsetAnimation) {
        m_columnDragOffsetAnimation->stop();
        delete m_columnDragOffsetAnimation;
        m_columnDragOffsetAnimation = nullptr;
    }
    m_columnDragAnimationEntries.clear();
    m_columnDragAnimationOldOrder.clear();
    m_columnDragAnimationOldOffsets.clear();
    m_columnDragAnimationOldFields.clear();
    m_columnDragAnimationProgress = 0.0;
    update();
}
