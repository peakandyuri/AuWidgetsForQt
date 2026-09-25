#include "AgPlotItem.h"

#include "AgPlot.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QtMath>

// ============================================================
// 内部共享工具（点标记绘制 / 画笔构建）
// ============================================================

namespace AgPlotInternal {

void drawPointMarker(QPainter *painter, const QPointF &pos,
                     AgPlotPointStyle style, qreal size, const QColor &color)
{
    if (style == AgPlotPointStyle::None || !color.isValid() || size <= 0)
        return;

    const qreal radius = size / 2.0;
    const QRectF box(pos.x() - radius, pos.y() - radius, size, size);
    const QPen outline(color, qMax<qreal>(1.0, size * 0.15));
    QPen linePen(color, 1.5);
    linePen.setCapStyle(Qt::FlatCap);

    switch (style) {
    case AgPlotPointStyle::Circle:
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawEllipse(box);
        break;
    case AgPlotPointStyle::HollowCircle:
        painter->setPen(outline);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(box);
        break;
    case AgPlotPointStyle::Square:
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawRect(box);
        break;
    case AgPlotPointStyle::HollowSquare:
        painter->setPen(outline);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(box);
        break;
    case AgPlotPointStyle::Diamond:
    case AgPlotPointStyle::HollowDiamond: {
        QPolygonF diamond;
        diamond << QPointF(pos.x(), pos.y() - radius)
                << QPointF(pos.x() + radius, pos.y())
                << QPointF(pos.x(), pos.y() + radius)
                << QPointF(pos.x() - radius, pos.y());
        if (style == AgPlotPointStyle::Diamond) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(color);
        } else {
            painter->setPen(outline);
            painter->setBrush(Qt::NoBrush);
        }
        painter->drawPolygon(diamond);
        break;
    }
    case AgPlotPointStyle::Triangle:
    case AgPlotPointStyle::HollowTriangle: {
        // 等边三角形（顶点朝上），质心对准 pos
        const qreal h = size * 0.866;   // sqrt(3)/2
        QPolygonF tri;
        tri << QPointF(pos.x(), pos.y() - h * 2.0 / 3.0)
            << QPointF(pos.x() + radius, pos.y() + h / 3.0)
            << QPointF(pos.x() - radius, pos.y() + h / 3.0);
        if (style == AgPlotPointStyle::Triangle) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(color);
        } else {
            painter->setPen(outline);
            painter->setBrush(Qt::NoBrush);
        }
        painter->drawPolygon(tri);
        break;
    }
    case AgPlotPointStyle::Cross:
        painter->setPen(linePen);
        painter->drawLine(QPointF(pos.x(), pos.y() - radius), QPointF(pos.x(), pos.y() + radius));
        painter->drawLine(QPointF(pos.x() - radius, pos.y()), QPointF(pos.x() + radius, pos.y()));
        break;
    case AgPlotPointStyle::XCross: {
        painter->setPen(linePen);
        const qreal diag = radius * 0.7071;
        painter->drawLine(QPointF(pos.x() - diag, pos.y() - diag), QPointF(pos.x() + diag, pos.y() + diag));
        painter->drawLine(QPointF(pos.x() - diag, pos.y() + diag), QPointF(pos.x() + diag, pos.y() - diag));
        break;
    }
    case AgPlotPointStyle::RingedCross:
        // 带圈的十字架：外圈 + 内部十字（十字略短于外圈直径）
        painter->setPen(outline);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(box);
        painter->setPen(linePen);
        painter->drawLine(QPointF(pos.x(), pos.y() - radius), QPointF(pos.x(), pos.y() + radius));
        painter->drawLine(QPointF(pos.x() - radius, pos.y()), QPointF(pos.x() + radius, pos.y()));
        break;
    case AgPlotPointStyle::None:
        break;
    }
}

QPen makeLinePen(const QColor &color, const QColor &fallback, qreal width,
                 Qt::PenStyle style, const QVector<qreal> &dashPattern, qreal dashOffset)
{
    QColor effective = color.isValid() ? color : fallback;
    if (!effective.isValid())
        effective = QColor(0x2f, 0x6f, 0xdb);

    QPen pen(effective, width);
    pen.setCapStyle(Qt::FlatCap);
    pen.setJoinStyle(Qt::RoundJoin);
    if (!dashPattern.isEmpty()) {
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern(dashPattern);
        pen.setDashOffset(dashOffset);
    } else {
        pen.setStyle(style);
        if (style == Qt::DashLine || style == Qt::DotLine
            || style == Qt::DashDotLine || style == Qt::DashDotDotLine) {
            pen.setDashOffset(dashOffset);
        }
    }
    return pen;
}

} // namespace AgPlotInternal

// ============================================================
// AgPlotItem 基类
// ============================================================

AgPlotItem::AgPlotItem() = default;

AgPlotItem::~AgPlotItem() = default;

int AgPlotItem::zLevel() const
{
    return m_zLevel;
}

void AgPlotItem::setZLevel(int z)
{
    if (m_zLevel == z)
        return;
    m_zLevel = z;
    if (m_plot)
        m_plot->reinsertItem(this);
}

bool AgPlotItem::isVisible() const
{
    return m_visible;
}

void AgPlotItem::setVisible(bool on)
{
    if (m_visible == on)
        return;
    m_visible = on;
    if (m_plot)
        m_plot->update();
}

AgPlotAxisPos AgPlotItem::xAxis() const
{
    return m_xAxis;
}

void AgPlotItem::setXAxis(AgPlotAxisPos pos)
{
    if (m_xAxis == pos)
        return;
    m_xAxis = pos;
    m_boundsValid = false;   // 自动量程要换轴重算
    if (m_plot)
        m_plot->update();
}

AgPlotAxisPos AgPlotItem::yAxis() const
{
    return m_yAxis;
}

void AgPlotItem::setYAxis(AgPlotAxisPos pos)
{
    if (m_yAxis == pos)
        return;
    m_yAxis = pos;
    m_boundsValid = false;
    if (m_plot)
        m_plot->update();
}

QRectF AgPlotItem::dataBounds() const
{
    if (!m_boundsValid) {
        m_bounds = computeBounds();
        m_boundsValid = true;
    }
    return m_bounds;
}

void AgPlotItem::invalidateBounds()
{
    m_boundsValid = false;
    if (m_plot)
        m_plot->update();
}

AgPlot *AgPlotItem::attachedPlot() const
{
    return m_plot;
}
