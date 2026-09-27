#include "AuColumnDef.h"

AuColumnDef::AuColumnDef()
    : m_width(120)
    , m_minWidth(40)
    , m_maxWidth(0)
    , m_widthMode(ColumnWidthMode::Fixed)
    , m_flexWeight(1)
    , m_sortable(false)
    , m_editable(false)
    , m_filterable(false)
    , m_focusable(true)
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

AuColumnDef::AuColumnDef(const QString &field, const QString &headerName)
    : AuColumnDef()
{
    m_field = field;
    m_headerName = headerName.isEmpty() ? field : headerName;
}

QString AuColumnDef::field() const
{
    return m_field;
}

void AuColumnDef::setField(const QString &field)
{
    m_field = field;
}

QString AuColumnDef::headerName() const
{
    return m_headerName;
}

void AuColumnDef::setHeaderName(const QString &headerName)
{
    m_headerName = headerName;
}

int AuColumnDef::width() const
{
    return m_width;
}

void AuColumnDef::setWidth(int width)
{
    m_width = width;
}

int AuColumnDef::minWidth() const
{
    return m_minWidth;
}

void AuColumnDef::setMinWidth(int minWidth)
{
    m_minWidth = minWidth;
}

int AuColumnDef::maxWidth() const
{
    return m_maxWidth;
}

void AuColumnDef::setMaxWidth(int maxWidth)
{
    m_maxWidth = maxWidth;
}

AuColumnDef::ColumnWidthMode AuColumnDef::widthMode() const
{
    return m_widthMode;
}

void AuColumnDef::setWidthMode(ColumnWidthMode mode)
{
    m_widthMode = mode;
}

int AuColumnDef::flexWeight() const
{
    return m_flexWeight;
}

void AuColumnDef::setFlexWeight(int weight)
{
    m_flexWeight = qMax(1, weight);
}

bool AuColumnDef::isSortable() const
{
    return m_sortable;
}

void AuColumnDef::setSortable(bool sortable)
{
    m_sortable = sortable;
}

bool AuColumnDef::isEditable() const
{
    return m_editable;
}

void AuColumnDef::setEditable(bool editable)
{
    m_editable = editable;
}

bool AuColumnDef::isFilterable() const
{
    return m_filterable;
}

void AuColumnDef::setFilterable(bool filterable)
{
    m_filterable = filterable;
}

bool AuColumnDef::isFocusable() const
{
    return m_focusable;
}

void AuColumnDef::setFocusable(bool focusable)
{
    m_focusable = focusable;
}

bool AuColumnDef::isVisible() const
{
    return m_visible;
}

void AuColumnDef::setVisible(bool visible)
{
    m_visible = visible;
}

bool AuColumnDef::isResizable() const
{
    return m_resizable;
}

void AuColumnDef::setResizable(bool resizable)
{
    m_resizable = resizable;
}

bool AuColumnDef::isTooltipEnabled() const
{
    return m_tooltipEnabled;
}

void AuColumnDef::setTooltipEnabled(bool enabled)
{
    m_tooltipEnabled = enabled;
}

AuColumnDef::PinMode AuColumnDef::pinMode() const
{
    return m_pinMode;
}

void AuColumnDef::setPinMode(PinMode mode)
{
    m_pinMode = mode;
}

QString AuColumnDef::cellRenderer() const
{
    return m_cellRenderer;
}

void AuColumnDef::setCellRenderer(const QString &renderer)
{
    m_cellRenderer = renderer;
}

QString AuColumnDef::headerRenderer() const
{
    return m_headerRenderer;
}

void AuColumnDef::setHeaderRenderer(const QString &renderer)
{
    m_headerRenderer = renderer;
}

QString AuColumnDef::cellEditor() const
{
    return m_cellEditor;
}

void AuColumnDef::setCellEditor(const QString &editor)
{
    m_cellEditor = editor;
}

bool AuColumnDef::isCheckbox() const
{
    return m_checkbox;
}

void AuColumnDef::setCheckbox(bool checkbox)
{
    m_checkbox = checkbox;
}

Qt::Alignment AuColumnDef::alignment() const
{
    return m_alignment;
}

void AuColumnDef::setAlignment(Qt::Alignment alignment)
{
    m_alignment = alignment;
}

AuColumnDef::TextWrapMode AuColumnDef::textWrapMode() const
{
    return m_textWrapMode;
}

void AuColumnDef::setTextWrapMode(TextWrapMode mode)
{
    m_textWrapMode = mode;
}

QFont AuColumnDef::headerFont() const
{
    return m_headerFont;
}

void AuColumnDef::setHeaderFont(const QFont &font)
{
    m_headerFont = font;
    m_headerFontSet = true;
}

bool AuColumnDef::hasHeaderFont() const
{
    return m_headerFontSet;
}

QColor AuColumnDef::headerForegroundColor() const
{
    return m_headerForegroundColor;
}

void AuColumnDef::setHeaderForegroundColor(const QColor &color)
{
    m_headerForegroundColor = color;
    m_headerForegroundColorSet = true;
}

bool AuColumnDef::hasHeaderForegroundColor() const
{
    return m_headerForegroundColorSet;
}

Qt::Alignment AuColumnDef::headerAlignment() const
{
    return m_headerAlignment;
}

void AuColumnDef::setHeaderAlignment(Qt::Alignment alignment)
{
    m_headerAlignment = alignment;
    m_headerAlignmentSet = true;
}

bool AuColumnDef::hasHeaderAlignment() const
{
    return m_headerAlignmentSet;
}

bool AuColumnDef::isGroup() const
{
    return !m_children.isEmpty();
}

const QVector<AuColumnDef> &AuColumnDef::children() const
{
    return m_children;
}

void AuColumnDef::setChildren(const QVector<AuColumnDef> &children)
{
    m_children = children;
}

void AuColumnDef::addChild(const AuColumnDef &child)
{
    m_children.append(child);
}

int AuColumnDef::childCount() const
{
    return m_children.size();
}
