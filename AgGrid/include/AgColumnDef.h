#ifndef AGCOLUMNDEF_H
#define AGCOLUMNDEF_H

#include "AgGrid_global.h"

#include <QString>
#include <QVariant>
#include <QVector>
#include <QFont>
#include <QColor>

class AGGRID_EXPORT AgColumnDef
{
public:
    enum class ColumnWidthMode {
        Fixed,   // width() 为固定像素值
        AutoFit, // 根据表头和可视内容自适应
        Flex     // 分享剩余空间
    };

    enum class PinMode {
        None,
        Left,
        Right
    };

    enum class TextWrapMode {
        NoWrap,          // 单行，超出部分被裁剪
        NoWrapTruncate,  // 单行，超出部分显示省略号
        NoWrapOverflow,  // 单行，允许溢出到相邻单元格
        WordWrap         // 根据列宽自动换行
    };

    AgColumnDef();
    explicit AgColumnDef(const QString &field, const QString &headerName = QString());

    QString field() const;
    void setField(const QString &field);

    QString headerName() const;
    void setHeaderName(const QString &headerName);

    int width() const;
    void setWidth(int width);

    int minWidth() const;
    void setMinWidth(int minWidth);

    int maxWidth() const;
    void setMaxWidth(int maxWidth);

    ColumnWidthMode widthMode() const;
    void setWidthMode(ColumnWidthMode mode);

    int flexWeight() const;
    void setFlexWeight(int weight);

    bool isSortable() const;
    void setSortable(bool sortable);

    bool isEditable() const;
    void setEditable(bool editable);

    bool isFilterable() const;
    void setFilterable(bool filterable);

    bool isVisible() const;
    void setVisible(bool visible);

    bool isResizable() const;
    void setResizable(bool resizable);

    PinMode pinMode() const;
    void setPinMode(PinMode mode);

    QString cellRenderer() const;
    void setCellRenderer(const QString &renderer);

    QString headerRenderer() const;
    void setHeaderRenderer(const QString &renderer);

    QString cellEditor() const;
    void setCellEditor(const QString &editor);

    bool isCheckbox() const;
    void setCheckbox(bool checkbox);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    TextWrapMode textWrapMode() const;
    void setTextWrapMode(TextWrapMode mode);

    QFont headerFont() const;
    void setHeaderFont(const QFont &font);
    bool hasHeaderFont() const;

    QColor headerForegroundColor() const;
    void setHeaderForegroundColor(const QColor &color);
    bool hasHeaderForegroundColor() const;

    Qt::Alignment headerAlignment() const;
    void setHeaderAlignment(Qt::Alignment alignment);
    bool hasHeaderAlignment() const;

    bool isGroup() const;
    const QVector<AgColumnDef> &children() const;
    void setChildren(const QVector<AgColumnDef> &children);
    void addChild(const AgColumnDef &child);
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
    bool m_headerFontSet;
    bool m_headerForegroundColorSet;
    bool m_headerAlignmentSet;
    QVector<AgColumnDef> m_children;
};

#endif // AGCOLUMNDEF_H
