#include "AgDarkGridStyle.h"

AgDarkGridStyle::AgDarkGridStyle()
{
}

AgDarkGridStyle::~AgDarkGridStyle()
{
}

AgGridStyle *AgDarkGridStyle::clone() const
{
    AgDarkGridStyle *copy = new AgDarkGridStyle();
    copy->m_sizeMode = m_sizeMode;
    return copy;
}

QVariant AgDarkGridStyle::value(StyleRole role, const AgGridModel *model, int row, int column) const
{
    switch (role) {
    case CornerRadius:
        return 8;

    case HeaderBackgroundColor:
        return QColor(50, 50, 55);
    case HeaderForegroundColor:
        return QColor(220, 220, 220);
    case SortIndicatorColor:
        return QColor(220, 220, 220);
    case HeaderBorderColor:
        return QColor(70, 70, 75);

    case CellBorderColor:
    case GridLineColor:
        return QColor(80, 80, 85);
    case BorderColor:
        return QColor(100, 100, 105);

    case SelectedRowBackgroundColor:
        return QColor(45, 70, 110);
    case AlternateRowBackgroundColor:
        return QColor(45, 45, 50);
    case RowBackgroundColor:
        return QColor(35, 35, 40);
    case HoverRowBackgroundColor:
        return QColor(50, 55, 65);

    case FocusedCellBorderColor:
        return QColor(0, 150, 255);
    case FocusedCellBorderWidth:
        return 2;

    case ScrollBarSliderColor:
        return QColor(120, 120, 120);
    case ScrollBarTrackBackgroundColor:
        return QColor(60, 60, 65);

    case CheckboxCheckedColor:
        return QColor(0, 150, 255);
    case CheckboxBorderColor:
        return QColor(120, 120, 125);
    case CheckboxCheckmarkColor:
        return QColor(255, 255, 255);

    case DragHandleIconColor:
        return QColor(130, 130, 135);
    case DragHandleHoveredIconColor:
        return QColor(180, 180, 185);
    case DraggedRowOverlayColor:
        return QColor(0, 0, 0, 102);

    case RowNumberForegroundColor:
        return QColor(150, 150, 155);

    case LoadingOverlayColor:
        return QColor(0, 0, 0, 130);
    case LoadingSpinnerColor:
        return QColor(0, 150, 255);
    case LoadingTextColor:
        return QColor(200, 200, 200);

    case CellForegroundColor:
        return QColor(220, 220, 220);

    case TreeExpanderColor:
        return QColor(160, 160, 165);
    case TreeExpanderHoveredColor:
        return QColor(0, 150, 255);

    default:
        break;
    }
    return AgGridStyle::value(role, model, row, column);
}
