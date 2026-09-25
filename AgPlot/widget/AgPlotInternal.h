#ifndef AGPLOT_INTERNAL_H
#define AGPLOT_INTERNAL_H

//
// 库内部共享的小工具，不对外导出。
//

#include "AgPlotTypes.h"

#include <QColor>
#include <QPainter>
#include <QPointF>
#include <QVector>

namespace AgPlotInternal {

/// 在 pos 处绘制一个 pointStyle 标记（size 为直径 / 边长，px）。
/// 实心类样式填充 color；空心类以 color 描边；Cross / XCross / RingedCross 画线。
void drawPointMarker(QPainter *painter, const QPointF &pos,
                     AgPlotPointStyle style, qreal size, const QColor &color);

/// 构建线画笔：color 无效时用 fallback；dashPattern 非空时用自定义虚线。
QPen makeLinePen(const QColor &color, const QColor &fallback, qreal width,
                 Qt::PenStyle style, const QVector<qreal> &dashPattern, qreal dashOffset);

} // namespace AgPlotInternal

#endif // AGPLOT_INTERNAL_H
