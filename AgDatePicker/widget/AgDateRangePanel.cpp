#include "AgDateRangePanel.h"
#include "AgDatePickerPanel.h"

#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

namespace {
const int kCalendarGap = 8; ///< 两个日历之间的间距（叠加各自内边距）
}

AgDateRangePanel::AgDateRangePanel(QWidget *parent)
    : QWidget(parent)
    , m_style(AgDatePickerStyle::globalStyle())
    , m_left(new AgDatePickerPanel(this))
    , m_right(new AgDatePickerPanel(this))
    , m_focusOwner(m_left)
    , m_picking(false)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_StyledBackground, false);

    for (AgDatePickerPanel *panel : { m_left, m_right }) {
        panel->setSelectionMode(AgDatePicker::SelectDay);
        panel->setRangeSelectionMode(true);
        panel->setFooterVisible(false);
        panel->setTitleNavigationEnabled(false); // 双日历不进入年 / 月九宫格页
        connect(panel, &AgDatePickerPanel::dateSelected,
                this, &AgDateRangePanel::dateSelected);
        connect(panel, &AgDatePickerPanel::dateHovered,
                this, &AgDateRangePanel::dateHovered);
    }
    // 左侧只保留 "<<" "<"，右侧只保留 ">" ">>"（内侧翻月由联动完成）
    m_left->setNavButtonsMask(0x3);
    m_right->setNavButtonsMask(0xC);

    // 翻月联动：保持右侧 = 左侧 + 1 月（showMonth 同月早退，不会循环）
    connect(m_left, &AgDatePickerPanel::monthShownChanged, this, [this]() {
        syncPeer(m_left);
    });
    connect(m_right, &AgDatePickerPanel::monthShownChanged, this, [this]() {
        syncPeer(m_right);
    });

    const QDate today = QDate::currentDate();
    m_left->showMonth(today.year(), today.month());
    const QDate next = QDate(today.year(), today.month(), 1).addMonths(1);
    if (next.isValid())
        m_right->showMonth(next.year(), next.month());
}

AgDateRangePanel::~AgDateRangePanel()
{
}

void AgDateRangePanel::setStyle(AgDatePickerStyle *style)
{
    if (!style || m_style == style)
        return;
    m_style = style;
    m_left->setStyle(style);
    m_right->setStyle(style);
    updateGeometry();
    update();
}

// ---------------------------------------------------------------------------
// footer（三槽位快捷链接；回调中可调用 AgDatePicker::hidePopup() 收起面板）

void AgDateRangePanel::setFooterVisible(bool visible)
{
    if (m_footerVisible == visible)
        return;
    m_footerVisible = visible;
    updateGeometry();
    update();
}

bool AgDateRangePanel::isFooterVisible() const
{
    return m_footerVisible;
}

void AgDateRangePanel::setFooterLink(int slot, const QString &text,
                                     const std::function<void()> &callback, bool custom)
{
    if (slot < 0 || slot > 2)
        return;
    if (!custom) {
        resetFooterLink(slot);
        return;
    }
    m_footerLinks[slot].text = text;
    m_footerLinks[slot].callback = callback;
    m_footerLinks[slot].custom = true;
    update();
}

void AgDateRangePanel::resetFooterLink(int slot)
{
    if (slot < 0 || slot > 2)
        return;
    m_footerLinks[slot] = FooterLinkData();
    update();
}

bool AgDateRangePanel::footerEffectiveVisible() const
{
    if (!m_footerVisible)
        return false;
    for (const FooterLinkData &link : m_footerLinks) {
        if (link.custom && !link.text.isEmpty())
            return true;
    }
    return false;
}

QRect AgDateRangePanel::footerRect() const
{
    if (!footerEffectiveVisible())
        return QRect();
    return QRect(0, height() - m_style->footerHeight(), width(), m_style->footerHeight());
}

QRect AgDateRangePanel::footerLinkRect(int slot) const
{
    if (slot < 0 || slot > 2)
        return QRect();
    const FooterLinkData &link = m_footerLinks[slot];
    if (!link.custom || link.text.isEmpty())
        return QRect();
    const QFontMetrics fm(m_style->font());
    const QRect footer = footerRect();
    const int w = fm.horizontalAdvance(link.text) + 16;
    switch (slot) {
    case 0: return QRect(m_style->panelHorizontalPadding() + 4, footer.top(), w, footer.height());
    case 1: return QRect((width() - w) / 2, footer.top(), w, footer.height());
    default: return QRect(width() - m_style->panelHorizontalPadding() - 4 - w,
                          footer.top(), w, footer.height());
    }
}

void AgDateRangePanel::setSelectedRange(const QDate &start, const QDate &end)
{
    m_committedStart = start;
    m_committedEnd = end;
    if (!m_picking) {
        m_left->setRangeStart(start);
        m_right->setRangeStart(start);
        m_left->setRangeEnd(end);
        m_right->setRangeEnd(end);
    }
}

void AgDateRangePanel::setDateRange(const QDate &min, const QDate &max)
{
    m_left->setDateRange(min, max);
    m_right->setDateRange(min, max);
}

void AgDateRangePanel::resetView()
{
    m_picking = false;
    const QDate anchor = m_committedStart.isValid() ? m_committedStart
                         : m_committedEnd.isValid() ? m_committedEnd
                                                    : QDate::currentDate();
    for (AgDatePickerPanel *panel : { m_left, m_right }) {
        panel->setRangeStart(m_committedStart);
        panel->setRangeEnd(m_committedEnd);
        panel->setRangePreviewEnd(QDate());
        panel->resetView(); // 复位悬停 / 焦点（显示月被直接赋为锚点月，不发联动信号）
    }
    // 右侧推后一个月（联动信号随之校正左侧，保持右侧 = 左侧 + 1 月）
    const QDate next = QDate(anchor.year(), anchor.month(), 1).addMonths(1);
    if (next.isValid())
        m_right->showMonth(next.year(), next.month());
    setFocusDate(anchor);
}

void AgDateRangePanel::beginPicking(const QDate &start)
{
    m_picking = true;
    for (AgDatePickerPanel *panel : { m_left, m_right }) {
        panel->setRangeStart(start);
        panel->setRangeEnd(QDate());
        panel->setRangePreviewEnd(QDate());
    }
    setFocusDate(start);
}

void AgDateRangePanel::endPicking()
{
    m_picking = false;
    for (AgDatePickerPanel *panel : { m_left, m_right }) {
        panel->setRangeStart(m_committedStart);
        panel->setRangeEnd(m_committedEnd);
        panel->setRangePreviewEnd(QDate());
    }
}

void AgDateRangePanel::setPreviewEnd(const QDate &date)
{
    m_left->setRangePreviewEnd(date);
    m_right->setRangePreviewEnd(date);
}

QDate AgDateRangePanel::focusDate() const
{
    return m_focusOwner ? m_focusOwner->focusDate() : QDate();
}

AgDatePickerPanel *AgDateRangePanel::focusPanel() const
{
    return m_focusOwner ? m_focusOwner : m_left;
}

void AgDateRangePanel::setFocusDate(const QDate &date)
{
    if (!date.isValid()) {
        m_focusOwner = m_left;
        m_left->setFocusDate(QDate());
        m_right->setFocusDate(QDate());
        return;
    }
    // 焦点属主按显示月归属选择：焦点日期与哪侧显示月一致归哪侧（含该侧
    // 前后月补齐日），其余按月份先后就近归属；此后导航 / 回车始终发给属主，
    // 属主跨月时自动翻月并经联动信号带动另一侧
    AgDatePickerPanel *owner = nullptr;
    if (date.year() == m_left->displayedYear() && date.month() == m_left->displayedMonth()) {
        owner = m_left;
    } else if (date.year() == m_right->displayedYear()
               && date.month() == m_right->displayedMonth()) {
        owner = m_right;
    } else {
        const QDate rightStart(m_right->displayedYear(), m_right->displayedMonth(), 1);
        owner = date < rightStart ? m_left : m_right;
    }
    m_focusOwner = owner;
    owner->setFocusDate(date);
    (owner == m_left ? m_right : m_left)->setFocusDate(QDate());
}

bool AgDateRangePanel::navigateKey(int key)
{
    return focusPanel()->navigateKey(key);
}

void AgDateRangePanel::activateFocus()
{
    focusPanel()->activateFocus();
}

QSize AgDateRangePanel::sizeHint() const
{
    const QSize single = m_left->sizeHint();
    // footer 生效时面板高度增加一条 footer 条（两个子日历保持原高度）
    const int footerH = footerEffectiveVisible() ? m_style->footerHeight() : 0;
    return QSize(2 * single.width() + kCalendarGap, single.height() + footerH);
}

void AgDateRangePanel::syncPeer(AgDatePickerPanel *sender)
{
    // 保持右侧 = 左侧 + 1 月；越过 QDate 边界（9999-12 / 0001-01）时另一侧保持不动
    if (sender == m_left) {
        const QDate next = QDate(m_left->displayedYear(), m_left->displayedMonth(), 1).addMonths(1);
        if (next.isValid() && next != QDate(m_right->displayedYear(), m_right->displayedMonth(), 1))
            m_right->showMonth(next.year(), next.month());
    } else {
        const QDate prev = QDate(m_right->displayedYear(), m_right->displayedMonth(), 1).addMonths(-1);
        if (prev.isValid() && prev != QDate(m_left->displayedYear(), m_left->displayedMonth(), 1))
            m_left->showMonth(prev.year(), prev.month());
    }
}

void AgDateRangePanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const QSize single = m_left->sizeHint();
    const int footerH = footerEffectiveVisible() ? m_style->footerHeight() : 0;
    m_left->setGeometry(0, 0, single.width(), height() - footerH);
    m_right->setGeometry(single.width() + kCalendarGap, 0, single.width(), height() - footerH);
}

void AgDateRangePanel::paintEvent(QPaintEvent * /*event*/)
{
    if (!footerEffectiveVisible())
        return;

    QPainter painter(this);
    const QRect footer = footerRect();

    // 顶部分隔线（与单日历 footer 一致）
    if (m_style->footerTopLineColor().isValid()) {
        painter.setPen(QPen(m_style->footerTopLineColor(), 1.0));
        painter.drawLine(QPointF(m_style->panelHorizontalPadding() / 2.0, footer.top() + 0.5),
                         QPointF(width() - m_style->panelHorizontalPadding() / 2.0,
                                 footer.top() + 0.5));
    }

    for (int slot = 0; slot < 3; ++slot) {
        const QRect rect = footerLinkRect(slot);
        if (rect.isNull())
            continue;
        const bool hovered = m_hoverFooterSlot == slot;
        painter.setPen(hovered ? m_style->footerLinkHoverColor() : m_style->footerLinkColor());
        painter.drawText(rect, Qt::AlignCenter, m_footerLinks[slot].text);
    }
}

void AgDateRangePanel::mouseMoveEvent(QMouseEvent *event)
{
    int slot = -1;
    if (footerEffectiveVisible() && footerRect().contains(event->pos())) {
        for (int i = 0; i < 3; ++i) {
            if (footerLinkRect(i).contains(event->pos())) {
                slot = i;
                break;
            }
        }
    }
    if (slot != m_hoverFooterSlot) {
        m_hoverFooterSlot = slot;
        update();
    }
    setCursor(slot >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QWidget::mouseMoveEvent(event);
}

void AgDateRangePanel::mousePressEvent(QMouseEvent *event)
{
    // footer 链接点击（区域不被两个子日历覆盖，事件天然落到本面板）
    if (footerEffectiveVisible() && footerRect().contains(event->pos())) {
        for (int slot = 0; slot < 3; ++slot) {
            if (!footerLinkRect(slot).contains(event->pos()))
                continue;
            if (m_footerLinks[slot].callback)
                m_footerLinks[slot].callback();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void AgDateRangePanel::leaveEvent(QEvent *event)
{
    if (m_hoverFooterSlot != -1) {
        m_hoverFooterSlot = -1;
        update();
    }
    setCursor(Qt::ArrowCursor);
    QWidget::leaveEvent(event);
}
