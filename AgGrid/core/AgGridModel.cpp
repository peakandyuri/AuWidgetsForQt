#include "AgGridModel.h"

#include <functional>
#include <algorithm>
#include <QMetaType>
#include <QDate>
#include <QDateTime>
#include <QTime>

AgGridModel::AgGridModel(QObject *parent)
    : QObject(parent)
    , m_headerLevelCount(1)
{
}

void AgGridModel::setColumnDefs(const QVector<AgColumnDef> &defs)
{
    m_topLevelColumnDefs = defs;
    flattenLeaves();
    rebuildFieldIndex();
    m_headerLevelCount = computeHeaderLevelCount();
    emit columnsChanged();
    emit layoutChanged();
}

const QVector<AgColumnDef> &AgGridModel::topLevelColumnDefs() const
{
    return m_topLevelColumnDefs;
}

void AgGridModel::setRowData(const QVector<QVariantMap> &rows)
{
    emit rowsAboutToReset();

    // 扁平入口：退出树形模式
    m_treeMode = false;
    m_treeRoots.clear();
    m_rowParent.clear();
    m_rowChildren.clear();
    m_rowDepth.clear();
    m_rowExpanded.clear();

    m_originalRowData = rows;
    applyFilterAndSort();
    emit rowsChanged();
    emit layoutChanged();
}

void AgGridModel::setTreeData(const QVector<AgTreeRow> &roots)
{
    emit rowsAboutToReset();

    m_treeMode = true;
    m_treeRoots = roots;
    flattenTreeRows();
    applyFilterAndSort();
    emit rowsChanged();
    emit layoutChanged();
}

bool AgGridModel::isTreeMode() const
{
    return m_treeMode;
}

const QVector<AgTreeRow> &AgGridModel::treeData() const
{
    return m_treeRoots;
}

int AgGridModel::rowDepth(int originalRow) const
{
    if (originalRow < 0 || originalRow >= m_originalRowData.size())
        return -1;
    if (!m_treeMode)
        return 0;
    return m_rowDepth.at(originalRow);
}

int AgGridModel::rowParent(int originalRow) const
{
    if (!m_treeMode || originalRow < 0 || originalRow >= m_rowParent.size())
        return -1;
    return m_rowParent.at(originalRow);
}

QVector<int> AgGridModel::rowChildren(int originalRow) const
{
    if (!m_treeMode || originalRow < 0 || originalRow >= m_rowChildren.size())
        return QVector<int>();
    return m_rowChildren.at(originalRow);
}

bool AgGridModel::rowHasChildren(int originalRow) const
{
    return !rowChildren(originalRow).isEmpty();
}

bool AgGridModel::isRowExpanded(int originalRow) const
{
    if (!m_treeMode || originalRow < 0 || originalRow >= m_rowExpanded.size())
        return false;
    return m_rowExpanded.at(originalRow);
}

void AgGridModel::setRowExpanded(int originalRow, bool expanded)
{
    if (!m_treeMode || originalRow < 0 || originalRow >= m_rowExpanded.size())
        return;
    if (m_rowChildren.at(originalRow).isEmpty())
        return;
    if (m_rowExpanded.at(originalRow) == expanded)
        return;
    m_rowExpanded[originalRow] = expanded;
    applyFilterAndSort();
    emit rowExpandedChanged(originalRow, expanded);
    emit layoutChanged();
}

void AgGridModel::expandAll()
{
    if (!m_treeMode)
        return;
    bool changed = false;
    for (int i = 0; i < m_rowExpanded.size(); ++i) {
        if (!m_rowChildren.at(i).isEmpty() && !m_rowExpanded.at(i)) {
            m_rowExpanded[i] = true;
            changed = true;
        }
    }
    finishExpandedBatch(changed);
}

void AgGridModel::collapseAll()
{
    collapseToDepth(0);
}

void AgGridModel::expandToDepth(int depth)
{
    if (!m_treeMode)
        return;
    bool changed = false;
    for (int i = 0; i < m_rowDepth.size(); ++i) {
        if (m_rowDepth.at(i) <= depth && !m_rowChildren.at(i).isEmpty()
            && !m_rowExpanded.at(i)) {
            m_rowExpanded[i] = true;
            changed = true;
        }
    }
    finishExpandedBatch(changed);
}

void AgGridModel::collapseToDepth(int depth)
{
    if (!m_treeMode)
        return;
    bool changed = false;
    for (int i = 0; i < m_rowDepth.size(); ++i) {
        if (m_rowDepth.at(i) >= depth && m_rowExpanded.at(i)) {
            m_rowExpanded[i] = false;
            changed = true;
        }
    }
    finishExpandedBatch(changed);
}

void AgGridModel::finishExpandedBatch(bool changed)
{
    if (!changed)
        return;
    applyFilterAndSort();
    emit layoutChanged();
}

void AgGridModel::flattenTreeRows()
{
    m_originalRowData.clear();
    m_rowParent.clear();
    m_rowChildren.clear();
    m_rowDepth.clear();
    m_rowExpanded.clear();

    std::function<void(const AgTreeRow &, int, int)> flatten =
        [&](const AgTreeRow &node, int parent, int depth) {
            const int index = m_originalRowData.size();
            m_originalRowData.append(node.data);
            m_rowParent.append(parent);
            m_rowDepth.append(depth);
            m_rowExpanded.append(node.expanded);
            m_rowChildren.append(QVector<int>());
            if (parent >= 0)
                m_rowChildren[parent].append(index);
            for (const AgTreeRow &child : node.children)
                flatten(child, index, depth + 1);
        };

    for (const AgTreeRow &root : m_treeRoots)
        flatten(root, -1, 0);
}

const QVector<QVariantMap> &AgGridModel::rowData() const
{
    return m_rowData;
}

const QVector<QVariantMap> &AgGridModel::originalRowData() const
{
    return m_originalRowData;
}

int AgGridModel::rowCount() const
{
    return m_rowData.size();
}

int AgGridModel::originalRowCount() const
{
    return m_originalRowData.size();
}

int AgGridModel::originalRowForVisibleRow(int visibleRow) const
{
    if (visibleRow < 0 || visibleRow >= m_visibleToOriginal.size())
        return -1;
    return m_visibleToOriginal.at(visibleRow);
}

int AgGridModel::visibleRowForOriginalRow(int originalRow) const
{
    if (originalRow < 0 || originalRow >= m_originalToVisible.size())
        return -1;
    return m_originalToVisible.at(originalRow);
}

int AgGridModel::columnCount() const
{
    return m_leafColumnDefs.size();
}

int AgGridModel::headerLevelCount() const
{
    return m_headerLevelCount;
}

int AgGridModel::columnIndexForField(const QString &field) const
{
    return m_fieldToColumn.value(field, -1);
}

QVector<QString> AgGridModel::leafFieldsForField(const QString &field) const
{
    QVector<QString> result;

    // 叶子字段优先：直接命中
    if (columnIndexForField(field) >= 0) {
        result.append(field);
        return result;
    }

    // 分组字段：递归查找，命中后返回该分组覆盖的所有叶子字段
    std::function<bool(const AgColumnDef &)> walk = [&](const AgColumnDef &def) -> bool {
        if (def.field() == field) {
            std::function<void(const AgColumnDef &)> collect = [&](const AgColumnDef &d) {
                if (d.isGroup()) {
                    for (const AgColumnDef &c : d.children())
                        collect(c);
                } else {
                    result.append(d.field());
                }
            };
            collect(def);
            return true;
        }
        if (def.isGroup()) {
            for (const AgColumnDef &c : def.children())
                if (walk(c))
                    return true;
        }
        return false;
    };

    for (const AgColumnDef &def : m_topLevelColumnDefs)
        if (walk(def))
            break;

    return result;
}

QString AgGridModel::fieldAtColumn(int column) const
{
    if (column < 0 || column >= m_leafColumnDefs.size())
        return QString();
    return m_leafColumnDefs.at(column)->field();
}

AgColumnDef AgGridModel::columnDefAt(int column) const
{
    if (column < 0 || column >= m_leafColumnDefs.size())
        return AgColumnDef();
    return *m_leafColumnDefs.at(column);
}

QVariant AgGridModel::cellValue(int row, int column) const
{
    if (row < 0 || row >= m_rowData.size())
        return QVariant();
    const QString field = fieldAtColumn(column);
    if (field.isEmpty())
        return QVariant();
    return m_rowData.at(row).value(field);
}

QVariant AgGridModel::cellValue(int row, const QString &field) const
{
    if (row < 0 || row >= m_rowData.size())
        return QVariant();
    return m_rowData.at(row).value(field);
}

bool AgGridModel::setCellValue(int row, const QString &field, const QVariant &value)
{
    if (row < 0 || row >= m_rowData.size())
        return false;
    const int col = columnIndexForField(field);
    if (col < 0)
        return false;
    m_rowData[row].insert(field, value);
    const int originalRow = m_visibleToOriginal.at(row);
    if (originalRow >= 0 && originalRow < m_originalRowData.size())
        m_originalRowData[originalRow].insert(field, value);
    emit dataChanged(row, col, row, col);
    return true;
}

bool AgGridModel::setCellValue(int row, int column, const QVariant &value)
{
    const QString field = fieldAtColumn(column);
    if (field.isEmpty())
        return false;
    return setCellValue(row, field, value);
}

QVariant AgGridModel::headerValue(int column) const
{
    if (column < 0 || column >= m_leafColumnDefs.size())
        return QVariant();
    return m_leafColumnDefs.at(column)->headerName();
}

void AgGridModel::setColumnWidth(int column, int width)
{
    if (column < 0 || column >= m_leafColumnDefs.size())
        return;

    AgColumnDef *def = m_leafColumnDefs[column];
    int clampedWidth = qMax(def->minWidth(), width);
    if (def->maxWidth() > 0)
        clampedWidth = qMin(def->maxWidth(), clampedWidth);

    if (def->width() == clampedWidth && def->widthMode() == AgColumnDef::ColumnWidthMode::Fixed)
        return;

    def->setWidth(clampedWidth);
    def->setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    // 只发 columnWidthsChanged：列宽变化不改变可见行 / 列坐标，
    // 发 layoutChanged 会让视图清空合并单元格与选中（坐标其实全部有效）
    emit columnWidthsChanged();
}

void AgGridModel::setColumnWidth(const QString &field, int width)
{
    const int column = columnIndexForField(field);
    if (column >= 0)
        setColumnWidth(column, width);
}

int AgGridModel::columnWidth(int column) const
{
    if (column < 0 || column >= m_leafColumnDefs.size())
        return 0;
    return m_leafColumnDefs.at(column)->width();
}

int AgGridModel::columnWidth(const QString &field) const
{
    const int column = columnIndexForField(field);
    return column >= 0 ? m_leafColumnDefs.at(column)->width() : 0;
}

void AgGridModel::setColumnPinMode(int column, AgColumnDef::PinMode mode)
{
    if (column < 0 || column >= m_leafColumnDefs.size())
        return;

    AgColumnDef *def = m_leafColumnDefs[column];
    if (def->pinMode() == mode)
        return;

    def->setPinMode(mode);
    // 只发 columnsChanged：停靠变化不改变可见行 / 列坐标，
    // 发 layoutChanged 会让视图清空合并单元格与选中（坐标其实全部有效）
    emit columnsChanged();
}

void AgGridModel::setColumnPinMode(const QString &field, AgColumnDef::PinMode mode)
{
    const int column = columnIndexForField(field);
    if (column >= 0)
        setColumnPinMode(column, mode);
}

AgColumnDef::PinMode AgGridModel::columnPinMode(int column) const
{
    if (column < 0 || column >= m_leafColumnDefs.size())
        return AgColumnDef::PinMode::None;
    return m_leafColumnDefs.at(column)->pinMode();
}

void AgGridModel::clear()
{
    emit rowsAboutToReset();

    m_rowData.clear();
    m_originalRowData.clear();
    m_visibleToOriginal.clear();
    m_originalToVisible.clear();
    m_topLevelColumnDefs.clear();
    m_leafColumnDefs.clear();
    m_fieldToColumn.clear();
    m_headerLevelCount = 1;
    m_sortRules.clear();
    m_filterCallback = nullptr;
    m_treeMode = false;
    m_treeRoots.clear();
    m_rowParent.clear();
    m_rowChildren.clear();
    m_rowDepth.clear();
    m_rowExpanded.clear();
    emit rowsChanged();
    emit columnsChanged();
    emit layoutChanged();
    emit filterChanged();
}

static bool variantLessThan(const QVariant &lhs, const QVariant &rhs)
{
    if (lhs == rhs)
        return false;
    if (!lhs.isValid() || lhs.isNull())
        return false;
    if (!rhs.isValid() || rhs.isNull())
        return true;

    const int lType = lhs.userType();
    const int rType = rhs.userType();
    if (lType == rType) {
        switch (lType) {
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Short:
        case QMetaType::UShort:
        case QMetaType::Char:
        case QMetaType::UChar:
        case QMetaType::Bool:
        case QMetaType::Long:
        case QMetaType::ULong:
            return lhs.toLongLong() < rhs.toLongLong();
        case QMetaType::Double:
        case QMetaType::Float:
            return lhs.toDouble() < rhs.toDouble();
        case QMetaType::QDate:
            return lhs.toDate() < rhs.toDate();
        case QMetaType::QDateTime:
            return lhs.toDateTime() < rhs.toDateTime();
        case QMetaType::QTime:
            return lhs.toTime() < rhs.toTime();
        default:
            break;
        }
    }
    return lhs.toString().localeAwareCompare(rhs.toString()) < 0;
}

void AgGridModel::sort(int column, Qt::SortOrder order)
{
    QList<AgSortRule> rules;
    rules << AgSortRule{column, order};
    sort(rules);
}

void AgGridModel::sort(const QList<AgSortRule> &rules)
{
    if (rules.isEmpty()) {
        clearSort();
        return;
    }

    // 过滤出有效规则
    QList<AgSortRule> validRules;
    validRules.reserve(rules.size());
    for (const AgSortRule &rule : rules) {
        if (rule.column < 0 || rule.column >= m_leafColumnDefs.size())
            continue;
        const QString field = m_leafColumnDefs.at(rule.column)->field();
        if (field.isEmpty())
            continue;
        validRules.append(rule);
    }

    if (validRules.isEmpty()) {
        clearSort();
        return;
    }

    m_sortRules = validRules;
    if (m_treeMode)
        applyFilterAndSort();   // 树模式：排序在同级兄弟铺平时生效
    else
        sortVisibleData();
    emit layoutChanged();
}

void AgGridModel::clearSort()
{
    if (m_sortRules.isEmpty() && !hasFilterCallback())
        return;

    m_sortRules.clear();
    applyFilterAndSort();
    emit layoutChanged();
}

void AgGridModel::setFilterCallback(const std::function<bool(const QVariantMap &row)> &callback)
{
    m_filterCallback = callback;
    applyFilterAndSort();
    emit layoutChanged();
    emit filterChanged();
}

void AgGridModel::clearFilterCallback()
{
    if (!m_filterCallback)
        return;

    m_filterCallback = nullptr;
    applyFilterAndSort();
    emit layoutChanged();
    emit filterChanged();
}

bool AgGridModel::hasFilterCallback() const
{
    return static_cast<bool>(m_filterCallback);
}

void AgGridModel::moveRows(const QVector<int> &fromOriginalIndices, int targetOriginalIndex)
{
    // 树形模式行序由父子结构与展开态决定，扁平移动会破坏层级，直接忽略
    if (m_treeMode)
        return;

    if (fromOriginalIndices.isEmpty())
        return;

    const int originalCount = m_originalRowData.size();
    if (originalCount == 0)
        return;

    QVector<int> from = fromOriginalIndices;
    std::sort(from.begin(), from.end());
    from.erase(std::unique(from.begin(), from.end()), from.end());

    for (int idx : from) {
        if (idx < 0 || idx >= originalCount)
            return;
    }

    const int count = from.size();
    if (targetOriginalIndex < 0)
        targetOriginalIndex = 0;
    if (targetOriginalIndex > originalCount - count)
        targetOriginalIndex = originalCount - count;

    bool contiguous = true;
    for (int i = 1; i < from.size(); ++i) {
        if (from.at(i) != from.at(i - 1) + 1) {
            contiguous = false;
            break;
        }
    }
    if (contiguous && from.first() == targetOriginalIndex)
        return;

    QVector<QVariantMap> movedRows;
    movedRows.reserve(count);
    for (int idx : from)
        movedRows.append(m_originalRowData.at(idx));

    for (int i = from.size() - 1; i >= 0; --i)
        m_originalRowData.removeAt(from.at(i));

    for (int i = 0; i < movedRows.size(); ++i)
        m_originalRowData.insert(targetOriginalIndex + i, movedRows.at(i));

    applyFilterAndSort();
    emit rowsMoved(from, targetOriginalIndex);
    emit rowsChanged();
    emit layoutChanged();
}

void AgGridModel::applyFilterAndSort()
{
    m_rowData.clear();
    m_visibleToOriginal.clear();

    const int originalCount = m_originalRowData.size();
    m_originalToVisible.fill(-1, originalCount);

    if (originalCount == 0)
        return;

    if (m_treeMode) {
        // 树模式：过滤级联 + 同级兄弟排序 + 按展开态铺平在重建时一次完成
        rebuildVisibleRowsTree();
        return;
    }

    if (m_filterCallback) {
        for (int i = 0; i < originalCount; ++i) {
            if (m_filterCallback(m_originalRowData.at(i))) {
                m_rowData.append(m_originalRowData.at(i));
                m_visibleToOriginal.append(i);
                m_originalToVisible[i] = m_rowData.size() - 1;
            }
        }
    } else {
        m_rowData = m_originalRowData;
        m_visibleToOriginal.resize(originalCount);
        for (int i = 0; i < originalCount; ++i) {
            m_visibleToOriginal[i] = i;
            m_originalToVisible[i] = i;
        }
    }

    sortVisibleData();
}

void AgGridModel::rebuildVisibleRowsTree()
{
    const int originalCount = m_originalRowData.size();

    // 1. 过滤命中标记（无过滤回调 = 全部命中）
    QVector<bool> kept(originalCount, true);
    if (m_filterCallback) {
        for (int i = 0; i < originalCount; ++i)
            kept[i] = m_filterCallback(m_originalRowData.at(i));
    }

    // 2. 级联保留：命中行的祖先链保留显示（后代是否露出仍取决于各级展开态）
    for (int i = originalCount - 1; i >= 0; --i) {
        if (!kept.at(i))
            continue;
        int parent = m_rowParent.at(i);
        while (parent >= 0 && !kept.at(parent)) {
            kept[parent] = true;
            parent = m_rowParent.at(parent);
        }
    }

    // 3. 深度优先铺平：同级兄弟按排序规则稳定排序后依次输出；
    //    仅展开的行继续下钻（默认折叠）。
    auto compareOriginal = [this](int a, int b) -> bool {
        for (const AgSortRule &rule : m_sortRules) {
            const QString field = fieldAtColumn(rule.column);
            if (field.isEmpty())
                continue;
            const QVariant va = m_originalRowData.at(a).value(field);
            const QVariant vb = m_originalRowData.at(b).value(field);
            if (va == vb)
                continue;
            const bool less = variantLessThan(va, vb);
            return rule.order == Qt::AscendingOrder ? less : !less;
        }
        return false;
    };

    std::function<void(int)> emitChildren;
    std::function<void(int)> emitRow = [&](int originalRow) {
        if (!kept.at(originalRow))
            return;
        m_rowData.append(m_originalRowData.at(originalRow));
        m_visibleToOriginal.append(originalRow);
        m_originalToVisible[originalRow] = m_rowData.size() - 1;
        if (m_rowExpanded.at(originalRow))
            emitChildren(originalRow);
    };
    emitChildren = [&](int parentRow) {
        QVector<int> children = m_rowChildren.at(parentRow);
        if (children.size() > 1)
            std::stable_sort(children.begin(), children.end(), compareOriginal);
        for (int child : children)
            emitRow(child);
    };

    QVector<int> roots;
    for (int i = 0; i < originalCount; ++i) {
        if (m_rowParent.at(i) < 0)
            roots.append(i);
    }
    if (roots.size() > 1)
        std::stable_sort(roots.begin(), roots.end(), compareOriginal);
    for (int root : roots)
        emitRow(root);
}

void AgGridModel::sortVisibleData()
{
    if (m_sortRules.isEmpty() || m_rowData.size() < 2)
        return;

    auto compareRows = [&](const QVariantMap &a, const QVariantMap &b) {
        for (const AgSortRule &rule : m_sortRules) {
            const QString field = fieldAtColumn(rule.column);
            if (field.isEmpty())
                continue;
            const QVariant va = a.value(field);
            const QVariant vb = b.value(field);
            if (va == vb)
                continue;
            const bool less = variantLessThan(va, vb);
            return rule.order == Qt::AscendingOrder ? less : !less;
        }
        return false;
    };

    QVector<int> order(m_rowData.size());
    for (int i = 0; i < order.size(); ++i)
        order[i] = i;

    std::stable_sort(order.begin(), order.end(),
                     [&](int a, int b) { return compareRows(m_rowData.at(a), m_rowData.at(b)); });

    QVector<QVariantMap> sortedData;
    QVector<int> sortedMapping;
    sortedData.reserve(m_rowData.size());
    sortedMapping.reserve(m_rowData.size());
    for (int idx : order) {
        sortedData.append(m_rowData.at(idx));
        sortedMapping.append(m_visibleToOriginal.at(idx));
    }

    m_rowData = std::move(sortedData);
    m_visibleToOriginal = std::move(sortedMapping);

    // 重新建立原始行到可见行的映射
    m_originalToVisible.fill(-1, m_originalRowData.size());
    for (int visibleRow = 0; visibleRow < m_visibleToOriginal.size(); ++visibleRow) {
        const int originalRow = m_visibleToOriginal.at(visibleRow);
        if (originalRow >= 0)
            m_originalToVisible[originalRow] = visibleRow;
    }
}

void AgGridModel::rebuildFieldIndex()
{
    m_fieldToColumn.clear();
    for (int i = 0; i < m_leafColumnDefs.size(); ++i)
        m_fieldToColumn.insert(m_leafColumnDefs.at(i)->field(), i);
}

void AgGridModel::flattenLeaves()
{
    m_leafColumnDefs.clear();

    QVector<const AgColumnDef *> stack;
    for (int i = m_topLevelColumnDefs.size() - 1; i >= 0; --i)
        stack.append(&m_topLevelColumnDefs[i]);

    while (!stack.isEmpty()) {
        const AgColumnDef *def = stack.takeLast();
        if (def->isGroup()) {
            const QVector<AgColumnDef> &children = def->children();
            for (int i = children.size() - 1; i >= 0; --i)
                stack.append(&children[i]);
        } else {
            m_leafColumnDefs.append(const_cast<AgColumnDef *>(def));
        }
    }
}

int AgGridModel::computeHeaderLevelCount() const
{
    int maxDepth = 0;

    std::function<void(const AgColumnDef &, int)> traverse = [&](const AgColumnDef &def, int depth) {
        if (def.isGroup()) {
            for (const AgColumnDef &child : def.children())
                traverse(child, depth + 1);
        } else {
            maxDepth = qMax(maxDepth, depth);
        }
    };

    for (const AgColumnDef &def : m_topLevelColumnDefs)
        traverse(def, 0);

    return maxDepth + 1;
}
