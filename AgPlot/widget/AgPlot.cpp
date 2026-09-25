#include "AgPlot.h"

#include "AgPlotCurve.h"
#include "AgPlotInternal.h"
#include "AgPlotPrimitive.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>
#include <QtMath>
#include <cmath>

namespace {

/// 刻度值/范围比较容差。
bool closeEnough(double a, double b)
{
    return std::fabs(a - b) <= 1e-9 * qMax(1.0, qMax(std::fabs(a), std::fabs(b)));
}

} // namespace

// ============================================================
// 构造 / 析构
// ============================================================

AgPlot::AgPlot(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    // 背景由 paintEvent 手动填充：无效颜色时保持透明（透出父部件）
    setAttribute(Qt::WA_NoSystemBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_xAxis = new AgPlotAxis(this);
    m_yAxis = new AgPlotAxis(this);
    m_xAxis2 = new AgPlotAxis(this);
    m_yAxis2 = new AgPlotAxis(this);

    // 默认单轴：X2 / Y2 隐藏；次轴网格默认关闭（避免与主轴网格重复）
    m_xAxis2->setVisible(false);
    m_yAxis2->setVisible(false);
    m_xAxis2->setGridVisible(false);
    m_yAxis2->setGridVisible(false);

    m_defaultStyle = new AgPlotStyle;
}

AgPlot::~AgPlot()
{
    qDeleteAll(m_items);
    delete m_customStyle;
    delete m_defaultStyle;
}

// ============================================================
// 坐标轴
// ============================================================

AgPlotAxis *AgPlot::xAxis() const
{
    return m_xAxis;
}

AgPlotAxis *AgPlot::yAxis() const
{
    return m_yAxis;
}

AgPlotAxis *AgPlot::xAxis2() const
{
    return m_xAxis2;
}

AgPlotAxis *AgPlot::yAxis2() const
{
    return m_yAxis2;
}

bool AgPlot::dualAxisEnabled() const
{
    return m_xAxis2->isVisible() && m_yAxis2->isVisible();
}

void AgPlot::setDualAxisEnabled(bool on)
{
    m_xAxis2->setVisible(on);
    m_yAxis2->setVisible(on);
}

AgPlotAxis *AgPlot::axisAt(AgPlotAxisPos pos) const
{
    switch (pos) {
    case AgPlotAxisPos::X2: return m_xAxis2;
    case AgPlotAxisPos::Y1: return m_yAxis;
    case AgPlotAxisPos::Y2: return m_yAxis2;
    case AgPlotAxisPos::X1: break;
    }
    return m_xAxis;
}

// ============================================================
// 图元管理
// ============================================================

void AgPlot::addItem(AgPlotItem *item)
{
    if (!item)
        return;
    if (item->attachedPlot()) {
        qWarning("AgPlot::addItem: 图元已属于某个 AgPlot，先从原控件移除再加入");
        return;
    }
    item->m_plot = this;

    // 按 zLevel 升序插入（稳定：同层级后加入的画在上层）
    int pos = 0;
    while (pos < m_items.size() && m_items.at(pos)->zLevel() <= item->zLevel())
        ++pos;
    m_items.insert(pos, item);

    assignDefaultColor(item);
    update();
}

void AgPlot::removeItem(AgPlotItem *item)
{
    if (!item)
        return;
    const int index = m_items.indexOf(item);
    if (index < 0)
        return;
    m_items.removeAt(index);
    item->m_plot = nullptr;
    if (m_trackerCurve == item) {   // 防止追踪器持有悬空指针
        m_trackerCurve = nullptr;
        m_trackerIndex = -1;
        m_trackerValid = false;
    }
    delete item;
    update();
}

void AgPlot::clearItems()
{
    if (m_items.isEmpty())
        return;
    for (AgPlotItem *item : m_items)
        item->m_plot = nullptr;
    qDeleteAll(m_items);
    m_items.clear();
    m_trackerCurve = nullptr;
    m_trackerIndex = -1;
    m_trackerValid = false;
    update();
}

QList<AgPlotItem *> AgPlot::items() const
{
    return m_items;
}

void AgPlot::reinsertItem(AgPlotItem *item)
{
    const int oldIndex = m_items.indexOf(item);
    if (oldIndex < 0)
        return;
    m_items.removeAt(oldIndex);
    int pos = 0;
    while (pos < m_items.size() && m_items.at(pos)->zLevel() <= item->zLevel())
        ++pos;
    m_items.insert(pos, item);
    update();
}

void AgPlot::assignDefaultColor(AgPlotItem *item)
{
    AgPlotStyle *st = effectiveStyle();
    if (AgPlotCurve *curve = dynamic_cast<AgPlotCurve *>(item)) {
        if (!curve->m_lineColor.isValid()) {
            const QColor color = st->nextCurveColor();
            curve->m_lineColor = color;
            if (curve->m_pointStyle != AgPlotPointStyle::None && !curve->m_pointColor.isValid())
                curve->m_pointColor = color;
        }
    } else if (AgPlotPoints *points = dynamic_cast<AgPlotPoints *>(item)) {
        if (!points->m_pointColor.isValid())
            points->m_pointColor = st->nextCurveColor();
    } else if (AgPlotLine *line = dynamic_cast<AgPlotLine *>(item)) {
        if (!line->m_lineColor.isValid())
            line->m_lineColor = st->nextCurveColor();
    } else if (AgPlotArea *area = dynamic_cast<AgPlotArea *>(item)) {
        if (!area->m_borderColor.isValid())
            area->m_borderColor = st->nextCurveColor();
    } else if (AgPlotRefLine *refLine = dynamic_cast<AgPlotRefLine *>(item)) {
        if (!refLine->m_lineColor.isValid())
            refLine->m_lineColor = st->nextCurveColor();
    }
}

// ============================================================
// 坐标映射
// ============================================================

QRectF AgPlot::plotRect() const
{
    return m_plotRect.isNull() ? calculatePlotRect() : m_plotRect;
}

qreal AgPlot::xToPixel(double x, AgPlotAxisPos axis) const
{
    const AgPlotAxis *ax = axisAt(axis);
    const QRectF rect = plotRect();
    const double span = ax->maximum() - ax->minimum();
    if (qFuzzyIsNull(span))
        return rect.left() + rect.width() / 2.0;
    return rect.left() + (x - ax->minimum()) / span * rect.width();
}

qreal AgPlot::yToPixel(double y, AgPlotAxisPos axis) const
{
    const AgPlotAxis *ax = axisAt(axis);
    const QRectF rect = plotRect();
    const double span = ax->maximum() - ax->minimum();
    if (qFuzzyIsNull(span))
        return rect.top() + rect.height() / 2.0;
    return rect.bottom() - (y - ax->minimum()) / span * rect.height();
}

double AgPlot::pixelToX(qreal pixelX, AgPlotAxisPos axis) const
{
    const AgPlotAxis *ax = axisAt(axis);
    const QRectF rect = plotRect();
    if (qFuzzyIsNull(rect.width()))
        return ax->minimum();
    return ax->minimum() + (pixelX - rect.left()) / rect.width() * (ax->maximum() - ax->minimum());
}

double AgPlot::pixelToY(qreal pixelY, AgPlotAxisPos axis) const
{
    const AgPlotAxis *ax = axisAt(axis);
    const QRectF rect = plotRect();
    if (qFuzzyIsNull(rect.height()))
        return ax->minimum();
    return ax->minimum() + (rect.bottom() - pixelY) / rect.height() * (ax->maximum() - ax->minimum());
}

// ============================================================
// 样式
// ============================================================

AgPlotStyle *AgPlot::style() const
{
    return effectiveStyle();
}

void AgPlot::setStyle(AgPlotStyle *style)
{
    if (style == m_customStyle)
        return;
    delete m_customStyle;
    m_customStyle = style;
    update();
}

AgPlotStyle *AgPlot::effectiveStyle() const
{
    if (m_customStyle)
        return m_customStyle;
    if (AgPlotStyle::globalStyle())
        return AgPlotStyle::globalStyle();
    return m_defaultStyle;
}

// ============================================================
// 交互（默认关闭）
// ============================================================

bool AgPlot::dragEnabled() const
{
    return m_dragEnabled;
}

void AgPlot::setDragEnabled(bool on)
{
    m_dragEnabled = on;
    if (!on)
        unsetCursor();
}

Qt::Orientations AgPlot::dragDirections() const
{
    return m_dragDirections;
}

void AgPlot::setDragDirections(Qt::Orientations directions)
{
    m_dragDirections = directions;
}

bool AgPlot::zoomEnabled() const
{
    return m_zoomEnabled;
}

void AgPlot::setZoomEnabled(bool on)
{
    m_zoomEnabled = on;
}

Qt::Orientations AgPlot::zoomDirections() const
{
    return m_zoomDirections;
}

void AgPlot::setZoomDirections(Qt::Orientations directions)
{
    m_zoomDirections = directions;
}

qreal AgPlot::zoomFactor() const
{
    return m_zoomFactor;
}

void AgPlot::setZoomFactor(qreal factor)
{
    m_zoomFactor = qMax<qreal>(1.01, factor);
}

bool AgPlot::trackerEnabled() const
{
    return m_trackerEnabled;
}

void AgPlot::setTrackerEnabled(bool on)
{
    if (m_trackerEnabled == on)
        return;
    m_trackerEnabled = on;
    if (!on && (m_trackerValid || m_trackerCurve)) {
        m_trackerValid = false;
        m_trackerCurve = nullptr;
        m_trackerIndex = -1;
        update();
    }
}

bool AgPlot::crosshairEnabled() const
{
    return m_crosshairEnabled;
}

void AgPlot::setCrosshairEnabled(bool on)
{
    if (m_crosshairEnabled == on)
        return;
    m_crosshairEnabled = on;
    if (!on && m_crosshairValid) {
        m_crosshairValid = false;
        update();
    }
}

void AgPlot::replot()
{
    update();
}

void AgPlot::panAxis(AgPlotAxis *axis, double dataDelta)
{
    if (dataDelta == 0.0)
        return;
    // 手动交互后该轴脱离自适应量程（limits 仍生效），否则会被 fitAutoRanges 弹回
    axis->m_autoRange = false;
    double mn = axis->m_min + dataDelta;
    double mx = axis->m_max + dataDelta;
    axis->clampToBounds(mn, mx);
    axis->setCurrentRange(mn, mx);
    update();
}

void AgPlot::zoomAxis(AgPlotAxis *axis, qreal factor, double anchor)
{
    if (qFuzzyCompare(factor, 1.0))
        return;
    axis->m_autoRange = false;
    double mn = anchor - (anchor - axis->m_min) * factor;
    double mx = anchor + (axis->m_max - anchor) * factor;
    axis->clampToBounds(mn, mx);
    axis->setCurrentRange(mn, mx);
    update();
}

void AgPlot::mousePressEvent(QMouseEvent *event)
{
    if (m_dragEnabled && event->button() == Qt::LeftButton
        && m_dragDirections != 0 && m_plotRect.contains(event->pos())) {
        m_dragging = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void AgPlot::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const QPoint pos = event->pos();
        const int dx = pos.x() - m_lastMousePos.x();
        const int dy = pos.y() - m_lastMousePos.y();
        m_lastMousePos = pos;

        if (m_dragDirections & Qt::Horizontal) {
            for (AgPlotAxis *ax : { m_xAxis, m_xAxis2 }) {
                if (!ax->isVisible())
                    continue;
                const double dataPerPixel = (ax->maximum() - ax->minimum()) / m_plotRect.width();
                panAxis(ax, -dx * dataPerPixel);
            }
        }
        if (m_dragDirections & Qt::Vertical) {
            for (AgPlotAxis *ay : { m_yAxis, m_yAxis2 }) {
                if (!ay->isVisible())
                    continue;
                const double dataPerPixel = (ay->maximum() - ay->minimum()) / m_plotRect.height();
                panAxis(ay, dy * dataPerPixel);
            }
        }
        // 拖动期间隐藏追踪器 / 十字线（位置已失效）
        if (m_trackerValid || m_crosshairValid) {
            m_trackerValid = false;
            m_crosshairValid = false;
        }
        event->accept();
        return;
    }

    updateTracker(event->pos());

    // 悬停光标提示可拖动
    if (m_dragEnabled && m_plotRect.contains(event->pos()))
        setCursor(Qt::OpenHandCursor);
    else
        unsetCursor();
    QWidget::mouseMoveEvent(event);
}

void AgPlot::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging) {
        m_dragging = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void AgPlot::leaveEvent(QEvent *event)
{
    if (!m_dragging)
        unsetCursor();
    if (m_trackerValid || m_crosshairValid) {
        m_trackerValid = false;
        m_crosshairValid = false;
        if (m_trackerCurve) {
            m_trackerCurve = nullptr;
            m_trackerIndex = -1;
            emit trackerChanged(nullptr, QPointF());
        }
        update();
    }
    QWidget::leaveEvent(event);
}

void AgPlot::wheelEvent(QWheelEvent *event)
{
    if (!m_zoomEnabled || !m_plotRect.contains(event->pos())) {
        event->ignore();
        return;
    }

    qreal steps = event->angleDelta().y() / 120.0;
    if (qFuzzyIsNull(steps) && (m_zoomDirections & Qt::Horizontal))
        steps = event->angleDelta().x() / 120.0;   // 横向滚轮兜底
    if (qFuzzyIsNull(steps)) {
        event->ignore();
        return;
    }

    // 向上滚放大（范围缩小），以光标位置为不动点
    const qreal factor = std::pow(1.0 / m_zoomFactor, steps);
    const QPoint pos = event->pos();

    if (m_zoomDirections & Qt::Horizontal) {
        for (AgPlotAxis *ax : { m_xAxis, m_xAxis2 }) {
            if (!ax->isVisible())
                continue;
            zoomAxis(ax, factor, pixelToX(pos.x(), ax == m_xAxis2 ? AgPlotAxisPos::X2
                                                                  : AgPlotAxisPos::X1));
        }
    }
    if (m_zoomDirections & Qt::Vertical) {
        for (AgPlotAxis *ay : { m_yAxis, m_yAxis2 }) {
            if (!ay->isVisible())
                continue;
            zoomAxis(ay, factor, pixelToY(pos.y(), ay == m_yAxis2 ? AgPlotAxisPos::Y2
                                                                  : AgPlotAxisPos::Y1));
        }
    }
    // 缩放后按新映射立即重新吸附追踪器 / 十字线
    updateTracker(pos);
    event->accept();
}

// ============================================================
// 布局与量程
// ============================================================

QSize AgPlot::sizeHint() const
{
    return QSize(420, 300);
}

QSize AgPlot::minimumSizeHint() const
{
    return QSize(160, 120);
}

void AgPlot::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void AgPlot::fitAutoRanges()
{
    struct AxisFit {
        bool has = false;
        double min = 0.0;
        double max = 0.0;
    };
    AxisFit fits[4];

    auto unite = [](AxisFit &fit, double value) {
        if (!fit.has) {
            fit.has = true;
            fit.min = value;
            fit.max = value;
        } else {
            fit.min = qMin(fit.min, value);
            fit.max = qMax(fit.max, value);
        }
    };

    for (AgPlotItem *item : m_items) {
        if (!item->isVisible())
            continue;
        const QRectF bounds = item->dataBounds();
        if (bounds.isNull() || !bounds.isValid())
            continue;
        unite(fits[int(item->xAxis())], bounds.left());
        unite(fits[int(item->xAxis())], bounds.right());
        unite(fits[int(item->yAxis())], bounds.top());
        unite(fits[int(item->yAxis())], bounds.bottom());
    }

    for (const AgPlotAxisPos pos : { AgPlotAxisPos::X1, AgPlotAxisPos::X2,
                                     AgPlotAxisPos::Y1, AgPlotAxisPos::Y2 }) {
        AgPlotAxis *ax = axisAt(pos);
        const AxisFit &fit = fits[int(pos)];
        if (!ax->m_autoRange)
            continue;

        if (!fit.has) {
            // 空轴跟随对侧轴：开启双轴但 X2 / Y2 还没有绑定数据时，
            // 镜像 X1 / Y1 的当前范围，避免显示无关的初始范围
            const int counterpart = (pos == AgPlotAxisPos::X2) ? int(AgPlotAxisPos::X1)
                                    : (pos == AgPlotAxisPos::Y2) ? int(AgPlotAxisPos::Y1)
                                    : (pos == AgPlotAxisPos::X1) ? int(AgPlotAxisPos::X2)
                                                                 : int(AgPlotAxisPos::Y1);
            if (fits[counterpart].has) {
                AgPlotAxis *other = axisAt(static_cast<AgPlotAxisPos>(counterpart));
                ax->setCurrentRange(other->minimum(), other->maximum());
            }
            continue;
        }

        double mn = fit.min;
        double mx = fit.max;
        if (closeEnough(mx, mn)) {
            mn -= 1.0;   // 单点数据扩展 +/-1
            mx += 1.0;
        } else {
            const double pad = (mx - mn) * ax->m_padding;
            mn -= pad;
            mx += pad;
        }
        if (ax->m_hasLimits) {
            mn = qMax(mn, ax->m_limitMin);
            mx = qMin(mx, ax->m_limitMax);
            if (mn > mx)
                std::swap(mn, mx);
        }
        ax->setCurrentRange(mn, mx);
    }
}

QRectF AgPlot::calculatePlotRect() const
{
    const AgPlotStyle *st = effectiveStyle();
    const QFontMetrics fm(st->tickLabelFont());

    // 边距需要在知道绘图区像素长度前确定：用估算长度生成刻度取最大刻度值宽度
    const int estimateYLength = qMax(50, height() - 60);

    auto yAxisMargin = [&](const AgPlotAxis *ax) -> int {
        if (!ax->isVisible())
            return 4;
        int margin = st->tickLength() + 4;
        if (ax->tickLabelsVisible()) {
            int maxWidth = 0;
            const QVector<double> ticks = ax->tickValues(estimateYLength);
            for (double value : ticks)
                maxWidth = qMax(maxWidth, fm.horizontalAdvance(ax->formatTickLabel(value)));
            margin += maxWidth + 4;
        }
        return margin;
    };
    auto xAxisMargin = [&](const AgPlotAxis *ax) -> int {
        if (!ax->isVisible())
            return 4;
        int margin = st->tickLength() + 4;
        if (ax->tickLabelsVisible())
            margin += fm.height() + 3;
        return margin;
    };

    const int left = yAxisMargin(m_yAxis);
    const int right = yAxisMargin(m_yAxis2);
    const int top = xAxisMargin(m_xAxis2);
    const int bottom = xAxisMargin(m_xAxis);

    return QRectF(left, top,
                  qMax<qreal>(10.0, width() - left - right),
                  qMax<qreal>(10.0, height() - top - bottom));
}

// ============================================================
// 绘制
// ============================================================

void AgPlot::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    AgPlotStyle *st = effectiveStyle();
    if (st->backgroundColor().isValid())
        painter.fillRect(rect(), st->backgroundColor());

    fitAutoRanges();
    m_plotRect = calculatePlotRect();
    const QRectF rect = m_plotRect;

    if (st->plotAreaBackground().isValid())
        painter.fillRect(rect, st->plotAreaBackground());

    drawGrid(&painter, rect, st);

    painter.save();
    painter.setClipRect(rect);
    for (AgPlotItem *item : m_items) {
        if (item->isVisible())
            item->paint(&painter, this);
    }
    painter.restore();

    drawAxes(&painter, rect, st);

    // 覆盖层：十字线 / 追踪器（最上层，不参与 zLevel 排序）
    drawCrosshair(&painter);
    drawTracker(&painter);
}

void AgPlot::drawGrid(QPainter *painter, const QRectF &rect, const AgPlotStyle *st)
{
    struct SideSpec {
        AgPlotAxisPos pos;
        bool vertical;   // X 轴的网格为纵向线
    };
    static const SideSpec sides[4] = {
        { AgPlotAxisPos::X1, true },  { AgPlotAxisPos::X2, true },
        { AgPlotAxisPos::Y1, false }, { AgPlotAxisPos::Y2, false },
    };

    for (const SideSpec &side : sides) {
        AgPlotAxis *ax = axisAt(side.pos);
        if (!ax->isVisible() || !ax->gridVisible())
            continue;

        QColor color = ax->gridColor().isValid() ? ax->gridColor() : st->gridColor();
        if (!color.isValid())
            continue;
        const qreal gridWidth = ax->gridWidth() > 0 ? ax->gridWidth() : st->gridWidth();
        QPen pen(color, gridWidth, ax->gridStyle());
        pen.setCapStyle(Qt::FlatCap);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);

        if (side.vertical) {
            for (double value : ax->tickValues(rect.width())) {
                const qreal x = xToPixel(value, side.pos);
                painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
            }
            if (ax->minorGridVisible()) {
                for (double value : ax->minorTickValues(rect.width())) {
                    const qreal x = xToPixel(value, side.pos);
                    painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
                }
            }
        } else {
            for (double value : ax->tickValues(rect.height())) {
                const qreal y = yToPixel(value, side.pos);
                painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
            }
            if (ax->minorGridVisible()) {
                for (double value : ax->minorTickValues(rect.height())) {
                    const qreal y = yToPixel(value, side.pos);
                    painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
                }
            }
        }
    }
}

void AgPlot::drawCrosshair(QPainter *painter)
{
    if (!m_crosshairEnabled || !m_crosshairValid)
        return;
    AgPlotStyle *st = effectiveStyle();
    QPen pen(st->crosshairColor(), 1, st->crosshairStyle());
    pen.setCapStyle(Qt::FlatCap);
    painter->save();
    painter->setClipRect(m_plotRect);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(m_crosshairPos.x(), m_plotRect.top()),
                      QPointF(m_crosshairPos.x(), m_plotRect.bottom()));
    painter->drawLine(QPointF(m_plotRect.left(), m_crosshairPos.y()),
                      QPointF(m_plotRect.right(), m_crosshairPos.y()));
    painter->restore();
}

void AgPlot::drawTracker(QPainter *painter)
{
    if (!m_trackerEnabled || !m_trackerValid || !m_trackerCurve || m_trackerIndex < 0)
        return;
    AgPlotStyle *st = effectiveStyle();
    const AgPlotCurve *curve = m_trackerCurve;
    const QPointF dataPoint = curve->pointAt(m_trackerIndex);
    const qreal px = xToPixel(dataPoint.x(), curve->xAxis());
    const qreal py = yToPixel(dataPoint.y(), curve->yAxis());

    // tooltip 值文本：优先用轴的自定义格式化（星期等）；
    // 默认不用刻度格式（刻度步长的小数位会抹掉数据精度），改用 6 位有效数字
    auto valueText = [](const AgPlotAxis *axis, double value) {
        if (axis->hasLabelFormatter())
            return axis->formatTickLabel(value);
        if (axis->labelPrecision() >= 0)
            return QString::number(value, 'f', axis->labelPrecision());
        return QString::number(value, 'g', 6);
    };

    // 引导线（跟随吸附点，而非原始光标）+ 空心圆标记，裁剪到绘图区
    painter->save();
    painter->setClipRect(m_plotRect);
    QPen guide(st->crosshairColor(), 1, st->crosshairStyle());
    guide.setCapStyle(Qt::FlatCap);
    painter->setPen(guide);
    painter->setBrush(Qt::NoBrush);
    painter->drawLine(QPointF(px, m_plotRect.top()), QPointF(px, m_plotRect.bottom()));
    painter->drawLine(QPointF(m_plotRect.left(), py), QPointF(m_plotRect.right(), py));
    const QColor markerColor = curve->lineColor().isValid() ? curve->lineColor()
                                                            : QColor(0x2f, 0x6f, 0xdb);
    AgPlotInternal::drawPointMarker(painter, QPointF(px, py),
                                    AgPlotPointStyle::HollowCircle, 9, markerColor);
    painter->restore();

    // tooltip：靠近边缘自动翻转
    const AgPlotAxis *ax = axisAt(curve->xAxis());
    const AgPlotAxis *ay = axisAt(curve->yAxis());
    const QString text = QStringLiteral("x: %1\ny: %2")
                             .arg(valueText(ax, dataPoint.x()),
                                  valueText(ay, dataPoint.y()));
    const QFontMetrics fm(st->trackerFont());
    QRectF box = fm.boundingRect(QRect(0, 0, 0, 0), Qt::TextExpandTabs, text)
                     .adjusted(-6, -4, 6, 4);
    if (box.width() < 40)
        box.setWidth(40);
    QPointF topLeft(px + 14, py - 14 - box.height());
    if (topLeft.x() + box.width() > width() - 2)
        topLeft.setX(px - 14 - box.width());
    if (topLeft.y() < 1)
        topLeft.setY(py + 14);
    box.moveTopLeft(topLeft);

    painter->setPen(st->trackerBorderColor());
    painter->setBrush(st->trackerBackgroundColor());
    painter->drawRoundedRect(box, 4, 4);
    painter->setFont(st->trackerFont());
    painter->setPen(st->trackerTextColor());
    painter->drawText(box.adjusted(6, 4, -6, -4), Qt::AlignLeft | Qt::AlignVCenter, text);
}

void AgPlot::updateTracker(const QPoint &pos)
{
    bool needUpdate = false;
    const bool inPlot = m_plotRect.contains(pos);

    // 十字线跟随原始光标
    const bool crossValid = m_crosshairEnabled && inPlot;
    if (crossValid != m_crosshairValid || (crossValid && m_crosshairPos != pos)) {
        m_crosshairValid = crossValid;
        m_crosshairPos = pos;
        needUpdate = true;
    }

    // 追踪器：吸附 x 就近的曲线点，多条曲线按像素 y 距离择优
    AgPlotCurve *bestCurve = nullptr;
    int bestIndex = -1;
    qreal bestDistY = 0.0;
    if (m_trackerEnabled && inPlot) {
        for (AgPlotItem *item : m_items) {
            if (!item->isVisible())
                continue;
            AgPlotCurve *curve = dynamic_cast<AgPlotCurve *>(item);
            if (!curve || curve->pointCount() == 0)
                continue;
            const double cursorX = pixelToX(pos.x(), curve->xAxis());
            const int index = curve->nearestIndexForX(cursorX);
            if (index < 0)
                continue;
            const qreal py = yToPixel(curve->pointAt(index).y(), curve->yAxis());
            const qreal dist = qAbs(py - pos.y());
            if (!bestCurve || dist < bestDistY) {
                bestCurve = curve;
                bestIndex = index;
                bestDistY = dist;
            }
        }
    }

    if (bestCurve != m_trackerCurve || bestIndex != m_trackerIndex) {
        m_trackerCurve = bestCurve;
        m_trackerIndex = bestIndex;
        m_trackerValid = (bestCurve != nullptr);
        emit trackerChanged(bestCurve, bestCurve ? bestCurve->pointAt(bestIndex) : QPointF());
        needUpdate = true;
    }
    if (needUpdate)
        update();
}

void AgPlot::drawAxes(QPainter *painter, const QRectF &rect, const AgPlotStyle *st)
{
    struct SideSpec {
        AgPlotAxisPos pos;
        bool horizontal;
        bool topOrRight;
    };
    static const SideSpec sides[4] = {
        { AgPlotAxisPos::X1, true,  false },
        { AgPlotAxisPos::X2, true,  true  },
        { AgPlotAxisPos::Y1, false, false },
        { AgPlotAxisPos::Y2, false, true  },
    };

    painter->save();
    painter->setFont(st->tickLabelFont());
    const QFontMetrics fm(st->tickLabelFont());

    for (const SideSpec &side : sides) {
        AgPlotAxis *ax = axisAt(side.pos);
        if (!ax->isVisible())
            continue;

        const QColor color = ax->axisColor().isValid() ? ax->axisColor() : st->axisColor();
        const QPen axisPen(color, 1);
        painter->setPen(axisPen);
        painter->setBrush(Qt::NoBrush);

        const bool showLabels = ax->tickLabelsVisible();
        const int tickLen = st->tickLength();
        const int minorLen = st->minorTicksVisible() ? st->minorTickLength() : 0;

        if (side.horizontal) {
            // X 轴：轴线水平，刻度朝上 / 下延伸
            const qreal axisY = side.topOrRight ? rect.top() : rect.bottom();
            const qreal dir = side.topOrRight ? -1.0 : 1.0;

            painter->drawLine(QPointF(rect.left(), axisY), QPointF(rect.right(), axisY));

            for (double value : ax->minorTickValues(rect.width())) {
                const qreal x = xToPixel(value, side.pos);
                painter->drawLine(QPointF(x, axisY), QPointF(x, axisY + dir * minorLen));
            }
            for (double value : ax->tickValues(rect.width())) {
                const qreal x = xToPixel(value, side.pos);
                painter->drawLine(QPointF(x, axisY), QPointF(x, axisY + dir * tickLen));
                if (showLabels) {
                    const QString text = ax->formatTickLabel(value);
                    const int textWidth = fm.horizontalAdvance(text);
                    const qreal textTop = axisY + dir * (tickLen + 3)
                                          - (side.topOrRight ? fm.height() : 0.0);
                    const QRectF textRect(x - textWidth / 2.0 - 1.0, textTop,
                                          textWidth + 2.0, fm.height());
                    painter->setPen(st->tickLabelColor());
                    painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, text);
                    painter->setPen(axisPen);
                }
            }
        } else {
            // Y 轴：轴线垂直，刻度朝左 / 右延伸
            const qreal axisX = side.topOrRight ? rect.right() : rect.left();
            const qreal dir = side.topOrRight ? 1.0 : -1.0;

            painter->drawLine(QPointF(axisX, rect.top()), QPointF(axisX, rect.bottom()));

            for (double value : ax->minorTickValues(rect.height())) {
                const qreal y = yToPixel(value, side.pos);
                painter->drawLine(QPointF(axisX, y), QPointF(axisX + dir * minorLen, y));
            }

            int maxLabelWidth = 0;
            if (showLabels) {
                for (double value : ax->tickValues(rect.height()))
                    maxLabelWidth = qMax(maxLabelWidth, fm.horizontalAdvance(ax->formatTickLabel(value)));
            }
            for (double value : ax->tickValues(rect.height())) {
                const qreal y = yToPixel(value, side.pos);
                painter->drawLine(QPointF(axisX, y), QPointF(axisX + dir * tickLen, y));
                if (showLabels) {
                    const QString text = ax->formatTickLabel(value);
                    const QRectF textRect = side.topOrRight
                        ? QRectF(axisX + dir * tickLen + 3.0, y - fm.height() / 2.0,
                                 maxLabelWidth + 2.0, fm.height())
                        : QRectF(axisX + dir * tickLen - 3.0 - maxLabelWidth - 2.0,
                                 y - fm.height() / 2.0, maxLabelWidth + 2.0, fm.height());
                    painter->setPen(st->tickLabelColor());
                    painter->drawText(textRect,
                                      side.topOrRight ? (Qt::AlignLeft | Qt::AlignVCenter)
                                                      : (Qt::AlignRight | Qt::AlignVCenter),
                                      text);
                    painter->setPen(axisPen);
                }
            }
        }
    }
    painter->restore();
}
