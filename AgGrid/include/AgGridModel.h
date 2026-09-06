#ifndef AGGRIDMODEL_H
#define AGGRIDMODEL_H

#include "AgGrid_global.h"
#include "AgColumnDef.h"

#include <QObject>
#include <QVector>
#include <QVariantMap>
#include <QHash>
#include <QList>
#include <functional>

/**
 * @brief 单条排序规则，用于多规则逻辑排序。
 *
 * 多条规则按列表顺序依次比较：第一列相等时才比较第二列，依此类推。
 */
struct AgSortRule {
    int column = -1;                        ///< 叶子列索引；-1 表示无效规则
    Qt::SortOrder order = Qt::AscendingOrder;  ///< 排序方向

    AgSortRule() = default;
    AgSortRule(int c, Qt::SortOrder o)
        : column(c)
        , order(o)
    {}
};

/**
 * @brief AgGridWidget 的数据模型，与视图解耦。
 *
 * 职责：
 *  - 持有列定义树（支持分组列，形成多级表头）；
 *  - 持有原始行数据（QVector<QVariantMap>，键为列 field）；
 *  - 维护"原始行 → 可见行"的过滤 / 排序映射；
 *  - 提供单元格读写、列宽、列停靠、排序规则与过滤回调。
 *
 * 坐标体系（重要）：
 *  - 可见行（visibleRow）：过滤排序后的行顺序，界面相关接口使用；
 *  - 原始行（originalRow）：setRowData 传入时的顺序，跨排序过滤稳定；
 *  - column：叶子列索引（列树展开后从左到右的顺序，隐藏列不改变索引）。
 *
 * 信号与重绘：模型发出的信号由 AgGridWidget 内部连接并触发重绘，
 * 一般无需手动关心；变更数据后界面自动更新。
 *
 * 所有者：模型由调用者创建与销毁（通常 new AgGridModel(parent)），
 * 一个模型可同时供多个 AgGridWidget 使用（数据对比场景）。
 */
class AGGRID_EXPORT AgGridModel : public QObject
{
    Q_OBJECT

public:
    explicit AgGridModel(QObject *parent = nullptr);

    // ---- 列定义 ----

    /// 替换整个列定义树（支持分组列形成多级表头）。
    /// 触发 columnsChanged + layoutChanged，视图重建全部列状态。
    void setColumnDefs(const QVector<AgColumnDef> &defs);
    /// 顶层列定义列表（分组列含 children）。
    const QVector<AgColumnDef> &topLevelColumnDefs() const;

    // ---- 行数据 ----

    /// 替换全部行数据（内部深拷贝保存为原始数据），随后立即应用过滤与排序。
    /// 触发 rowsChanged + layoutChanged。
    void setRowData(const QVector<QVariantMap> &rows);
    /// 可见数据（过滤排序后的顺序）。
    const QVector<QVariantMap> &rowData() const;
    /// 原始数据（setRowData 传入的顺序）。
    const QVector<QVariantMap> &originalRowData() const;

    /// 可见行数量（过滤后）。
    int rowCount() const;
    /// 叶子列数量。
    int columnCount() const;
    /// 表头层级数量（无分组列为 1）。
    int headerLevelCount() const;

    /// 原始行数量。
    int originalRowCount() const;
    /// 可见行 → 原始行；无效返回 -1。
    int originalRowForVisibleRow(int visibleRow) const;
    /// 原始行 → 可见行；被过滤隐藏返回 -1。
    int visibleRowForOriginalRow(int originalRow) const;

    // ---- 列查找 ----

    /// field → 叶子列索引；不存在返回 -1。
    int columnIndexForField(const QString &field) const;
    /// 某列（可指向分组列）下的全部叶子 field，按显示顺序。
    QVector<QString> leafFieldsForField(const QString &field) const;
    /// 叶子列索引 → field。
    QString fieldAtColumn(int column) const;
    /// 叶子列索引 → 列定义（返回拷贝）。
    AgColumnDef columnDefAt(int column) const;

    // ---- 列停靠 / 列宽 ----

    /// 设置列停靠模式（column 为叶子列索引）。
    void setColumnPinMode(int column, AgColumnDef::PinMode mode);
    /// 按 field 设置停靠（可指向分组列，整组停靠）。
    void setColumnPinMode(const QString &field, AgColumnDef::PinMode mode);
    /// 查询列停靠状态。
    AgColumnDef::PinMode columnPinMode(int column) const;

    // ---- 单元格读写 ----

    /// 按可见行 + 叶子列索引读值。
    QVariant cellValue(int row, int column) const;
    /// 按可见行 + field 读值；field 不存在返回无效 QVariant。
    QVariant cellValue(int row, const QString &field) const;
    /// 按可见行 + field 写值（修改原始数据）。成功触发 dataChanged 并返回 true。
    bool setCellValue(int row, const QString &field, const QVariant &value);
    /// 按可见行 + 叶子列索引写值。
    bool setCellValue(int row, int column, const QVariant &value);

    /// 叶子列的表头文字（即列定义 headerName）。
    QVariant headerValue(int column) const;

    /// 设置列宽（Fixed 模式生效）。column 为叶子列索引。
    void setColumnWidth(int column, int width);
    /// 按 field 设置列宽。
    void setColumnWidth(const QString &field, int width);
    /// 读取列宽（叶子列索引）。
    int columnWidth(int column) const;
    /// 按 field 读取列宽。
    int columnWidth(const QString &field) const;

    /// 清空全部数据与列定义（模型回到未初始化状态）。
    void clear();

    // ---- 排序 / 过滤 ----

    /// 单列排序（等价于单条规则的 sort(rules)）。
    void sort(int column, Qt::SortOrder order);
    /// 多规则逻辑排序：按列表顺序依次比较；空列表等价 clearSort()。
    /// 无效列（越界或无 field）的规则被忽略。触发 rowsChanged + layoutChanged。
    void sort(const QList<AgSortRule> &rules);
    /// 清除排序（保留过滤），恢复原始顺序。触发 layoutChanged。
    void clearSort();

    /// 设置过滤回调：对每行返回 true 表示保留。设置后立即生效。
    void setFilterCallback(const std::function<bool(const QVariantMap &row)> &callback);
    /// 清除过滤回调（恢复全部行）。
    void clearFilterCallback();
    /// 是否设置了过滤回调。
    bool hasFilterCallback() const;

    /// 行拖拽排序的模型入口：把一批原始行移动到 targetOriginalIndex 之前。
    /// 触发 rowsMoved + rowsChanged + layoutChanged。
    void moveRows(const QVector<int> &fromOriginalIndices, int targetOriginalIndex);

signals:
    /// 单元格数据变化（可见行坐标，闭区间范围）。
    void dataChanged(int topRow, int leftColumn, int bottomRow, int rightColumn);
    /// 可见行集合内容变化（数据替换、过滤、排序、移动行）。
    void rowsChanged();
    /// 行拖拽移动完成（原始行号）。
    void rowsMoved(const QVector<int> &fromOriginalIndices, int targetOriginalIndex);
    /// 列定义被整体替换。
    void columnsChanged();
    /// 列宽变化。
    void columnWidthsChanged();
    /// 重大布局变化（setRowData / setColumnDefs / 排序过滤 / 行移动）；
    /// 视图会重置 AutoFit 缓存、重建列偏移，并清空合并单元格区域。
    void layoutChanged();
    /// 过滤状态变化。
    void filterChanged();

private:
    /// 重建 field → 列索引 哈希表（setColumnDefs 后调用）。
    void rebuildFieldIndex();
    /// 展开列定义树，生成叶子列列表 m_leafColumnDefs。
    void flattenLeaves();
    /// 计算表头层级数（列树最大深度）。
    int computeHeaderLevelCount() const;
    /// 按过滤回调 + 排序规则重建可见行映射。
    void applyFilterAndSort();
    /// 执行多规则排序（对可见数据排序）。
    void sortVisibleData();

    QVector<AgColumnDef> m_topLevelColumnDefs;   ///< 顶层列定义树
    QVector<AgColumnDef *> m_leafColumnDefs;     ///< 展开后的叶子列（指向 m_topLevelColumnDefs 内部）
    QVector<QVariantMap> m_rowData;              ///< 可见数据（过滤排序后）
    QVector<QVariantMap> m_originalRowData;      ///< 原始数据
    QVector<int> m_visibleToOriginal;            ///< 可见行 → 原始行映射
    QVector<int> m_originalToVisible;            ///< 原始行 → 可见行映射（-1 = 被过滤）
    QHash<QString, int> m_fieldToColumn;         ///< field → 叶子列索引
    int m_headerLevelCount;                      ///< 表头层级数

    QList<AgSortRule> m_sortRules;               ///< 当前排序规则
    std::function<bool(const QVariantMap &row)> m_filterCallback;  ///< 过滤回调（true 保留）
};

#endif // AGGRIDMODEL_H
