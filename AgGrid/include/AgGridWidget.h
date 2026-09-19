#ifndef AGGRIDWIDGET_H
#define AGGRIDWIDGET_H

#include "AgGrid_global.h"
#include "AgGridModel.h"
#include "AgGridStyle.h"
#include "AgCellRenderer.h"
#include "AgHeaderRenderer.h"
#include "AgCellEditor.h"
#include "AgMultiLineCellEditor.h"
#include "AgCellEditValidator.h"

#include <QWidget>
#include <QHash>
#include <QSet>
#include <QImage>
#include <QSharedPointer>
#include <QPainterPath>
#include <QVariantAnimation>
#include <functional>

class AgGridScrollBar;
class QLabel;
class QTimer;

/**
 * @brief 基于 QPainter 自绘的高性能数据表格控件（参考 AG Grid 交互风格）。
 *
 * 核心特性：
 *  - 虚拟化渲染：仅绘制可视区及缓冲区内的行，支持十万行级数据；
 *  - 多级表头（列定义树）、列停靠（左/右 Pin）、列拖拽排序与显隐动画；
 *  - 单列排序 / 多规则逻辑排序 / 自定义过滤回调；
 *  - 行选中模式（单选 / 多选 / 不可选中）：多选支持点击切换与 Ctrl+点击累积；
 *  - 单元格编辑（可插拔编辑器）+ 提交校验器；
 *  - 单元格合并（setSpan，语义参考 QTableView::setSpan）；
 *  - 内置行号列、复选框列、行拖拽手柄列，支持行拖拽重排；
 *  - 多表格滚动 / 列宽同步（配合 AgGridSyncGroup）；
 *  - 内置加载遮罩（setLoading）：数据请求等异步场景的蒙层 + 旋转圆环 + 文案，淡入淡出；
 *  - 样式系统（全局样式 / 独立样式 / 按行列上下文，小中大三种尺寸模式）。
 *
 * 坐标体系（重要）：
 *  - row：当前“可见行”索引（过滤、排序、拖拽重排之后的顺序）；
 *    勾选状态与行拖拽信号使用“原始行”索引，跨排序过滤稳定；
 *  - column：叶子列索引（列定义树展开后从左到右的顺序）；
 *  - 内置辅助列使用负值编号：-1 复选框列、-2 行拖拽手柄列、-3 行号列。
 *
 * 基本用法：
 * @code
 *   AgGridModel *model = new AgGridModel(this);
 *   model->setColumnDefs({...});
 *   model->setRowData(rows);
 *   AgGridWidget *grid = new AgGridWidget(this);
 *   grid->setModel(model);
 * @endcode
 *
 * 所有未显式 setStyle() 的控件自动使用全局样式（AgGridStyle::globalStyle()）。
 */
class AGGRID_EXPORT AgGridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AgGridWidget(QWidget *parent = nullptr);
    ~AgGridWidget();

    /// 绑定数据模型（控件不获得模型所有权）。切换模型会重置滚动、选择、勾选、
    /// 合并区域与列隐藏状态。内置信号转发：数据/布局变化自动重绘。
    void setModel(AgGridModel *model);
    /// 当前绑定的模型；未绑定为 nullptr。
    AgGridModel *model() const;

    /// 表头总高度（层级数 × 单级高度）。
    int headerHeight() const;
    /// 单级表头高度（样式键 HeaderHeight）。
    int headerLevelHeight() const;
    /// 默认行高（样式键 RowHeight）。启用自动换行的行可能高于该值。
    int rowHeight() const;

    /// 当前选中行（可见行号）；多选模式为最后点击的行（锚点）；无选中为 -1。
    int selectedRow() const;
    /// 当前选中列（叶子列索引）；无选中为 -1。
    int selectedColumn() const;

    /// 行选中模式。
    enum class SelectionMode {
        Single,    ///< 单选（默认）：点击选中一行；Ctrl+点击已选中行可取消选中
        Multiple,  ///< 多选：点击 / Ctrl+点击均切换该行选中（点击即选中，再点击取消，可累积多行）
        None       ///< 不可选中：鼠标 / 键盘 / 选中 API 均不改变选中状态
    };
    Q_ENUM(SelectionMode)

    /// 设置行选中模式（默认单选）。切换为不可选中时清空现有选中；
    /// 切换为单选时仅保留最后点击的行。
    void setSelectionMode(SelectionMode mode);
    /// 当前行选中模式。
    SelectionMode selectionMode() const;

    /// 可视区上下额外渲染的缓冲行数（默认 3）：滚动更平滑，但过大影响绘制性能。
    int visibleRowBuffer() const;
    void setVisibleRowBuffer(int buffer);

    /// 设置控件独立样式（控件接管所有权并负责销毁）。
    /// 传入 nullptr 恢复为跟随全局样式 AgGridStyle::globalStyle()。
    void setStyle(AgGridStyle *style);
    /// 当前生效的样式。
    AgGridStyle *style() const;

    /// 尺寸模式（转发 AgGridStyle::SizeMode）：小 / 中 / 大三档密度。
    using SizeMode = AgGridStyle::SizeMode;
    /// 切换尺寸模式：内部克隆当前样式并调整高度、字体、内边距等。
    void setSizeMode(SizeMode mode);
    SizeMode sizeMode() const;

    /// 注册单元格渲染器，供 AgColumnDef::setCellRenderer(name) 按名引用。
    /// 列未指定名称时使用内置默认文本渲染器；checkbox 列强制使用内置复选框渲染器。
    void registerCellRenderer(const QString &name, QSharedPointer<AgCellRenderer> renderer);
    /// 注册单元格编辑器，供 AgColumnDef::setCellEditor(name) 按名引用。
    /// 列未指定名称时使用内置单行编辑器。
    void registerCellEditor(const QString &name, QSharedPointer<AgCellEditor> editor);
    /// 注册表头渲染器，供 AgColumnDef::setHeaderRenderer(name) 按名引用。
    /// 列未指定名称时绘制标准表头（背景 + 文字 + 排序箭头）。
    void registerHeaderRenderer(const QString &name, QSharedPointer<AgHeaderRenderer> renderer);

    /// 设置列宽（像素），转发给模型并触发重排。column 为叶子列索引。
    void setColumnWidth(int column, int width);
    /// 按列 field 名设置列宽（可指向叶子列或分组列，分组列作用于其全部叶子列）。
    void setColumnWidth(const QString &field, int width);

    /// 按表头与可视内容自适应该列宽度。
    void autoSizeColumn(int column);
    /// 按内容自适应全部列宽。
    void autoSizeAllColumns();

    /// 程序化开始编辑单元格（等价双击）。返回是否成功进入编辑。
    bool editCell(int row, int column);
    /// 开始编辑并注入初始文本（对应键盘直接输入字符的场景）。
    bool editCell(int row, int column, const QString &initialText);
    /// 当前是否有编辑器打开。
    bool isEditing() const;
    /// 关闭编辑器。commit=true 时取值提交（经校验器裁决）；false 直接放弃。
    void closeEditor(bool commit = true);

    /// 滚动定位的停靠方式。
    enum class ScrollHint {
        EnsureVisible,   ///< 仅在不可见时滚动到刚好可见
        PositionTop,     ///< 顶端对齐
        PositionCenter,  ///< 居中
        PositionBottom   ///< 底端对齐
    };
    Q_ENUM(ScrollHint)

    /// 垂直滚动定位到某行。
    void scrollToRow(int row, ScrollHint hint = ScrollHint::EnsureVisible);
    /// 水平滚动定位到某列。
    void scrollToColumn(int column, ScrollHint hint = ScrollHint::EnsureVisible);
    /// 定位到单元格；合并单元格按整个合并区域计算可视范围。
    void scrollToCell(int row, int column, ScrollHint hint = ScrollHint::EnsureVisible);

    /// 垂直滚动像素偏移（可直接读写，用于多表格同步等外部控制）。
    int verticalOffset() const;
    /// 水平滚动像素偏移。
    int horizontalOffset() const;
    void setVerticalOffset(int value);
    void setHorizontalOffset(int value);

    /// 停靠列到左侧 / 右侧（column 为叶子列索引）。
    void pinColumn(int column, AgColumnDef::PinMode mode);
    /// 按 field 停靠列（可指向分组列，整组停靠）。
    void pinColumn(const QString &field, AgColumnDef::PinMode mode);
    /// 查询列停靠状态。
    AgColumnDef::PinMode columnPinMode(int column) const;

    // 列显示/隐藏：column 为叶子列索引（最后一个层级的顺序），field 可指向叶子列或分组列
    /// 隐藏 / 显示列（带位移动画）。隐藏列宽度按 0 参与布局，索引不变。
    void setColumnVisible(int column, bool visible);
    void setColumnVisible(const QString &field, bool visible);
    /// 查询列可见性。
    bool isColumnVisible(int column) const;

    /// 选中某行（保留当前列选择）。多选模式下替换为仅选中该行；不可选中模式下无效。
    void selectRow(int row);
    /// 选中单元格；落在合并覆盖格上时自动映射到合并锚点。多选模式下替换为仅选中该行；
    /// 不可选中模式下无效。
    void selectCell(int row, int column);
    /// 多选模式：整体替换选中行集合（可见行号）；单选模式等价 selectRow(其中最小行号)；
    /// 不可选中模式下无效。不改变滚动位置。
    void selectRows(const QSet<int> &rows);
    /// 取消选中。
    void clearSelection();
    /// 全部选中行（可见行号）；单选模式返回当前行；不可选中模式返回空集。
    QSet<int> selectedRows() const;
    /// 全部选中行（原始行号，跨排序过滤稳定，持久化推荐使用）。
    QSet<int> selectedOriginalRows() const;
    /// 某可见行是否处于选中状态（多选模式包含 Ctrl 累积选中的行）。
    bool isRowSelected(int row) const;
    /// 布局变化（排序/过滤/移动行）后按记录的原始行恢复选中（框架内部自动调用）。
    void syncSelectionAfterLayoutChange();

    // 单元格合并：参考 QTableView::setSpan。(row, column) 为合并区域左上角锚点，
    // 合并后锚点按整个合并区域绘制，被覆盖单元格不再绘制其数据；
    // 点击/悬停/编辑/键盘导航均落在锚点单元格上。
    // rowSpanCount/columnSpanCount 传 1x1 表示清除该处合并；新合并会替换与其重叠的旧合并。
    // 合并区域不会跨越列停靠区域边界；行索引为当前可见行（排序/过滤后不自动跟随数据）。
    /// 设置合并区域（锚点为左上角，跨度超出模型边界自动截断、跨停靠区域自动截断）。
    void setSpan(int row, int column, int rowSpanCount, int columnSpanCount);
    /// 清除该单元格所在的合并区域。
    void clearSpan(int row, int column);
    /// 清除全部合并区域。
    void clearSpans();
    /// 该单元格所在合并区域的行跨度（不在合并内返回 1）。
    int rowSpan(int row, int column) const;
    /// 该单元格所在合并区域的列跨度（不在合并内返回 1）。
    int columnSpan(int row, int column) const;
    /// 是否存在任何合并区域。
    bool hasSpans() const;

    // 行号列
    /// 显示 / 隐藏内置行号列（显示“原始行号 + 1”，右对齐）。
    void setRowNumberColumnVisible(bool visible);
    bool isRowNumberColumnVisible() const;

    // 复选框列
    /// 显示 / 隐藏内置复选框列（表头复选框支持全选 / 半选）；隐藏时清空全部勾选。
    void setCheckboxColumnVisible(bool visible);
    bool isCheckboxColumnVisible() const;

    /// 某可见行是否勾选。
    bool isRowChecked(int row) const;
    /// 某可见行是否处于半选状态（树形模式由子行勾选状态聚合得出）。
    bool isRowPartiallyChecked(int row) const;
    /// 设置某可见行的勾选状态。
    /// 树形模式下级联：勾选父行同时勾选全部子行，父行状态由子行聚合（半选）。
    /// 折叠不可见的子行同样参与级联；行可选回调在树形模式接收原始行号。
    void setRowChecked(int row, bool checked);
    /// 该行是否允许勾选（默认全部允许，可由回调裁决）。
    bool isRowCheckable(int row) const;
    /// 自定义行可选回调：返回 false 的行复选框置灰不可勾选。
    void setRowCheckableCallback(const std::function<bool(const AgGridModel *, int)> &callback);

    /// 全选所有行（与表头复选框全选等效）。
    void selectAllRows();
    /// 清空全部勾选。
    void clearAllCheckedRows();

    /// 勾选行集合（可见行号）。
    QSet<int> checkedRows() const;
    /// 勾选行集合（原始行号；跨排序过滤稳定，持久化推荐使用）。
    QSet<int> checkedOriginalRows() const;

    /// 当前排序列（叶子列索引；-1 表示未排序）。
    int sortedColumn() const;
    /// 当前排序方向。
    Qt::SortOrder sortOrder() const;

    /// 表头点击排序总开关（默认关闭）。仅对列定义 sortable=true 的列生效。
    void setSortingEnabled(bool enabled);
    bool isSortingEnabled() const;

    /// 设置多规则逻辑排序（按顺序依次比较）。
    void setSortRules(const QList<AgSortRule> &rules);
    /// 当前排序规则。
    QList<AgSortRule> sortRules() const;
    /// 清除排序（恢复原始顺序）。
    void clearSort();

    // 过滤
    /// 设置过滤回调：对每行（QVariantMap）返回 true 表示保留。
    void setFilterCallback(const std::function<bool(const QVariantMap &row)> &callback);
    /// 清除过滤。
    void clearFilter();
    /// 是否设置了过滤回调。
    bool isFiltered() const;

    /// 设置编辑提交校验器（对全部编辑生效，详见 AgCellEditValidator）。
    void setCellEditValidator(QSharedPointer<AgCellEditValidator> validator);
    /// 动态可编辑回调：在列 editable=true 的基础上按 (row, column) 最终裁决是否可编辑。
    void setCellEditableCallback(const std::function<bool(const AgGridModel *, int, int)> &callback);

    /// 单元格 tooltip 内容回调：列 tooltipEnabled=true 且内容超出列宽被裁剪时，
    /// 以返回值作为 tooltip 文本（支持 HTML 富文本）；返回空串表示该格不显示。
    /// 未设置回调时显示单元格文本本身。截断判定不受回调影响。
    void setCellTooltipTextCallback(const std::function<QString(const AgGridModel *, int, int, const QString &)> &callback);

    // 网格线
    /// 显示 / 隐藏数据单元格左右（列方向）边框线（默认显示）。隐藏后 body 区的数据列
    /// （含合并单元格）只保留上下（行方向）边框线；表格自带的行号列 / 复选框列 /
    /// 拖拽手柄列仍绘制完整边框，表头边框与停靠区分隔线也不受影响。
    void setVerticalGridLinesVisible(bool visible);
    bool isVerticalGridLinesVisible() const;

    // 行拖拽排序
    /// 开启行拖拽重排（显示拖拽手柄列）。
    void setRowDragEnabled(bool enabled);
    bool isRowDragEnabled() const;
    /// 自定义拖动时浮动提示文字（默认显示“共 N 行”）。
    void setDragPreviewTextCallback(const std::function<QString(const AgGridModel *, const QVector<int> &)> &callback);

    // 加载遮罩
    /// 显示 / 隐藏加载遮罩（半透明蒙层 + 旋转圆环 + 文案，带淡入淡出）。
    /// 用于数据请求等异步场景，由调用方控制时机：发起请求前 setLoading(true)，
    /// 数据返回后 setLoading(false)。
    /// 显示期间鼠标 / 键盘交互被屏蔽、滚动条隐藏、打开中的编辑器自动关闭（不提交）。
    void setLoading(bool loading);
    /// 加载遮罩是否处于显示状态（setLoading 设置的状态；淡出过渡中已返回 false）。
    bool isLoading() const;
    /// 设置加载文字（显示在圆环下方）。传空字符串只显示圆环；
    /// 从未设置时使用样式键 LoadingText（默认“加载中…”）。
    void setLoadingText(const QString &text);
    /// 当前加载文字；null 表示跟随样式键 LoadingText。
    QString loadingText() const;

    // 数据变化动画
    /// 数据变化动画总开关（默认开启），覆盖两类动画：
    /// 排序 / 过滤的行位移动画（行从旧布局位置移向新位置，出视口 / 进视口按方向队列出入），
    /// 以及重设数据（setRowData / setTreeData / clear）的切换动画
    /// （旧内容整体上移出场、新内容自视口下方移入）。时长见
    /// setSortAnimationDuration() / setDataResetAnimationDuration()。
    void setDataAnimationEnabled(bool enabled);
    bool isDataAnimationEnabled() const;
    /// 排序 / 过滤位移动画时长（毫秒，默认 100）。设为 0 关闭位移动画（立即生效）。
    void setSortAnimationDuration(int ms);
    int sortAnimationDuration() const;
    /// 重设数据切换动画时长（毫秒，默认 200）。设为 0 关闭切换动画（立即生效）。
    void setDataResetAnimationDuration(int ms);
    int dataResetAnimationDuration() const;

    // 树形表格（模型经 setTreeData 载入层级数据后自动生效）
    /// 当前是否处于树形模式。
    bool isTreeMode() const;
    /// 展开或折叠某行（row 为可见行号；叶子行无效果）。
    /// 走模型 layoutChanged：视图自动刷新，合并区域清空、选中按原始行恢复。
    void setRowExpanded(int row, bool expanded);
    /// 行当前是否展开（row 为可见行号；叶子行 / 非树形模式返回 false）。
    bool isRowExpanded(int row) const;
    /// 行是否有子行（决定是否显示展开箭头；支持双击 / 键盘切换）。
    bool rowHasChildren(int row) const;
    /// 行的层级（根 = 0；非树形模式返回 0）。
    int rowDepth(int row) const;
    /// 全部展开。
    void expandAll();
    /// 全部折叠。
    void collapseAll();
    /// 展开深度 ≤ depth 的所有行（可见行到达深度 depth + 1）。
    void expandToDepth(int depth);
    /// 折叠深度 ≥ depth 的所有行（可见行到达深度 depth）。
    void collapseToDepth(int depth);

signals:
    /// 单元格被左键单击（row 可能为内置辅助列编号；合并格返回锚点坐标）。
    void cellClicked(int row, int column);
    /// 单元格被双击。
    void cellDoubleClicked(int row, int column);
    /// 悬停单元格发生变化。
    void cellHovered(int row, int column);
    /// 编辑开始（编辑器已创建并显示）。
    void cellEditingStarted(int row, int column);
    /// 编辑结束（提交或取消都会发出）。
    void cellEditingFinished(int row, int column);
    /// 编辑提交成功且通过校验，模型已更新。
    void cellValueChanged(int row, int column, const QVariant &value);
    /// 选中状态变化（含清除）。
    void selectionChanged();

    // 复选框列
    /// 复选框列某行勾选状态变化（row 为可见行号）。
    void rowCheckedChanged(int row, bool checked);
    /// 勾选集合整体变化（全选 / 清空 / 行拖拽移动带勾选行等）。
    void checkedStateChanged();

    // 数据列内置复选框
    /// 数据列（columnDef.isCheckbox()）的复选框值被点击 / 空格切换。
    void cellCheckStateChanged(int row, int column, bool checked);

    /// 排序变化（点击表头 / setSortRules / clearSort）。
    void sortChanged(int column, Qt::SortOrder order);
    /// 过滤状态变化。
    void filterChanged();

    // 滚动偏移变化（拖滚动条、滚轮、键盘、程序滚动等所有来源）
    void verticalScrollChanged(int value);
    void horizontalScrollChanged(int value);

    // 列宽变化（模型层转发）
    void columnWidthsChanged();

    /// 单元格右键（globalPos 为全局坐标，可直接用于 QMenu::exec）。
    void cellRightClicked(int row, int column, const QPoint &globalPos);
    /// 表头右键（column 为该节点首个叶子列；level 为 1 起始层级）。
    void headerRightClicked(int column, int level, const QPoint &globalPos);
    /// 行拖拽重排完成（原始行号；targetOriginalIndex 为插入目标位置）。
    void rowsDragged(const QVector<int> &originalRows, int targetOriginalIndex);

    // 树形表格
    /// 树形行展开状态变化（row 为可见行号；expandAll 等批量操作不逐行发送）。
    void rowExpandedChanged(int row, bool expanded);

protected:
    /// 绘制流程：背景 → 圆角面板 → 表头 → 行背景 → 各区域行/网格线 →
    /// 辅助列 → 拖拽覆盖层 → 焦点框 → 区域分隔线 → 底部收尾线 → 外边框 → 加载遮罩。
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class AgGridStyle;

    /// 列的三种水平区域：左停靠 / 中间滚动体 / 右停靠。
    enum class ColumnRegion {
        LeftPinned,
        Body,
        RightPinned
    };

    // 单元格合并区域（存储与有效范围均为可见行/叶子列坐标）
    struct CellSpan {
        int anchorRow = -1;      ///< 锚点（左上角）行；-1 表示无效
        int anchorColumn = -1;   ///< 锚点（左上角）列
        int rowCount = 1;        ///< 行跨度（存储值可能超出当前模型，使用前经 effectiveSpan 裁剪）
        int columnCount = 1;     ///< 列跨度

        bool isValid() const { return anchorRow >= 0; }
        int bottomRow() const { return anchorRow + rowCount - 1; }
        int rightColumn() const { return anchorColumn + columnCount - 1; }
        bool contains(int row, int column) const
        {
            return row >= anchorRow && row <= bottomRow()
                && column >= anchorColumn && column <= rightColumn();
        }
    };

    /// (row, column) → 64 位哈希键。
    static qint64 spanCellKey(int row, int column);
    /// 把存储的合并裁剪为当前有效范围（模型边界 + 锚点所在停靠区域），越界即无效。
    CellSpan effectiveSpan(const CellSpan &span) const;
    /// 查找包含 (row, column) 的有效合并区域；不在任何合并内时 isValid() 为 false。
    CellSpan spanAt(int row, int column) const;
    /// (row, column) 是否为某合并的被覆盖格（非锚点）。
    bool isSpanCoveredCell(int row, int column) const;
    /// 合并区域总宽（像素，逐列累加，不触发行高缓存）。
    int spanWidth(const CellSpan &span) const;
    /// 合并区域的可视矩形（锚点左上角，宽高为跨行跨列累加）。
    QRect spanRect(const CellSpan &span) const;
    /// 按锚点键移除一个合并及其覆盖索引。
    void removeSpan(qint64 anchorKey);
    /// 移除与指定矩形重叠的全部合并（setSpan 替换旧合并用）。
    void removeOverlappingSpans(int topRow, int leftColumn, int bottomRow, int rightColumn);
    /// 剔除锚点已越出当前模型范围的合并。
    void pruneSpans();

    void connectModel();
    void disconnectModel();
    void updateScrollRange();
    /// 依据内容宽度/高度与视口大小更新滚动条显隐与量程。
    void updateScrollBars();
    void updateScrollBarsCompute();
    /// 根据列宽、停靠、隐藏状态重建列偏移表与列区域分组。
    void rebuildColumnOffsets();
    /// 把样式应用到滚动条（颜色、粗细）。
    void applyStyleToScrollBars();

    int totalContentWidth() const;
    int totalContentHeight() const;
    int bodyHeight() const;

    /// 屏幕位置 → 单元格；合并覆盖格映射到锚点；辅助列返回负值编号；无效返回 (-1, -1)。
    QPoint cellAtPosition(const QPoint &pos) const;
    /// 单元格可视矩形；合并单元格（含被覆盖格）返回整个合并区域矩形。
    QRect cellRect(int row, int column) const;
    int columnX(int column) const;
    int baseColumnX(int column) const;
    int columnWidth(int column) const;
    int rowY(int row) const;
    int rowHeight(int row) const;

    bool isColumnHidden(int column) const;
    ColumnRegion columnRegion(int column) const;

    QPair<int, int> visibleColumnRange() const;
    QPair<int, int> visibleRowRange() const;
    int viewportWidthForColumns() const;

    int resizeHandleAt(const QPoint &pos) const;
    void startColumnResize(int column, int x);
    void updateColumnResize(int x);
    void endColumnResize();

    void drawBackground(QPainter *painter);
    QColor panelBackgroundColor() const;
    void drawHeader(QPainter *painter);
    void drawHeaderRecursive(QPainter *painter, const QVector<AgColumnDef> &defs, int depth, int &leafIndex, ColumnRegion region);
    void drawHeaderNode(QPainter *painter, const AgColumnDef &def, int depth, int leafStart, int leafEnd, ColumnRegion region);
    /// 全宽行背景（行底色 / 斑马纹 / 选中 / 悬停），先于单元格内容绘制。
    void drawRowBackgrounds(QPainter *painter, int firstRow, int lastRow);
    /// 单元格内容绘制；被合并覆盖的格跳过，锚点按合并矩形绘制。
    void drawRows(QPainter *painter, int firstRow, int lastRow, int firstColumn, int lastColumn);
    /// 网格线；被合并覆盖的格不画内框，锚点一次画整个合并区域外框。
    void drawGridLines(QPainter *painter, int firstColumn, int lastColumn, int firstRow, int lastRow);
    /// 焦点格边框；合并格边框环绕整个合并区域并按停靠边界裁剪。
    void drawCellFocus(QPainter *painter);
    /// 左右停靠区域与固定列区的分隔竖线。
    void drawRegionSeparators(QPainter *painter);
    /// 加载遮罩：半透明蒙层 + 旋转圆环 + 文案，按不透明度 m_loadingOpacity 绘制。
    void drawLoadingOverlay(QPainter *painter);
    /// 左侧 sticky 区域右边界（固定列与实际发生位移的左停靠列的最大右边缘）。
    /// 与 drawRegionSeparators 的分隔线一致；body 内容/焦点框/编辑器需在此边界右侧。
    int stickyLeftBoundaryX() const;
    /// 右侧 sticky 区域左边界（实际发生位移的右停靠列的最小左边缘；未生效时为视口宽度）。
    int stickyRightBoundaryX() const;

    int measureAutoFitWidth(int column);
    int clampColumnWidth(int column, int width) const;
    void invalidateAutoFit(int column);
    void fitColumnToContent(int column);
    /// 滚动 / 数据变化时对 AutoFit 列增量重测（只增不减，避免宽度抖动）。
    void refreshAutoFitColumns();

    int cornerRadius() const;
    QRect contentPanelRect() const;
    QPainterPath roundedClipPath() const;
    bool isInsideRoundedRect(const QPoint &pos) const;
    void updateRoundedMask();

    // 行号列
    static constexpr int kRowNumberColumn = -3;   ///< 行号列的负值编号
    int rowNumberColumnWidth() const;
    bool isRowNumberColumn(int column) const;
    QRect rowNumberCellRect(int row) const;
    void drawRowNumberCell(QPainter *painter, int row);

    // 复选框列
    static constexpr int kCheckboxColumn = -1;    ///< 复选框列的负值编号
    int specialColumnPadding() const;
    int checkboxColumnX() const;
    int checkboxColumnWidth() const;
    bool isCheckboxColumn(int column) const;
    QRect checkboxHeaderRect() const;
    QRect checkboxCellRect(int row) const;
    void drawCheckboxHeader(QPainter *painter);
    void drawCheckboxCell(QPainter *painter, int row);
    void toggleCheckboxHeader();
    void toggleCheckboxCell(int row);
    void drawCheckbox(QPainter *painter, const QRect &rect, bool checked, bool partial = false, bool enabled = true) const;

    // 行拖拽排序
    static constexpr int kDragHandleColumn = -2;  ///< 行拖拽手柄列的负值编号
    int dragHandleColumnX() const;
    int dragHandleColumnWidth() const;
    /// 行号列 + 复选框列 + 拖拽手柄列的总宽（左侧固定不滚动区域）。
    int fixedColumnsWidth() const;
    bool isDragHandleColumn(int column) const;
    QRect dragHandleCellRect(int row) const;
    void drawDragHandleCell(QPainter *painter, int row);
    void startRowDrag(int visibleRow, const QPoint &globalPos);
    void updateRowDrag(const QPoint &globalPos);
    void endRowDrag();
    void cancelRowDrag();
    int targetVisibleIndexForDrag(const QPoint &pos) const;
    void updateDragFloatWidget(const QPoint &globalPos);
    void updateDragFloatWidgetPosition(const QPoint &globalPos);
    void updateDragFloatWidgetText();
    void startAutoScroll();
    void stopAutoScroll();
    void onAutoScrollTimeout();
    /// 行移动后同步勾选集合（按原始行号迁移被拖动行的勾选）。
    void updateCheckedRowsAfterMove(const QVector<int> &draggedOriginalRows, int targetOriginalIndex);

    // 数据变化动画：排序 FLIP（行按旧布局位置 → 新布局位置位移）+ 重设数据切换
    /// 快照当前 可见行→原始行 顺序与行偏移，作为下一次布局变化的动画起点。
    void captureLayoutSnapshot();
    /// 布局变化（排序 / 过滤）后按快照计算各行位移并播放 200ms 位移动画；
    /// 移出视口的行夹取到视口外沿退场，新进入视口的行从所属方向的外沿入场。
    void startSortMoveAnimation();
    void finishSortMoveAnimation();
    /// 重设数据：旧内容快照整体上移出场，新内容自视口下方移入（200ms）。
    void startDataResetAnimation();
    void finishDataResetAnimation();

    void toggleSort(int column);
    void drawSortIndicator(QPainter *painter, const QRect &rect, Qt::SortOrder order) const;
    int headerColumnAt(int x) const;
    bool headerHitAt(const QPoint &pos, int &column, int &level) const;
    int checkableRowCount() const;

    // 树形表格
    /// 树列（首个可见叶子列）；非树形模式返回 -1。
    int treeColumn() const;
    /// 树形首列的缩进 + 箭头占位总宽（与默认渲染器的几何约定一致）。
    int treeLeadingWidth(int depth) const;
    /// 行的展开箭头热区（控件坐标）；行无子行 / 非树形模式返回无效矩形。
    QRect treeExpanderRect(int row) const;
    /// 切换行展开态（可见行号）。
    void toggleRowExpanded(int row);
    /// 展开 / 折叠动画的统一入口：展开即时应用模型（子行淡入 + 兄弟行下移）；
    /// 折叠先播放淡出 + 上移，动画结束时才应用模型折叠（末帧与最终状态一致，无跳变）。
    void setRowExpandedAnimated(int row, bool expanded);
    /// 应用展开 / 折叠动画进度（0~1）。
    void applyTreeRowAnimation(qreal progress);
    /// 结束动画：清理插值条目；折叠路径在此应用模型折叠。
    void finishTreeAnimation();
    /// 渐显 / 渐隐子行的当前不透明度（非动画行恒为 1）。
    qreal rowAnimationOpacity(int row) const;
    /// 行图层过滤：树动画分三层绘制（0=全部 / 1=渐变子行 / 2=平移行 / 3=静态行）。
    bool treeLayerAcceptRow(int row) const;
    /// 原始行子树内的全部原始行（含自身；非树形模式返回空）。
    QVector<int> subtreeRows(int originalRow) const;

    // 复选框列（树形级联三态）
    /// 树形模式：按原始行裁决可勾选（回调在树形模式接收原始行号，覆盖折叠行）。
    bool isRowCheckableOriginal(int originalRow) const;
    /// 树形模式：可勾选原始行总数（含折叠不可见的行）。
    int checkableOriginalRowCount() const;
    /// 树形模式：处于勾选状态的可勾选原始行数量（聚合勾选可能含不可勾选祖先，需过滤）。
    int checkedCheckableOriginalCount() const;
    /// 原始行三态。
    Qt::CheckState rowCheckStateOriginal(int originalRow) const;
    /// 以原始行为根级联勾选 / 取消整个子树，并重算祖先链三态。
    void cascadeRowCheck(int originalRow, bool checked);
    /// 自底向上重算 originalRow 祖先链的三态（勾父全选子 / 子勾父半选）。
    void recomputeAncestorCheckStates(int originalRow);
    /// 重建整棵树的三态（数据重载 / 回调变化后调用）。
    void recomputeAllCheckStates();

    struct RowDragAnimationEntry {
        int visibleRow;
        int startOffset;
        int endOffset;
    };
    void startDragReorderAnimation();
    void applyDragReorderAnimation(qreal progress);
    void finishDragReorderAnimation();

    int topLevelLeafCount(const AgColumnDef &def) const;
    int firstLeafOfTopLevelIndex(int topLevelIndex) const;
    int topLevelColumnIndexAt(int x) const;
    ColumnRegion topLevelColumnRegion(int topLevelIndex) const;

    // 任意层级列拖拽：以“父节点子路径”定位拖动范围
    QVector<int> leafPath(int leafColumn) const;
    const AgColumnDef *nodeAtPath(const QVector<int> &path) const;
    int firstLeafOfPath(const QVector<int> &path) const;
    int leafCountOfPath(const QVector<int> &path) const;

    void startColumnDrag(const QVector<int> &parentPath, int index, const QPoint &globalPos);
    void updateColumnDrag(const QPoint &globalPos);
    void endColumnDrag();
    void cancelColumnDrag();
    int targetColumnIndexForDrag(const QPoint &pos) const;
    void updateColumnDragFloatWidgetPosition(const QPoint &globalPos);
    void updateColumnDragFloatWidgetText();

    struct ColumnDragAnimationEntry {
        int column;
        int startOffset;
        int endOffset;
    };
    int columnContentOffset(int column) const;
    void startColumnReorderAnimation();
    void applyColumnReorderAnimation(qreal progress);
    void finishColumnReorderAnimation();

    // 列隐藏/显示动画
    void animateColumnVisibility(int firstLeaf, int lastLeaf, bool visible);
    void finishColumnVisibilityAnimation();
    qreal columnOpacity(int column) const;

    void updateEditorGeometry();
    void onGlobalStyleChanged();
    /// Tab 导航：从 (row, column) 起按 dColumn 方向找下一个可编辑且未隐藏/未覆盖的格。
    bool findNextEditableCell(int &row, int &column, int dColumn) const;
    /// 方向键移动选中格；跳过隐藏列与合并覆盖格。
    void moveSelection(int dRow, int dColumn);
    /// 按当前模式应用数据区点击的行选中裁决（多选模式下点击与 Ctrl+点击均为切换该行）。
    bool applyRowSelectionClick(int row, int column, bool ctrl);
    /// 按当前模式应用复选框点击（内置复选框列 / 数据列复选框单元格）的行选中裁决；
    /// 多选模式为切换加入 / 移出，withColumn 为 true 时同时把列作为选中锚点。
    bool applyCheckboxClickSelection(int row, int column, bool withColumn);
    void ensureCellVisible(int row, int column);
    int targetVerticalOffset(int row, ScrollHint hint, int spanBottomRow = -1) const;
    int targetHorizontalOffset(int column, ScrollHint hint, int spanRightColumn = -1) const;
    bool isCellEditable(int row, int column) const;
    void toggleCheckboxCellValue(int row, int column);
    bool tryFinishCurrentEdit(bool focusOut = false);
    void showEditError(const QString &message);
    void clearEditError();
    /// 计算 pos 处悬停单元格的 tooltip 文本（空串 = 不显示）。
    /// 仅列 tooltipEnabled=true 且内容超出列宽被裁剪时返回文本，
    /// 截断判定的几何约定与默认渲染器一致（左右内边距 + 树形首列缩进/箭头占位）。
    QString cellTooltipText(const QPoint &pos) const;
    /// Ctrl+C 复制当前选中格（内容来自渲染器 copyContent）。
    void copySelectedCell();

    void updateHoverCell(const QPoint &pos);
    void clearHoverCell();
    /// hover 变化整体重绘 body 区：合并单元格的锚点按整个合并区域填充 hover 背景，
    /// 任何局部行/单元格刷新都会让被覆盖行切片残留旧像素（视觉上逐一出现、与整行高亮不同步）。
    /// 可视行经过虚拟化后数量有限，整体重绘代价可忽略。
    void updateHoverVisual();
    /// 通知渲染器鼠标进入单元格（cellPos 相对单元格左上角）。
    void notifyRendererEnter(int row, int column, const QPoint &cellPos);
    void notifyRendererLeave(int row, int column);
    /// 树形展开箭头热区：手型光标（优先于渲染器的光标判定）。
    void applyTreeExpanderCursor(int column, int row, const QPoint &pos);
    /// 解析某列生效的单元格渲染器（checkbox 列 → 内置复选框；否则按注册名，缺省默认文本渲染器）。
    QSharedPointer<AgCellRenderer> rendererForColumn(int column) const;
    bool isRowDragging(int visibleRow) const;

    QSharedPointer<AgHeaderRenderer> headerRendererForColumn(int column) const;
    QRect headerNodeRect(int column, int level) const;
    int headerLeafAt(const QPoint &pos, int *level = nullptr, QPoint *headerPos = nullptr) const;
    void updateHeaderHover(const QPoint &pos);
    void clearHeaderHover();
    void notifyHeaderRendererEnter(int column, int level, const QRect &rect, const QPoint &headerPos);
    void notifyHeaderRendererLeave();

    /// 标记行高缓存失效（行高默认值 + 渲染器 preferredHeight 逐行计算）。
    void invalidateRowHeights();
    void invalidateRowHeight(int row);
    /// 重建行高与行偏移缓存（const，按需惰性调用）。
    void rebuildRowOffsets() const;

    // ---- 成员变量 ----
    AgGridModel *m_model;
    int m_xOffset;                 ///< 水平滚动偏移（像素）
    int m_yOffset;                 ///< 垂直滚动偏移（像素）
    int m_selectedRow;             ///< 选中行（可见行号，-1 无）
    int m_selectedColumn;          ///< 选中列（叶子列索引，-1 无）
    int m_hoverRow;                ///< 悬停行（合并格记录锚点行）
    int m_hoverColumn;             ///< 悬停列（合并格记录锚点列）
    QPoint m_hoverPos;             ///< 最近一次鼠标位置（展开箭头 hover 判定）
    int m_hoverHeaderColumn;       ///< 悬停表头节点首个叶子列
    int m_hoverHeaderLevel;        ///< 悬停表头层级
    QPoint m_hoverHeaderPos;       ///< 悬停表头内相对坐标
    bool m_headerRenderClickPending;  ///< 表头渲染器热区按下待释放标记
    int m_visibleRowBuffer;        ///< 可视区上下缓冲行数
    QVector<int> m_columnOffsets;  ///< 列偏移表（size = columnCount + 1，首项为 0）
    QVector<int> m_autoFitWidths;  ///< AutoFit 列测量结果缓存
    QVector<bool> m_autoFitDirty;  ///< AutoFit 测量脏标记
    bool m_refreshingAutoFitColumns = false;  ///< 防止 AutoFit 重入

    mutable QVector<int> m_rowHeights;    ///< 行高缓存（惰性重建）
    mutable QVector<int> m_rowOffsets;    ///< 行偏移缓存（size = rowCount + 1）
    mutable bool m_rowOffsetsDirty;       ///< 行偏移缓存脏标记

    QVector<ColumnRegion> m_columnRegions;   ///< 每列所属区域
    QVector<int> m_leftPinnedColumns;        ///< 左停靠列索引表
    QVector<int> m_bodyColumns;              ///< body 区列索引表
    QVector<int> m_rightPinnedColumns;       ///< 右停靠列索引表
    int m_leftPinnedWidth;                   ///< 左停靠区总宽
    int m_rightPinnedWidth;                  ///< 右停靠区总宽

    QSet<QString> m_hiddenFields;            ///< 隐藏列的 field 集合

    // 单元格合并：锚点 -> 合并信息；被覆盖单元格 -> 锚点 key
    QHash<qint64, CellSpan> m_spans;
    QHash<qint64, qint64> m_spanCovered;

    bool m_showRowNumberColumn;              ///< 是否显示行号列
    bool m_showCheckboxColumn;               ///< 是否显示复选框列
    bool m_verticalGridLinesVisible;         ///< 是否绘制单元格左右（列方向）边框线
    QSet<int> m_checkedOriginalRows;         ///< 勾选行集合（原始行号，跨排序过滤稳定）
    QSet<int> m_partialOriginalRows;         ///< 半选行集合（原始行号；树形模式父子聚合）

    int m_sortedColumn;                      ///< 当前排序列（-1 未排序）
    Qt::SortOrder m_sortOrder;               ///< 当前排序方向
    bool m_sortingEnabled;                   ///< 表头点击排序总开关
    QList<AgSortRule> m_sortRules;           ///< 多规则排序

    int m_selectedOriginalRow;               ///< 选中行对应的原始行号（布局变化后恢复选中用）
    SelectionMode m_selectionMode;           ///< 行选中模式（单选 / 多选 / 不可选中）
    QSet<int> m_selectedOriginalRows;        ///< 多选选中行集合（原始行号，跨排序过滤稳定）

    AgGridScrollBar *m_hScrollBar;           ///< 自绘水平滚动条
    AgGridScrollBar *m_vScrollBar;           ///< 自绘垂直滚动条
    bool m_hScrollBarVisible;
    bool m_vScrollBarVisible;
    AgGridStyle *m_style;                    ///< 当前样式（全局或独立）
    bool m_ownStyle;                         ///< 是否拥有样式所有权
    bool m_explicitStyle;                    ///< 是否显式设置过样式（false = 跟随全局）
    SizeMode m_sizeMode;                     ///< 当前尺寸模式
    bool m_styleClonedForSizeMode;           ///< 尺寸模式是否已克隆独立样式

    QSharedPointer<AgCellRenderer> m_defaultCellRenderer;      ///< 默认文本渲染器
    QSharedPointer<AgCellRenderer> m_checkboxCellRenderer;     ///< 内置复选框渲染器
    QHash<QString, QSharedPointer<AgCellRenderer>> m_cellRenderers;      ///< 注册的渲染器
    QHash<QString, QSharedPointer<AgHeaderRenderer>> m_headerRenderers;  ///< 注册的表头渲染器

    QSharedPointer<AgCellEditor> m_defaultCellEditor;            ///< 默认单行编辑器
    QSharedPointer<AgCellEditor> m_defaultMultiLineCellEditor;   ///< 内置多行编辑器
    QHash<QString, QSharedPointer<AgCellEditor>> m_cellEditors;  ///< 注册的编辑器
    QSharedPointer<AgCellEditValidator> m_cellEditValidator;     ///< 编辑提交校验器
    std::function<bool(const AgGridModel *, int, int)> m_cellEditableCallback;  ///< 动态可编辑回调
    std::function<QString(const AgGridModel *, int, int, const QString &)> m_cellTooltipTextCallback;  ///< tooltip 内容回调
    std::function<bool(const AgGridModel *, int)> m_rowCheckableCallback;       ///< 行可选回调
    QWidget *m_activeEditor;                 ///< 当前编辑器控件
    QSharedPointer<AgCellEditor> m_activeEditorImpl;             ///< 当前编辑器实现
    int m_editRow;                           ///< 编辑行（锚点行）
    int m_editColumn;                        ///< 编辑列（锚点列）
    bool m_committing;                       ///< 提交中（防重入）

    bool m_isResizing;                       ///< 列宽调整中
    int m_resizeColumn;                      ///< 调整中的列
    int m_resizeStartX;                      ///< 调整起始 x
    int m_resizeStartWidth;                  ///< 调整起始列宽

    // 行拖拽排序
    bool m_rowDragEnabled;                   ///< 行拖拽总开关
    bool m_isRowDragging;                    ///< 拖拽进行中
    QPoint m_dragStartPos;                   ///< 按下位置（判定拖拽启动）
    int m_dragStartVisibleRow;               ///< 拖拽起始可见行
    QVector<int> m_draggingOriginalRows;     ///< 被拖动行（原始行号）
    int m_dragTargetVisibleIndex;            ///< 插入目标可见位置
    int m_rowDragLastY;                      ///< 上次拖拽 y（判定方向）
    int m_rowDragDirection;                  ///< 拖拽方向（1 下 / -1 上 / 0 未定）
    QLabel *m_dragFloatWidget;               ///< 拖动浮动提示
    QTimer *m_autoScrollTimer;               ///< 拖到边缘时自动滚动定时器
    std::function<QString(const AgGridModel *, const QVector<int> &)> m_dragPreviewTextCallback;  ///< 拖动提示文字回调

    QVector<RowDragAnimationEntry> m_dragAnimationEntries;  ///< 行重排动画条目
    QVector<int> m_dragAnimationOldOrder;    ///< 动画前可见行顺序
    QVector<int> m_dragAnimationOldOffsets;  ///< 动画前行偏移
    qreal m_dragAnimationProgress;           ///< 动画进度 0~1
    QVariantAnimation *m_dragOffsetAnimation;  ///< 行重排动画驱动

    // 树形展开 / 折叠动画（机制同行拖拽：旧偏移 → 新偏移插值 + 子行透明度渐变）
    struct TreeRowAnimationEntry {
        int visibleRow;      ///< 受影响行（可见行号）
        int startOffset;     ///< 动画起始 y 偏移
        int endOffset;       ///< 动画结束 y 偏移
    };
    QVector<TreeRowAnimationEntry> m_treeAnimationEntries;  ///< 平移行（兄弟行下移 / 上移）
    QSet<int> m_treeAnimationFadeRows;                      ///< 渐显 / 渐隐的子行（可见行号）
    QSet<int> m_treeAnimationMoveRows;                      ///< 平移行集合（可见行号，图层过滤用）
    int m_treeLayerFilter;                                  ///< 绘制图层过滤（0=全部 / 1=渐变 / 2=平移 / 3=静态）
    bool m_treeAnimationExpanding;                          ///< true=展开淡入 / false=折叠淡出
    bool m_treeAnimationPendingCollapse;                    ///< 折叠动画结束时才应用模型折叠
    int m_treeAnimationPendingRow;                          ///< 待应用折叠的原始行
    qreal m_treeAnimationProgress;                          ///< 动画进度 0~1
    QVariantAnimation *m_treeAnimation;                     ///< 展开折叠动画驱动

    // 列拖拽排序
    bool m_isColumnDragging;                 ///< 列拖拽进行中
    bool m_isPotentialColumnDrag;            ///< 按下未达拖拽阈值（区分点击与拖动）
    int m_potentialColumnDragColumn;         ///< 潜在拖动列
    QPoint m_potentialColumnDragStartPos;    ///< 按下位置
    int m_columnDragStartColumn;             ///< 拖动起始列
    QVector<int> m_columnDragParentPath;     ///< 拖动节点在列树中的父路径
    int m_columnDragParentLeafStart;         ///< 拖动范围的起始叶子列
    int m_columnDragParentLeafEnd;           ///< 拖动范围的结束叶子列
    QVector<int> m_draggingTopLevelColumns;  ///< 被拖动的顶层列
    int m_columnDragTargetIndex;             ///< 插入目标索引
    int m_columnDragLastX;                   ///< 上次拖拽 x（判定方向）
    int m_columnDragDirection;               ///< 拖拽方向
    ColumnRegion m_columnDragRegion;         ///< 拖动目标区域
    int m_columnDragRegionFirst;             ///< 目标区域起始叶子列
    int m_columnDragRegionLast;              ///< 目标区域结束叶子列
    QVector<QString> m_columnDragAnimationOldFields;  ///< 动画前列顺序（field）
    QVector<ColumnDragAnimationEntry> m_columnDragAnimationEntries;  ///< 列重排动画条目
    QVector<int> m_columnDragAnimationOldOrder;   ///< 动画前列顺序
    QVector<int> m_columnDragAnimationOldOffsets; ///< 动画前列偏移
    qreal m_columnDragAnimationProgress;     ///< 动画进度 0~1
    QVariantAnimation *m_columnDragOffsetAnimation;  ///< 列重排动画驱动

    // 列隐藏/显示动画
    bool m_columnVisAnimating;               ///< 显隐动画进行中
    int m_columnVisTargetFirst;              ///< 动画目标起始叶子列
    int m_columnVisTargetLast;               ///< 动画目标结束叶子列
    bool m_columnVisHiding;                  ///< true=隐藏动画 / false=显示动画
    qreal m_columnVisProgress;               ///< 动画进度 0~1
    QVector<ColumnDragAnimationEntry> m_columnVisShiftEntries;  ///< 右侧列位移插值条目
    QVariantAnimation *m_columnVisAnimation; ///< 显隐动画驱动

    // 加载遮罩
    bool m_loading;                          ///< 加载遮罩请求状态（setLoading 设置）
    qreal m_loadingOpacity;                  ///< 遮罩当前不透明度 0~1（淡入/淡出动画驱动）
    QString m_loadingText;                   ///< 加载文字（null = 跟随样式键 LoadingText）
    int m_loadingSpinnerAngle;               ///< 圆环旋转角（度）
    QTimer *m_loadingTimer;                  ///< 圆环旋转动画定时器
    QVariantAnimation *m_loadingAnimation;   ///< 遮罩淡入/淡出动画驱动

    // 数据变化动画（排序 FLIP / 重设数据切换）
    bool m_dataAnimationEnabled;             ///< 数据变化动画总开关（默认开启）
    int m_sortAnimationDuration;             ///< 排序位移动画时长（毫秒，默认 100）
    int m_resetAnimationDuration;            ///< 重设数据切换动画时长（毫秒，默认 200）
    QVector<int> m_animLastOrder;            ///< 布局稳定后的可见行→原始行快照（动画起点）
    QVector<int> m_animLastOffsets;          ///< 快照对应的行偏移表（size = rowCount + 1）
    struct DataMoveAnimationEntry {
        int visibleRow;                      ///< 新可见行号
        int startOffset;                     ///< 动画起点 y（旧布局位置，已夹取到视口外沿）
        int endOffset;                       ///< 动画终点 y（新布局位置）
    };
    QVector<DataMoveAnimationEntry> m_dataAnimationEntries;  ///< 排序位移动画条目
    qreal m_dataAnimationProgress;           ///< 排序位移动画进度 0~1
    QVariantAnimation *m_dataAnimation;      ///< 排序位移动画驱动

    QImage m_dataResetSnapshot;              ///< 重设数据前的 body 内容快照（像素）
    bool m_dataResetPending;                 ///< 已捕获快照，待模型应用后开演
    bool m_dataResetAnimating;               ///< 切换动画进行中
    qreal m_dataResetProgress;               ///< 切换动画进度 0~1
    int m_dataResetSlideHeight;              ///< 切换动画滑动距离（开演时的 body 高度）
    QVariantAnimation *m_dataResetAnimation; ///< 切换动画驱动

    static constexpr int kResizeHandleTolerance = 4;  ///< 列宽调整热区容差（像素）
};

#endif // AGGRIDWIDGET_H
