#ifndef AGCOLUMNDEF_H
#define AGCOLUMNDEF_H

#include "AgGrid_global.h"

#include <QString>
#include <QVariant>
#include <QVector>
#include <QFont>
#include <QColor>

/**
 * @brief 列定义（值类型，可拷贝），描述一列或一个分组列的全部属性。
 *
 * 列定义是树：分组列通过 setChildren()/addChild() 携带子列，
 * 在 AgGridWidget 中展开为多级表头；叶子列才是实际承载数据的列。
 *
 * 默认值速查：
 *  - width=120，minWidth=40，maxWidth=0（不限制），widthMode=Fixed，flexWeight=1
 *  - sortable=false，editable=false，filterable=false，resizable=true，visible=true
 *  - pinMode=None，alignment=AlignLeft|AlignVCenter，textWrapMode=NoWrap
 *  - checkbox=false，headerAlignment=AlignCenter，表头字体/前景色未设置（使用样式默认）
 *
 * 典型用法：
 * @code
 *   AgColumnDef name("name", "姓名");   // field, 表头名（缺省同 field）
 *   name.setWidth(120).setSortable(true);   // 链式需各 setter 返回引用时自行封装
 *   AgColumnDef group("Multi");
 *   group.addChild(AgColumnDef("a", "A"));
 *   group.addChild(AgColumnDef("b", "B"));  // 分组列 → 多级表头
 * @endcode
 */
class AGGRID_EXPORT AgColumnDef
{
public:
    /// 列宽模式。
    enum class ColumnWidthMode {
        Fixed,   ///< 固定像素：使用 width() 的值
        AutoFit, ///< 内容自适应：按表头与可视内容测量（只增不减），受 minWidth/maxWidth 约束
        Flex     ///< 弹性填充：按 flexWeight() 权重分享剩余空间，受 minWidth() 约束
    };

    /// 列停靠模式：停靠列在水平滚动时保持可见。
    enum class PinMode {
        None,  ///< 不停靠
        Left,  ///< 停靠到左侧
        Right  ///< 停靠到右侧
    };

    /// 文本换行模式（影响绘制与行高）。
    enum class TextWrapMode {
        NoWrap,          ///< 单行，超出部分被裁剪
        NoWrapTruncate,  ///< 单行，超出部分显示省略号
        NoWrapOverflow,  ///< 单行，允许溢出到相邻单元格
        WordWrap         ///< 根据列宽自动换行，行高按内容自适应
    };

    AgColumnDef();
    /// 构造列定义；headerName 缺省时使用 field。
    explicit AgColumnDef(const QString &field, const QString &headerName = QString());

    // ---- 基本属性 ----

    /// 数据键名：行 QVariantMap 中取值的键。
    QString field() const;
    void setField(const QString &field);

    /// 表头显示文字（默认同 field）。
    QString headerName() const;
    void setHeaderName(const QString &headerName);

    /// Fixed 模式列宽（像素），默认 120。
    int width() const;
    void setWidth(int width);

    /// 最小列宽（默认 40），对 AutoFit / Flex / 手动拖宽均生效。
    int minWidth() const;
    void setMinWidth(int minWidth);

    /// 最大列宽（默认 0 表示不限制）。
    int maxWidth() const;
    void setMaxWidth(int maxWidth);

    /// 列宽模式（默认 Fixed）。
    ColumnWidthMode widthMode() const;
    void setWidthMode(ColumnWidthMode mode);

    /// Flex 模式权重（默认 1），数值越大分得剩余空间越多。
    int flexWeight() const;
    void setFlexWeight(int weight);

    // ---- 行为开关 ----

    /// 允许点击表头排序（还需 AgGridWidget::setSortingEnabled(true)）。
    bool isSortable() const;
    void setSortable(bool sortable);

    /// 允许编辑（复选框列表示允许点击切换；双击/回车/输入字符进入编辑）。
    bool isEditable() const;
    void setEditable(bool editable);

    /// 允许过滤（预留）。
    bool isFilterable() const;
    void setFilterable(bool filterable);

    /// 初始可见性（默认 true；运行时显隐用 AgGridWidget::setColumnVisible）。
    bool isVisible() const;
    void setVisible(bool visible);

    /// 允许拖动表头边缘调宽 / 参与 AutoFit（默认 true）。
    bool isResizable() const;
    void setResizable(bool resizable);

    /// 停靠模式（默认 None）。
    PinMode pinMode() const;
    void setPinMode(PinMode mode);

    // ---- 插拔组件（按注册名引用）----

    /// 单元格渲染器注册名；空 = 默认文本渲染器，checkbox 列强制内置复选框渲染器。
    QString cellRenderer() const;
    void setCellRenderer(const QString &renderer);

    /// 表头渲染器注册名；空 = 标准表头绘制。
    QString headerRenderer() const;
    void setHeaderRenderer(const QString &renderer);

    /// 单元格编辑器注册名；空 = 默认单行编辑器。
    QString cellEditor() const;
    void setCellEditor(const QString &editor);

    // ---- 显示 ----

    /// 复选框列：值按 bool 渲染为复选框，点击 / 空格直接切换（配合 setEditable(true)）。
    bool isCheckbox() const;
    void setCheckbox(bool checkbox);

    /// 单元格文字对齐（默认 AlignLeft|AlignVCenter）。
    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    /// 换行模式（默认 NoWrap；WordWrap 时行高自适应内容）。
    TextWrapMode textWrapMode() const;
    void setTextWrapMode(TextWrapMode mode);

    // ---- 表头覆盖项（优先级高于样式默认值）----

    /// 表头字体；hasHeaderFont() 为 false 时使用样式 HeaderFont。
    QFont headerFont() const;
    void setHeaderFont(const QFont &font);
    bool hasHeaderFont() const;

    /// 表头前景色；hasHeaderForegroundColor() 为 false 时使用样式 HeaderForegroundColor。
    QColor headerForegroundColor() const;
    void setHeaderForegroundColor(const QColor &color);
    bool hasHeaderForegroundColor() const;

    /// 表头对齐；hasHeaderAlignment() 为 false 时使用样式 HeaderAlignment。
    Qt::Alignment headerAlignment() const;
    void setHeaderAlignment(Qt::Alignment alignment);
    bool hasHeaderAlignment() const;

    // ---- 分组（多级表头）----

    /// 是否分组列（设置了子列即为分组）。
    bool isGroup() const;
    /// 子列列表（叶子列该列表为空）。
    const QVector<AgColumnDef> &children() const;
    /// 整体替换子列。
    void setChildren(const QVector<AgColumnDef> &children);
    /// 追加一个子列。
    void addChild(const AgColumnDef &child);
    /// 子列数量。
    int childCount() const;

private:
    QString m_field;
    QString m_headerName;
    int m_width;
    int m_minWidth;
    int m_maxWidth;
    ColumnWidthMode m_widthMode;
    int m_flexWeight;
    bool m_sortable;
    bool m_editable;
    bool m_filterable;
    bool m_visible;
    bool m_resizable;
    PinMode m_pinMode;
    QString m_cellRenderer;
    QString m_headerRenderer;
    QString m_cellEditor;
    bool m_checkbox;
    Qt::Alignment m_alignment;
    TextWrapMode m_textWrapMode;
    QFont m_headerFont;
    QColor m_headerForegroundColor;
    Qt::Alignment m_headerAlignment;
    bool m_headerFontSet;             ///< 是否显式设置过表头字体
    bool m_headerForegroundColorSet;  ///< 是否显式设置过表头前景色
    bool m_headerAlignmentSet;        ///< 是否显式设置过表头对齐
    QVector<AgColumnDef> m_children;  ///< 子列（分组列使用）
};

#endif // AGCOLUMNDEF_H
