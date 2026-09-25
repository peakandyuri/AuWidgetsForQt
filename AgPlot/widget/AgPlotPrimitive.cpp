#include "AgPlotPrimitive.h"

#include "AgPlot.h"
#include "AgPlotInternal.h"

#include <QPainter>
#include <QPainterPath>

// ============================================================
// AgPlotPoints 独立"点"
// ============================================================

AgPlotPoints::AgPlotPoints() = default;

AgPlotPoints::~AgPlotPoints() = default;

void AgPlotPoints::setPoints(const QVector<QPointF> &points)
{
    m_points = points;
    invalidateBounds();
}

void AgPlotPoints::addPoint(double x, double y)
{
    m_points.append(QPointF(x, y));
    invalidateBounds();
}

void AgPlotPoints::clearPoints()
{
    m_points.clear();
    invalidateBounds();
}

int AgPlotPoints::pointCount() const
{
    return m_points.size();
}

AgPlotPointStyle AgPlotPoints::pointStyle() const
{
    return m_pointStyle;
}

void AgPlotPoints::setPointStyle(AgPlotPointStyle style)
{
    m_pointStyle = style;
    if (attachedPlot())
        attachedPlot()->update();
}

int AgPlotPoints::pointSize() const
{
    return m_pointSize;
}

void AgPlotPoints::setPointSize(int size)
{
    m_pointSize = qMax(1, size);
    if (attachedPlot())
        attachedPlot()->update();
}

QColor AgPlotPoints::pointColor() const
{
    return m_pointColor;
}

void AgPlotPoints::setPointColor(const QColor &color)
{
    m_pointColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

QRectF AgPlotPoints::computeBounds() const
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
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

void AgPlotPoints::paint(QPainter *painter, const AgPlot *plot)
{
    if (m_points.isEmpty() || m_pointStyle == AgPlotPointStyle::None)
        return;

    const QRectF rect = plot->plotRect();
    const double xMinV = plot->pixelToX(rect.left(), xAxis());
    const double xMaxV = plot->pixelToX(rect.right(), xAxis());

    for (const QPointF &pt : m_points) {
        if (pt.x() < xMinV || pt.x() > xMaxV)
            continue;
        AgPlotInternal::drawPointMarker(painter,
                                        QPointF(plot->xToPixel(pt.x(), xAxis()),
                                                plot->yToPixel(pt.y(), yAxis())),
                                        m_pointStyle, m_pointSize, m_pointColor);
    }
}

// ============================================================
// AgPlotLine 独立"线"
// ============================================================

AgPlotLine::AgPlotLine() = default;

AgPlotLine::~AgPlotLine() = default;

void AgPlotLine::setPoints(const QVector<QPointF> &points)
{
    m_points = points;
    invalidateBounds();
}

void AgPlotLine::addPoint(double x, double y)
{
    m_points.append(QPointF(x, y));
    invalidateBounds();
}

void AgPlotLine::clearPoints()
{
    m_points.clear();
    invalidateBounds();
}

int AgPlotLine::pointCount() const
{
    return m_points.size();
}

QColor AgPlotLine::lineColor() const
{
    return m_lineColor;
}

void AgPlotLine::setLineColor(const QColor &color)
{
    m_lineColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AgPlotLine::lineWidth() const
{
    return m_lineWidth;
}

void AgPlotLine::setLineWidth(qreal width)
{
    m_lineWidth = qMax<qreal>(0.0, width);
    if (attachedPlot())
        attachedPlot()->update();
}

Qt::PenStyle AgPlotLine::lineStyle() const
{
    return m_lineStyle;
}

void AgPlotLine::setLineStyle(Qt::PenStyle style)
{
    m_lineStyle = style;
    if (attachedPlot())
        attachedPlot()->update();
}

QVector<qreal> AgPlotLine::dashPattern() const
{
    return m_dashPattern;
}

void AgPlotLine::setDashPattern(const QVector<qreal> &pattern)
{
    m_dashPattern = pattern;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AgPlotLine::dashOffset() const
{
    return m_dashOffset;
}

void AgPlotLine::setDashOffset(qreal offset)
{
    m_dashOffset = offset;
    if (attachedPlot())
        attachedPlot()->update();
}

QRectF AgPlotLine::computeBounds() const
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
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

void AgPlotLine::paint(QPainter *painter, const AgPlot *plot)
{
    if (m_points.size() < 2)
        return;

    QPainterPath path;
    path.moveTo(plot->xToPixel(m_points.constFirst().x(), xAxis()),
                plot->yToPixel(m_points.constFirst().y(), yAxis()));
    for (int i = 1; i < m_points.size(); ++i) {
        path.lineTo(plot->xToPixel(m_points.at(i).x(), xAxis()),
                    plot->yToPixel(m_points.at(i).y(), yAxis()));
    }
    painter->setPen(AgPlotInternal::makeLinePen(m_lineColor, QColor(), m_lineWidth,
                                                m_lineStyle, m_dashPattern, m_dashOffset));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path);
}

// ============================================================
// AgPlotArea 独立"面"
// ============================================================

AgPlotArea::AgPlotArea()
    : m_fillColor(47, 111, 219, 70)
{
}

AgPlotArea::~AgPlotArea() = default;

void AgPlotArea::setPoints(const QVector<QPointF> &points)
{
    m_points = points;
    invalidateBounds();
}

void AgPlotArea::addPoint(double x, double y)
{
    m_points.append(QPointF(x, y));
    invalidateBounds();
}

void AgPlotArea::clearPoints()
{
    m_points.clear();
    invalidateBounds();
}

int AgPlotArea::pointCount() const
{
    return m_points.size();
}

QColor AgPlotArea::fillColor() const
{
    return m_fillColor;
}

void AgPlotArea::setFillColor(const QColor &color)
{
    m_fillColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

bool AgPlotArea::borderVisible() const
{
    return m_borderVisible;
}

void AgPlotArea::setBorderVisible(bool on)
{
    if (m_borderVisible == on)
        return;
    m_borderVisible = on;
    if (attachedPlot())
        attachedPlot()->update();
}

QColor AgPlotArea::borderColor() const
{
    return m_borderColor;
}

void AgPlotArea::setBorderColor(const QColor &color)
{
    m_borderColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AgPlotArea::borderWidth() const
{
    return m_borderWidth;
}

void AgPlotArea::setBorderWidth(qreal width)
{
    m_borderWidth = qMax<qreal>(0.0, width);
    if (attachedPlot())
        attachedPlot()->update();
}

Qt::PenStyle AgPlotArea::borderStyle() const
{
    return m_borderStyle;
}

void AgPlotArea::setBorderStyle(Qt::PenStyle style)
{
    m_borderStyle = style;
    if (attachedPlot())
        attachedPlot()->update();
}

QVector<qreal> AgPlotArea::borderDashPattern() const
{
    return m_borderDashPattern;
}

void AgPlotArea::setBorderDashPattern(const QVector<qreal> &pattern)
{
    m_borderDashPattern = pattern;
    if (attachedPlot())
        attachedPlot()->update();
}

QRectF AgPlotArea::computeBounds() const
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
    return QRectF(minX, minY, maxX - minX, maxY - minY);
}

void AgPlotArea::paint(QPainter *painter, const AgPlot *plot)
{
    if (m_points.size() < 2)
        return;

    QPainterPath path;
    path.moveTo(plot->xToPixel(m_points.constFirst().x(), xAxis()),
                plot->yToPixel(m_points.constFirst().y(), yAxis()));
    for (int i = 1; i < m_points.size(); ++i) {
        path.lineTo(plot->xToPixel(m_points.at(i).x(), xAxis()),
                    plot->yToPixel(m_points.at(i).y(), yAxis()));
    }
    path.closeSubpath();   // 自动闭合

    if (m_fillColor.isValid() && m_fillColor.alpha() > 0)
        painter->fillPath(path, m_fillColor);

    if (m_borderVisible) {
        painter->setPen(AgPlotInternal::makeLinePen(m_borderColor, m_borderColor,
                                                    m_borderWidth, m_borderStyle,
                                                    m_borderDashPattern, 0.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    }
}

// ============================================================
// AgPlotRefLine 参考线（水平 / 垂直全幅）
// ============================================================

AgPlotRefLine::AgPlotRefLine() = default;

AgPlotRefLine::AgPlotRefLine(Orientation orientation)
    : m_orientation(orientation)
{
}

AgPlotRefLine::~AgPlotRefLine() = default;

AgPlotRefLine::Orientation AgPlotRefLine::orientation() const
{
    return m_orientation;
}

void AgPlotRefLine::setOrientation(Orientation orientation)
{
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    if (attachedPlot())
        attachedPlot()->update();
}

double AgPlotRefLine::value() const
{
    return m_value;
}

void AgPlotRefLine::setValue(double value)
{
    if (qFuzzyCompare(m_value, value))
        return;
    m_value = value;
    if (attachedPlot())
        attachedPlot()->update();
}

QColor AgPlotRefLine::lineColor() const
{
    return m_lineColor;
}

void AgPlotRefLine::setLineColor(const QColor &color)
{
    m_lineColor = color;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AgPlotRefLine::lineWidth() const
{
    return m_lineWidth;
}

void AgPlotRefLine::setLineWidth(qreal width)
{
    m_lineWidth = qMax<qreal>(0.0, width);
    if (attachedPlot())
        attachedPlot()->update();
}

Qt::PenStyle AgPlotRefLine::lineStyle() const
{
    return m_lineStyle;
}

void AgPlotRefLine::setLineStyle(Qt::PenStyle style)
{
    m_lineStyle = style;
    if (attachedPlot())
        attachedPlot()->update();
}

QVector<qreal> AgPlotRefLine::dashPattern() const
{
    return m_dashPattern;
}

void AgPlotRefLine::setDashPattern(const QVector<qreal> &pattern)
{
    m_dashPattern = pattern;
    if (attachedPlot())
        attachedPlot()->update();
}

qreal AgPlotRefLine::dashOffset() const
{
    return m_dashOffset;
}

void AgPlotRefLine::setDashOffset(qreal offset)
{
    m_dashOffset = offset;
    if (attachedPlot())
        attachedPlot()->update();
}

QRectF AgPlotRefLine::computeBounds() const
{
    // 参考线不参与自适应量程（见类注释）
    return QRectF();
}

void AgPlotRefLine::paint(QPainter *painter, const AgPlot *plot)
{
    const QRectF rect = plot->plotRect();
    painter->setPen(AgPlotInternal::makeLinePen(m_lineColor, QColor(), m_lineWidth,
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
