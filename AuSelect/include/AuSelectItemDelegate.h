#ifndef AUSELECTITEMDELEGATE_H
#define AUSELECTITEMDELEGATE_H

#include "AuSelect_global.h"
#include "AuSelectStyle.h"

#include <QStyledItemDelegate>

/**
 * 自定义数据角色：AuSelect 内置模型与委托之间的约定。
 * 自定义委托（AuSelectItemDelegate 子类）通过这些角色读取行类型、可用性等状态。
 */
namespace AuSelectItemRole {
enum {
    RowType = Qt::UserRole + 1,   ///< int：RowGroupHeader / RowItem
    ItemEnabled,                  ///< bool：选项是否可用（禁用项灰显且不可选）
    ItemChecked,                  ///< bool：是否处于选中 / 勾选状态
    ItemHighlighted,              ///< bool：是否为键盘导航当前高亮行
    ItemInGroup,                  ///< bool：选项是否属于某个分组（渲染时缩进）
    UserData                      ///< QVariant：addItem 时传入的用户数据
};
enum RowType {
    RowGroupHeader = 0,           ///< 组名行：仅渲染文本，无 hover / 不可选
    RowItem = 1                   ///< 选项行
};
}

/**
 * @brief AuSelect 下拉项委托基类：内置样式化渲染（背景 / hover / 选中 / 禁用 /
 *        分组缩进 / 对勾 / 图标 / 过滤命中高亮），子类可完全接管或调用基类后补充绘制。
 *
 * 替换方式：select->setItemDelegate(new MyDelegate(select))。
 * 典型自定义：重写 paint()，调用 AuSelectItemDelegate::paint() 先画底，再绘制角标等；
 * 或在 sizeHint() 中返回更高的行高以承载复杂内容。
 */
class AUSELECT_EXPORT AuSelectItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /// style 指针由 AuSelect 管理，生命周期与控件一致，不允许为 nullptr。
    explicit AuSelectItemDelegate(AuSelectStyle *style, QObject *parent = nullptr);

    AuSelectStyle *style() const;
    /// 切换样式实例（AuSelect::setStyle 时同步调用；自定义子类如缓存了样式也应重写响应）。
    void setStyle(AuSelectStyle *style);

    /// 当前过滤文本（用于命中文字高亮；空串 = 无高亮）。
    void setFilterText(const QString &text);
    QString filterText() const;

    /// 多选模式：选中项右侧绘制对勾；单选模式不绘制。
    void setMultiSelection(bool multi);
    bool isMultiSelection() const;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

protected:
    /// 绘制“对勾”标记（子类可复用）。
    void drawCheckmark(QPainter *painter, const QRectF &rect, const QColor &color) const;

private:
    void drawItemText(QPainter *painter, const QRect &rect, const QString &text,
                      const QFont &font, const QColor &color) const;

    AuSelectStyle *m_style;
    QString m_filterText;
    bool m_multiSelection;
};

#endif // AUSELECTITEMDELEGATE_H
