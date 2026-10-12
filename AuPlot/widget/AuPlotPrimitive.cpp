#include "AuPlotPrimitive.h"

#include "AuPlot.h"
#include "AuPlotInternal.h"

#include <QPainter>
#include <QPainterPath>
#include <cmath>

// ============================================================
// AuPlotPoints 独立"点"
// ============================================================

AuPlotPoints::AuPlotPoints() = default;

AuPlotPoints::~AuPlotPoints() = default;

void AuPlotPoints::setPoints(const QVector<QPointF> &points)
{
    m_points = points;
    invalidateBounds();
}

void AuPlotPoints::addPoint(double x, double y)
{
    m_points.append(QPointF(x, y));
    invalidateBounds();
}

void AuPlotPoints::clearPoints()
{
    m_points.clear();
    invalidateBounds();
}

int AuPlotPoints::pointCount() const
{
    return m_points.size();
}

AuPlotPointStyle AuPlotPoints::pointStyle() const
{
    return m_pointStyle;
}

void AuPlotPoints::setPointStyle(AuPlotPointStyle style)
{
    m_pointStyle = style;
    if (attachedPlot())
        attachedPlot()->update();
}

int AuPlotPoints::pointSize() const
{
    return m_pointSize;
}

void AuPlotPoints::setPointSize(int size)
{
    m_pointSize = qMax(1, size);
    if (attachedPlot())
        attachedPlot()->update();
}

QColor AuPlotPoints::pointColor() const
{
    return m_pointColor;
}

void AuPlotPoints::setPointColor(const QColor &color)
{
    m_pointColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

QRectF AuPlotPoints::computeBounds() const
{
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
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

void AuPlotPoints::paint(QPainter *painter, const AuPlot *plot)
{
    if (m_points.isEmpty() || m_pointStyle == AuPlotPointStyle::None)
        return;

    const QRectF rect = plot->plotRect();
    const double xMinV = plot->pixelToX(rect.left(), xAxis());
    const double xMaxV = plot->pixelToX(rect.right(), xAxis());

    for (const QPointF &pt : m_points) {
        if (!AuPlotInternal::isFinitePoint(pt))
            continue;
        if (pt.x() < xMinV || pt.x() > xMaxV)
            continue;
        AuPlotInternal::drawPointMarker(painter,
                                        QPointF(plot->xToPixel(pt.x(), xAxis()),
                                                plot->yToPixel(pt.y(), yAxis())),
                                        m_pointStyle, m_pointSize, m_pointColor);
    }
}

void AuPlotPoints::drawLegendIcon(QPainter *painter, const QRectF &iconRect)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    AuPlotInternal::drawPointMarker(painter, iconRect.center(), m_pointStyle,
                                    qMin<qreal>(qreal(m_pointSize), iconRect.height() + 2.0),
                                    m_pointColor);
}

// ============================================================
// AuPlotLine 独立"线"
// ============================================================

AuPlotLine::AuPlotLine() = default;

AuPlotLine::~AuPlotLine() = default;

void AuPlotLine::setPoints(const QVector<QPointF> &points)
{
    m_points = points;
    invalidateBounds();
}

void AuPlotLine::addPoint(double x, double y)
{
    m_points.append(QPointF(x, y));
    invalidateBounds();
}

void AuPlotLine::clearPoints()
{
    m_points.clear();
    invalidateBounds();
}

int AuPlotLine::pointCount() const
{
    return m_points.size();
}

QColor AuPlotLine::lineColor() const
{
    return m_lineColor;
}

void AuPlotLine::setLineColor(const QColor &color)
{
    m_lineColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AuPlotLine::lineWidth() const
{
    return m_lineWidth;
}

void AuPlotLine::setLineWidth(qreal width)
{
    m_lineWidth = qMax<qreal>(0.0, width);
    if (attachedPlot())
        attachedPlot()->update();
}

Qt::PenStyle AuPlotLine::lineStyle() const
{
    return m_lineStyle;
}

void AuPlotLine::setLineStyle(Qt::PenStyle style)
{
    m_lineStyle = style;
    if (attachedPlot())
        attachedPlot()->update();
}

QVector<qreal> AuPlotLine::dashPattern() const
{
    return m_dashPattern;
}

void AuPlotLine::setDashPattern(const QVector<qreal> &pattern)
{
    m_dashPattern = pattern;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AuPlotLine::dashOffset() const
{
    return m_dashOffset;
}

void AuPlotLine::setDashOffset(qreal offset)
{
    m_dashOffset = offset;
    if (attachedPlot())
        attachedPlot()->update();
}

QRectF AuPlotLine::computeBounds() const
{
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
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

void AuPlotLine::paint(QPainter *painter, const AuPlot *plot)
{
    if (m_points.size() < 2)
        return;

    // NaN / Inf 点断开线段：各连续段单独一条子路径
    QPainterPath path;
    bool hasCurrent = false;
    for (const QPointF &pt : m_points) {
        if (!AuPlotInternal::isFinitePoint(pt)) {
            hasCurrent = false;
            continue;
        }
        const QPointF px(plot->xToPixel(pt.x(), xAxis()),
                         plot->yToPixel(pt.y(), yAxis()));
        if (hasCurrent)
            path.lineTo(px);
        else {
            path.moveTo(px);
            hasCurrent = true;
        }
    }
    if (!hasCurrent)
        return;

    painter->setPen(AuPlotInternal::makeLinePen(m_lineColor, QColor(), m_lineWidth,
                                                m_lineStyle, m_dashPattern, m_dashOffset));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}

void AuPlotLine::drawLegendIcon(QPainter *painter, const QRectF &iconRect)
{
    const qreal midY = iconRect.center().y();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(AuPlotInternal::makeLinePen(m_lineColor, m_lineColor, m_lineWidth,
                                                m_lineStyle, m_dashPattern, m_dashOffset));
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(iconRect.left(), midY), QPointF(iconRect.right(), midY));
}

// ============================================================
// AuPlotArea 独立"面"
// ============================================================

AuPlotArea::AuPlotArea()
    : m_fillColor(47, 111, 219, 70)
{
}

AuPlotArea::~AuPlotArea() = default;

void AuPlotArea::setPoints(const QVector<QPointF> &points)
{
    m_points = points;
    invalidateBounds();
}

void AuPlotArea::addPoint(double x, double y)
{
    m_points.append(QPointF(x, y));
    invalidateBounds();
}

void AuPlotArea::clearPoints()
{
    m_points.clear();
    invalidateBounds();
}

int AuPlotArea::pointCount() const
{
    return m_points.size();
}

QColor AuPlotArea::fillColor() const
{
    return m_fillColor;
}

void AuPlotArea::setFillColor(const QColor &color)
{
    m_fillColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

bool AuPlotArea::borderVisible() const
{
    return m_borderVisible;
}

void AuPlotArea::setBorderVisible(bool on)
{
    if (m_borderVisible == on)
        return;
    m_borderVisible = on;
    if (attachedPlot())
        attachedPlot()->update();
}

QColor AuPlotArea::borderColor() const
{
    return m_borderColor;
}

void AuPlotArea::setBorderColor(const QColor &color)
{
    m_borderColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AuPlotArea::borderWidth() const
{
    return m_borderWidth;
}

void AuPlotArea::setBorderWidth(qreal width)
{
    m_borderWidth = qMax<qreal>(0.0, width);
    if (attachedPlot())
        attachedPlot()->update();
}

Qt::PenStyle AuPlotArea::borderStyle() const
{
    return m_borderStyle;
}

void AuPlotArea::setBorderStyle(Qt::PenStyle style)
{
    m_borderStyle = style;
    if (attachedPlot())
        attachedPlot()->update();
}

QVector<qreal> AuPlotArea::borderDashPattern() const
{
    return m_borderDashPattern;
}

void AuPlotArea::setBorderDashPattern(const QVector<qreal> &pattern)
{
    m_borderDashPattern = pattern;
    if (attachedPlot())
        attachedPlot()->update();
}

QRectF AuPlotArea::computeBounds() const
{
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
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

void AuPlotArea::paint(QPainter *painter, const AuPlot *plot)
{
    if (m_points.size() < 2)
        return;

    // NaN / Inf 顶点把多边形切成多个连续段，各段独立闭合填充 / 描边
    QPainterPath path;
    int runStart = -1;   // 当前连续段的起始索引；-1 = 无活动段
    const int n = m_points.size();
    auto closeRun = [&](int end) {
        if (runStart < 0 || end - runStart < 1)
            return;
        path.moveTo(plot->xToPixel(m_points.at(runStart).x(), xAxis()),
                    plot->yToPixel(m_points.at(runStart).y(), yAxis()));
        for (int i = runStart + 1; i <= end; ++i)
            path.lineTo(plot->xToPixel(m_points.at(i).x(), xAxis()),
                        plot->yToPixel(m_points.at(i).y(), yAxis()));
        path.closeSubpath();   // 自动闭合
    };
    for (int i = 0; i < n; ++i) {
        if (!AuPlotInternal::isFinitePoint(m_points.at(i))) {
            closeRun(i - 1);
            runStart = -1;
        } else if (runStart < 0) {
            runStart = i;
        }
    }
    closeRun(n - 1);
    if (path.isEmpty())
        return;

    if (m_fillColor.isValid() && m_fillColor.alpha() > 0)
        painter->fillPath(path, m_fillColor);

    if (m_borderVisible) {
        painter->setPen(AuPlotInternal::makeLinePen(m_borderColor, m_borderColor,
                                                    m_borderWidth, m_borderStyle,
                                                    m_borderDashPattern, 0.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    }
}

void AuPlotArea::drawLegendIcon(QPainter *painter, const QRectF &iconRect)
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    if (m_fillColor.isValid() && m_fillColor.alpha() > 0) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_fillColor);
        painter->drawRect(iconRect);
    }
    if (m_borderVisible) {
        painter->setPen(AuPlotInternal::makeLinePen(m_borderColor, m_borderColor,
                                                    1.0, m_borderStyle,
                                                    m_borderDashPattern, 0.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(iconRect);
    }
}

// ============================================================
// AuPlotRefLine 参考线（水平 / 垂直全幅）
// ============================================================

AuPlotRefLine::AuPlotRefLine() = default;

AuPlotRefLine::AuPlotRefLine(Orientation orientation)
    : m_orientation(orientation)
{
}

AuPlotRefLine::~AuPlotRefLine() = default;

AuPlotRefLine::Orientation AuPlotRefLine::orientation() const
{
    return m_orientation;
}

void AuPlotRefLine::setOrientation(Orientation orientation)
{
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    if (attachedPlot())
        attachedPlot()->update();
}

double AuPlotRefLine::value() const
{
    return m_value;
}

void AuPlotRefLine::setValue(double value)
{
    if (qFuzzyCompare(m_value, value))
        return;
    m_value = value;
    if (attachedPlot())
        attachedPlot()->update();
}

QColor AuPlotRefLine::lineColor() const
{
    return m_lineColor;
}

void AuPlotRefLine::setLineColor(const QColor &color)
{
    m_lineColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AuPlotRefLine::lineWidth() const
{
    return m_lineWidth;
}

void AuPlotRefLine::setLineWidth(qreal width)
{
    m_lineWidth = qMax<qreal>(0.0, width);
    if (attachedPlot())
        attachedPlot()->update();
}

Qt::PenStyle AuPlotRefLine::lineStyle() const
{
    return m_lineStyle;
}

void AuPlotRefLine::setLineStyle(Qt::PenStyle style)
{
    m_lineStyle = style;
    if (attachedPlot())
        attachedPlot()->update();
}

QVector<qreal> AuPlotRefLine::dashPattern() const
{
    return m_dashPattern;
}

void AuPlotRefLine::setDashPattern(const QVector<qreal> &pattern)
{
    m_dashPattern = pattern;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AuPlotRefLine::dashOffset() const
{
    return m_dashOffset;
}

void AuPlotRefLine::setDashOffset(qreal offset)
{
    m_dashOffset = offset;
    if (attachedPlot())
        attachedPlot()->update();
}

QRectF AuPlotRefLine::computeBounds() const
{
    // 参考线不参与自适应量程（见类注释）
    return QRectF();
}

void AuPlotRefLine::paint(QPainter *painter, const AuPlot *plot)
{
    if (!std::isfinite(m_value))
        return;
    const QRectF rect = plot->plotRect();
    painter->setPen(AuPlotInternal::makeLinePen(m_lineColor, QColor(), m_lineWidth,
                                                m_lineStyle, m_dashPattern, m_dashOffset));
    painter->setBrush(Qt::NoBrush);
    if (m_orientation == Orientation::Horizontal) {
        const qreal y = plot->yToPixel(m_value, yAxis());
        painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    } else {
        const qreal x = plot->xToPixel(m_value, xAxis());
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
}

void AuPlotRefLine::drawLegendIcon(QPainter *painter, const QRectF &iconRect)
{
    const qreal midY = iconRect.center().y();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(AuPlotInternal::makeLinePen(m_lineColor, m_lineColor, m_lineWidth,
                                                m_lineStyle, m_dashPattern, m_dashOffset));
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(iconRect.left(), midY), QPointF(iconRect.right(), midY));
}
