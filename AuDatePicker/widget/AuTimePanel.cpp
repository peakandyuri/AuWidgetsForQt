#include "AuTimePanel.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QVariantAnimation>
#include <QWheelEvent>

namespace {

/// 独立使用（未被外部指定高度）时可见条目行数
constexpr int kVisibleRows = 10;
/// 滚轮每格滚动的条目数
constexpr int kWheelStepItems = 3;
/// 列两侧留白（与日历面板右栏内边距一致）
constexpr int kColumnsPadding = 8;

} // namespace

AuTimePanel::AuTimePanel(QWidget *parent)
    : QWidget(parent)
    , m_style(AuDatePickerStyle::globalStyle())
    , m_granularity(HoursMinutesSeconds)
    , m_hoverColumn(-1)
    , m_hoverValue(-1)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus); // 键盘交互由宿主控件承载
    setAttribute(Qt::WA_StyledBackground, false);

    for (int i = 0; i < 3; ++i) {
        m_offsets[i] = 0.0;
        QVariantAnimation *anim = new QVariantAnimation(this);
        anim->setDuration(150);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QVariantAnimation::valueChanged, this, [this, i](const QVariant &value) {
            m_offsets[i] = value.toReal();
            update();
        });
        m_scrollAnims[i] = anim;
    }

    AuDatePickerStyle::registerPanelObserver(this); // 独立内嵌时跟随全局样式更换
}

AuTimePanel::~AuTimePanel()
{
    AuDatePickerStyle::unregisterPanelObserver(this);
}

// ---------------------------------------------------------------------------
// 配置

void AuTimePanel::setStyle(AuDatePickerStyle *style)
{
    if (!style || m_style == style)
        return;
    m_style = style;
    updateGeometry();
    update();
}

AuDatePickerStyle *AuTimePanel::style() const
{
    return m_style;
}

void AuTimePanel::setGranularity(Granularity granularity)
{
    if (m_granularity == granularity)
        return;
    m_granularity = granularity;
    for (int i = 0; i < 3; ++i) {
        m_scrollAnims[i]->stop();
        m_offsets[i] = 0.0;
    }
    clampOffsets();
    updateGeometry();
    update();
}

AuTimePanel::Granularity AuTimePanel::granularity() const
{
    return m_granularity;
}

void AuTimePanel::setSelectedTime(const QTime &time)
{
    if (m_selected == time)
        return;
    m_selected = time;
    // 定位：选中条目滚到列中部（无动画，直接就位）
    for (int c = 0; c < columnCount(); ++c) {
        const int value = selectedValue(c);
        if (value >= 0)
            scrollToItem(c, value, false);
    }
    update();
}

QTime AuTimePanel::selectedTime() const
{
    return m_selected;
}

void AuTimePanel::setTimeRange(const QTime &min, const QTime &max)
{
    QTime lo = min.isValid() ? min : QTime();
    QTime hi = max.isValid() ? max : QTime();
    if (lo.isValid() && hi.isValid() && lo > hi)
        qSwap(lo, hi);
    if (lo == m_min && hi == m_max)
        return;
    m_min = lo;
    m_max = hi;
    update();
}

QTime AuTimePanel::minimumTime() const
{
    return m_min;
}

QTime AuTimePanel::maximumTime() const
{
    return m_max;
}

bool AuTimePanel::isTimeValid(const QTime &time) const
{
    if (!time.isValid())
        return false;
    if (m_min.isValid() && time < m_min)
        return false;
    if (m_max.isValid() && time > m_max)
        return false;
    return true;
}

bool AuTimePanel::isItemSelectable(int column, int value) const
{
    const int cols = columnCount();
    if (column < 0 || column >= cols || value < 0)
        return false;
    if (!m_min.isValid() && !m_max.isValid())
        return true;
    // 选中小时 / 分钟（列级联的锚点；未选中时按 0 处理，点击时列 1/2 会重新以选择为锚）
    const int anchorHour = m_selected.isValid() ? m_selected.hour() : 0;
    const int anchorMinute = m_selected.isValid() ? m_selected.minute() : 0;
    switch (column) {
    case 0: {
        // 时：[h:00:00, h:59:59] 与窗口有交集
        const QTime lo(value, 0, 0);
        const QTime hi(value, 59, 59);
        if (m_min.isValid() && hi < m_min)
            return false;
        if (m_max.isValid() && lo > m_max)
            return false;
        return true;
    }
    case 1: {
        // 分：当前小时下 [h:m:00, h:m:59] 与窗口有交集
        const QTime lo(anchorHour, value, 0);
        const QTime hi(anchorHour, value, 59);
        if (m_min.isValid() && hi < m_min)
            return false;
        if (m_max.isValid() && lo > m_max)
            return false;
        return true;
    }
    default:
        // 秒：精确判定
        return isTimeValid(QTime(anchorHour, anchorMinute, value));
    }
}

// ---------------------------------------------------------------------------
// 命中测试 / 几何辅助

QTime AuTimePanel::timeAt(const QPoint &pos) const
{
    int column = -1;
    int value = -1;
    if (!itemAt(pos, &column, &value))
        return QTime();
    const int anchorHour = m_selected.isValid() ? m_selected.hour() : 0;
    const int anchorMinute = m_selected.isValid() ? m_selected.minute() : 0;
    switch (column) {
    case 0: return QTime(value, anchorMinute, m_selected.isValid() ? m_selected.second() : 0);
    case 1: return QTime(anchorHour, value, m_selected.isValid() ? m_selected.second() : 0);
    default: return QTime(anchorHour, anchorMinute, value);
    }
}

QRect AuTimePanel::timeItemRect(int column, int value) const
{
    if (column < 0 || column >= columnCount() || value < 0 || value >= itemCount(column))
        return QRect();
    const QRectF rect = itemRect(column, value);
    return rect.toAlignedRect();
}

// ---------------------------------------------------------------------------
// 数据

int AuTimePanel::columnCount() const
{
    switch (m_granularity) {
    case HoursOnly: return 1;
    case HoursMinutes: return 2;
    case HoursMinutesSeconds: return 3;
    default: return 0;
    }
}

int AuTimePanel::itemCount(int column) const
{
    return column == 0 ? 24 : 60;
}

QString AuTimePanel::columnHeader(int column) const
{
    switch (column) {
    case 0: return QStringLiteral(u"时");
    case 1: return QStringLiteral(u"分");
    default: return QStringLiteral(u"秒");
    }
}

int AuTimePanel::selectedValue(int column) const
{
    if (!m_selected.isValid() || column < 0 || column >= columnCount())
        return -1;
    switch (column) {
    case 0: return m_selected.hour();
    case 1: return m_selected.minute();
    default: return m_selected.second();
    }
}

// ---------------------------------------------------------------------------
// 布局

int AuTimePanel::headerHeight() const
{
    return m_style->weekdayRowHeight();
}

QRect AuTimePanel::headerRect() const
{
    return QRect(0, 0, width(), headerHeight());
}

QRect AuTimePanel::listRect() const
{
    return QRect(0, headerHeight(), width(), qMax(0, height() - headerHeight()));
}

int AuTimePanel::columnsWidth() const
{
    const int cols = columnCount();
    if (cols <= 0)
        return 0;
    const int cw = m_style->timeColumnWidth();
    const int sp = m_style->cellSpacing();
    return cols * cw + (cols - 1) * sp;
}

int AuTimePanel::columnsLeft() const
{
    return qMax(0, (width() - columnsWidth()) / 2);
}

QRect AuTimePanel::columnRect(int column) const
{
    const int cols = columnCount();
    if (column < 0 || column >= cols)
        return QRect();
    const int cw = m_style->timeColumnWidth();
    const int sp = m_style->cellSpacing();
    return QRect(columnsLeft() + column * (cw + sp), 0, cw, height());
}

QRectF AuTimePanel::itemRect(int column, int value) const
{
    const QRect col = columnRect(column);
    if (col.isNull() || value < 0 || value >= itemCount(column))
        return QRectF();
    const qreal y = listRect().top() + value * m_style->timeItemHeight() - m_offsets[column];
    return QRectF(col.left(), y, col.width(), m_style->timeItemHeight());
}

QSize AuTimePanel::sizeHint() const
{
    const int w = columnsWidth() + 2 * kColumnsPadding;
    const int h = headerHeight() + kVisibleRows * m_style->timeItemHeight();
    return QSize(qMax(w, 60), h);
}

// ---------------------------------------------------------------------------
// 滚动

int AuTimePanel::maxScrollOffset(int column) const
{
    if (column < 0 || column >= columnCount())
        return 0;
    const qreal total = itemCount(column) * m_style->timeItemHeight();
    return qMax(0, qRound(total - listRect().height()));
}

void AuTimePanel::scrollToItem(int column, int value, bool animated)
{
    if (column < 0 || column >= columnCount() || value < 0 || value >= itemCount(column))
        return;
    const qreal itemH = m_style->timeItemHeight();
    const qreal target = qBound<qreal>(0.0,
                                       value * itemH + itemH / 2.0 - listRect().height() / 2.0,
                                       maxScrollOffset(column));
    if (animated) {
        startScrollTo(column, target);
    } else {
        m_scrollAnims[column]->stop();
        m_offsets[column] = target;
        update();
    }
}

void AuTimePanel::startScrollTo(int column, qreal target)
{
    if (column < 0 || column >= columnCount())
        return;
    target = qBound<qreal>(0.0, target, maxScrollOffset(column));
    if (qFuzzyCompare(m_offsets[column], target))
        return;
    m_scrollAnims[column]->stop();
    m_scrollAnims[column]->setStartValue(m_offsets[column]);
    m_scrollAnims[column]->setEndValue(target);
    m_scrollAnims[column]->start();
}

void AuTimePanel::clampOffsets()
{
    for (int i = 0; i < 3; ++i) {
        if (i >= columnCount()) {
            m_offsets[i] = 0.0;
            continue;
        }
        m_offsets[i] = qBound<qreal>(0.0, m_offsets[i], maxScrollOffset(i));
    }
}

// ---------------------------------------------------------------------------
// 选择 / 悬停

int AuTimePanel::columnAtX(int x) const
{
    const int cols = columnCount();
    if (cols <= 0)
        return -1;
    const int cw = m_style->timeColumnWidth();
    const int sp = m_style->cellSpacing();
    const int left = columnsLeft();
    if (x < left)
        return 0;
    const int right = left + columnsWidth();
    if (x >= right)
        return cols - 1;
    const int rel = x - left;
    const int stride = cw + sp;
    return qMin(cols - 1, rel / stride);
}

bool AuTimePanel::itemAt(const QPoint &pos, int *column, int *value) const
{
    const int cols = columnCount();
    const QRect list = listRect();
    if (cols <= 0 || !list.contains(pos))
        return false;
    const int c = columnAtX(pos.x());
    const QRect col = columnRect(c);
    if (pos.x() < col.left() || pos.x() > col.right())
        return false;
    const qreal itemH = m_style->timeItemHeight();
    const int index = int((pos.y() - list.top() + m_offsets[c]) / itemH);
    if (index < 0 || index >= itemCount(c))
        return false;
    if (column)
        *column = c;
    if (value)
        *value = index;
    return true;
}

QTime AuTimePanel::clampWithinColumn(int column, const QTime &candidate) const
{
    if (!candidate.isValid() || isTimeValid(candidate))
        return candidate;
    QTime lo = candidate;
    QTime hi = candidate;
    switch (column) {
    case 0:
        lo = QTime(candidate.hour(), 0, 0);
        hi = QTime(candidate.hour(), 59, 59);
        break;
    case 1:
        lo = QTime(candidate.hour(), candidate.minute(), 0);
        hi = QTime(candidate.hour(), candidate.minute(), 59);
        break;
    default:
        return candidate; // 秒列点击前已精确校验
    }
    if (m_min.isValid() && lo < m_min)
        lo = m_min;
    if (m_max.isValid() && hi > m_max)
        hi = m_max;
    if (lo.isValid() && hi.isValid() && lo > hi)
        return QTime(); // 该列范围内无合法值（正常情况下条目已判可选，不会到达）
    if (candidate < lo)
        return lo;
    if (candidate > hi)
        return hi;
    return candidate;
}

void AuTimePanel::selectAt(const QPoint &pos)
{
    int column = -1;
    int value = -1;
    if (!itemAt(pos, &column, &value))
        return;
    if (!isItemSelectable(column, value))
        return;
    const int anchorHour = m_selected.isValid() ? m_selected.hour() : 0;
    const int anchorMinute = m_selected.isValid() ? m_selected.minute() : 0;
    const int anchorSecond = m_selected.isValid() ? m_selected.second() : 0;
    QTime candidate;
    switch (column) {
    case 0:
        candidate = clampWithinColumn(0, QTime(value, anchorMinute, anchorSecond));
        break;
    case 1:
        candidate = clampWithinColumn(1, QTime(anchorHour, value, anchorSecond));
        break;
    default:
        candidate = QTime(anchorHour, anchorMinute, value);
        break;
    }
    if (!candidate.isValid() || candidate == m_selected)
        return;
    m_selected = candidate;
    // 选中值可能被钳位跨列变化：三列统一滚到新值
    for (int c = 0; c < columnCount(); ++c)
        scrollToItem(c, selectedValue(c), true);
    update();
    emit timeChanged(m_selected);
}

void AuTimePanel::updateHover(const QPoint &pos)
{
    int column = -1;
    int value = -1;
    if (!itemAt(pos, &column, &value) || !isItemSelectable(column, value)) {
        column = -1;
        value = -1;
    }
    if (column != m_hoverColumn || value != m_hoverValue) {
        m_hoverColumn = column;
        m_hoverValue = value;
        update();
    }
}

// ---------------------------------------------------------------------------
// 事件

void AuTimePanel::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(m_style->font());

    const int cols = columnCount();
    if (cols <= 0)
        return;

    // 列头（行高与日历星期行一致，颜色同源）
    painter.setPen(m_style->weekdayForegroundColor());
    for (int c = 0; c < cols; ++c)
        painter.drawText(headerRect() & columnRect(c), Qt::AlignCenter, columnHeader(c));

    // 列头下分隔线（与日历面板 footer 分隔线同源配色）
    if (m_style->footerTopLineColor().isValid()) {
        painter.setPen(QPen(m_style->footerTopLineColor(), 1.0));
        painter.drawLine(QPointF(columnsLeft() + 0.5, headerRect().bottom() + 0.5),
                         QPointF(columnsLeft() + columnsWidth() - 0.5, headerRect().bottom() + 0.5));
    }

    // 各列条目（按滚动偏移平移，裁剪到条目区）
    const QRect list = listRect();
    const qreal radius = m_style->cellRadius();
    for (int c = 0; c < cols; ++c) {
        painter.save();
        painter.setClipRect(columnRect(c).adjusted(0, headerHeight(), 0, 0).intersected(list));
        const int count = itemCount(c);
        const int selValue = selectedValue(c);
        for (int v = 0; v < count; ++v) {
            const QRectF rect = itemRect(c, v);
            if (rect.bottom() < list.top() || rect.top() > list.bottom())
                continue;
            const bool selected = m_selected.isValid() && v == selValue;
            const bool enabled = isItemSelectable(c, v);
            const bool hovered = m_hoverColumn == c && m_hoverValue == v;
            if (selected) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(m_style->cellSelectedBackgroundColor());
                painter.drawRoundedRect(rect.adjusted(1.0, 1.0, -1.0, -1.0), radius, radius);
            } else if (hovered && enabled) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(m_style->cellHoverBackgroundColor());
                painter.drawRoundedRect(rect.adjusted(1.0, 1.0, -1.0, -1.0), radius, radius);
            }
            painter.setPen(selected ? m_style->cellSelectedForegroundColor()
                                    : (enabled ? m_style->cellForegroundColor()
                                               : m_style->cellDisabledForegroundColor()));
            painter.drawText(rect, Qt::AlignCenter, QString::number(v));
        }
        painter.restore();
    }
}

void AuTimePanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    clampOffsets(); // 可视高度变化后收敛滚动范围
    // 弹层展开动画期间部件以中间尺寸布局，居中定位会按当时高度计算；
    // 尺寸到位后按当前选中值重新居中（未选中时仅收敛，不跳动）
    if (m_selected.isValid()) {
        for (int c = 0; c < columnCount(); ++c) {
            const int value = selectedValue(c);
            if (value >= 0)
                scrollToItem(c, value, false);
        }
    }
    update();
}

void AuTimePanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    selectAt(event->pos());
}

void AuTimePanel::mouseMoveEvent(QMouseEvent *event)
{
    updateHover(event->pos());
    QWidget::mouseMoveEvent(event);
}

void AuTimePanel::leaveEvent(QEvent *event)
{
    if (m_hoverColumn != -1 || m_hoverValue != -1) {
        m_hoverColumn = -1;
        m_hoverValue = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

void AuTimePanel::wheelEvent(QWheelEvent *event)
{
    const int cols = columnCount();
    if (cols <= 0) {
        event->ignore();
        return;
    }
    // 光标 x 就近映射到列（列间隙 / 列头上滚动同样生效）
    // Qt6 移除了 QWheelEvent::pos()，统一取 position()（Qt5 侧为 posF()）
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    int column = columnAtX(static_cast<int>(event->position().x()));
#else
    int column = columnAtX(event->pos().x());
#endif
    if (column < 0)
        column = 0;
    const int delta = event->angleDelta().y();
    const int steps = (delta > 0 ? -1 : 1) * qMax(1, qAbs(delta) / 120) * kWheelStepItems;
    const qreal itemH = m_style->timeItemHeight();
    const qreal target = qBound<qreal>(0.0, m_offsets[column] + steps * itemH,
                                       maxScrollOffset(column));
    startScrollTo(column, target);
    event->accept();
}
