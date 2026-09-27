#include "AuSelectModel.h"
#include "AuSelectItemDelegate.h"

// ---------------------------------------------------------------------------
// AuSelectListModel

AuSelectListModel::AuSelectListModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_highlightRow(-1)
    , m_nextGroupId(0)
{
}

int AuSelectListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_records.size();
}

Qt::ItemFlags AuSelectListModel::flags(const QModelIndex &index) const
{
    Q_UNUSED(index);
    // 交互语义（可选 / 禁用）由委托与 AuSelect 根据自定义角色决定，
    // 这里统一允许事件派发，避免 QListView 自身把禁用行完全隔离。
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QVariant AuSelectListModel::data(const QModelIndex &index, int role) const
{
    const int row = index.row();
    if (row < 0 || row >= m_records.size())
        return QVariant();

    const Record &record = m_records.at(row);
    switch (role) {
    case Qt::DisplayRole:
        return record.text;
    case Qt::DecorationRole:
        return record.type == Record::Item ? record.icon : QIcon();
    case AuSelectItemRole::RowType:
        return record.type == Record::GroupHeader
                   ? int(AuSelectItemRole::RowGroupHeader)
                   : int(AuSelectItemRole::RowItem);
    case AuSelectItemRole::ItemEnabled:
        return record.type == Record::Item ? record.enabled : false;
    case AuSelectItemRole::ItemChecked:
        return record.type == Record::Item && m_checkedRows.contains(row);
    case AuSelectItemRole::ItemHighlighted:
        return row == m_highlightRow && record.type == Record::Item;
    case AuSelectItemRole::ItemInGroup:
        return record.type == Record::Item && record.groupId >= 0;
    case AuSelectItemRole::UserData:
        return record.type == Record::Item ? record.data : QVariant();
    default:
        return QVariant();
    }
}

int AuSelectListModel::appendGroupHeader(const QString &title)
{
    const int row = m_records.size();
    beginInsertRows(QModelIndex(), row, row);
    Record record;
    record.type = Record::GroupHeader;
    record.text = title;
    record.groupId = m_nextGroupId++;
    m_records.append(record);
    endInsertRows();
    return record.groupId;
}

int AuSelectListModel::appendItem(const QString &text, const QVariant &data,
                                  const QIcon &icon, bool enabled, int groupId)
{
    return insertItem(m_records.size(), text, data, icon, enabled, groupId);
}

int AuSelectListModel::insertItem(int row, const QString &text, const QVariant &data,
                                  const QIcon &icon, bool enabled, int groupId)
{
    row = qBound(0, row, m_records.size());
    beginInsertRows(QModelIndex(), row, row);
    Record record;
    record.type = Record::Item;
    record.text = text;
    record.data = data;
    record.icon = icon;
    record.enabled = enabled;
    record.groupId = groupId;
    m_records.insert(row, record);
    endInsertRows();
    return row;
}

void AuSelectListModel::removeRecord(int row)
{
    if (row < 0 || row >= m_records.size())
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_records.removeAt(row);
    endRemoveRows();

    // 行号整体前移：修正勾选集合与高亮行
    QSet<int> shifted;
    for (int checkedRow : qAsConst(m_checkedRows)) {
        if (checkedRow == row)
            continue;
        shifted.insert(checkedRow > row ? checkedRow - 1 : checkedRow);
    }
    m_checkedRows = shifted;
    if (m_highlightRow == row)
        m_highlightRow = -1;
    else if (m_highlightRow > row)
        --m_highlightRow;
}

void AuSelectListModel::clear()
{
    beginResetModel();
    m_records.clear();
    m_checkedRows.clear();
    m_highlightRow = -1;
    m_nextGroupId = 0;
    endResetModel();
}

void AuSelectListModel::setRecordEnabled(int row, bool enabled)
{
    if (row < 0 || row >= m_records.size())
        return;
    m_records[row].enabled = enabled;
    touchRow(row);
}

void AuSelectListModel::setRecordIcon(int row, const QIcon &icon)
{
    if (row < 0 || row >= m_records.size())
        return;
    m_records[row].icon = icon;
    touchRow(row);
}

const AuSelectListModel::Record *AuSelectListModel::recordAt(int row) const
{
    return (row >= 0 && row < m_records.size()) ? &m_records.at(row) : nullptr;
}

int AuSelectListModel::groupIdOfHeaderRow(int row) const
{
    if (row < 0 || row >= m_records.size())
        return -1;
    const Record &record = m_records.at(row);
    return record.type == Record::GroupHeader ? record.groupId : -1;
}

void AuSelectListModel::setChecked(int row, bool checked)
{
    if (row < 0 || row >= m_records.size())
        return;
    if (m_records.at(row).type != Record::Item)
        return;
    if (checked)
        m_checkedRows.insert(row);
    else
        m_checkedRows.remove(row);
    touchRow(row);
}

void AuSelectListModel::setCheckedRows(const QSet<int> &rows)
{
    QSet<int> affected = m_checkedRows | rows;
    m_checkedRows = rows;
    for (int row : qAsConst(affected))
        touchRow(row);
}

void AuSelectListModel::clearChecked()
{
    const QSet<int> affected = m_checkedRows;
    m_checkedRows.clear();
    for (int row : qAsConst(affected))
        touchRow(row);
}

void AuSelectListModel::setHighlightRow(int row)
{
    if (row == m_highlightRow)
        return;
    const int previous = m_highlightRow;
    m_highlightRow = row;
    if (previous >= 0)
        touchRow(previous);
    if (row >= 0)
        touchRow(row);
}

void AuSelectListModel::touchRow(int row)
{
    if (row < 0 || row >= m_records.size())
        return;
    emit dataChanged(index(row), index(row));
}

// ---------------------------------------------------------------------------
// AuSelectFilterProxyModel

AuSelectFilterProxyModel::AuSelectFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(false);
}

void AuSelectFilterProxyModel::setFilterText(const QString &text)
{
    if (m_filterText == text)
        return;
    m_filterText = text;
    invalidateFilter();
}

int AuSelectFilterProxyModel::firstVisibleSourceRow() const
{
    const QAbstractItemModel *source = sourceModel();
    if (!source)
        return -1;
    for (int row = 0; row < source->rowCount(); ++row) {
        if (filterAcceptsRow(row, QModelIndex()))
            return row;
    }
    return -1;
}

bool AuSelectFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    Q_UNUSED(sourceParent);
    const QAbstractItemModel *source = sourceModel();
    if (!source)
        return false;

    if (m_filterText.isEmpty())
        return true;

    const int type = source->index(sourceRow, 0).data(AuSelectItemRole::RowType).toInt();
    if (type == AuSelectItemRole::RowItem) {
        const QString text = source->index(sourceRow, 0).data(Qt::DisplayRole).toString();
        return text.contains(m_filterText, Qt::CaseInsensitive);
    }

    // 组名行：组内任一选项命中即显示
    for (int row = sourceRow + 1; row < source->rowCount(); ++row) {
        const QModelIndex index = source->index(row, 0);
        if (index.data(AuSelectItemRole::RowType).toInt() == AuSelectItemRole::RowGroupHeader)
            break;
        const QString text = index.data(Qt::DisplayRole).toString();
        if (text.contains(m_filterText, Qt::CaseInsensitive))
            return true;
    }
    return false;
}
