#include "AgScrollBar.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QCursor>
#include <QtMath>

constexpr int AgScrollBar::kMinSliderLength;
constexpr qreal AgScrollBar::kCollapsedExtent;
constexpr qreal AgScrollBar::kExpandedExtent;
constexpr qreal AgScrollBar::kSliderMargin;

AgScrollBar::AgScrollBar(Qt::Orientation orientation, QWidget *parent)
    : QScrollBar(orientation, parent)
    , m_sliderColor(QColor(150, 150, 150))
    , m_backgroundColor(QColor(220, 220, 220))
    , m_sliderExtent(kCollapsedExtent)
    , m_backgroundOpacity(0.0)
    , m_contentOpacity(1.0)
    , m_isExpanded(false)
    , m_isDragging(false)
    , m_dragStartValue(0)
    , m_dragStartPos(0)
    , m_repeatTimer(new QTimer(this))
    , m_repeatDirection(0)
    , m_trackMargin(static_cast<int>(kSliderMargin))
{
    setSingleStep(1);
    setPageStep(10);
    setContextMenuPolicy(Qt::NoContextMenu);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_NativeWindow, false);
    setFocusPolicy(Qt::NoFocus); // 纯鼠标 / 滚轮驱动，避免在弹层中抢走键盘焦点

    m_extentAnimation = new QPropertyAnimation(this, "sliderExtent", this);
    m_extentAnimation->setDuration(150);
    m_extentAnimation->setEasingCurve(QEasingCurve::InOutSine);

    m_opacityAnimation = new QPropertyAnimation(this, "backgroundOpacity", this);
    m_opacityAnimation->setDuration(150);
    m_opacityAnimation->setEasingCurve(QEasingCurve::InOutSine);

    m_repeatTimer->setSingleShot(true);
    connect(m_repeatTimer, &QTimer::timeout, this, &AgScrollBar::onRepeatTimeout);
}

AgScrollBar::~AgScrollBar()
{
}

QColor AgScrollBar::sliderColor() const
{
    return m_sliderColor;
}

void AgScrollBar::setSliderColor(const QColor &color)
{
    m_sliderColor = color;
    update();
}

QColor AgScrollBar::backgroundColor() const
{
    return m_backgroundColor;
}

void AgScrollBar::setBackgroundColor(const QColor &color)
{
    m_backgroundColor = color;
    update();
}

QColor AgScrollBar::separatorColor() const
{
    return m_separatorColor;
}

void AgScrollBar::setSeparatorColor(const QColor &color)
{
    m_separatorColor = color;
    update();
}

int AgScrollBar::trackMargin() const
{
    return m_trackMargin;
}

void AgScrollBar::setTrackMargin(int margin)
{
    m_trackMargin = qMax(0, margin);
    update();
}

qreal AgScrollBar::contentOpacity() const
{
    return m_contentOpacity;
}

void AgScrollBar::setContentOpacity(qreal opacity)
{
    const qreal clamped = qBound(0.0, opacity, 1.0);
    if (m_contentOpacity == clamped)
        return;
    m_contentOpacity = clamped;
    update();
}

void AgScrollBar::paintEvent(QPaintEvent * /*event*/)
{
    if (m_contentOpacity <= 0.0)
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(m_contentOpacity);

    // 分隔线（纵向滚动条与内容区域区分，默认不绘制）
    if (orientation() == Qt::Vertical && m_separatorColor.isValid()) {
        painter.save();
        painter.setPen(m_separatorColor);
        painter.drawLine(0, 0, 0, height() - 1);
        painter.restore();
    }

    // 背景（鼠标悬停时显示，只绘制可拖动的轨道区域）
    const QRect track = trackRect();
    if (m_backgroundOpacity > 0.0 && track.isValid()) {
        painter.setOpacity(m_contentOpacity * m_backgroundOpacity);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_backgroundColor);
        painter.drawRoundedRect(track, 6, 6);
        painter.setOpacity(m_contentOpacity);
    }

    // 滑块
    const QRect slider = sliderRect();
    if (slider.isValid() && maximum() > minimum()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_sliderColor);
        const qreal radius = m_sliderExtent / 2.0;
        painter.drawRoundedRect(slider, radius, radius);
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void AgScrollBar::enterEvent(QEnterEvent * /*event*/)
#else
void AgScrollBar::enterEvent(QEvent * /*event*/)
#endif
{
    startExpandAnimation(true);
}

void AgScrollBar::leaveEvent(QEvent * /*event*/)
{
    if (!m_isDragging)
        startExpandAnimation(false);
}

void AgScrollBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || maximum() <= minimum())
        return;

    const QRect slider = sliderRect();
    const int posValue = (orientation() == Qt::Horizontal) ? event->pos().x() : event->pos().y();
    const int sliderStart = (orientation() == Qt::Horizontal) ? slider.left() : slider.top();

    if (slider.contains(event->pos())) {
        m_isDragging = true;
        m_dragStartValue = value();
        m_dragStartPos = posValue;
        stopRepeatTimer();
    } else {
        m_repeatDirection = (posValue < sliderStart) ? -1 : 1;
        setValue(qBound(minimum(), value() + m_repeatDirection * pageStep(), maximum()));
        startRepeatTimer(m_repeatDirection);
    }
}

void AgScrollBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        if (maximum() <= minimum())
            return;

        const int currentPos = (orientation() == Qt::Horizontal) ? event->pos().x() : event->pos().y();
        const int deltaPixels = currentPos - m_dragStartPos;

        const QRect track = trackRect();
        const int trackLength = (orientation() == Qt::Horizontal) ? track.width() : track.height();
        const int sliderLength = (orientation() == Qt::Horizontal) ? sliderRect().width() : sliderRect().height();
        const int available = qMax(1, trackLength - sliderLength);

        const int range = maximum() - minimum();
        const int deltaValue = qRound(static_cast<qreal>(deltaPixels) * range / available);

        setValue(qBound(minimum(), m_dragStartValue + deltaValue, maximum()));
    } else if (m_repeatDirection != 0) {
        const int dir = pageScrollDirectionAt(event->pos());
        if (dir == 0) {
            stopRepeatTimer();
        } else if (dir != m_repeatDirection) {
            m_repeatDirection = dir;
        }
    }
}

void AgScrollBar::mouseReleaseEvent(QMouseEvent * /*event*/)
{
    m_isDragging = false;
    stopRepeatTimer();
    if (!rect().contains(mapFromGlobal(QCursor::pos())))
        startExpandAnimation(false);
}

void AgScrollBar::wheelEvent(QWheelEvent *event)
{
    const int delta = (orientation() == Qt::Horizontal) ? event->angleDelta().x()
                                                        : event->angleDelta().y();
    if (delta == 0)
        return;

    const int step = (event->modifiers() & Qt::ShiftModifier) ? pageStep() : singleStep();
    const int deltaSteps = qRound(delta / 120.0) * step * 3;
    setValue(qBound(minimum(), value() - deltaSteps, maximum()));
    event->accept();
}

qreal AgScrollBar::sliderExtent() const
{
    return m_sliderExtent;
}

void AgScrollBar::setSliderExtent(qreal extent)
{
    m_sliderExtent = extent;
    update();
}

qreal AgScrollBar::backgroundOpacity() const
{
    return m_backgroundOpacity;
}

void AgScrollBar::setBackgroundOpacity(qreal opacity)
{
    m_backgroundOpacity = qBound(0.0, opacity, 1.0);
    update();
}

QRect AgScrollBar::sliderRect() const
{
    const QRect track = trackRect();
    if (maximum() <= minimum())
        return QRect();

    const int range = maximum() - minimum();
    const qreal pageProportion = static_cast<qreal>(pageStep()) / (range + pageStep());

    if (orientation() == Qt::Horizontal) {
        const int length = qMax(kMinSliderLength, qRound(pageProportion * track.width()));
        const int available = qMax(1, track.width() - length);
        const int x = track.x() + qRound(static_cast<qreal>(value() - minimum()) * available / range);
        const int y = track.y() + (track.height() - static_cast<int>(m_sliderExtent)) / 2;
        return QRect(x, y, length, static_cast<int>(m_sliderExtent));
    } else {
        const int length = qMax(kMinSliderLength, qRound(pageProportion * track.height()));
        const int available = qMax(1, track.height() - length);
        const int y = track.y() + qRound(static_cast<qreal>(value() - minimum()) * available / range);
        const int x = track.x() + (track.width() - static_cast<int>(m_sliderExtent)) / 2;
        return QRect(x, y, static_cast<int>(m_sliderExtent), length);
    }
}

QRect AgScrollBar::trackRect() const
{
    if (orientation() == Qt::Horizontal) {
        return rect().adjusted(m_trackMargin, 0, -m_trackMargin, 0);
    } else {
        return rect().adjusted(0, m_trackMargin, 0, -m_trackMargin);
    }
}

void AgScrollBar::startExpandAnimation(bool expand)
{
    m_isExpanded = expand;

    m_extentAnimation->stop();
    m_extentAnimation->setStartValue(m_sliderExtent);
    m_extentAnimation->setEndValue(expand ? kExpandedExtent : kCollapsedExtent);
    m_extentAnimation->start();

    m_opacityAnimation->stop();
    m_opacityAnimation->setStartValue(m_backgroundOpacity);
    m_opacityAnimation->setEndValue(expand ? 1.0 : 0.0);
    m_opacityAnimation->start();
}

int AgScrollBar::pageScrollDirectionAt(const QPoint &pos) const
{
    const QRect slider = sliderRect();
    if (!slider.isValid() || slider.contains(pos))
        return 0;

    const int posValue = (orientation() == Qt::Horizontal) ? pos.x() : pos.y();
    const int sliderStart = (orientation() == Qt::Horizontal) ? slider.left() : slider.top();
    const int sliderEnd = (orientation() == Qt::Horizontal) ? slider.right() : slider.bottom();

    if (posValue < sliderStart)
        return -1;
    if (posValue > sliderEnd)
        return 1;
    return 0;
}

void AgScrollBar::startRepeatTimer(int direction)
{
    m_repeatDirection = direction;
    m_repeatTimer->stop();
    m_repeatTimer->setInterval(300);
    m_repeatTimer->setSingleShot(true);
    m_repeatTimer->start();
}

void AgScrollBar::stopRepeatTimer()
{
    m_repeatDirection = 0;
    m_repeatTimer->stop();
}

void AgScrollBar::onRepeatTimeout()
{
    if (m_repeatDirection == 0 || maximum() <= minimum())
        return;

    const int newValue = qBound(minimum(), value() + m_repeatDirection * pageStep(), maximum());
    if (newValue == value()) {
        stopRepeatTimer();
        return;
    }

    setValue(newValue);
    m_repeatTimer->setInterval(80);
    m_repeatTimer->setSingleShot(false);
    m_repeatTimer->start();
}
