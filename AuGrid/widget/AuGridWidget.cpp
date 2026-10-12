// ============================================================================
// AuGridWidget 核心实现：构造 / 析构、模型绑定、样式与尺寸模式、滚动条、
// 行高与列偏移布局、单元格几何、合并区域（span）存储与查询。
// 其余实现按功能域拆分：
//   AuGridWidgetPaint.cpp       绘制（paintEvent 与各 drawXxx）
//   AuGridWidgetInteraction.cpp 鼠标 / 键盘 / 悬停 / 选中 / 滚动定位
//   AuGridWidgetEditing.cpp     单元格编辑
//   AuGridWidgetColumns.cpp     列（停靠 / 显隐 / 宽度 / AutoFit / 列拖拽）
//   AuGridWidgetRows.cpp        行（勾选 / 栴形 / 排序 / 过滤 / 行拖拽）
//   AuGridWidgetAnimation.cpp   数据变化动画与加载遮罩
// ============================================================================

#include "AuGridWidget.h"
#include "AuScrollBar.h"
#include "AuDefaultCellRenderer.h"
#include "AuCheckBoxCellRenderer.h"
#include "AuButtonCellRenderer.h"
#include "AuDefaultCellEditor.h"
#include "AuGridWidget_p.h"

#include <QApplication>
#include <QLabel>
#include <QPainterPath>
#include <QTimer>
#include <QToolTip>
#include <QVariantAnimation>

constexpr int AuGridWidget::kResizeHandleTolerance;
// OutCubic 缓动：快出缓停（数据变化动画各阶段使用，接近 web 端过渡手感）
static qreal auOutCubic(qreal t)
{
    t = qBound(0.0, t, 1.0);
    const qreal u = 1.0 - t;
    return 1.0 - u * u * u;
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
    , m_cellRendererPressCell(-1, -1)
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

    // 内置按钮 / 链接渲染器：setCellRenderer("button" / "link") 即用（零配置走
    // 单元格值解析）；同名注册可覆盖内置实例
    registerCellRenderer(QStringLiteral("button"),
                         QSharedPointer<AuCellRenderer>(new AuButtonCellRenderer(AuCellButton::Type::Button)));
    registerCellRenderer(QStringLiteral("link"),
                         QSharedPointer<AuCellRenderer>(new AuButtonCellRenderer(AuCellButton::Type::Link)));
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

AuGridStyle *AuGridWidget::style() const
{
    return m_style;
}

void AuGridWidget::resizeEvent(QResizeEvent * /*event*/)
{
    rebuildColumnOffsets();
    updateScrollBars();
    captureLayoutSnapshot();   // Flex 列宽随视口变化，动画基线的行偏移保持新鲜
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

