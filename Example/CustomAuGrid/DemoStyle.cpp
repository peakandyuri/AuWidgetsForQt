#include "DemoStyle.h"
#include "AuGridModel.h"

#include <QFont>
#include <QColor>

QVariant DemoStyle::value(StyleRole role, const AuGridModel *model, int row, int column) const
{
    switch (role) {
    case HeaderBackgroundColor:
        return QColor(230, 240, 255);
    case HeaderForegroundColor:
        return QColor(30, 60, 120);
    case HeaderFont:
        return QFont("Microsoft YaHei", 11, QFont::Bold);
    case HeaderAlignment:
        return static_cast<int>(Qt::AlignCenter);
    case SelectedRowBackgroundColor:
        return QColor(255, 220, 180);
    case AlternateRowBackgroundColor:
        return QColor(250, 250, 250);
    case FocusedCellBorderColor:
        return QColor(0, 120, 215);
    case FocusedCellBorderWidth:
        return 2;
    case ScrollBarSliderColor:
        return QColor(120, 120, 120);
    case ScrollBarTrackBackgroundColor:
        return QColor(255, 255, 255);
    case CornerRadius:
        return 8;
    case BorderColor:
        return QColor(180, 180, 180);

    // 单元格级样式示例：按 row/column/value 动态返回
    case Font:
        if (row % 7 == 0)
            return QFont("Microsoft YaHei", 13, QFont::Bold);
        if (row >= 0 && model) {
            const QString text = model->cellValue(row, column).toString();
            if (text.startsWith("R9"))
                return QFont("Arial", 14, QFont::Bold);
        }
        break;
    case CellForegroundColor:
        if (row % 5 == 0 && column == 1)
            return QColor(220, 20, 60);
        if (row >= 0 && model) {
            const QString text = model->cellValue(row, column).toString();
            if (text.contains("99"))
                return QColor(0, 128, 0);
        }
        break;
    case CellBackgroundColor:
        if (row % 11 == 0 && column == 2)
            return QColor(255, 255, 200);
        if (row >= 0 && model) {
            // 根据其它列的值来着色：当 col0 的值以 "R3" 开头时，给 col4 加背景
            const QString col0 = model->cellValue(row, "col0").toString();
            if (column == 4 && col0.startsWith("R3"))
                return QColor(230, 240, 255);
        }
        break;
    case CellTextAlignment:
        if (row % 3 == 0 && column == 4)
            return static_cast<int>(Qt::AlignCenter);
        if (row >= 0 && model) {
            const QString text = model->cellValue(row, column).toString();
            if (text.endsWith("C0"))
                return static_cast<int>(Qt::AlignRight);
        }
        break;

    default:
        break;
    }
    return AuGridStyle::value(role, model, row, column);
}
