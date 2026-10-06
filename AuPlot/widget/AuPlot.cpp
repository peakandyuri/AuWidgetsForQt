#include "AuPlot.h"

#include "AuPlotCurve.h"
#include "AuPlotInternal.h"
#include "AuPlotPrimitive.h"

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

AuPlot::AuPlot(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    // 背景由 paintEvent 手动填充：无效颜色时保持透明（透出父部件）
    setAttribute(Qt::WA_NoSystemBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_xAxis = new AuPlotAxis(this);
    m_yAxis = new AuPlotAxis(this);
    m_xAxis2 = new AuPlotAxis(this);
    m_yAxis2 = new AuPlotAxis(this);

    // 默认单轴：X2 / Y2 隐藏；次轴网格默认关闭（避免与主轴网格重复）
    m_xAxis2->setVisible(false);
    m_yAxis2->setVisible(false);
    m_xAxis2->setGridVisible(false);
    m_yAxis2->setGridVisible(false);

    m_defaultStyle = new AuPlotStyle;
}

AuPlot::~AuPlot()
{
    qDeleteAll(m_items);
    delete m_customStyle;
    delete m_defaultStyle;
}

// ============================================================
// 坐标轴
// ============================================================

AuPlotAxis *AuPlot::xAxis() const
{
    return m_xAxis;
}

AuPlotAxis *AuPlot::yAxis() const
{
    return m_yAxis;
}

AuPlotAxis *AuPlot::xAxis2() const
{
    return m_xAxis2;
}

AuPlotAxis *AuPlot::yAxis2() const
{
    return m_yAxis2;
}

bool AuPlot::dualAxisEnabled() const
{
    return m_xAxis2->isVisible() && m_yAxis2->isVisible();
}

void AuPlot::setDualAxisEnabled(bool on)
{
    m_xAxis2->setVisible(on);
    m_yAxis2->setVisible(on);
}

AuPlotAxis *AuPlot::axisAt(AuPlotAxisPos pos) const
{
    switch (pos) {
    case AuPlotAxisPos::X2: return m_xAxis2;
    case AuPlotAxisPos::Y1: return m_yAxis;
    case AuPlotAxisPos::Y2: return m_yAxis2;
    case AuPlotAxisPos::X1: break;
    }
    return m_xAxis;
}

// ============================================================
// 图元管理
// ============================================================

void AuPlot::addItem(AuPlotItem *item)
{
    if (!item)
        return;
    if (item->attachedPlot()) {
        qWarning("AuPlot::addItem: 图元已属于某个 AuPlot，先从原控件移除再加入");
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

void AuPlot::removeItem(AuPlotItem *item)
{
    if (!item)
        return;
    const int index = m_items.indexOf(item);
    if (index < 0)
        return;
    m_items.removeAt(index);
    item->m_plot = nullptr;
    for (int i = m_trackerHits.size() - 1; i >= 0; --i) {   // 防止追踪器持有悬空指针
        if (m_trackerHits.at(i).first == item)
            m_trackerHits.removeAt(i);
    }
    if (m_trackerHits.isEmpty())
        m_trackerValid = false;
    delete item;
    update();
}

void AuPlot::clearItems()
{
    if (m_items.isEmpty())
        return;
    for (AuPlotItem *item : m_items)
        item->m_plot = nullptr;
    qDeleteAll(m_items);
    m_items.clear();
    m_trackerHits.clear();
    m_trackerValid = false;
    update();
}

QList<AuPlotItem *> AuPlot::items() const
{
    return m_items;
}

void AuPlot::reinsertItem(AuPlotItem *item)
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

void AuPlot::assignDefaultColor(AuPlotItem *item)
{
    AuPlotStyle *st = effectiveStyle();
    if (AuPlotCurve *curve = dynamic_cast<AuPlotCurve *>(item)) {
        if (!curve->m_lineColor.isValid()) {
            const QColor color = st->nextCurveColor();
            curve->m_lineColor = color;
            if (curve->m_pointStyle != AuPlotPointStyle::None && !curve->m_pointColor.isValid())
                curve->m_pointColor = color;
        }
    } else if (AuPlotPoints *points = dynamic_cast<AuPlotPoints *>(item)) {
        if (!points->m_pointColor.isValid())
            points->m_pointColor = st->nextCurveColor();
    } else if (AuPlotLine *line = dynamic_cast<AuPlotLine *>(item)) {
        if (!line->m_lineColor.isValid())
            line->m_lineColor = st->nextCurveColor();
    } else if (AuPlotArea *area = dynamic_cast<AuPlotArea *>(item)) {
        if (!area->m_borderColor.isValid())
            area->m_borderColor = st->nextCurveColor();
    } else if (AuPlotRefLine *refLine = dynamic_cast<AuPlotRefLine *>(item)) {
        if (!refLine->m_lineColor.isValid())
            refLine->m_lineColor = st->nextCurveColor();
    }
}

// ============================================================
// 坐标映射
// ============================================================

QRectF AuPlot::plotRect() const
{
    return m_plotRect.isNull() ? calculatePlotRect() : m_plotRect;
}

qreal AuPlot::xToPixel(double x, AuPlotAxisPos axis) const
{
    const AuPlotAxis *ax = axisAt(axis);
    const QRectF rect = plotRect();
    const double span = ax->maximum() - ax->minimum();
    if (qFuzzyIsNull(span))
        return rect.left() + rect.width() / 2.0;
    return rect.left() + (x - ax->minimum()) / span * rect.width();
}

qreal AuPlot::yToPixel(double y, AuPlotAxisPos axis) const
{
    const AuPlotAxis *ax = axisAt(axis);
    const QRectF rect = plotRect();
    const double span = ax->maximum() - ax->minimum();
    if (qFuzzyIsNull(span))
        return rect.top() + rect.height() / 2.0;
    return rect.bottom() - (y - ax->minimum()) / span * rect.height();
}

double AuPlot::pixelToX(qreal pixelX, AuPlotAxisPos axis) const
{
    const AuPlotAxis *ax = axisAt(axis);
    const QRectF rect = plotRect();
    if (qFuzzyIsNull(rect.width()))
        return ax->minimum();
    return ax->minimum() + (pixelX - rect.left()) / rect.width() * (ax->maximum() - ax->minimum());
}

double AuPlot::pixelToY(qreal pixelY, AuPlotAxisPos axis) const
{
    const AuPlotAxis *ax = axisAt(axis);
    const QRectF rect = plotRect();
    if (qFuzzyIsNull(rect.height()))
        return ax->minimum();
    return ax->minimum() + (rect.bottom() - pixelY) / rect.height() * (ax->maximum() - ax->minimum());
}

// ============================================================
// 样式
// ============================================================

AuPlotStyle *AuPlot::style() const
{
    return effectiveStyle();
}

void AuPlot::setStyle(AuPlotStyle *style)
{
    if (style == m_customStyle)
        return;
    delete m_customStyle;
    m_customStyle = style;
    update();
}

AuPlotStyle *AuPlot::effectiveStyle() const
{
    if (m_customStyle)
        return m_customStyle;
    if (AuPlotStyle::globalStyle())
        return AuPlotStyle::globalStyle();
    return m_defaultStyle;
}

// ============================================================
// 交互（默认关闭）
// ============================================================

bool AuPlot::dragEnabled() const
{
    return m_dragEnabled;
}

void AuPlot::setDragEnabled(bool on)
{
    m_dragEnabled = on;
    if (!on)
        unsetCursor();
}

Qt::Orientations AuPlot::dragDirections() const
{
    return m_dragDirections;
}

void AuPlot::setDragDirections(Qt::Orientations directions)
{
    m_dragDirections = directions;
}

bool AuPlot::zoomEnabled() const
{
    return m_zoomEnabled;
}

void AuPlot::setZoomEnabled(bool on)
{
    m_zoomEnabled = on;
}

Qt::Orientations AuPlot::zoomDirections() const
{
    return m_zoomDirections;
}

void AuPlot::setZoomDirections(Qt::Orientations directions)
{
    m_zoomDirections = directions;
}

qreal AuPlot::zoomFactor() const
{
    return m_zoomFactor;
}

void AuPlot::setZoomFactor(qreal factor)
{
    m_zoomFactor = qMax<qreal>(1.01, factor);
}

bool AuPlot::trackerEnabled() const
{
    return m_trackerEnabled;
}

void AuPlot::setTrackerEnabled(bool on)
{
    if (m_trackerEnabled == on)
        return;
    m_trackerEnabled = on;
    if (!on && (m_trackerValid || !m_trackerHits.isEmpty())) {
        m_trackerValid = false;
        m_trackerHits.clear();
        update();
    }
}

bool AuPlot::trackerSnapOutOfRange() const
{
    return m_trackerSnapOutOfRange;
}

void AuPlot::setTrackerSnapOutOfRange(bool on)
{
    if (m_trackerSnapOutOfRange == on)
        return;
    m_trackerSnapOutOfRange = on;
    if (m_trackerValid)
        updateTracker(m_trackerPos);   // 按新规则立即重算当前吸附
    else
        update();
}

bool AuPlot::crosshairEnabled() const
{
    return m_crosshairEnabled;
}

void AuPlot::setCrosshairEnabled(bool on)
{
    if (m_crosshairEnabled == on)
        return;
    m_crosshairEnabled = on;
    if (!on && m_crosshairValid) {
        m_crosshairValid = false;
        update();
    }
}

Qt::Orientations AuPlot::crosshairDirections() const
{
    return m_crosshairDirections;
}

void AuPlot::setCrosshairDirections(Qt::Orientations directions)
{
    if (m_crosshairDirections == directions)
        return;
    m_crosshairDirections = directions;
    if (m_crosshairValid)
        update();
}

void AuPlot::replot()
{
    update();
}

void AuPlot::panAxis(AuPlotAxis *axis, double dataDelta)
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

void AuPlot::zoomAxis(AuPlotAxis *axis, qreal factor, double anchor)
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

void AuPlot::mousePressEvent(QMouseEvent *event)
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

void AuPlot::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const QPoint pos = event->pos();
        const int dx = pos.x() - m_lastMousePos.x();
        const int dy = pos.y() - m_lastMousePos.y();
        m_lastMousePos = pos;

        if (m_dragDirections & Qt::Horizontal) {
            for (AuPlotAxis *ax : { m_xAxis, m_xAxis2 }) {
                if (!ax->isVisible())
                    continue;
                const double dataPerPixel = (ax->maximum() - ax->minimum()) / m_plotRect.width();
                panAxis(ax, -dx * dataPerPixel);
            }
        }
        if (m_dragDirections & Qt::Vertical) {
            for (AuPlotAxis *ay : { m_yAxis, m_yAxis2 }) {
                if (!ay->isVisible())
                    continue;
                const double dataPerPixel = (ay->maximum() - ay->minimum()) / m_plotRect.height();
                panAxis(ay, dy * dataPerPixel);
            }
        }
        // 拖动期间隐藏追踪器 / 十字线（位置已失效）
        if (m_trackerValid || m_crosshairValid || !m_trackerHits.isEmpty()) {
            m_trackerValid = false;
            m_crosshairValid = false;
            m_trackerHits.clear();
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

void AuPlot::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging) {
        m_dragging = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void AuPlot::leaveEvent(QEvent *event)
{
    if (!m_dragging)
        unsetCursor();
    if (m_trackerValid || m_crosshairValid || !m_trackerHits.isEmpty()) {
        m_trackerValid = false;
        m_crosshairValid = false;
        if (!m_trackerHits.isEmpty()) {
            m_trackerHits.clear();
            emit trackerChanged(nullptr, QPointF());
        }
        update();
    }
    QWidget::leaveEvent(event);
}

void AuPlot::wheelEvent(QWheelEvent *event)
{
    // Qt6 移除了 QWheelEvent::pos()/posF()，统一取 position()（Qt5 侧为 posF()）
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPointF wheelPos = event->position();
#else
    const QPointF wheelPos = event->posF();
#endif
    if (!m_zoomEnabled || !m_plotRect.contains(wheelPos.toPoint())) {
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
    const QPoint pos = wheelPos.toPoint();

    if (m_zoomDirections & Qt::Horizontal) {
        for (AuPlotAxis *ax : { m_xAxis, m_xAxis2 }) {
            if (!ax->isVisible())
                continue;
            zoomAxis(ax, factor, pixelToX(pos.x(), ax == m_xAxis2 ? AuPlotAxisPos::X2
                                                                  : AuPlotAxisPos::X1));
        }
    }
    if (m_zoomDirections & Qt::Vertical) {
        for (AuPlotAxis *ay : { m_yAxis, m_yAxis2 }) {
            if (!ay->isVisible())
                continue;
            zoomAxis(ay, factor, pixelToY(pos.y(), ay == m_yAxis2 ? AuPlotAxisPos::Y2
                                                                  : AuPlotAxisPos::Y1));
        }
    }
    // 缩放后按新映射立即重新吸附追踪器 / 十字线
    updateTracker(pos);
    event->accept();
}

// ============================================================
// 布局与量程
// ============================================================

QSize AuPlot::sizeHint() const
{
    return QSize(420, 300);
}

QSize AuPlot::minimumSizeHint() const
{
    return QSize(160, 120);
}

void AuPlot::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    update();
}

void AuPlot::fitAutoRanges()
{
    // 全部轴都是固定量程时无需自适应：直接返回，避免每帧为数据边界做
    // 全量计算（实时追加场景的关键热路径）
    bool anyAutoRange = false;
    for (const AuPlotAxisPos pos : { AuPlotAxisPos::X1, AuPlotAxisPos::X2,
                                     AuPlotAxisPos::Y1, AuPlotAxisPos::Y2 }) {
        if (axisAt(pos)->m_autoRange) {
            anyAutoRange = true;
            break;
        }
    }
    if (!anyAutoRange)
        return;

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

    for (AuPlotItem *item : m_items) {
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

    for (const AuPlotAxisPos pos : { AuPlotAxisPos::X1, AuPlotAxisPos::X2,
                                     AuPlotAxisPos::Y1, AuPlotAxisPos::Y2 }) {
        AuPlotAxis *ax = axisAt(pos);
        const AxisFit &fit = fits[int(pos)];
        if (!ax->m_autoRange)
            continue;

        if (!fit.has) {
            // 空轴跟随对侧轴：开启双轴但 X2 / Y2 还没有绑定数据时，
            // 镜像 X1 / Y1 的当前范围，避免显示无关的初始范围
            const int counterpart = (pos == AuPlotAxisPos::X2) ? int(AuPlotAxisPos::X1)
                                    : (pos == AuPlotAxisPos::Y2) ? int(AuPlotAxisPos::Y1)
                                    : (pos == AuPlotAxisPos::X1) ? int(AuPlotAxisPos::X2)
                                                                 : int(AuPlotAxisPos::Y1);
            if (fits[counterpart].has) {
                AuPlotAxis *other = axisAt(static_cast<AuPlotAxisPos>(counterpart));
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

QRectF AuPlot::calculatePlotRect() const
{
    const AuPlotStyle *st = effectiveStyle();
    const QFontMetrics fm(st->tickLabelFont());

    // 边距需要在知道绘图区像素长度前确定：用估算长度生成刻度取最大刻度值宽度
    const int estimateYLength = qMax(50, height() - 60);

    auto yAxisMargin = [&](const AuPlotAxis *ax) -> int {
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
    auto xAxisMargin = [&](const AuPlotAxis *ax) -> int {
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

void AuPlot::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    AuPlotStyle *st = effectiveStyle();
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
    for (AuPlotItem *item : m_items) {
        if (item->isVisible())
            item->paint(&painter, this);
    }
    painter.restore();

    drawAxes(&painter, rect, st);

    // 覆盖层：十字线 / 追踪器（最上层，不参与 zLevel 排序）
    drawCrosshair(&painter);
    drawTracker(&painter);
}

void AuPlot::drawGrid(QPainter *painter, const QRectF &rect, const AuPlotStyle *st)
{
    struct SideSpec {
        AuPlotAxisPos pos;
        bool vertical;   // X 轴的网格为纵向线
    };
    static const SideSpec sides[4] = {
        { AuPlotAxisPos::X1, true },  { AuPlotAxisPos::X2, true },
        { AuPlotAxisPos::Y1, false }, { AuPlotAxisPos::Y2, false },
    };

    for (const SideSpec &side : sides) {
        AuPlotAxis *ax = axisAt(side.pos);
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

void AuPlot::drawCrosshair(QPainter *painter)
{
    if (!m_crosshairEnabled || !m_crosshairValid || m_crosshairDirections == 0)
        return;
    AuPlotStyle *st = effectiveStyle();
    QPen pen(st->crosshairColor(), 1, st->crosshairStyle());
    pen.setCapStyle(Qt::FlatCap);
    painter->save();
    painter->setClipRect(m_plotRect);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    // 十字线最多两条：纵线 + 横线，按方向配置绘制
    if (m_crosshairDirections & Qt::Vertical) {
        painter->drawLine(QPointF(m_crosshairPos.x(), m_plotRect.top()),
                          QPointF(m_crosshairPos.x(), m_plotRect.bottom()));
    }
    if (m_crosshairDirections & Qt::Horizontal) {
        painter->drawLine(QPointF(m_plotRect.left(), m_crosshairPos.y()),
                          QPointF(m_plotRect.right(), m_crosshairPos.y()));
    }
    painter->restore();
}

void AuPlot::drawTracker(QPainter *painter)
{
    if (!m_trackerEnabled || !m_trackerValid || m_trackerHits.isEmpty())
        return;
    AuPlotStyle *st = effectiveStyle();

    // tooltip 值文本：优先用轴的自定义格式化（星期等）；
    // 默认不用刻度格式（刻度步长的小数位会抹掉数据精度），改用 6 位有效数字
    auto valueText = [](const AuPlotAxis *axis, double value) {
        if (axis->hasLabelFormatter())
            return axis->formatTickLabel(value);
        if (axis->labelPrecision() >= 0)
            return QString::number(value, 'f', axis->labelPrecision());
        return QString::number(value, 'g', 6);
    };

    // 每条曲线一个吸附标记（曲线色空心圆），并收集 tooltip 的数值行
    struct TipLine {
        QColor color;
        QString text;
    };
    QVector<TipLine> lines;

    painter->save();
    painter->setClipRect(m_plotRect);
    for (const auto &hit : m_trackerHits) {
        AuPlotCurve *curve = hit.first;
        if (!curve || hit.second < 0 || hit.second >= curve->pointCount())
            continue;   // 索引已失效（数据被清空且尚未重吸附）
        const QPointF dataPoint = curve->pointAt(hit.second);
        const qreal px = xToPixel(dataPoint.x(), curve->xAxis());
        const qreal py = yToPixel(dataPoint.y(), curve->yAxis());
        const QColor markerColor = curve->lineColor().isValid() ? curve->lineColor()
                                                                : QColor(0x2f, 0x6f, 0xdb);
        AuPlotInternal::drawPointMarker(painter, QPointF(px, py),
                                        AuPlotPointStyle::HollowCircle, 9, markerColor);
        const AuPlotAxis *ax = axisAt(curve->xAxis());
        const AuPlotAxis *ay = axisAt(curve->yAxis());
        lines.append({ markerColor, QStringLiteral("x: %1  y: %2")
                                       .arg(valueText(ax, dataPoint.x()),
                                            valueText(ay, dataPoint.y())) });
    }
    painter->restore();

    if (lines.isEmpty())
        return;

    // 汇总 tooltip：一个框显示所有曲线的数值，锚定鼠标位置；
    // 右侧放不下翻到左侧，上方放不下翻到下方，仍越界则钳制在控件内
    const QFontMetrics fm(st->trackerFont());
    qreal textWidth = 0.0;
    for (const TipLine &line : lines)
        textWidth = qMax(textWidth, qreal(fm.horizontalAdvance(line.text)));
    const qreal dot = qMax<qreal>(6.0, fm.height() / 2.0);   // 行首色点直径
    const qreal rowHeight = fm.height();
    const qreal rowGap = 2.0;
    const qreal padX = 8.0;
    const qreal padY = 5.0;
    QRectF box(0, 0,
               padX * 2.0 + dot + 6.0 + textWidth,
               padY * 2.0 + lines.size() * rowHeight + (lines.size() - 1) * rowGap);

    const QPoint pos = m_trackerPos;
    QPointF topLeft(pos.x() + 16, pos.y() - 16 - box.height());
    if (topLeft.x() + box.width() > width() - 2)
        topLeft.setX(pos.x() - 16 - box.width());
    if (topLeft.y() < 1)
        topLeft.setY(pos.y() + 16);
    topLeft.setX(qBound<qreal>(1.0, topLeft.x(), qMax<qreal>(1.0, width() - box.width() - 1.0)));
    topLeft.setY(qBound<qreal>(1.0, topLeft.y(), qMax<qreal>(1.0, height() - box.height() - 1.0)));
    box.moveTopLeft(topLeft);

    painter->setPen(st->trackerBorderColor());
    painter->setBrush(st->trackerBackgroundColor());
    painter->drawRoundedRect(box, 4, 4);
    painter->setFont(st->trackerFont());

    qreal rowTop = box.top() + padY;
    for (const TipLine &line : lines) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(line.color);
        painter->drawEllipse(QPointF(box.left() + padX + dot / 2.0, rowTop + rowHeight / 2.0),
                             dot / 2.0, dot / 2.0);
        painter->setPen(st->trackerTextColor());
        painter->drawText(QPointF(box.left() + padX + dot + 6.0, rowTop + fm.ascent()),
                          line.text);
        rowTop += rowHeight + rowGap;
    }
}

void AuPlot::updateTracker(const QPoint &pos)
{
    bool needUpdate = false;
    const bool inPlot = m_plotRect.contains(pos);

    // 十字线跟随原始光标（横竖各最多一条，方向可配置）
    const bool crossValid = m_crosshairEnabled && m_crosshairDirections != 0 && inPlot;
    if (crossValid != m_crosshairValid || (crossValid && m_crosshairPos != pos)) {
        m_crosshairValid = crossValid;
        m_crosshairPos = pos;
        needUpdate = true;
    }

    // 追踪器：以鼠标 x 为基准，每条可见曲线各自吸附最近的数据点；
    // 追踪线离开曲线数据范围后该曲线不再吸附（标记与 tip 行隐藏）
    QVector<QPair<AuPlotCurve *, int>> hits;
    if (m_trackerEnabled && inPlot) {
        for (AuPlotItem *item : m_items) {
            if (!item->isVisible())
                continue;
            AuPlotCurve *curve = dynamic_cast<AuPlotCurve *>(item);
            if (!curve || curve->pointCount() == 0)
                continue;
            const double cursorX = pixelToX(pos.x(), curve->xAxis());
            if (!m_trackerSnapOutOfRange) {
                const QRectF bounds = curve->dataBounds();
                if (bounds.isValid() && !bounds.isNull()
                    && (cursorX < bounds.left() || cursorX > bounds.right()))
                    continue;
            }
            const int index = curve->nearestIndexForX(cursorX);
            if (index >= 0)
                hits.append(qMakePair(curve, index));
        }
    }

    if (hits != m_trackerHits) {
        if (hits.isEmpty() && !m_trackerHits.isEmpty()) {
            emit trackerChanged(nullptr, QPointF());   // 全部曲线离开吸附
        } else {
            for (const auto &hit : hits) {
                if (!m_trackerHits.contains(hit))
                    emit trackerChanged(hit.first, hit.first->pointAt(hit.second));
            }
        }
        m_trackerHits = hits;
        m_trackerValid = !hits.isEmpty();
        m_trackerPos = pos;
        needUpdate = true;
    } else if (m_trackerValid && m_trackerPos != pos) {
        // 吸附点未变但鼠标在动：tooltip 跟随鼠标，需要重绘
        m_trackerPos = pos;
        needUpdate = true;
    }
    if (needUpdate)
        update();
}

void AuPlot::drawAxes(QPainter *painter, const QRectF &rect, const AuPlotStyle *st)
{
    struct SideSpec {
        AuPlotAxisPos pos;
        bool horizontal;
        bool topOrRight;
    };
    static const SideSpec sides[4] = {
        { AuPlotAxisPos::X1, true,  false },
        { AuPlotAxisPos::X2, true,  true  },
        { AuPlotAxisPos::Y1, false, false },
        { AuPlotAxisPos::Y2, false, true  },
    };

    painter->save();
    painter->setFont(st->tickLabelFont());
    const QFontMetrics fm(st->tickLabelFont());

    for (const SideSpec &side : sides) {
        AuPlotAxis *ax = axisAt(side.pos);
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
