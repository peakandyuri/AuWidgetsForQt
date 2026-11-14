#include "AuPushButton.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

AuPushButton::AuPushButton(QWidget *parent)
    : QPushButton(parent)
    , m_highlight(false)
    , m_radius(4)
    , m_padding(18)
    , m_background(QColor(0, 0, 0, 10))
    , m_hoverBackground(QColor(0, 0, 0, 22))
    , m_pressBackground(QColor(0, 0, 0, 38))
    , m_borderColor(QColor(0, 0, 0, 40))
    , m_textColor(QColor(0, 0, 0, 220))
    , m_disabledTextColor(QColor(0, 0, 0, 80))
{
    setAttribute(Qt::WA_Hover, true);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(30);
}

AuPushButton::AuPushButton(const QString &text, QWidget *parent)
    : AuPushButton(parent)
{
    setText(text);
}

AuPushButton::~AuPushButton()
{
}

void AuPushButton::setHighlight(bool highlight)
{
    if (m_highlight == highlight)
        return;
    m_highlight = highlight;
    update();
}

bool AuPushButton::isHighlight() const
{
    return m_highlight;
}

void AuPushButton::setBackground(const QColor &color) { m_background = color; update(); }
void AuPushButton::setHoverBackground(const QColor &color) { m_hoverBackground = color; update(); }
void AuPushButton::setPressBackground(const QColor &color) { m_pressBackground = color; update(); }
void AuPushButton::setBorderColor(const QColor &color) { m_borderColor = color; update(); }
void AuPushButton::setTextColor(const QColor &color) { m_textColor = color; update(); }
void AuPushButton::setDisabledTextColor(const QColor &color) { m_disabledTextColor = color; update(); }
void AuPushButton::setRadius(int radius) { m_radius = qMax(0, radius); update(); }
void AuPushButton::setContentPadding(int padding) { m_padding = qMax(0, padding); updateGeometry(); }

QSize AuPushButton::sizeHint() const
{
    const QFontMetrics fm(font());
    // 不依赖 isDefault()：setDefaultButton 不会触发 updateGeometry，宽度提示
    // 会过期；按钮行统一宽度（minimumWidth 由样式下发）布局更稳定
    const int w = fm.horizontalAdvance(text()) + m_padding * 2 + 2;
    const int h = qMax(fm.height() + 10, minimumHeight());
    return QSize(qMax(w, 0), h);
}

QColor AuPushButton::effectiveBackground() const
{
    if (!isEnabled())
        return m_background;    // 禁用态不响应悬停 / 按下形态
    if (m_highlight) {
        const QColor bg = isDown() ? m_pressBackground
                                   : (underMouse() ? m_hoverBackground : m_background);
        return bg.isValid() ? bg : m_background;    // 无效色回退常态背景
    }
    if (isDown())
        return m_pressBackground.isValid() ? m_pressBackground : m_background;
    if (underMouse())
        return m_hoverBackground.isValid() ? m_hoverBackground : m_background;
    return m_background;
}

QColor AuPushButton::focusRingColor() const
{
    // 焦点环需与常态描边可区分（键盘导航可见性）：取文字色收敛 alpha
    QColor ring = isEnabled() ? m_textColor : m_disabledTextColor;
    ring.setAlpha(110);
    return ring;
}

void AuPushButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    if (m_radius > 0)
        path.addRoundedRect(r, m_radius, m_radius);
    else
        path.addRect(r);

    // 背景（主按钮 / 常态 / 悬停 / 按下；无效色 = 透明底）
    const QColor bg = effectiveBackground();
    if (bg.isValid()) {
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawPath(path);
    }

    // 描边
    if (m_borderColor.isValid() && !m_highlight) {
        p.setPen(QPen(m_borderColor, 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    // 焦点环（键盘导航可见性；主按钮以描边环强化）
    if (hasFocus() && isEnabled()) {
        QColor ring = focusRingColor();
        QPen pen(ring, m_highlight ? 1.5 : 1.0);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    // 文字
    p.setFont(font());
    p.setPen(isEnabled() ? m_textColor : m_disabledTextColor);
    p.drawText(rect(), Qt::AlignCenter, text());
}

bool AuPushButton::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::HoverEnter:    // 悬停状态只在进入 / 离开翻转，Move 无需重绘
    case QEvent::HoverLeave:
    case QEvent::EnabledChange:
        update();
        break;
    default:
        break;
    }
    return QPushButton::event(event);
}
