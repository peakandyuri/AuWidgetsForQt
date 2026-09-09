#include "AgColumnDef.h"

AgColumnDef::AgColumnDef()
    : m_width(120)
    , m_minWidth(40)
    , m_maxWidth(0)
    , m_widthMode(ColumnWidthMode::Fixed)
    , m_flexWeight(1)
    , m_sortable(false)
    , m_editable(false)
    , m_filterable(false)
    , m_visible(true)
    , m_resizable(true)
    , m_tooltipEnabled(false)
    , m_pinMode(PinMode::None)
    , m_alignment(Qt::AlignLeft | Qt::AlignVCenter)
    , m_textWrapMode(TextWrapMode::NoWrap)
    , m_checkbox(false)
    , m_headerAlignment(Qt::AlignCenter)
    , m_headerFontSet(false)
    , m_headerForegroundColorSet(false)
    , m_headerAlignmentSet(false)
{
}

AgColumnDef::AgColumnDef(const QString &field, const QString &headerName)
    : AgColumnDef()
{
    m_field = field;
    m_headerName = headerName.isEmpty() ? field : headerName;
}

QString AgColumnDef::field() const
{
    return m_field;
}

void AgColumnDef::setField(const QString &field)
{
    m_field = field;
}

QString AgColumnDef::headerName() const
{
    return m_headerName;
}

void AgColumnDef::setHeaderName(const QString &headerName)
{
    m_headerName = headerName;
}

int AgColumnDef::width() const
{
    return m_width;
}

void AgColumnDef::setWidth(int width)
{
    m_width = width;
}

int AgColumnDef::minWidth() const
{
    return m_minWidth;
}

void AgColumnDef::setMinWidth(int minWidth)
{
    m_minWidth = minWidth;
}

int AgColumnDef::maxWidth() const
{
    return m_maxWidth;
}

void AgColumnDef::setMaxWidth(int maxWidth)
{
    m_maxWidth = maxWidth;
}

AgColumnDef::ColumnWidthMode AgColumnDef::widthMode() const
{
    return m_widthMode;
}

void AgColumnDef::setWidthMode(ColumnWidthMode mode)
{
    m_widthMode = mode;
}

int AgColumnDef::flexWeight() const
{
    return m_flexWeight;
}

void AgColumnDef::setFlexWeight(int weight)
{
    m_flexWeight = qMax(1, weight);
}

bool AgColumnDef::isSortable() const
{
    return m_sortable;
}

void AgColumnDef::setSortable(bool sortable)
{
    m_sortable = sortable;
}

bool AgColumnDef::isEditable() const
{
    return m_editable;
}

void AgColumnDef::setEditable(bool editable)
{
    m_editable = editable;
}

bool AgColumnDef::isFilterable() const
{
    return m_filterable;
}

void AgColumnDef::setFilterable(bool filterable)
{
    m_filterable = filterable;
}

bool AgColumnDef::isVisible() const
{
    return m_visible;
}

void AgColumnDef::setVisible(bool visible)
{
    m_visible = visible;
}

bool AgColumnDef::isResizable() const
{
    return m_resizable;
}

void AgColumnDef::setResizable(bool resizable)
{
    m_resizable = resizable;
}

bool AgColumnDef::isTooltipEnabled() const
{
    return m_tooltipEnabled;
}

void AgColumnDef::setTooltipEnabled(bool enabled)
{
    m_tooltipEnabled = enabled;
}

AgColumnDef::PinMode AgColumnDef::pinMode() const
{
    return m_pinMode;
}

void AgColumnDef::setPinMode(PinMode mode)
{
    m_pinMode = mode;
}

QString AgColumnDef::cellRenderer() const
{
    return m_cellRenderer;
}

void AgColumnDef::setCellRenderer(const QString &renderer)
{
    m_cellRenderer = renderer;
}

QString AgColumnDef::headerRenderer() const
{
    return m_headerRenderer;
}

void AgColumnDef::setHeaderRenderer(const QString &renderer)
{
    m_headerRenderer = renderer;
}

QString AgColumnDef::cellEditor() const
{
    return m_cellEditor;
}

void AgColumnDef::setCellEditor(const QString &editor)
{
    m_cellEditor = editor;
}

bool AgColumnDef::isCheckbox() const
{
    return m_checkbox;
}

void AgColumnDef::setCheckbox(bool checkbox)
{
    m_checkbox = checkbox;
}

Qt::Alignment AgColumnDef::alignment() const
{
    return m_alignment;
}

void AgColumnDef::setAlignment(Qt::Alignment alignment)
{
    m_alignment = alignment;
}

AgColumnDef::TextWrapMode AgColumnDef::textWrapMode() const
{
    return m_textWrapMode;
}

void AgColumnDef::setTextWrapMode(TextWrapMode mode)
{
    m_textWrapMode = mode;
}

QFont AgColumnDef::headerFont() const
{
    return m_headerFont;
}

void AgColumnDef::setHeaderFont(const QFont &font)
{
    m_headerFont = font;
    m_headerFontSet = true;
}

bool AgColumnDef::hasHeaderFont() const
{
    return m_headerFontSet;
}

QColor AgColumnDef::headerForegroundColor() const
{
    return m_headerForegroundColor;
}

void AgColumnDef::setHeaderForegroundColor(const QColor &color)
{
    m_headerForegroundColor = color;
    m_headerForegroundColorSet = true;
}

bool AgColumnDef::hasHeaderForegroundColor() const
{
    return m_headerForegroundColorSet;
}

Qt::Alignment AgColumnDef::headerAlignment() const
{
    return m_headerAlignment;
}

void AgColumnDef::setHeaderAlignment(Qt::Alignment alignment)
{
    m_headerAlignment = alignment;
    m_headerAlignmentSet = true;
}

bool AgColumnDef::hasHeaderAlignment() const
{
    return m_headerAlignmentSet;
}

bool AgColumnDef::isGroup() const
{
    return !m_children.isEmpty();
}

const QVector<AgColumnDef> &AgColumnDef::children() const
{
    return m_children;
}

void AgColumnDef::setChildren(const QVector<AgColumnDef> &children)
{
    m_children = children;
}

void AgColumnDef::addChild(const AgColumnDef &child)
{
    m_children.append(child);
}

int AgColumnDef::childCount() const
{
    return m_children.size();
}
