#include "AuPlotCurve.h"

#include "AuPlot.h"
#include "AuPlotInternal.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>

namespace {

/// 在点集中找落在 [xMinV, xMaxV] 的索引范围；升序数据用二分查找，否则线性扫描。
/// 返回 false 表示整个数据集都在可见区外。
bool visibleIndexRange(const QVector<QPointF> &points, bool xAscending,
                       double xMinV, double xMaxV, int *firstIndex, int *lastIndex)
{
    const int n = points.size();
    if (n == 0 || !(xMaxV >= xMinV))
        return false;

    if (xAscending) {
        // 第一个 x >= xMinV 的索引
        int lo = 0;
        int hi = n;
        while (lo < hi) {
            const int mid = (lo + hi) / 2;
            if (points.at(mid).x() < xMinV)
                lo = mid + 1;
            else
                hi = mid;
        }
        const int first = lo;
        // 第一个 x > xMaxV 的索引
        lo = first;
        hi = n;
        while (lo < hi) {
            const int mid = (lo + hi) / 2;
            if (points.at(mid).x() <= xMaxV)
                lo = mid + 1;
            else
                hi = mid;
        }
        const int last = lo - 1;
        if (first >= n || last < first)
            return false;
        *firstIndex = first;
        *lastIndex = last;
        return true;
    }

    // 乱序数据：线性扫描（图元数据一般不会大到需要抽稀）
    int first = -1;
    int last = -1;
    for (int i = 0; i < n; ++i) {
        const double x = points.at(i).x();
        if (x >= xMinV && x <= xMaxV) {
            if (first < 0)
                first = i;
            last = i;
        }
    }
    if (first < 0)
        return false;
    *firstIndex = first;
    *lastIndex = last;
    return true;
}

/**
 * 大数据量抽稀：把 [a, b] 的点按像素列做 min/max 抽稀，每列最多保留
 * 首 / 最小 / 最大 / 末 4 个点，保证峰谷形状不丢；点数不多时原样返回。
 */
QVector<QPointF> decimateVisible(const QVector<QPointF> &points, int a, int b,
                                 double xMinV, double xMaxV, qreal pixelWidth)
{
    const int count = b - a + 1;
    const int budget = qMax(8, int(pixelWidth) * 2);
    if (count <= budget || !(pixelWidth >= 1.0))
        return points.mid(a, count);

    const int columns = qMax(1, int(pixelWidth));
    const double span = xMaxV - xMinV;

    struct ColumnAgg {
        int iFirst = -1;
        int iLast = -1;
        int iMin = -1;
        int iMax = -1;
        double minY = 0.0;
        double maxY = 0.0;
    };
    QVector<ColumnAgg> columns2(columns);

    for (int i = a; i <= b; ++i) {
        const QPointF &pt = points.at(i);
        int col = int((pt.x() - xMinV) / span * columns);
        col = qBound(0, col, columns - 1);
        ColumnAgg &agg = columns2[col];
        if (agg.iFirst < 0) {
            agg.iFirst = i;
            agg.iMin = i;
            agg.iMax = i;
            agg.minY = pt.y();
            agg.maxY = pt.y();
        } else {
            if (pt.y() < agg.minY) {
                agg.minY = pt.y();
                agg.iMin = i;
            }
            if (pt.y() > agg.maxY) {
                agg.maxY = pt.y();
                agg.iMax = i;
            }
        }
        agg.iLast = i;
    }

    QVector<QPointF> out;
    out.reserve(budget + 8);
    int prevEmitted = -1;
    auto emitIndex = [&](int i) {
        if (i < 0 || i == prevEmitted)
            return;
        out << points.at(i);
        prevEmitted = i;
    };

    emitIndex(a);   // 区间起点，保证线从边缘进入
    for (const ColumnAgg &agg : columns2) {
        if (agg.iFirst < 0)
            continue;
        // 按索引顺序发：首 -> 最小/最大（按索引排）-> 末
        if (agg.iMin < agg.iMax) {
            emitIndex(agg.iFirst);
            emitIndex(agg.iMin);
            emitIndex(agg.iMax);
        } else {
            emitIndex(agg.iFirst);
            emitIndex(agg.iMax);
            emitIndex(agg.iMin);
        }
        emitIndex(agg.iLast);
    }
    emitIndex(b);   // 区间终点，保证线画到边缘
    return out;
}

} // namespace

// ============================================================
// AuPlotCurve
// ============================================================

AuPlotCurve::AuPlotCurve()
    : m_fillColor(47, 111, 219, 60)
{
}

AuPlotCurve::~AuPlotCurve() = default;

void AuPlotCurve::setPoints(const QVector<QPointF> &points)
{
    m_points = points;
    m_xAscending = true;
    for (int i = 1; i < m_points.size(); ++i) {
        if (m_points.at(i).x() < m_points.at(i - 1).x()) {
            m_xAscending = false;
            break;
        }
    }
    invalidateBounds();
}

void AuPlotCurve::addPoint(double x, double y)
{
    if (!m_points.isEmpty() && x < m_points.last().x())
        m_xAscending = false;
    m_points.append(QPointF(x, y));
    invalidateBounds();
}

void AuPlotCurve::clearPoints()
{
    m_points.clear();
    m_xAscending = true;
    invalidateBounds();
}

int AuPlotCurve::pointCount() const
{
    return m_points.size();
}

QPointF AuPlotCurve::pointAt(int index) const
{
    return m_points.value(index);
}

bool AuPlotCurve::isXAscending() const
{
    return m_xAscending;
}

int AuPlotCurve::nearestIndexForX(double x) const
{
    const int n = m_points.size();
    if (n == 0)
        return -1;

    if (m_xAscending) {
        // 找第一个 x >= 给定值的索引，与前一个比较谁更近
        int lo = 0;
        int hi = n;
        while (lo < hi) {
            const int mid = (lo + hi) / 2;
            if (m_points.at(mid).x() < x)
                lo = mid + 1;
            else
                hi = mid;
        }
        if (lo == 0)
            return 0;
        if (lo == n)
            return n - 1;
        const double distPrev = qAbs(m_points.at(lo - 1).x() - x);
        const double distNext = qAbs(m_points.at(lo).x() - x);
        return distPrev <= distNext ? lo - 1 : lo;
    }

    int best = 0;
    double bestDist = qAbs(m_points.at(0).x() - x);
    for (int i = 1; i < n; ++i) {
        const double dist = qAbs(m_points.at(i).x() - x);
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

// ---- 线 ----

bool AuPlotCurve::lineVisible() const
{
    return m_lineVisible;
}

void AuPlotCurve::setLineVisible(bool on)
{
    if (m_lineVisible == on)
        return;
    m_lineVisible = on;
    if (attachedPlot())
        attachedPlot()->update();
}

QColor AuPlotCurve::lineColor() const
{
    return m_lineColor;
}

void AuPlotCurve::setLineColor(const QColor &color)
{
    m_lineColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AuPlotCurve::lineWidth() const
{
    return m_lineWidth;
}

void AuPlotCurve::setLineWidth(qreal width)
{
    m_lineWidth = qMax<qreal>(0.0, width);
    if (attachedPlot())
        attachedPlot()->update();
}

Qt::PenStyle AuPlotCurve::lineStyle() const
{
    return m_lineStyle;
}

void AuPlotCurve::setLineStyle(Qt::PenStyle style)
{
    m_lineStyle = style;
    if (attachedPlot())
        attachedPlot()->update();
}

QVector<qreal> AuPlotCurve::dashPattern() const
{
    return m_dashPattern;
}

void AuPlotCurve::setDashPattern(const QVector<qreal> &pattern)
{
    m_dashPattern = pattern;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AuPlotCurve::dashOffset() const
{
    return m_dashOffset;
}

void AuPlotCurve::setDashOffset(qreal offset)
{
    m_dashOffset = offset;
    if (attachedPlot())
        attachedPlot()->update();
}

// ---- 点标记 ----

AuPlotPointStyle AuPlotCurve::pointStyle() const
{
    return m_pointStyle;
}

void AuPlotCurve::setPointStyle(AuPlotPointStyle style)
{
    m_pointStyle = style;
    if (attachedPlot())
        attachedPlot()->update();
}

int AuPlotCurve::pointSize() const
{
    return m_pointSize;
}

void AuPlotCurve::setPointSize(int size)
{
    m_pointSize = qMax(1, size);
    if (attachedPlot())
        attachedPlot()->update();
}

QColor AuPlotCurve::pointColor() const
{
    return m_pointColor;
}

void AuPlotCurve::setPointColor(const QColor &color)
{
    m_pointColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

// ---- 面填充 ----

bool AuPlotCurve::fillVisible() const
{
    return m_fillVisible;
}

void AuPlotCurve::setFillVisible(bool on)
{
    if (m_fillVisible == on)
        return;
    m_fillVisible = on;
    if (attachedPlot())
        attachedPlot()->update();
}

QColor AuPlotCurve::fillColor() const
{
    return m_fillColor;
}

void AuPlotCurve::setFillColor(const QColor &color)
{
    m_fillColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

double AuPlotCurve::fillBaseline() const
{
    return m_fillBaseline;
}

void AuPlotCurve::setFillBaseline(double y)
{
    m_fillBaseline = y;
    if (attachedPlot())
        attachedPlot()->update();
}

// ---- 绘制 ----

QRectF AuPlotCurve::computeBounds() const
{
    if (m_points.isEmpty())
        return QRectF();

    double minX = m_points.constFirst().x();
    double maxX = minX;
    double minY = m_points.constFirst().y();
    double maxY = minY;
    for (const QPointF &pt : m_points) {
        minX = qMin(minX, pt.x());
        maxX = qMax(maxX, pt.x());
        minY = qMin(minY, pt.y());
        maxY = qMax(maxY, pt.y());
    }
    if (m_fillVisible) {
        minY = qMin(minY, m_fillBaseline);
        maxY = qMax(maxY, m_fillBaseline);
    }
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

void AuPlotCurve::paint(QPainter *painter, const AuPlot *plot)
{
    const int n = m_points.size();
    if (n == 0 || (!m_lineVisible && !m_fillVisible && m_pointStyle == AuPlotPointStyle::None))
        return;

    const QRectF rect = plot->plotRect();
    const double xMinV = plot->pixelToX(rect.left(), xAxis());
    const double xMaxV = plot->pixelToX(rect.right(), xAxis());

    int a = 0;
    int b = -1;
    if (!visibleIndexRange(m_points, m_xAscending, xMinV, xMaxV, &a, &b))
        return;
    // 各向外扩一个点，让线在裁剪边缘自然进入 / 离开
    a = qMax(0, a - 1);
    b = qMin(n - 1, b + 1);

    // 像素换算（预计算缩放平移，避免逐点调用虚函数）
    const AuPlotAxis *ax = plot->axisAt(xAxis());
    const AuPlotAxis *ay = plot->axisAt(yAxis());
    const double xSpan = ax->maximum() - ax->minimum();
    const double ySpan = ay->maximum() - ay->minimum();
    auto toPixelX = [&](double x) {
        return rect.left() + (x - ax->minimum()) / xSpan * rect.width();
    };
    auto toPixelY = [&](double y) {
        return rect.bottom() - (y - ay->minimum()) / ySpan * rect.height();
    };

    const QVector<QPointF> drawn = decimateVisible(m_points, a, b, xMinV, xMaxV, rect.width());
    if (drawn.isEmpty())
        return;

    // 数据点 -> 像素点
    QVector<QPointF> pixels(drawn.size());
    for (int i = 0; i < drawn.size(); ++i)
        pixels[i] = QPointF(toPixelX(drawn.at(i).x()), toPixelY(drawn.at(i).y()));

    const QColor fallbackLineColor = m_lineColor.isValid()
                                         ? m_lineColor
                                         : QColor(0x2f, 0x6f, 0xdb);

    // 面：曲线与基线之间填充（在线下层）
    if (m_fillVisible && m_fillColor.isValid() && m_fillColor.alpha() > 0) {
        QPainterPath fillPath;
        fillPath.moveTo(pixels.first());
        for (int i = 1; i < pixels.size(); ++i)
            fillPath.lineTo(pixels.at(i));
        fillPath.lineTo(pixels.last().x(), toPixelY(m_fillBaseline));
        fillPath.lineTo(pixels.first().x(), toPixelY(m_fillBaseline));
        fillPath.closeSubpath();
        painter->fillPath(fillPath, m_fillColor);
    }

    // 线。
    // 密集曲线（点数超过像素列数 2 倍，抽稀后仍大量近垂直线段）时切换为 0 宽装饰笔：
    // 抗锯齿的常规描边在这种几何上会退化到几百毫秒，装饰笔走光栅引擎的快速
    // 线段路径只需 1~2ms，且视觉上密集折线本来就呈 1px 细带，无差别。
    if (m_lineVisible && pixels.size() >= 2) {
        const bool dense = pixels.size() > int(rect.width()) * 2;
        QPen pen = AuPlotInternal::makeLinePen(m_lineColor, fallbackLineColor,
                                               m_lineWidth, m_lineStyle,
                                               m_dashPattern, m_dashOffset);
        if (dense) {
            pen.setWidth(0);   // 装饰笔：1px 快速路径
            // 虚线间距单位随线宽缩放，切换后视觉间距保持一致
            if (pen.style() == Qt::CustomDashLine && m_lineWidth > 1.0) {
                QVector<qreal> scaled = m_dashPattern;
                for (int i = 0; i < scaled.size(); ++i)
                    scaled[i] *= m_lineWidth;
                pen.setDashPattern(scaled);
            }
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPolyline(pixels.constData(), pixels.size());
        } else {
            QPainterPath linePath;
            linePath.moveTo(pixels.first());
            for (int i = 1; i < pixels.size(); ++i)
                linePath.lineTo(pixels.at(i));
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(linePath);
        }
    }

    // 点标记：点数超过像素可容纳数量时按步长抽稀，避免糊成一团
    if (m_pointStyle != AuPlotPointStyle::None) {
        const int maxMarkers = qMax(1, int(rect.width()) / 2);
        const int stride = qMax(1, pixels.size() / maxMarkers);
        for (int i = 0; i < pixels.size(); i += stride)
            AuPlotInternal::drawPointMarker(painter, pixels.at(i), m_pointStyle,
                                            m_pointSize,
                                            m_pointColor.isValid() ? m_pointColor
                                                                   : fallbackLineColor);
    }
}
