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

struct AgSortRule {
    int column = -1;
    Qt::SortOrder order = Qt::AscendingOrder;

    AgSortRule() = default;
    AgSortRule(int c, Qt::SortOrder o)
        : column(c)
        , order(o)
    {}
};

class AGGRID_EXPORT AgGridModel : public QObject
{
    Q_OBJECT

public:
    explicit AgGridModel(QObject *parent = nullptr);

    void setColumnDefs(const QVector<AgColumnDef> &defs);
    const QVector<AgColumnDef> &topLevelColumnDefs() const;

    void setRowData(const QVector<QVariantMap> &rows);
    const QVector<QVariantMap> &rowData() const;
    const QVector<QVariantMap> &originalRowData() const;

    int rowCount() const;
    int columnCount() const;
    int headerLevelCount() const;

    int originalRowCount() const;
    int originalRowForVisibleRow(int visibleRow) const;
    int visibleRowForOriginalRow(int originalRow) const;

    int columnIndexForField(const QString &field) const;
    QVector<QString> leafFieldsForField(const QString &field) const;
    QString fieldAtColumn(int column) const;
    AgColumnDef columnDefAt(int column) const;

    void setColumnPinMode(int column, AgColumnDef::PinMode mode);
    void setColumnPinMode(const QString &field, AgColumnDef::PinMode mode);
    AgColumnDef::PinMode columnPinMode(int column) const;

    QVariant cellValue(int row, int column) const;
    QVariant cellValue(int row, const QString &field) const;
    bool setCellValue(int row, const QString &field, const QVariant &value);
    bool setCellValue(int row, int column, const QVariant &value);

    QVariant headerValue(int column) const;

    void setColumnWidth(int column, int width);
    void setColumnWidth(const QString &field, int width);
    int columnWidth(int column) const;
    int columnWidth(const QString &field) const;

    void clear();

    void sort(int column, Qt::SortOrder order);
    void sort(const QList<AgSortRule> &rules);
    void clearSort();

    void setFilterCallback(const std::function<bool(const QVariantMap &row)> &callback);
    void clearFilterCallback();
    bool hasFilterCallback() const;

    void moveRows(const QVector<int> &fromOriginalIndices, int targetOriginalIndex);

signals:
    void dataChanged(int topRow, int leftColumn, int bottomRow, int rightColumn);
    void rowsChanged();
    void rowsMoved(const QVector<int> &fromOriginalIndices, int targetOriginalIndex);
    void columnsChanged();
    void columnWidthsChanged();
    void layoutChanged();
    void filterChanged();

private:
    void rebuildFieldIndex();
    void flattenLeaves();
    int computeHeaderLevelCount() const;
    void applyFilterAndSort();
    void sortVisibleData();

    QVector<AgColumnDef> m_topLevelColumnDefs;
    QVector<AgColumnDef *> m_leafColumnDefs;
    QVector<QVariantMap> m_rowData;
    QVector<QVariantMap> m_originalRowData;
    QVector<int> m_visibleToOriginal;
    QVector<int> m_originalToVisible;
    QHash<QString, int> m_fieldToColumn;
    int m_headerLevelCount;

    QList<AgSortRule> m_sortRules;
    std::function<bool(const QVariantMap &row)> m_filterCallback;
};

#endif // AGGRIDMODEL_H
