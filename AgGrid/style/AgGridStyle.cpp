#include "AgGridStyle.h"

#include "AgGridModel.h"
#include "AgGridWidget.h"
#include "AgDarkGridStyle.h"

AgGridStyle *AgGridStyle::s_globalStyle = nullptr;
AgGridStyle *AgGridStyle::s_defaultLightStyle = nullptr;
QVector<AgGridWidget *> AgGridStyle::s_observers;

AgGridStyle::AgGridStyle()
    : m_sizeMode(SizeMode::Small)
{
}

AgGridStyle::~AgGridStyle()
{
}

QVariant AgGridStyle::value(StyleRole role, const AgGridModel * /*model*/, int /*row*/, int /*column*/) const
{
    return defaultValue(role);
}

QColor AgGridStyle::color(StyleRole role, const AgGridModel *model, int row, int column) const
{
    return value(role, model, row, column).value<QColor>();
}

int AgGridStyle::integer(StyleRole role, const AgGridModel *model, int row, int column) const
{
    return value(role, model, row, column).toInt();
}

QFont AgGridStyle::font(const AgGridModel *model, int row, int column) const
{
    return value(Font, model, row, column).value<QFont>();
}

Qt::Alignment AgGridStyle::alignment(const AgGridModel *model, int row, int column) const
{
    return static_cast<Qt::Alignment>(value(DefaultTextAlignment, model, row, column).toInt());
}

QVariant AgGridStyle::defaultValue(StyleRole role) const
{
    const bool medium = (m_sizeMode == SizeMode::Medium);
    const bool large = (m_sizeMode == SizeMode::Large);

    auto scaledFont = [&](int delta) {
        QFont font(QStringLiteral("Microsoft YaHei"));
        const int pointSize = font.pointSize();
        if (pointSize > 0)
            font.setPointSize(pointSize + delta);
        else if (font.pixelSize() > 0)
            font.setPixelSize(font.pixelSize() + delta);
        return font;
    };

    switch (role) {
    case HeaderHeight:
        return large ? 44 : (medium ? 36 : 28);
    case RowHeight:
        return large ? 40 : (medium ? 32 : 24);
    case ScrollBarExtent:
        return 10;
    case CornerRadius:
        return 8;

    case HeaderBackgroundColor:
        return QColor(240, 240, 240);
    case HeaderForegroundColor:
        return QColor(0, 0, 0);
    case HeaderBorderColor:
        return QColor(200, 200, 200);
    case HeaderFont:
        return scaledFont(large ? 1 : (medium ? 0 : -1));
    case HeaderAlignment:
        return static_cast<int>(Qt::Alignment());

    case CellBorderColor:
    case GridLineColor:
        return QColor(220, 220, 220);
    case RowBackgroundColor:
        return QColor();
    case SelectedRowBackgroundColor:
        return QColor(200, 220, 255);
    case AlternateRowBackgroundColor:
        return QColor(250, 250, 250);
    case HoverRowBackgroundColor:
        return QColor(235, 245, 255);
    case FocusedCellBorderColor:
        return QColor(0, 120, 215);
    case FocusedCellBorderWidth:
        return 2;

    case ScrollBarSliderColor:
        return QColor(150, 150, 150);
    case ScrollBarTrackBackgroundColor:
        return QColor(220, 220, 220);

    case Font:
        return scaledFont(large ? 1 : (medium ? 0 : -1));
    case DefaultTextAlignment:
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);

    case CellBackgroundColor:
        return QColor();
    case CellForegroundColor:
        return QColor();
    case CellTextAlignment:
        return static_cast<int>(Qt::Alignment());
    case CellHorizontalPadding:
        return large ? 10 : (medium ? 8 : 6);
    case CellVerticalPadding:
        return large ? 4 : (medium ? 2 : 0);
    case BorderColor:
        return QColor(200, 200, 200);

    case CheckboxSize:
        return large ? 20 : (medium ? 18 : 16);
    case CheckboxCheckedColor:
        return QColor(0, 120, 215);
    case CheckboxUncheckedColor:
        return QColor();
    case CheckboxBorderColor:
        return QColor(150, 150, 150);
    case CheckboxCheckmarkColor:
        return QColor(255, 255, 255);

    case DragHandleIconColor:
        return QColor(160, 160, 160);
    case DragHandleHoveredIconColor:
        return QColor(100, 100, 100);
    case DragHandleDotSize:
        return large ? 5 : (medium ? 4 : 3);
    case DragHandleBackgroundColor:
        return QColor();
    case DragHandleHoveredBackgroundColor:
        return QColor();
    case DraggedRowOverlayColor:
        return QColor(255, 255, 255, 102);

    case RowNumberFontBold:
        return false;
    case RowNumberForegroundColor:
        return QColor(120, 120, 120);

    case LoadingOverlayColor:
        return QColor(255, 255, 255, 160);
    case LoadingSpinnerColor:
        return QColor(0, 120, 215);
    case LoadingSpinnerSize:
        return large ? 32 : (medium ? 28 : 24);
    case LoadingTextColor:
        return QColor(80, 80, 80);
    case LoadingText:
        return QStringLiteral("加载中…");

    case TreeIndentWidth:
        return large ? 20 : (medium ? 18 : 16);
    case TreeExpanderSize:
        return large ? 18 : (medium ? 16 : 14);
    case TreeExpanderColor:
        return QColor(130, 130, 130);
    case TreeExpanderHoveredColor:
        return QColor(0, 120, 215);
    }

    return QVariant();
}

AgGridStyle *AgGridStyle::globalStyle()
{
    if (!s_defaultLightStyle)
        s_defaultLightStyle = createLightStyle();
    return s_globalStyle ? s_globalStyle : s_defaultLightStyle;
}

void AgGridStyle::setGlobalStyle(AgGridStyle *style)
{
    if (s_globalStyle == style)
        return;

    delete s_globalStyle;
    s_globalStyle = style;
    notifyGlobalStyleChanged();
}

AgGridStyle::SizeMode AgGridStyle::sizeMode() const
{
    return m_sizeMode;
}

void AgGridStyle::setSizeMode(SizeMode mode)
{
    m_sizeMode = mode;
}

AgGridStyle *AgGridStyle::clone() const
{
    AgGridStyle *copy = new AgGridStyle();
    copy->m_sizeMode = m_sizeMode;
    return copy;
}

AgGridStyle *AgGridStyle::createLightStyle()
{
    return new AgGridStyle();
}

AgGridStyle *AgGridStyle::createDarkStyle()
{
    return new AgDarkGridStyle();
}

void AgGridStyle::registerGlobalStyleObserver(AgGridWidget *widget)
{
    if (widget && !s_observers.contains(widget))
        s_observers.append(widget);
}

void AgGridStyle::unregisterGlobalStyleObserver(AgGridWidget *widget)
{
    s_observers.removeAll(widget);
}

void AgGridStyle::notifyGlobalStyleChanged()
{
    for (AgGridWidget *widget : s_observers)
        widget->onGlobalStyleChanged();
}
