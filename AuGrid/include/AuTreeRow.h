#ifndef AUTREEROW_H
#define AUTREEROW_H

#include <QVector>
#include <QVariantMap>

/**
 * @brief 树形表格的行节点：一行数据 + 子行集合。
 *
 * 通过 AuGridModel::setTreeData() 提供层级数据，模型内部按深度优先
 * 铺平为原始行，并维护父子 / 深度 / 展开态元数据。
 *
 * 与 QTreeView 多列表格的对应关系：
 *  - data 的每个键对应一列的 field，界面按列渲染；
 *  - children 非空的行显示展开箭头，可折叠 / 展开；
 *  - children 为空的行是叶子行。
 */
struct AuTreeRow {
    /// 本行的列数据（键 = 列 field；缺失的键按空值显示）。
    QVariantMap data;
    /// 子行集合；空 = 叶子行。
    QVector<AuTreeRow> children;
    /// setTreeData 时的初始展开态（默认折叠；后续请用模型的
    /// setRowExpanded / expandAll / expandToDepth 等函数控制，不会回写本结构）。
    bool expanded = false;

    AuTreeRow() = default;
    /// 便捷构造：仅携带行数据（叶子行）。
    AuTreeRow(const QVariantMap &row)
        : data(row)
    {}
};

#endif // AUTREEROW_H
