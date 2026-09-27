#ifndef AUSELECTMODEL_H
#define AUSELECTMODEL_H

#include <QAbstractListModel>
#include <QIcon>
#include <QSet>
#include <QSortFilterProxyModel>

/**
 * @brief AuSelect 下拉数据模型：扁平行结构（组名行 + 选项行交替）。
 *
 * 分组：addGroupHeader() 追加一个组名行，其后未分组的选项自动归属该组；
 *       groupId < 0 表示未分组选项（渲染时不缩进）。
 * 状态：勾选集合（多选）与键盘导航高亮行保存在模型内，通过自定义角色暴露给委托。
 */
class AuSelectListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    struct Record {
        enum Type { GroupHeader, Item };
        Record() : type(Item), enabled(true), groupId(-1) {}

        Type type;
        QString text;      ///< 组名 / 选项显示文本
        QVariant data;     ///< 选项用户数据（组名行无意义）
        QIcon icon;        ///< 选项图标（组名行无意义）
        bool enabled;      ///< 选项是否可用（组名行无意义）
        int groupId;       ///< 所属组 id（-1 = 未分组）
    };

    explicit AuSelectListModel(QObject *parent = nullptr);

    // ---- 数据维护 ----
    int appendGroupHeader(const QString &title);   ///< 返回组 id
    int appendItem(const QString &text, const QVariant &data,
                   const QIcon &icon, bool enabled, int groupId);
    int insertItem(int row, const QString &text, const QVariant &data,
                   const QIcon &icon, bool enabled, int groupId);
    void removeRecord(int row);
    void clear();
    void setRecordEnabled(int row, bool enabled);
    void setRecordIcon(int row, const QIcon &icon);

    // ---- 查询 ----
    const Record *recordAt(int row) const;
    int groupIdOfHeaderRow(int row) const;         ///< 组名行 -> 组 id（非组名行返回 -1）
    int count() const { return m_records.size(); }

    // ---- 状态（勾选 / 高亮）----
    const QSet<int> &checkedRows() const { return m_checkedRows; }
    void setChecked(int row, bool checked);
    void setCheckedRows(const QSet<int> &rows);
    void clearChecked();
    int highlightRow() const { return m_highlightRow; }
    void setHighlightRow(int row);

    // QAbstractListModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
    void touchRow(int row);                        ///< 对单行发出 dataChanged

    QVector<Record> m_records;
    QSet<int> m_checkedRows;
    int m_highlightRow;
    int m_nextGroupId;
};

/**
 * @brief 过滤代理：按显示文本做不区分大小写的包含匹配。
 *
 * 组名行在其组内任一选项命中时显示（与 ant-design 的分组过滤语义一致）。
 */
class AuSelectFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit AuSelectFilterProxyModel(QObject *parent = nullptr);

    void setFilterText(const QString &text);
    QString filterText() const { return m_filterText; }

    /// 过滤后第一个可见行的源行号（无可见行返回 -1）。
    int firstVisibleSourceRow() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_filterText;
};

#endif // AUSELECTMODEL_H
