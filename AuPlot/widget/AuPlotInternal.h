#ifndef AUPLOT_INTERNAL_H
#define AUPLOT_INTERNAL_H

//
// 库内部共享的小工具，不对外导出。
//

#include "AuPlotTypes.h"

#include <QColor>
#include <QPainter>
#include <QPointF>
#include <QVector>
#include <cmath>

namespace AuPlotInternal {

/// 数据点坐标是否有效（NaN / Inf 视为无效：绘制时断开线段，量程统计时跳过）。
inline bool isFinitePoint(const QPointF &pt)
{
    return std::isfinite(pt.x()) && std::isfinite(pt.y());
}

/// 无有效数据的图元边界：NaN 矩形。fitAutoRanges 以 qIsFinite 识别并跳过；
/// 不能用空 QRectF() 表示——(0,0) 单点的合法边界同样是空矩形，无法区分。
inline QRectF nullDataBounds()
{
    const double nan = qQNaN();
    return QRectF(nan, nan, nan, nan);
}

/// 在 pos 处绘制一个 pointStyle 标记（size 为直径 / 边长，px）。
/// 实心类样式填充 color；空心类以 color 描边；Cross / XCross / RingedCross 画线。
void drawPointMarker(QPainter *painter, const QPointF &pos,
                     AuPlotPointStyle style, qreal size, const QColor &color);

/// 构建线画笔：color 无效时用 fallback；dashPattern 非空时用自定义虚线。
QPen makeLinePen(const QColor &color, const QColor &fallback, qreal width,
                 Qt::PenStyle style, const QVector<qreal> &dashPattern, qreal dashOffset);

} // namespace AuPlotInternal

#endif // AUPLOT_INTERNAL_H
