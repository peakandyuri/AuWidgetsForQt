#include "AuTitleBar.h"
#include "AuWindow.h"
#include "AuWindow_p.h"
#include "AuWindowStyle.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

// 拖动阈值：按下后移动超过该距离才真正发起系统移动，
// 之前的短暂按住留给双击最大化判定。
static const int kDragStartThreshold = 4;

AuTitleBar::AuTitleBar(AuWindow *window)
    : QWidget(window)
    , m_window(window)
    , m_buttons(ButtonAll)
    , m_maximized(false)
    , m_dragPending(false)
    , m_systemDragPending(false)
    , m_hoverButton(ButtonNone)
    , m_pressedButton(ButtonNone)
{
    Q_ASSERT(window);
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_NoMousePropagation, true);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true);

    // 三槽位容器：普通容器（不可设 WA_TransparentForMouseEvents——该属性
    // 会连带屏蔽整个子树的鼠标事件，Qt 5.12 实证：childAt 也会跳过透明
    // 容器的子组件，槽内交互组件将完全收不到点击）。容器空白处可拖动
    // 窗体靠事件传播实现：容器自身不消费事件，按下的鼠标事件被忽略后
    // 上抛给标题栏处理；hitTestCaption 沿父链判定交互区时跳过容器本身。
    for (int i = 0; i < 3; ++i) {
        QWidget *section = new QWidget(this);
        section->setFocusPolicy(Qt::NoFocus);
        QHBoxLayout *lay = new QHBoxLayout(section);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        section->installEventFilter(this);
        m_sections[i] = section;
    }
}

// ---- 图标与标题 ----

void AuTitleBar::setIcon(const QIcon &icon)
{
    m_icon = icon;
    update();
}

void AuTitleBar::setTitle(const QString &title)
{
    m_title = title;
    update();
    updateGeometry();
}

// ---- 操作按钮 ----

void AuTitleBar::setButtons(Buttons buttons)
{
    m_buttons = buttons;
    updateGeometry();
    relayoutSections();
    update();
}

void AuTitleBar::setButtonVisible(Button button, bool visible)
{
    Buttons b = m_buttons;
    if (visible)
        b |= button;
    else
        b &= ~button;
    setButtons(b);
}

bool AuTitleBar::isButtonVisible(Button button) const
{
    return m_buttons.testFlag(button);
}

void AuTitleBar::updateMaximizeState(bool maximized)
{
    if (m_maximized == maximized)
        return;
    m_maximized = maximized;
    update();
}

// ---- 自定义组件槽位 ----

void AuTitleBar::addWidget(QWidget *widget, Slot slot, bool grabMouse)
{
    if (!widget || slot < 0 || slot > SlotBeforeButtons)
        return;
    QWidget *section = m_sections[slot];
    widget->setParent(section);
    static_cast<QHBoxLayout *>(section->layout())->addWidget(widget, 0, Qt::AlignVCenter);
    setWidgetGrabMouse(widget, grabMouse);
    relayoutSections();
}

void AuTitleBar::removeWidget(QWidget *widget)
{
    if (!widget)
        return;
    for (int i = 0; i < 3; ++i) {
        QWidget *section = m_sections[i];
        if (widget->parentWidget() == section) {
            section->layout()->removeWidget(widget);
            widget->setParent(nullptr);
            relayoutSections();
            return;
        }
    }
}

void AuTitleBar::setWidgetGrabMouse(QWidget *widget, bool grab)
{
    if (!widget)
        return;
    // 抢占 = 组件正常接收鼠标事件（默认）；不抢占 = 鼠标事件穿透，区域可拖动窗体。
    widget->setAttribute(Qt::WA_TransparentForMouseEvents, !grab);
}

bool AuTitleBar::isWidgetGrabMouse(QWidget *widget) const
{
    return widget && !widget->testAttribute(Qt::WA_TransparentForMouseEvents);
}

// ---- 尺寸 ----

QSize AuTitleBar::sizeHint() const
{
    const AuWindowStyle *style = m_window->style();
    return QSize(minimumSizeHint().width(), style->titleBarHeight());
}

QSize AuTitleBar::minimumSizeHint() const
{
    const AuWindowStyle *style = m_window->style();
    const int width = style->titleBarLeftPadding() + style->titleIconSize()
                      + style->titleIconSpacing() + 60 + buttonsBlockWidth();
    return QSize(width, style->titleBarHeight());
}

int AuTitleBar::visibleButtonCount() const
{
    int count = 0;
    if (m_buttons.testFlag(ButtonMinimize))
        ++count;
    if (m_buttons.testFlag(ButtonMaximize))
        ++count;
    if (m_buttons.testFlag(ButtonClose))
        ++count;
    return count;
}

int AuTitleBar::buttonsBlockWidth() const
{
    return visibleButtonCount() * m_window->style()->titleBarButtonWidth();
}

// ---- 命中测试 ----

AuTitleBar::Button AuTitleBar::buttonAt(const QPoint &pos) const
{
    const int block = buttonsBlockWidth();
    if (pos.y() < 0 || pos.y() >= height() || block <= 0)
        return ButtonNone;
    if (pos.x() < width() - block || pos.x() >= width())
        return ButtonNone;
    int indexFromRight = (width() - 1 - pos.x()) / m_window->style()->titleBarButtonWidth();
    // 可见按钮从右往左依次为：关闭、最大化、最小化
    Button order[3] = { ButtonClose, ButtonMaximize, ButtonMinimize };
    for (int i = 0; i < 3; ++i) {
        if (!m_buttons.testFlag(order[i]))
            continue;
        if (indexFromRight == 0)
            return order[i];
        --indexFromRight;
    }
    return ButtonNone;
}

bool AuTitleBar::hitTestCaption(const QPoint &pos) const
{
    if (!rect().contains(pos))
        return false;
    if (buttonAt(pos) != ButtonNone)
        return false;
    // 沿父链向上：遇到任何"抢占鼠标"的可见子组件即视为交互区。
    // 三个槽位容器是透传容器（自身不交互，空白处需可拖动窗体），跳过；
    // 设为不抢占（WA_TransparentForMouseEvents）的组件同理不算交互区。
    QWidget *w = childAt(pos);
    while (w && w != this) {
        const bool isSection = (w == m_sections[0] || w == m_sections[1]
                                || w == m_sections[2]);
        if (w->isVisible() && !isSection
            && !w->testAttribute(Qt::WA_TransparentForMouseEvents))
            return false;
        w = w->parentWidget();
    }
    return true;
}

// ---- 布局 ----

void AuTitleBar::resetDragState()
{
    m_dragPending = false;
    m_systemDragPending = false;
}

void AuTitleBar::relayoutSections()
{
    if (!m_window)
        return;
    const AuWindowStyle *style = m_window->style();
    const int h = height();

    int x = style->titleBarLeftPadding();
    if (!m_icon.isNull()) {
        x += style->titleIconSize() + style->titleIconSpacing();
    }

    const int buttonsLeft = width() - buttonsBlockWidth();
    const int spacing = 6;
    const QFontMetrics fm(style->titleFont());
    const int titleW = m_title.isEmpty() ? 0 : fm.boundingRect(m_title).width();

    const int leftW = m_sections[SlotAfterTitle]->sizeHint().width();
    const int leftX = x + titleW + (leftW > 0 ? spacing : 0);

    const int rightW = m_sections[SlotBeforeButtons]->sizeHint().width();
    const int rightX = buttonsLeft - (rightW > 0 ? spacing : 0) - rightW;

    const int centerW = m_sections[SlotCenter]->sizeHint().width();
    const int blankLeft = leftX + leftW;
    const int blankRight = (rightW > 0 ? rightX : buttonsLeft);
    int centerX = blankLeft + (blankRight - blankLeft - centerW) / 2;
    centerX = qBound(blankLeft, centerX, qMax(blankLeft, blankRight - centerW));

    m_sections[SlotAfterTitle]->setGeometry(leftX, 0, leftW, h);
    m_sections[SlotCenter]->setGeometry(centerX, 0, centerW, h);
    m_sections[SlotBeforeButtons]->setGeometry(rightX, 0, rightW, h);
    update();
}

// ---- 绘制 ----

void AuTitleBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    const AuWindowStyle *style = m_window->style();
    const bool active = m_window->isActiveWindow();

    // 标题栏位于窗体顶部，顶部两角需与窗体圆角一致。
    // 用抗锯齿路径直接填充（而非 clip 后 drawRect）：Qt 的 clip 路径
    // 无抗锯齿，会在圆角处留下锯齿。直边外扩 1px，由控件裁剪收口，
    // 保证底边/侧边仍是锐利直线（AA 只作用于顶部圆弧）。
    const bool maxState = m_window->isMaximized() || m_window->isFullScreen();
    const int rTL = maxState ? 0
                             : qMin(m_window->cornerRadius(Qt::TopLeftCorner), height() / 2);
    const int rTR = maxState ? 0
                             : qMin(m_window->cornerRadius(Qt::TopRightCorner), height() / 2);
    if (rTL > 0 || rTR > 0) {
        QPainterPath path;
        path.moveTo(0, height() + 1);
        if (rTL > 0) {
            path.lineTo(0, rTL);
            path.arcTo(QRectF(0, 0, 2 * rTL, 2 * rTL), 180, -90);
        } else {
            path.lineTo(0, 0);
        }
        if (rTR > 0) {
            path.lineTo(width() - rTR, 0);
            path.arcTo(QRectF(width() - 2 * rTR, 0, 2 * rTR, 2 * rTR), 90, -90);
        } else {
            path.lineTo(width(), 0);
        }
        path.lineTo(width(), height() + 1);
        path.closeSubpath();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(active ? style->titleBarBackgroundColor()
                                : style->titleBarInactiveBackgroundColor());
        painter.drawPath(path);
        painter.setRenderHint(QPainter::Antialiasing, false);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(active ? style->titleBarBackgroundColor()
                                : style->titleBarInactiveBackgroundColor());
        painter.drawRect(rect());
    }

    // 图标 + 标题
    int x = style->titleBarLeftPadding();
    const int centerY = height() / 2;
    if (!m_icon.isNull()) {
        const int size = style->titleIconSize();
        QPixmap pm = m_icon.pixmap(QSize(size, size));
        if (!pm.isNull())
            painter.drawPixmap(x + (size - pm.width()) / 2, centerY - pm.height() / 2, pm);
        x += size + style->titleIconSpacing();
    }

    // 标题绘制右界：最近的槽位 / 按钮区
    int titleRight = width() - buttonsBlockWidth() - 6;
    for (int i = 0; i < 3; ++i) {
        if (m_sections[i]->width() > 0)
            titleRight = qMin(titleRight, m_sections[i]->x() - 4);
    }

    if (!m_title.isEmpty() && titleRight > x) {
        painter.setFont(style->titleFont());
        painter.setPen(active ? style->titleForegroundColor()
                              : style->titleInactiveForegroundColor());
        const QFontMetrics fm(style->titleFont());
        const QString text = fm.elidedText(m_title, Qt::ElideRight, titleRight - x);
        painter.drawText(QRect(x, 0, titleRight - x, height()),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
    }

    drawButtons(painter);

    // 标题栏区段的窗体边框线重描：标题栏背景与按钮悬停背景会把窗体外沿的
    // 1px 边框盖掉（顶边一行 + 两侧上段），不处理则边框呈"凵"形、与标题栏
    // 脱节。这里在标题栏之上按窗体同一形状（窗口坐标系，自然裁剪到本控件
    // 矩形）重描边框，边框即呈"口"字形完整包裹标题栏，衔接处像素一致。
    const QColor border = style->windowBorderColor();
    if (border.isValid()) {
        const bool rounded = !(m_window->isMaximized() || m_window->isFullScreen())
                             && m_window->windowRadius() > 0;
        painter.save();
        // 标题栏贴窗体顶部摆放，窗口坐标系原点平移到本控件坐标系
        painter.translate(-QPointF(pos()));
        painter.setRenderHint(QPainter::Antialiasing, rounded);
        painter.setPen(QPen(border, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(AuWindowDetail::auWindowBorderPath(
            QRectF(m_window->rect()), rounded,
            m_window->cornerRadius(Qt::TopLeftCorner),
            m_window->cornerRadius(Qt::TopRightCorner),
            m_window->cornerRadius(Qt::BottomLeftCorner),
            m_window->cornerRadius(Qt::BottomRightCorner)));
        painter.restore();
        painter.setRenderHint(QPainter::Antialiasing, false);
    }
}

void AuTitleBar::drawButtons(QPainter &painter)
{
    const AuWindowStyle *style = m_window->style();
    const int btnW = style->titleBarButtonWidth();
    const bool enabled = m_window->isEnabled();

    Button order[3] = { ButtonClose, ButtonMaximize, ButtonMinimize };
    int xRight = width();
    for (int i = 0; i < 3; ++i) {
        const Button button = order[i];
        if (!m_buttons.testFlag(button))
            continue;
        const QRect rect(xRight - btnW, 0, btnW, height());
        xRight -= btnW;

        const bool hovered = (m_hoverButton == button);
        const bool pressed = (m_pressedButton == button && hovered);
        const bool isClose = (button == ButtonClose);

        // 背景
        QColor bg;
        if (pressed)
            bg = isClose ? style->closeButtonPressBackgroundColor()
                         : style->buttonPressBackgroundColor();
        else if (hovered)
            bg = isClose ? style->closeButtonHoverBackgroundColor()
                         : style->buttonHoverBackgroundColor();
        if (bg.isValid()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(bg);
            painter.drawRect(rect);
        }

        // 图标
        QColor glyph = isClose && (hovered || pressed)
                           ? style->closeButtonGlyphColor()
                           : (enabled ? style->buttonGlyphColor()
                                      : style->buttonDisabledGlyphColor());
        if (!glyph.isValid())
            glyph = style->buttonGlyphColor();
        drawButtonGlyph(painter, button, rect, glyph);
    }
}

void AuTitleBar::drawButtonGlyph(QPainter &painter, Button button, const QRect &rect, const QColor &color)
{
    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color);
    pen.setWidthF(1.0);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (button) {
    case ButtonMinimize:
        // 中线下方的短横线
        painter.drawLine(QPointF(cx - 5, cy + 3.5), QPointF(cx + 5, cy + 3.5));
        break;
    case ButtonMaximize:
        if (!m_maximized) {
            painter.drawRect(QRectF(cx - 5 + 0.5, cy - 5 + 0.5, 9, 9));
        } else {
            // 还原：前后两个叠加方框
            const QColor bg = m_window->isActiveWindow()
                                  ? m_window->style()->titleBarBackgroundColor()
                                  : m_window->style()->titleBarInactiveBackgroundColor();
            painter.drawRect(QRectF(cx - 4.5 + 0.5, cy - 2.5 + 0.5, 7, 7));
            painter.setBrush(bg);
            painter.drawRect(QRectF(cx - 5.5 + 0.5, cy - 5.5 + 0.5, 7, 7));
            painter.setBrush(Qt::NoBrush);
        }
        break;
    case ButtonClose:
        painter.drawLine(QPointF(cx - 4.5, cy - 4.5), QPointF(cx + 4.5, cy + 4.5));
        painter.drawLine(QPointF(cx - 4.5, cy + 4.5), QPointF(cx + 4.5, cy - 4.5));
        break;
    default:
        break;
    }
    painter.setRenderHint(QPainter::Antialiasing, false);
}

// ---- 鼠标交互 ----

void AuTitleBar::mousePressEvent(QMouseEvent *event)
{
    m_systemDragPending = false;
    if (event->button() == Qt::LeftButton) {
        m_pressedButton = buttonAt(event->pos());
        if (m_pressedButton != ButtonNone) {
            update();
        } else if (hitTestCaption(event->pos())) {
            // 记录起点，移动超过阈值后发起系统拖动（保留双击判定窗口）
            m_dragPending = true;
            m_dragStartPos = event->globalPos();
        }
    }
    QWidget::mousePressEvent(event);
}

void AuTitleBar::mouseMoveEvent(QMouseEvent *event)
{
    const Button hover = buttonAt(event->pos());
    if (hover != m_hoverButton) {
        m_hoverButton = hover;
        update();
    }

    if (m_systemDragPending) {
        // 已请求系统移动。WM 完成指针接管前可能仍有少量在途移动事件到达，
        // 若立即回退手动（grabMouse 重新抢占）会让 WM 的抓取失败、退化为
        // 不跟随指针的空转循环（实测 KWin 系 WM）。故设一个接管宽限期：
        // 仅当持续收到移动事件超过宽限期（WM 未接管）才回退手动拖动。
        if (QDateTime::currentMSecsSinceEpoch() - m_systemDragSentMs > 150) {
            m_systemDragPending = false;
            if (event->buttons() & Qt::LeftButton) {
                m_window->startManualMove(m_dragStartPos);
                return;
            }
        }
    }

    if (m_dragPending
        && (event->buttons() & Qt::LeftButton)
        && (event->globalPos() - m_dragStartPos).manhattanLength() > kDragStartThreshold) {
        m_dragPending = false;
        m_systemDragPending = true;
        m_systemDragSentMs = QDateTime::currentMSecsSinceEpoch();
        m_window->performTitleBarDrag(m_dragStartPos);
    }
    QWidget::mouseMoveEvent(event);
}

void AuTitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_window->noteMouseRelease();
    if (event->button() == Qt::LeftButton) {
        m_systemDragPending = false;
        const Button released = m_pressedButton;
        m_pressedButton = ButtonNone;
        if (released != ButtonNone && buttonAt(event->pos()) == released) {
            switch (released) {
            case ButtonMinimize: emit minimizeButtonClicked(); break;
            case ButtonMaximize: emit maximizeButtonClicked(); break;
            case ButtonClose:    emit closeButtonClicked(); break;
            default: break;
            }
        }
        m_dragPending = false;
        update();
    }
    QWidget::mouseReleaseEvent(event);
}

void AuTitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && hitTestCaption(event->pos())) {
        m_dragPending = false;
        emit titleDoubleClicked();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void AuTitleBar::leaveEvent(QEvent *event)
{
    if (m_hoverButton != ButtonNone) {
        m_hoverButton = ButtonNone;
        update();
    }
    QWidget::leaveEvent(event);
}

void AuTitleBar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange
        || event->type() == QEvent::EnabledChange) {
        update();
    }
    QWidget::changeEvent(event);
}

void AuTitleBar::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayoutSections();
}

bool AuTitleBar::eventFilter(QObject *watched, QEvent *event)
{
    // 槽位容器内容变化（sizeHint 改变）时重新排布
    if (watched == m_sections[0] || watched == m_sections[1] || watched == m_sections[2]) {
        if (event->type() == QEvent::LayoutRequest || event->type() == QEvent::ChildRemoved
            || event->type() == QEvent::Hide || event->type() == QEvent::Show)
            relayoutSections();
    }
    return QWidget::eventFilter(watched, event);
}
