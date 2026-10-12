#include "AuPlotItem.h"

#include "AuPlot.h"

#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QtMath>

// ============================================================
// 内部共享工具（点标记绘制 / 画笔构建）
// ============================================================

namespace AuPlotInternal {

void drawPointMarker(QPainter *painter, const QPointF &pos,
                     AuPlotPointStyle style, qreal size, const QColor &color)
{
    if (style == AuPlotPointStyle::None || !color.isValid() || size <= 0)
        return;

    const qreal radius = size / 2.0;
    const QRectF box(pos.x() - radius, pos.y() - radius, size, size);
    const QPen outline(color, qMax<qreal>(1.0, size * 0.15));
    QPen linePen(color, 1.5);
    linePen.setCapStyle(Qt::FlatCap);

    switch (style) {
    case AuPlotPointStyle::Circle:
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawEllipse(box);
        break;
    case AuPlotPointStyle::HollowCircle:
        painter->setPen(outline);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(box);
        break;
    case AuPlotPointStyle::Square:
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawRect(box);
        break;
    case AuPlotPointStyle::HollowSquare:
        painter->setPen(outline);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(box);
        break;
    case AuPlotPointStyle::Diamond:
    case AuPlotPointStyle::HollowDiamond: {
        QPolygonF diamond;
        diamond << QPointF(pos.x(), pos.y() - radius)
                << QPointF(pos.x() + radius, pos.y())
                << QPointF(pos.x(), pos.y() + radius)
                << QPointF(pos.x() - radius, pos.y());
        if (style == AuPlotPointStyle::Diamond) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(color);
        } else {
            painter->setPen(outline);
            painter->setBrush(Qt::NoBrush);
        }
        painter->drawPolygon(diamond);
        break;
    }
    case AuPlotPointStyle::Triangle:
    case AuPlotPointStyle::HollowTriangle: {
        // 等边三角形（顶点朝上），质心对准 pos
        const qreal h = size * 0.866;   // sqrt(3)/2
        QPolygonF tri;
        tri << QPointF(pos.x(), pos.y() - h * 2.0 / 3.0)
            << QPointF(pos.x() + radius, pos.y() + h / 3.0)
            << QPointF(pos.x() - radius, pos.y() + h / 3.0);
        if (style == AuPlotPointStyle::Triangle) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(color);
        } else {
            painter->setPen(outline);
            painter->setBrush(Qt::NoBrush);
        }
        painter->drawPolygon(tri);
        break;
    }
    case AuPlotPointStyle::Cross:
        painter->setPen(linePen);
        painter->drawLine(QPointF(pos.x(), pos.y() - radius), QPointF(pos.x(), pos.y() + radius));
        painter->drawLine(QPointF(pos.x() - radius, pos.y()), QPointF(pos.x() + radius, pos.y()));
        break;
    case AuPlotPointStyle::XCross: {
        painter->setPen(linePen);
        const qreal diag = radius * 0.7071;
        painter->drawLine(QPointF(pos.x() - diag, pos.y() - diag), QPointF(pos.x() + diag, pos.y() + diag));
        painter->drawLine(QPointF(pos.x() - diag, pos.y() + diag), QPointF(pos.x() + diag, pos.y() - diag));
        break;
    }
    case AuPlotPointStyle::RingedCross:
        // 带圈的十字架：外圈 + 内部十字（十字略短于外圈直径）
        painter->setPen(outline);
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(box);
        painter->setPen(linePen);
        painter->drawLine(QPointF(pos.x(), pos.y() - radius), QPointF(pos.x(), pos.y() + radius));
        painter->drawLine(QPointF(pos.x() - radius, pos.y()), QPointF(pos.x() + radius, pos.y()));
        break;
    case AuPlotPointStyle::None:
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

} // namespace AuPlotInternal

// ============================================================
// AuPlotItem 基类
// ============================================================

AuPlotItem::AuPlotItem() = default;

AuPlotItem::~AuPlotItem() = default;

int AuPlotItem::zLevel() const
{
    return m_zLevel;
}

void AuPlotItem::setZLevel(int z)
{
    if (m_zLevel == z)
        return;
    m_zLevel = z;
    if (m_plot)
        m_plot->reinsertItem(this);
}

bool AuPlotItem::isVisible() const
{
    return m_visible;
}

void AuPlotItem::setVisible(bool on)
{
    if (m_visible == on)
        return;
    m_visible = on;
    if (m_plot)
        m_plot->handleItemVisibility(this, on);
}

QString AuPlotItem::name() const
{
    return m_name;
}

void AuPlotItem::setName(const QString &name)
{
    if (m_name == name)
        return;
    m_name = name;
    if (m_plot)
        m_plot->update();
}

AuPlotAxisPos AuPlotItem::xAxis() const
{
    return m_xAxis;
}

void AuPlotItem::setXAxis(AuPlotAxisPos pos)
{
    if (m_xAxis == pos)
        return;
    m_xAxis = pos;
    m_boundsValid = false;   // 自动量程要换轴重算
    if (m_plot)
        m_plot->update();
}

AuPlotAxisPos AuPlotItem::yAxis() const
{
    return m_yAxis;
}

void AuPlotItem::setYAxis(AuPlotAxisPos pos)
{
    if (m_yAxis == pos)
        return;
    m_yAxis = pos;
    m_boundsValid = false;
    if (m_plot)
        m_plot->update();
}

QRectF AuPlotItem::dataBounds() const
{
    if (!m_boundsValid) {
        m_bounds = computeBounds();
        m_boundsValid = true;
    }
    return m_bounds;
}

void AuPlotItem::drawLegendIcon(QPainter *painter, const QRectF &iconRect)
{
    Q_UNUSED(painter);
    Q_UNUSED(iconRect);
}

void AuPlotItem::invalidateBounds()
{
    m_boundsValid = false;
    if (m_plot)
        m_plot->update();
}

AuPlot *AuPlotItem::attachedPlot() const
{
    return m_plot;
}
