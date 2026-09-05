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
#include <QSharedPointer>
#include <QPainterPath>
#include <QVariantAnimation>
#include <functional>

class AgGridScrollBar;
class QLabel;
class QTimer;

class AGGRID_EXPORT AgGridWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AgGridWidget(QWidget *parent = nullptr);
    ~AgGridWidget();

    void setModel(AgGridModel *model);
    AgGridModel *model() const;

    int headerHeight() const;
    int headerLevelHeight() const;
    int rowHeight() const;

    int selectedRow() const;
    int selectedColumn() const;

    int visibleRowBuffer() const;
    void setVisibleRowBuffer(int buffer);

    void setStyle(AgGridStyle *style);
    AgGridStyle *style() const;

    using SizeMode = AgGridStyle::SizeMode;
    void setSizeMode(SizeMode mode);
    SizeMode sizeMode() const;

    void registerCellRenderer(const QString &name, QSharedPointer<AgCellRenderer> renderer);
    void registerCellEditor(const QString &name, QSharedPointer<AgCellEditor> editor);
    void registerHeaderRenderer(const QString &name, QSharedPointer<AgHeaderRenderer> renderer);

    void setColumnWidth(int column, int width);
    void setColumnWidth(const QString &field, int width);

    void autoSizeColumn(int column);
    void autoSizeAllColumns();

    bool editCell(int row, int column);
    bool editCell(int row, int column, const QString &initialText);
    bool isEditing() const;
    void closeEditor(bool commit = true);

    enum class ScrollHint {
        EnsureVisible,
        PositionTop,
        PositionCenter,
        PositionBottom
    };
    Q_ENUM(ScrollHint)

    void scrollToRow(int row, ScrollHint hint = ScrollHint::EnsureVisible);
    void scrollToColumn(int column, ScrollHint hint = ScrollHint::EnsureVisible);
    void scrollToCell(int row, int column, ScrollHint hint = ScrollHint::EnsureVisible);

    // 滚动偏移量（像素）：可直接读写，用于多表格同步
    int verticalOffset() const;
    int horizontalOffset() const;
    void setVerticalOffset(int value);
    void setHorizontalOffset(int value);

    void pinColumn(int column, AgColumnDef::PinMode mode);
    void pinColumn(const QString &field, AgColumnDef::PinMode mode);
    AgColumnDef::PinMode columnPinMode(int column) const;

    // 列显示/隐藏：column 为叶子列索引（最后一个层级的顺序），field 可指向叶子列或分组列
    void setColumnVisible(int column, bool visible);
    void setColumnVisible(const QString &field, bool visible);
    bool isColumnVisible(int column) const;

    void selectRow(int row);
    void selectCell(int row, int column);
    void clearSelection();
    void syncSelectionAfterLayoutChange();

    // 行号列
    void setRowNumberColumnVisible(bool visible);
    bool isRowNumberColumnVisible() const;

    // 复选框列
    void setCheckboxColumnVisible(bool visible);
    bool isCheckboxColumnVisible() const;

    bool isRowChecked(int row) const;
    void setRowChecked(int row, bool checked);
    bool isRowCheckable(int row) const;
    void setRowCheckableCallback(const std::function<bool(const AgGridModel *, int)> &callback);

    void selectAllRows();
    void clearAllCheckedRows();

    QSet<int> checkedRows() const;
    QSet<int> checkedOriginalRows() const;

    int sortedColumn() const;
    Qt::SortOrder sortOrder() const;

    void setSortingEnabled(bool enabled);
    bool isSortingEnabled() const;

    void setSortRules(const QList<AgSortRule> &rules);
    QList<AgSortRule> sortRules() const;
    void clearSort();

    // 过滤
    void setFilterCallback(const std::function<bool(const QVariantMap &row)> &callback);
    void clearFilter();
    bool isFiltered() const;

    void setCellEditValidator(QSharedPointer<AgCellEditValidator> validator);
    void setCellEditableCallback(const std::function<bool(const AgGridModel *, int, int)> &callback);

    // 行拖拽排序
    void setRowDragEnabled(bool enabled);
    bool isRowDragEnabled() const;
    void setDragPreviewTextCallback(const std::function<QString(const AgGridModel *, const QVector<int> &)> &callback);

signals:
    void cellClicked(int row, int column);
    void cellDoubleClicked(int row, int column);
    void cellHovered(int row, int column);
    void cellEditingStarted(int row, int column);
    void cellEditingFinished(int row, int column);
    void cellValueChanged(int row, int column, const QVariant &value);
    void selectionChanged();

    // 复选框列
    void rowCheckedChanged(int row, bool checked);
    void checkedStateChanged();

    // 数据列内置复选框
    void cellCheckStateChanged(int row, int column, bool checked);

    void sortChanged(int column, Qt::SortOrder order);
    void filterChanged();

    // 滚动偏移变化（拖滚动条、滚轮、键盘、程序滚动等所有来源）
    void verticalScrollChanged(int value);
    void horizontalScrollChanged(int value);

    // 列宽变化（模型层转发）
    void columnWidthsChanged();

    void cellRightClicked(int row, int column, const QPoint &globalPos);
    void headerRightClicked(int column, int level, const QPoint &globalPos);
    void rowsDragged(const QVector<int> &originalRows, int targetOriginalIndex);

protected:
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
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    friend class AgGridStyle;
    enum class ColumnRegion {
        LeftPinned,
        Body,
        RightPinned
    };

    void connectModel();
    void disconnectModel();
    void updateScrollRange();
    void updateScrollBars();
    void updateScrollBarsCompute();
    void rebuildColumnOffsets();
    void applyStyleToScrollBars();

    int totalContentWidth() const;
    int totalContentHeight() const;
    int bodyHeight() const;

    QPoint cellAtPosition(const QPoint &pos) const;
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
    void drawRowBackgrounds(QPainter *painter, int firstRow, int lastRow);
    void drawRows(QPainter *painter, int firstRow, int lastRow, int firstColumn, int lastColumn);
    void drawGridLines(QPainter *painter, int firstColumn, int lastColumn, int firstRow, int lastRow);
    void drawCellFocus(QPainter *painter);
    void drawRegionSeparators(QPainter *painter);

    int measureAutoFitWidth(int column);
    int clampColumnWidth(int column, int width) const;
    void invalidateAutoFit(int column);
    void fitColumnToContent(int column);
    void refreshAutoFitColumns();

    int cornerRadius() const;
    QRect contentPanelRect() const;
    QPainterPath roundedClipPath() const;
    bool isInsideRoundedRect(const QPoint &pos) const;
    void updateRoundedMask();

    // 行号列
    static constexpr int kRowNumberColumn = -3;
    int rowNumberColumnWidth() const;
    bool isRowNumberColumn(int column) const;
    QRect rowNumberCellRect(int row) const;
    void drawRowNumberCell(QPainter *painter, int row);

    // 复选框列
    static constexpr int kCheckboxColumn = -1;
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
    static constexpr int kDragHandleColumn = -2;
    int dragHandleColumnX() const;
    int dragHandleColumnWidth() const;
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
    void updateCheckedRowsAfterMove(const QVector<int> &draggedOriginalRows, int targetOriginalIndex);

    void toggleSort(int column);
    void drawSortIndicator(QPainter *painter, const QRect &rect, Qt::SortOrder order) const;
    int headerColumnAt(int x) const;
    bool headerHitAt(const QPoint &pos, int &column, int &level) const;
    int checkableRowCount() const;

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
    bool findNextEditableCell(int &row, int &column, int dColumn) const;
    void moveSelection(int dRow, int dColumn);
    void ensureCellVisible(int row, int column);
    int targetVerticalOffset(int row, ScrollHint hint) const;
    int targetHorizontalOffset(int column, ScrollHint hint) const;
    bool isCellEditable(int row, int column) const;
    void toggleCheckboxCellValue(int row, int column);
    bool tryFinishCurrentEdit(bool focusOut = false);
    void showEditError(const QString &message);
    void clearEditError();
    void copySelectedCell();

    void updateHoverCell(const QPoint &pos);
    void clearHoverCell();
    void notifyRendererEnter(int row, int column, const QPoint &cellPos);
    void notifyRendererLeave(int row, int column);
    QSharedPointer<AgCellRenderer> rendererForColumn(int column) const;
    bool isRowDragging(int visibleRow) const;

    QSharedPointer<AgHeaderRenderer> headerRendererForColumn(int column) const;
    QRect headerNodeRect(int column, int level) const;
    int headerLeafAt(const QPoint &pos, int *level = nullptr, QPoint *headerPos = nullptr) const;
    void updateHeaderHover(const QPoint &pos);
    void clearHeaderHover();
    void notifyHeaderRendererEnter(int column, int level, const QRect &rect, const QPoint &headerPos);
    void notifyHeaderRendererLeave();

    void invalidateRowHeights();
    void invalidateRowHeight(int row);
    void rebuildRowOffsets() const;

    AgGridModel *m_model;
    int m_xOffset;
    int m_yOffset;
    int m_selectedRow;
    int m_selectedColumn;
    int m_hoverRow;
    int m_hoverColumn;
    int m_hoverHeaderColumn;
    int m_hoverHeaderLevel;
    QPoint m_hoverHeaderPos;
    bool m_headerRenderClickPending;
    int m_visibleRowBuffer;
    QVector<int> m_columnOffsets;
    QVector<int> m_autoFitWidths;
    QVector<bool> m_autoFitDirty;
    bool m_refreshingAutoFitColumns = false;

    mutable QVector<int> m_rowHeights;
    mutable QVector<int> m_rowOffsets;
    mutable bool m_rowOffsetsDirty;

    QVector<ColumnRegion> m_columnRegions;
    QVector<int> m_leftPinnedColumns;
    QVector<int> m_bodyColumns;
    QVector<int> m_rightPinnedColumns;
    int m_leftPinnedWidth;
    int m_rightPinnedWidth;

    QSet<QString> m_hiddenFields;

    bool m_showRowNumberColumn;
    bool m_showCheckboxColumn;
    QSet<int> m_checkedOriginalRows;

    int m_sortedColumn;
    Qt::SortOrder m_sortOrder;
    bool m_sortingEnabled;
    QList<AgSortRule> m_sortRules;

    int m_selectedOriginalRow;

    AgGridScrollBar *m_hScrollBar;
    AgGridScrollBar *m_vScrollBar;
    bool m_hScrollBarVisible;
    bool m_vScrollBarVisible;
    AgGridStyle *m_style;
    bool m_ownStyle;
    bool m_explicitStyle;
    SizeMode m_sizeMode;
    bool m_styleClonedForSizeMode;

    QSharedPointer<AgCellRenderer> m_defaultCellRenderer;
    QSharedPointer<AgCellRenderer> m_checkboxCellRenderer;
    QHash<QString, QSharedPointer<AgCellRenderer>> m_cellRenderers;
    QHash<QString, QSharedPointer<AgHeaderRenderer>> m_headerRenderers;

    QSharedPointer<AgCellEditor> m_defaultCellEditor;
    QSharedPointer<AgCellEditor> m_defaultMultiLineCellEditor;
    QHash<QString, QSharedPointer<AgCellEditor>> m_cellEditors;
    QSharedPointer<AgCellEditValidator> m_cellEditValidator;
    std::function<bool(const AgGridModel *, int, int)> m_cellEditableCallback;
    std::function<bool(const AgGridModel *, int)> m_rowCheckableCallback;
    QWidget *m_activeEditor;
    QSharedPointer<AgCellEditor> m_activeEditorImpl;
    int m_editRow;
    int m_editColumn;
    bool m_committing;

    bool m_isResizing;
    int m_resizeColumn;
    int m_resizeStartX;
    int m_resizeStartWidth;

    // 行拖拽排序
    bool m_rowDragEnabled;
    bool m_isRowDragging;
    QPoint m_dragStartPos;
    int m_dragStartVisibleRow;
    QVector<int> m_draggingOriginalRows;
    int m_dragTargetVisibleIndex;
    int m_rowDragLastY;
    int m_rowDragDirection;
    QLabel *m_dragFloatWidget;
    QTimer *m_autoScrollTimer;
    std::function<QString(const AgGridModel *, const QVector<int> &)> m_dragPreviewTextCallback;

    QVector<RowDragAnimationEntry> m_dragAnimationEntries;
    QVector<int> m_dragAnimationOldOrder;
    QVector<int> m_dragAnimationOldOffsets;
    qreal m_dragAnimationProgress;
    QVariantAnimation *m_dragOffsetAnimation;

    // 列拖拽排序
    bool m_isColumnDragging;
    bool m_isPotentialColumnDrag;
    int m_potentialColumnDragColumn;
    QPoint m_potentialColumnDragStartPos;
    int m_columnDragStartColumn;
    QVector<int> m_columnDragParentPath;
    int m_columnDragParentLeafStart;
    int m_columnDragParentLeafEnd;
    QVector<int> m_draggingTopLevelColumns;
    int m_columnDragTargetIndex;
    int m_columnDragLastX;
    int m_columnDragDirection;
    ColumnRegion m_columnDragRegion;
    int m_columnDragRegionFirst;
    int m_columnDragRegionLast;
    QVector<QString> m_columnDragAnimationOldFields;
    QVector<ColumnDragAnimationEntry> m_columnDragAnimationEntries;
    QVector<int> m_columnDragAnimationOldOrder;
    QVector<int> m_columnDragAnimationOldOffsets;
    qreal m_columnDragAnimationProgress;
    QVariantAnimation *m_columnDragOffsetAnimation;

    // 列隐藏/显示动画
    bool m_columnVisAnimating;
    int m_columnVisTargetFirst;
    int m_columnVisTargetLast;
    bool m_columnVisHiding;
    qreal m_columnVisProgress;
    QVector<ColumnDragAnimationEntry> m_columnVisShiftEntries;
    QVariantAnimation *m_columnVisAnimation;

    static constexpr int kResizeHandleTolerance = 4;
};

#endif // AGGRIDWIDGET_H
