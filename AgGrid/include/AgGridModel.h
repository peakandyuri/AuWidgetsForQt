#ifndef AGGRIDMODEL_H
#define AGGRIDMODEL_H

#include "AgGrid_global.h"
#include "AgColumnDef.h"
#include "AgTreeRow.h"

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
 * @brief 整行自定义比较函数（多规则排序之外的另一种排序方式）。
 *
 * 入参为两行的原始数据（QVariantMap，键为列 field）。返回值 <0 表示 a 排在 b 前，
 * >0 表示 b 排在 a 前，0 表示相等（稳定排序保持原顺序）。函数内部可自由读取
 * 任意列、组合派生逻辑（如先看 A 列取值区间再比 B 列），不受“列 + 升降序”
 * 规则的表达能力限制。
 */
using AgRowComparator = std::function<int(const QVariantMap &a, const QVariantMap &b)>;

/**
 * @brief AgGridWidget 的数据模型，与视图解耦。
 *
 * 职责：
 *  - 持有列定义树（支持分组列，形成多级表头）；
 *  - 持有原始行数据（QVector<QVariantMap>，键为列 field）；
 *  - 可选树形模式（setTreeData）：行数据带父子层级，按展开态铺平为可见行；
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
    /// 原始行 → 可见行；被过滤或折叠隐藏返回 -1。
    int visibleRowForOriginalRow(int originalRow) const;

    // ---- 树形数据 ----

    /// 以树形结构替换全部行数据（默认全部折叠，仅显示根行；
    /// 节点 expanded 字段可逐行指定初始态）。内部按深度优先铺平为原始行，
    /// 并维护父子 / 深度 / 展开态元数据。触发 rowsChanged + layoutChanged。
    /// 与 setRowData 互斥，后调用者生效。
    void setTreeData(const QVector<AgTreeRow> &roots);
    /// 是否处于树形模式（setTreeData 后为 true，setRowData / clear 后为 false）。
    bool isTreeMode() const;
    /// setTreeData 传入的原始树结构。
    const QVector<AgTreeRow> &treeData() const;

    /// 原始行的层级（根 = 0；非树模式的有效行返回 0，无效返回 -1）。
    int rowDepth(int originalRow) const;
    /// 原始行的父行；根行 / 非树模式返回 -1。
    int rowParent(int originalRow) const;
    /// 原始行的子行（原始行号，按传入顺序）；叶子行 / 非树模式返回空。
    QVector<int> rowChildren(int originalRow) const;
    /// 原始行是否有子行（决定是否显示展开箭头）。
    bool rowHasChildren(int originalRow) const;

    /// 原始行当前是否展开（叶子行 / 非树模式恒为 false）。
    bool isRowExpanded(int originalRow) const;
    /// 展开或折叠某个原始行（叶子行无效果）。触发 rowExpandedChanged + layoutChanged。
    void setRowExpanded(int originalRow, bool expanded);
    /// 全部展开。触发 layoutChanged（不逐行发 rowExpandedChanged）。
    void expandAll();
    /// 全部折叠。触发 layoutChanged。
    void collapseAll();
    /// 展开深度 ≤ depth 的所有行（可见行到达深度 depth + 1）。触发 layoutChanged。
    void expandToDepth(int depth);
    /// 折叠深度 ≥ depth 的所有行（可见行到达深度 depth）。触发 layoutChanged。
    void collapseToDepth(int depth);

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

    /// 设置整行自定义比较函数（AgRowComparator）：设置后立即生效并重新排序，
    /// 生效期间优先于 sort(rules)（规则保留但不参与比较）；传 nullptr 等价
    /// clearSortComparator()。树形模式作用于同级兄弟。触发 layoutChanged。
    void setSortComparator(const AgRowComparator &comparator);
    /// 清除自定义比较函数（保留规则排序），恢复规则 / 原始顺序（保留过滤）。触发 layoutChanged。
    void clearSortComparator();
    /// 是否设置了自定义比较函数。
    bool hasSortComparator() const;

    /// 设置过滤回调：对每行返回 true 表示保留。设置后立即生效。
    void setFilterCallback(const std::function<bool(const QVariantMap &row)> &callback);
    /// 清除过滤回调（恢复全部行）。
    void clearFilterCallback();
    /// 是否设置了过滤回调。
    bool hasFilterCallback() const;

    /// 行拖拽排序的模型入口：把一批原始行移动到 targetOriginalIndex 之前。
    /// 触发 rowsMoved + rowsChanged + layoutChanged。树形模式下忽略（行序由父子结构决定）。
    void moveRows(const QVector<int> &fromOriginalIndices, int targetOriginalIndex);

    /// 删除一批数据行（入参为删除前的原始行号，可乱序、可重复，内部自动去重校验）。
    /// 触发 rowsAboutToBeRemoved（删除前，旧数据仍可读取）+ rowsRemoved + layoutChanged。
    /// 注意：删除会使后续行的原始行号整体前移，跨删除保留的行号引用会失效；
    /// 树形模式下忽略（行序由父子结构决定，需直接操作树数据后 setTreeData）。
    void removeRows(const QVector<int> &originalIndices);

signals:
    /// 单元格数据变化（可见行坐标，闭区间范围）。
    void dataChanged(int topRow, int leftColumn, int bottomRow, int rightColumn);
    /// 整体重设行数据之前发出（setRowData / setTreeData / clear）。
    /// 此时旧数据仍可读取，视图可据此捕获数据切换动画所需的旧内容快照。
    void rowsAboutToReset();
    /// 删除一批行之前发出（removeRows），入参为删除前的原始行号。
    /// 此时旧数据仍可读取，视图可据此捕获行删除消失动画所需的旧行像素。
    void rowsAboutToBeRemoved(const QVector<int> &originalRows);
    /// 可见行集合内容变化（数据替换、过滤、排序、移动行）。
    void rowsChanged();
    /// 行拖拽移动完成（原始行号）。
    void rowsMoved(const QVector<int> &fromOriginalIndices, int targetOriginalIndex);
    /// 一批行已被删除（removeRows），入参为删除前的原始行号（随后续删除操作递减漂移）。
    void rowsRemoved(const QVector<int> &originalRows);
    /// 列定义被整体替换。
    void columnsChanged();
    /// 列宽变化。
    void columnWidthsChanged();
    /// 重大布局变化（setRowData / setColumnDefs / 排序过滤 / 行移动 / 展开折叠）；
    /// 视图会重置 AutoFit 缓存、重建列偏移，并清空合并单元格区域。
    void layoutChanged();
    /// 过滤状态变化。
    void filterChanged();
    /// 树形单行展开状态变化（原始行号；expandAll 等批量操作不逐行发送）。
    void rowExpandedChanged(int originalRow, bool expanded);

private:
    /// 重建 field → 列索引 哈希表（setColumnDefs 后调用）。
    void rebuildFieldIndex();
    /// 展开列定义树，生成叶子列列表 m_leafColumnDefs。
    void flattenLeaves();
    /// 计算表头层级数（列树最大深度）。
    int computeHeaderLevelCount() const;
    /// 按过滤回调 + 排序规则重建可见行映射。
    void applyFilterAndSort();
    /// 执行多规则排序（对可见数据排序；仅扁平模式）。
    void sortVisibleData();
    /// 树形模式：按过滤级联 + 同级兄弟排序 + 展开态重建可见行映射。
    void rebuildVisibleRowsTree();
    /// 把 m_treeRoots 铺平为原始行并重建树元数据（setTreeData 后调用）。
    void flattenTreeRows();
    /// 展开态批量修改的收尾：有变化时重建可见行并发 layoutChanged。
    void finishExpandedBatch(bool changed);

    QVector<AgColumnDef> m_topLevelColumnDefs;   ///< 顶层列定义树
    QVector<AgColumnDef *> m_leafColumnDefs;     ///< 展开后的叶子列（指向 m_topLevelColumnDefs 内部）
    QVector<QVariantMap> m_rowData;              ///< 可见数据（过滤排序后）
    QVector<QVariantMap> m_originalRowData;      ///< 原始数据
    QVector<int> m_visibleToOriginal;            ///< 可见行 → 原始行映射
    QVector<int> m_originalToVisible;            ///< 原始行 → 可见行映射（-1 = 被过滤）
    QHash<QString, int> m_fieldToColumn;         ///< field → 叶子列索引
    int m_headerLevelCount;                      ///< 表头层级数

    QList<AgSortRule> m_sortRules;               ///< 当前排序规则
    AgRowComparator m_sortComparator;            ///< 整行自定义比较函数（优先于规则）
    std::function<bool(const QVariantMap &row)> m_filterCallback;  ///< 过滤回调（true 保留）

    // ---- 树形数据 ----
    QVector<AgTreeRow> m_treeRoots;              ///< setTreeData 传入的原始树
    QVector<int> m_rowParent;                    ///< 原始行 → 父原始行（-1 = 根）
    QVector<QVector<int> > m_rowChildren;        ///< 原始行 → 子原始行列表（传入顺序）
    QVector<int> m_rowDepth;                     ///< 原始行 → 层级（根 = 0）
    QVector<bool> m_rowExpanded;                 ///< 原始行 → 展开态
    bool m_treeMode = false;                     ///< 是否树形模式
};

#endif // AGGRIDMODEL_H
