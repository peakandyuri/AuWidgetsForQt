#include "AuPlotCurve.h"

#include "AuPlot.h"
#include "AuPlotInternal.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>
#include <cmath>

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
        int iNan = -1;      // 列内首个非有限 y：断点必须随抽稀保留
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
        // min/max 比较对 NaN 恒为 false，非有限点只能显式记录：
        // 否则列内的 NaN 缺数据点被抽稀丢弃，断线被画成连线
        if (agg.iNan < 0 && !qIsFinite(pt.y()))
            agg.iNan = i;
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
        if (agg.iNan >= 0) {
            // 列内含缺数据点：发首 / 断点 / 末，绘制端按 NaN 分段，
            // 线与填充在该点断开（丢弃列内 min/max，保断裂优先于保形状）
            emitIndex(agg.iFirst);
            emitIndex(agg.iNan);
        } else if (agg.iMin < agg.iMax) {
            // 按索引顺序发：首 -> 最小/最大（按索引排）-> 末
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
    bool boundsValid = false;
    double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0;
    for (int i = 0; i < m_points.size(); ++i) {
        const QPointF &pt = m_points.at(i);
        if (i > 0 && pt.x() < m_points.at(i - 1).x())
            m_xAscending = false;
        if (!AuPlotInternal::isFinitePoint(pt))
            continue;   // NaN / Inf 不参与量程（绘制时断段）
        if (!boundsValid) {
            minX = maxX = pt.x();
            minY = maxY = pt.y();
            boundsValid = true;
        } else {
            minX = qMin(minX, pt.x());
            maxX = qMax(maxX, pt.x());
            minY = qMin(minY, pt.y());
            maxY = qMax(maxY, pt.y());
        }
    }
    m_runningBounds = QRectF(minX, minY, maxX - minX, maxY - minY);
    m_runningBoundsValid = boundsValid;
    invalidateBounds();
}

void AuPlotCurve::addPoint(double x, double y)
{
    if (!m_points.isEmpty() && x < m_points.last().x())
        m_xAscending = false;
    m_points.append(QPointF(x, y));
    // 增量扩展包围盒：实时追加时避免每帧 O(N) 全量重扫。
    // 缓存无效（removeFirstPoints / 初始）时保持无效，由下一次 computeBounds
    // 全量重建——单靠新点重建会丢掉存量数据的极值
    if (m_runningBoundsValid) {
        if (std::isfinite(x)) {
            if (x < m_runningBounds.left())
                m_runningBounds.setLeft(x);
            if (x > m_runningBounds.right())
                m_runningBounds.setRight(x);
        }
        if (std::isfinite(y)) {
            if (y < m_runningBounds.top())
                m_runningBounds.setTop(y);
            if (y > m_runningBounds.bottom())
                m_runningBounds.setBottom(y);
        }
    }
    invalidateBounds();
}

void AuPlotCurve::removeFirstPoints(int count)
{
    const int n = m_points.size();
    if (count <= 0 || n == 0)
        return;
    if (count >= n) {
        clearPoints();
        return;
    }
    m_points.remove(0, count);
    // 移除前缀不破坏剩余数据的相对顺序（升序标记不变），
    // 但极值可能随被移除的点丢失，包围盒下次全量重算一次
    m_runningBoundsValid = false;
    invalidateBounds();
}

void AuPlotCurve::clearPoints()
{
    m_points.clear();
    m_xAscending = true;
    m_runningBoundsValid = false;
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
    // 基线扩展只作用于返回副本、绝不写回缓存：否则全量重扫后关掉填充或
    // 调低基线，缓存里残留的旧基线会让 Y 量程一直被撑大
    auto applyFillBaseline = [this](const QRectF &bounds) {
        if (!m_fillVisible)
            return bounds;
        QRectF out = bounds;
        out.setTop(qMin(out.top(), m_fillBaseline));
        out.setBottom(qMax(out.bottom(), m_fillBaseline));
        return out;
    };

    // 增量缓存在（addPoint 逐点扩展）：O(1) 返回，实时追加不必全量重扫
    if (m_runningBoundsValid)
        return applyFillBaseline(m_runningBounds);

    // 全量重扫：跳过 NaN / Inf 点（无效点不参与量程）
    bool boundsValid = false;
    double minX = 0.0, maxX = 0.0, minY = 0.0, maxY = 0.0;
    for (const QPointF &pt : m_points) {
        if (!AuPlotInternal::isFinitePoint(pt))
            continue;
        if (!boundsValid) {
            minX = maxX = pt.x();
            minY = maxY = pt.y();
            boundsValid = true;
        } else {
            minX = qMin(minX, pt.x());
            maxX = qMax(maxX, pt.x());
            minY = qMin(minY, pt.y());
            maxY = qMax(maxY, pt.y());
        }
    }
    if (!boundsValid)
        return QRectF();

    m_runningBounds = QRectF(minX, minY, maxX - minX, maxY - minY);
    m_runningBoundsValid = true;
    return applyFillBaseline(m_runningBounds);
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

    // 按 NaN / Inf 断点把像素序列切成连续段：线与填充在断点处断开，
    // 无效点本身不绘制（传感器断采 / 缺数据的常规表达方式）
    QVector<QVector<QPointF>> runs;
    QVector<QPointF> current;
    for (const QPointF &px : pixels) {
        if (!std::isfinite(px.x()) || !std::isfinite(px.y())) {
            if (!current.isEmpty())
                runs.append(current);
            current.clear();
            continue;
        }
        current.append(px);
    }
    if (!current.isEmpty())
        runs.append(current);

    const QColor fallbackLineColor = m_lineColor.isValid()
                                         ? m_lineColor
                                         : QColor(0x2f, 0x6f, 0xdb);

    // 面：各连续段与基线之间填充（在线下层）
    if (m_fillVisible && m_fillColor.isValid() && m_fillColor.alpha() > 0) {
        const qreal baselineY = toPixelY(m_fillBaseline);
        for (const QVector<QPointF> &run : runs) {
            if (run.size() < 2)
                continue;
            QPainterPath fillPath;
            fillPath.moveTo(run.first());
            for (int i = 1; i < run.size(); ++i)
                fillPath.lineTo(run.at(i));
            fillPath.lineTo(run.last().x(), baselineY);
            fillPath.lineTo(run.first().x(), baselineY);
            fillPath.closeSubpath();
            painter->fillPath(fillPath, m_fillColor);
        }
    }

    // 线。
    // 密集曲线（点数超过像素列数 2 倍，抽稀后仍大量近垂直线段）时切换为 0 宽装饰笔：
    // 抗锯齿的常规描边在这种几何上会退化到几百毫秒，装饰笔走光栅引擎的快速
    // 线段路径只需 1~2ms，且视觉上密集折线本来就呈 1px 细带，无差别。
    if (m_lineVisible) {
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
        }
        for (const QVector<QPointF> &run : runs) {
            if (run.size() < 2)
                continue;
            if (dense) {
                painter->setPen(pen);
                painter->setBrush(Qt::NoBrush);
                painter->drawPolyline(run.constData(), run.size());
            } else {
                QPainterPath linePath;
                linePath.moveTo(run.first());
                for (int i = 1; i < run.size(); ++i)
                    linePath.lineTo(run.at(i));
                painter->setPen(pen);
                painter->setBrush(Qt::NoBrush);
                painter->drawPath(linePath);
            }
        }
    }

    // 点标记：点数超过像素可容纳数量时按步长抽稀，避免糊成一团；无效点跳过
    if (m_pointStyle != AuPlotPointStyle::None) {
        const int maxMarkers = qMax(1, int(rect.width()) / 2);
        const int stride = qMax(1, pixels.size() / maxMarkers);
        for (int i = 0; i < pixels.size(); i += stride) {
            const QPointF &px = pixels.at(i);
            if (!std::isfinite(px.x()) || !std::isfinite(px.y()))
                continue;
            AuPlotInternal::drawPointMarker(painter, px, m_pointStyle,
                                            m_pointSize,
                                            m_pointColor.isValid() ? m_pointColor
                                                                   : fallbackLineColor);
        }
    }
}

void AuPlotCurve::drawLegendIcon(QPainter *painter, const QRectF &iconRect)
{
    const QColor color = m_lineColor.isValid() ? m_lineColor : QColor(0x2f, 0x6f, 0xdb);
    const qreal midY = iconRect.center().y();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(AuPlotInternal::makeLinePen(color, color, m_lineWidth,
                                                m_lineStyle, m_dashPattern, m_dashOffset));
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(iconRect.left(), midY), QPointF(iconRect.right(), midY));
    if (m_pointStyle != AuPlotPointStyle::None) {
        AuPlotInternal::drawPointMarker(painter, iconRect.center(), m_pointStyle,
                                        qMin<qreal>(qreal(m_pointSize), iconRect.height() + 2.0),
                                        m_pointColor.isValid() ? m_pointColor : color);
    }
}
