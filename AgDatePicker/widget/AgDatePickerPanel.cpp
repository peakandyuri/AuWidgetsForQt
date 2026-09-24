#include "AgDatePickerPanel.h"

#include <QEvent>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtMath>

namespace {

// 悬停元素类型
enum HoverKind {
    HoverNone = 0,
    HoverNav,            ///< DayView 箭头（index 0="<<" 1="<" 2=">" 3=">>"）
    HoverTitleYear,      ///< DayView "xxxx年" 分段
    HoverTitleMonth,     ///< DayView "xx月" 分段
    HoverMonthHeadYear,  ///< MonthView head 年份文本
    HoverScrollUp,       ///< Month/YearView 上箭头
    HoverScrollDown,     ///< Month/YearView 下箭头
    HoverDayCell,        ///< index = row * 7 + col
    HoverMonthCell,      ///< index 0-11
    HoverYearCell,       ///< index 0-11（当前页内）
    HoverFooterLink      ///< index = 槽位 0-2
};

// 箭头方向
enum ArrowDirection { ArrowLeft, ArrowRight, ArrowUp, ArrowDown };

/// 星期标题（周一 ~ 周日）
QString weekdayLabel(int index)
{
    static const QString labels[7] = {
        QStringLiteral(u"一"), QStringLiteral(u"二"), QStringLiteral(u"三"),
        QStringLiteral(u"四"), QStringLiteral(u"五"), QStringLiteral(u"六"),
        QStringLiteral(u"日")
    };
    return labels[qBound(0, index, 6)];
}

} // namespace

AgDatePickerPanel::AgDatePickerPanel(QWidget *parent)
    : QWidget(parent)
    , m_style(AgDatePickerStyle::globalStyle())
    , m_mode(AgDatePicker::SelectDay)
    , m_view(DayView)
    , m_focusIndex(-1)
    , m_rangeMode(false)
    , m_navMask(0xF)
    , m_titleNavEnabled(true)
    , m_footerVisible(true)
    , m_footerWidget(nullptr)
    , m_hoverKind(HoverNone)
    , m_hoverIndex(-1)
    , m_hoverExtra(false)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus); // 焦点与键盘交互由 AgDatePicker 编辑框承载
    setAttribute(Qt::WA_StyledBackground, false);

    const QDate today = QDate::currentDate();
    m_year = today.year();
    m_month = today.month();
    m_monthViewYear = today.year();
    m_yearPageStart = (today.year() / 12) * 12;
    m_scrollPos = today.year() / 12;

    m_scrollAnim = new QVariantAnimation(this);
    m_scrollAnim->setDuration(150);
    m_scrollAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_scrollAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_scrollPos = value.toReal();
        update();
    });
}

AgDatePickerPanel::~AgDatePickerPanel()
{
}

// ---------------------------------------------------------------------------
// 配置

void AgDatePickerPanel::setStyle(AgDatePickerStyle *style)
{
    if (m_style == style || !style)
        return;
    m_style = style;
    updateGeometry();
    update();
}

AgDatePickerStyle *AgDatePickerPanel::style() const
{
    return m_style;
}

void AgDatePickerPanel::setSelectionMode(AgDatePicker::SelectionMode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    update();
}

AgDatePicker::SelectionMode AgDatePickerPanel::selectionMode() const
{
    return m_mode;
}

void AgDatePickerPanel::setSelectedDate(const QDate &date)
{
    m_selected = date;
    update();
}

QDate AgDatePickerPanel::selectedDate() const
{
    return m_selected;
}

void AgDatePickerPanel::setDateRange(const QDate &min, const QDate &max)
{
    m_min = min;
    m_max = max;
    if (m_min.isValid() && m_max.isValid() && m_min > m_max)
        qSwap(m_min, m_max);
    // 收敛当前页面状态，避免停留在已不可选的年份页
    m_monthViewYear = qBound(minYearBound(), m_monthViewYear, maxYearBound());
    const int page = qBound(minPageIndex(), pageOfSettled(), maxPageIndex());
    if (page != pageOfSettled()) {
        m_scrollAnim->stop();
        m_scrollPos = page;
        m_yearPageStart = page * 12;
    }
    update();
}

QDate AgDatePickerPanel::minimumDate() const
{
    return m_min;
}

QDate AgDatePickerPanel::maximumDate() const
{
    return m_max;
}

bool AgDatePickerPanel::isDateSelectable(const QDate &date) const
{
    if (!date.isValid())
        return false;
    if (m_min.isValid() && date < m_min)
        return false;
    if (m_max.isValid() && date > m_max)
        return false;
    return true;
}

bool AgDatePickerPanel::isMonthSelectable(int year, int month) const
{
    const QDate start(year, month, 1);
    if (!start.isValid())
        return false;
    const QDate end(year, month, start.daysInMonth());
    if (m_min.isValid() && end < m_min)
        return false;
    if (m_max.isValid() && start > m_max)
        return false;
    return true;
}

bool AgDatePickerPanel::isYearSelectable(int year) const
{
    const QDate start(year, 1, 1);
    const QDate end(year, 12, 31);
    if (!start.isValid())
        return false;
    if (m_min.isValid() && end < m_min)
        return false;
    if (m_max.isValid() && start > m_max)
        return false;
    return true;
}

// ---------------------------------------------------------------------------
// 键盘导航

QDate AgDatePickerPanel::focusDate() const
{
    return m_focusDate;
}

void AgDatePickerPanel::setFocusDate(const QDate &date)
{
    if (!date.isValid()) {
        if (m_focusDate.isValid()) {
            m_focusDate = QDate();
            update();
        }
        return;
    }
    if (m_focusDate == date)
        return;
    m_focusDate = date;
    if (m_view == DayView) {
        // 焦点移出当前 42 格（含前后月补齐）时自动翻月包含焦点日期
        const QDate gridStart = dayGridStart();
        if (date < gridStart || date >= gridStart.addDays(42))
            showMonth(date.year(), date.month());
        else
            update();
    } else {
        update();
    }
}

bool AgDatePickerPanel::navigateKey(int key)
{
    switch (m_view) {
    case DayView: {
        QDate base = m_focusDate.isValid() ? m_focusDate
                                           : (m_selected.isValid() ? m_selected
                                                                   : QDate::currentDate());
        QDate target;
        switch (key) {
        case Qt::Key_Left: target = base.addDays(-1); break;
        case Qt::Key_Right: target = base.addDays(1); break;
        case Qt::Key_Up: target = base.addDays(-7); break;
        case Qt::Key_Down: target = base.addDays(7); break;
        case Qt::Key_PageUp: target = base.addMonths(-1); break;
        case Qt::Key_PageDown: target = base.addMonths(1); break;
        default: return false;
        }
        if (!target.isValid())
            return false;
        // 钳制到可选范围边界（QDate 有效边界之外由 addDays 返回无效值自然拦截）
        if (m_min.isValid() && target < m_min)
            target = m_min;
        if (m_max.isValid() && target > m_max)
            target = m_max;
        if (!isDateSelectable(target))
            return false;
        setFocusDate(target);
        return true;
    }
    case MonthView:
    case YearView: {
        int index = m_focusIndex >= 0 ? m_focusIndex : -1;
        if (index < 0) {
            // 初始焦点：选中项（或当前年 / 月）所在格
            if (m_view == MonthView)
                index = (m_selected.isValid() && m_selected.year() == m_monthViewYear)
                            ? m_selected.month() - 1
                            : QDate::currentDate().month() - 1;
            else
                index = qBound(0, (m_selected.isValid() ? m_selected.year()
                                                        : QDate::currentDate().year())
                                      - m_yearPageStart,
                               11);
        }
        switch (key) {
        case Qt::Key_Left: index = qMax(0, index - 1); break;
        case Qt::Key_Right: index = qMin(11, index + 1); break;
        case Qt::Key_Up: index = qMax(0, index - 4); break;
        case Qt::Key_Down: index = qMin(11, index + 4); break;
        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            if (m_view == MonthView)
                shiftMonthViewYear(key == Qt::Key_PageUp ? -1 : 1);
            else
                scrollYearsBy(key == Qt::Key_PageUp ? -1 : 1);
            return true;
        default:
            return false;
        }
        if (index != m_focusIndex) {
            m_focusIndex = index;
            update();
        }
        return true;
    }
    }
    return false;
}

void AgDatePickerPanel::activateFocus()
{
    switch (m_view) {
    case DayView:
        if (m_focusDate.isValid() && isDateSelectable(m_focusDate))
            emit dateSelected(m_focusDate);
        break;
    case MonthView:
        if (m_focusIndex >= 0 && m_focusIndex < 12)
            activateMonth(m_focusIndex + 1);
        break;
    case YearView:
        if (m_focusIndex >= 0 && m_focusIndex < 12)
            activateYear(m_yearPageStart + m_focusIndex);
        break;
    }
}

// ---------------------------------------------------------------------------
// 范围选择

void AgDatePickerPanel::setRangeSelectionMode(bool on)
{
    if (m_rangeMode == on)
        return;
    m_rangeMode = on;
    update();
}

bool AgDatePickerPanel::isRangeSelectionMode() const
{
    return m_rangeMode;
}

void AgDatePickerPanel::setRangeStart(const QDate &date)
{
    if (m_rangeStart == date)
        return;
    m_rangeStart = date;
    update();
}

void AgDatePickerPanel::setRangeEnd(const QDate &date)
{
    if (m_rangeEnd == date)
        return;
    m_rangeEnd = date;
    update();
}

void AgDatePickerPanel::setRangePreviewEnd(const QDate &date)
{
    if (m_rangePreviewEnd == date)
        return;
    m_rangePreviewEnd = date;
    update();
}

void AgDatePickerPanel::setTitleNavigationEnabled(bool enabled)
{
    if (m_titleNavEnabled == enabled)
        return;
    m_titleNavEnabled = enabled;
    update();
}

void AgDatePickerPanel::setNavButtonsMask(int mask)
{
    mask &= 0xF;
    if (m_navMask == mask)
        return;
    m_navMask = mask;
    update();
}


void AgDatePickerPanel::setFooterVisible(bool visible)
{
    if (m_footerVisible == visible)
        return;
    m_footerVisible = visible;
    if (m_footerWidget)
        m_footerWidget->setVisible(visible);
    updateGeometry();
    update();
}

bool AgDatePickerPanel::isFooterVisible() const
{
    return m_footerVisible;
}

void AgDatePickerPanel::setFooterLink(int slot, const QString &text,
                                      const std::function<void()> &callback, bool custom)
{
    if (slot < 0 || slot > 2)
        return;
    m_footerLinks[slot].text = custom ? text : QString();
    m_footerLinks[slot].callback = custom ? callback : nullptr;
    m_footerLinks[slot].custom = custom;
    update();
}

void AgDatePickerPanel::resetFooterLink(int slot)
{
    setFooterLink(slot, QString(), nullptr, false);
}

void AgDatePickerPanel::setFooterWidget(QWidget *widget)
{
    if (m_footerWidget == widget)
        return;
    if (m_footerWidget)
        m_footerWidget->deleteLater();
    m_footerWidget = widget;
    if (m_footerWidget) {
        m_footerWidget->setParent(this);
        m_footerWidget->setGeometry(footerRect());
        m_footerWidget->setVisible(m_footerVisible);
    }
    update();
}

QWidget *AgDatePickerPanel::footerWidget() const
{
    return m_footerWidget;
}

// ---------------------------------------------------------------------------
// 页面状态

AgDatePickerPanel::View AgDatePickerPanel::view() const
{
    return m_view;
}

int AgDatePickerPanel::displayedYear() const
{
    switch (m_view) {
    case MonthView: return m_monthViewYear;
    case YearView: return m_yearPageStart;
    default: return m_year;
    }
}

int AgDatePickerPanel::displayedMonth() const
{
    return (m_view == DayView) ? m_month : 0;
}

void AgDatePickerPanel::resetView()
{
    const QDate anchor = m_selected.isValid() ? m_selected : QDate::currentDate();
    m_year = anchor.year();
    m_month = anchor.month();
    m_monthViewYear = qBound(minYearBound(), anchor.year(), maxYearBound());
    const int page = qBound(minPageIndex(), anchor.year() / 12, maxPageIndex());
    m_scrollAnim->stop();
    m_scrollPos = page;
    m_yearPageStart = page * 12;

    m_view = (m_mode == AgDatePicker::SelectDay) ? DayView
             : (m_mode == AgDatePicker::SelectMonth) ? MonthView
                                                      : YearView;
    m_hoverKind = HoverNone;
    m_hoverIndex = -1;

    // 键盘焦点复位：日历页定位到选中日 / 今天，月 / 年页定位到对应格
    m_focusDate = anchor;
    if (m_view == MonthView)
        m_focusIndex = anchor.month() - 1;
    else if (m_view == YearView)
        m_focusIndex = qBound(0, anchor.year() - m_yearPageStart, 11);
    else
        m_focusIndex = -1;
    update();
}

bool AgDatePickerPanel::goBack()
{
    switch (m_mode) {
    case AgDatePicker::SelectDay:
    case AgDatePicker::SelectDateRange: // 双日历组合恒为日历页（不会进入此分支）
        if (m_view == DayView)
            return false; // 日历页为初始页，交给调用方收起面板
        m_view = DayView;
        break;
    case AgDatePicker::SelectMonth:
        if (m_view != YearView)
            return false; // 月份页为初始页
        m_view = MonthView;
        break;
    case AgDatePicker::SelectYear:
        return false; // 年份页为初始页
    }
    m_hoverKind = HoverNone;
    m_hoverIndex = -1;
    update();
    return true;
}

void AgDatePickerPanel::scrollYearsBy(int pages)
{
    if (m_view != YearView)
        return;
    const int target = qBound(minPageIndex(), pageOfSettled() + pages, maxPageIndex());
    startYearScrollTo(target);
}

QSize AgDatePickerPanel::sizeHint() const
{
    const int pad = m_style->panelHorizontalPadding();
    const int sp = m_style->cellSpacing();
    const int w = 2 * pad + 7 * m_style->dayCellWidth() + 6 * sp;
    int h = m_style->headHeight() + m_style->weekdayRowHeight()
            + 6 * m_style->dayCellHeight() + 5 * sp;
    // footer 隐藏时底部保留与左右一致的内边距（与 bodyRect 保持一致）
    h += m_footerVisible ? m_style->footerHeight() : pad;
    return QSize(w, h);
}

// ---------------------------------------------------------------------------
// 命中测试 / 几何辅助

QDate AgDatePickerPanel::dateAt(const QPoint &pos) const
{
    if (m_view != DayView)
        return QDate();
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            if (dayCellRect(row, col).contains(pos)) {
                const QDate date = dayAtCell(row, col);
                return isDayCellSelectable(date) ? date : QDate();
            }
        }
    }
    return QDate();
}

int AgDatePickerPanel::monthAt(const QPoint &pos) const
{
    if (m_view != MonthView)
        return -1;
    for (int index = 0; index < 12; ++index) {
        if (monthCellRect(index / 4, index % 4).contains(pos)) {
            const int month = index + 1;
            return isMonthSelectable(m_monthViewYear, month) ? month : -1;
        }
    }
    return -1;
}

int AgDatePickerPanel::yearAt(const QPoint &pos) const
{
    if (m_view != YearView || m_scrollAnim->state() == QAbstractAnimation::Running)
        return -1;
    for (int index = 0; index < 12; ++index) {
        if (monthCellRect(index / 4, index % 4).contains(pos)) {
            const int year = m_yearPageStart + index;
            return isYearSelectable(year) ? year : -1;
        }
    }
    return -1;
}

QRect AgDatePickerPanel::dateCellRect(const QDate &date) const
{
    if (m_view != DayView || !date.isValid())
        return QRect();
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            if (dayAtCell(row, col) == date)
                return dayCellRect(row, col);
        }
    }
    return QRect();
}

QRect AgDatePickerPanel::monthCellRect(int month) const
{
    if (m_view != MonthView || month < 1 || month > 12)
        return QRect();
    return monthCellRect((month - 1) / 4, (month - 1) % 4);
}

QRect AgDatePickerPanel::yearCellRect(int year) const
{
    if (m_view != YearView)
        return QRect();
    const int page = year / 12;
    if (page != m_yearPageStart / 12)
        return QRect();
    const int index = year - page * 12;
    return monthCellRect(index / 4, index % 4);
}

QRect AgDatePickerPanel::titleYearRect() const
{
    QRect yearRect, monthRect;
    titleRects(&yearRect, &monthRect);
    return yearRect;
}

QRect AgDatePickerPanel::titleMonthRect() const
{
    QRect yearRect, monthRect;
    titleRects(&yearRect, &monthRect);
    return monthRect;
}

QRect AgDatePickerPanel::footerLinkRect(int slot) const
{
    if (slot < 0 || slot > 2 || !m_footerVisible || m_footerWidget)
        return QRect();
    const QString text = m_footerLinks[slot].custom
                             ? m_footerLinks[slot].text
                             : (slot == 1 ? footerShortcutText() : QString());
    if (text.isEmpty())
        return QRect();
    const QFontMetrics fm(m_style->font());
    const QRect footer = footerRect();
    const int w = fm.horizontalAdvance(text) + 16;
    switch (slot) {
    case 0: return QRect(m_style->panelHorizontalPadding() + 4, footer.top(), w, footer.height());
    case 1: return QRect((width() - w) / 2, footer.top(), w, footer.height());
    default: return QRect(width() - m_style->panelHorizontalPadding() - 4 - w,
                          footer.top(), w, footer.height());
    }
}

// ---------------------------------------------------------------------------
// 布局

QRect AgDatePickerPanel::headRect() const
{
    return QRect(0, 0, width(), m_style->headHeight());
}

QRect AgDatePickerPanel::bodyRect() const
{
    const int top = m_style->headHeight();
    const int pad = m_style->panelHorizontalPadding();
    // 底部：footer 可见时让位给 footer，否则留出与左右一致的内边距（不贴面板边缘）
    const int bottom = height() - (m_footerVisible ? m_style->footerHeight() : pad);
    return QRect(0, top, width(), qMax(0, bottom - top));
}

QRect AgDatePickerPanel::footerRect() const
{
    if (!m_footerVisible)
        return QRect();
    return QRect(0, height() - m_style->footerHeight(), width(), m_style->footerHeight());
}

QRect AgDatePickerPanel::weekdayRowRect() const
{
    const QRect body = bodyRect();
    const int pad = m_style->panelHorizontalPadding();
    return QRect(body.left() + pad, body.top(), qMax(0, body.width() - 2 * pad),
                 m_style->weekdayRowHeight());
}

QRect AgDatePickerPanel::dayGridRect() const
{
    const QRect body = bodyRect();
    const int pad = m_style->panelHorizontalPadding();
    const int top = body.top() + m_style->weekdayRowHeight();
    const int sp = m_style->cellSpacing();
    return QRect(body.left() + pad, top, qMax(0, body.width() - 2 * pad),
                 6 * m_style->dayCellHeight() + 5 * sp);
}

QRect AgDatePickerPanel::gridCellRect(const QRect &grid, int rows, int cols,
                                      int row, int col) const
{
    const int sp = m_style->cellSpacing();
    const int cw = (grid.width() - (cols - 1) * sp) / cols;
    const int ch = (grid.height() - (rows - 1) * sp) / rows;
    return QRect(grid.left() + col * (cw + sp), grid.top() + row * (ch + sp), cw, ch);
}

QRect AgDatePickerPanel::dayCellRect(int row, int col) const
{
    return gridCellRect(dayGridRect(), 6, 7, row, col);
}

QRect AgDatePickerPanel::monthCellRect(int row, int col) const
{
    // 月 / 年页共用 body 区域（含星期行空间），4 列 x 3 行铺满；与日历页一致
    // 左右内缩 panelHorizontalPadding（双日历 / 单日历边距一致）
    const int pad = m_style->panelHorizontalPadding();
    return gridCellRect(bodyRect().adjusted(pad, 0, -pad, 0), 3, 4, row, col);
}

QRect AgDatePickerPanel::yearCellRect(int row, int col) const
{
    return monthCellRect(row, col);
}

QRect AgDatePickerPanel::navButtonRect(int index) const
{
    const int b = m_style->navButtonSize();
    const QRect head = headRect();
    const int y = head.top() + (head.height() - b) / 2;
    const int pad = m_style->panelHorizontalPadding();
    const int gap = 2;
    switch (index) {
    case 0: return QRect(pad, y, b, b);                          // "<<"
    case 1: return QRect(pad + b + gap, y, b, b);                // "<"
    case 2: return QRect(width() - pad - 2 * b - gap, y, b, b);  // ">"
    default: return QRect(width() - pad - b, y, b, b);           // ">>"
    }
}

void AgDatePickerPanel::titleRects(QRect *yearRect, QRect *monthRect) const
{
    const QFontMetrics fm(m_style->font());
    const QString yearText = QString::number(m_year) + QStringLiteral(u"年");
    const QString monthText = QString::number(m_month) + QStringLiteral(u"月");
    const int wYear = fm.horizontalAdvance(yearText);
    const int wMonth = fm.horizontalAdvance(monthText);
    const int startX = (width() - wYear - wMonth) / 2;
    const QRect head = headRect();
    if (yearRect)
        *yearRect = QRect(startX - 2, head.top(), wYear + 2, head.height());
    if (monthRect)
        *monthRect = QRect(startX + wYear, head.top(), wMonth + 2, head.height());
}

QRect AgDatePickerPanel::scrollButtonRect(bool up) const
{
    const int b = m_style->navButtonSize();
    const QRect head = headRect();
    const int y = head.top() + (head.height() - b) / 2;
    const int pad = m_style->panelHorizontalPadding();
    const int gap = 2;
    return up ? QRect(width() - pad - 2 * b - gap, y, b, b)
              : QRect(width() - pad - b, y, b, b);
}

QRect AgDatePickerPanel::monthHeadYearRect() const
{
    const QFontMetrics fm(m_style->font());
    const QString text = QString::number(m_monthViewYear) + QStringLiteral(u"年");
    const int w = fm.horizontalAdvance(text) + 4;
    const QRect head = headRect();
    return QRect(m_style->panelHorizontalPadding() + 2, head.top(), w, head.height());
}

// ---------------------------------------------------------------------------
// 年份页滚动

int AgDatePickerPanel::yearPageCount() const
{
    return 12;
}

int AgDatePickerPanel::minYearBound() const
{
    const int fallback = QDate::currentDate().year() - 100;
    return qMax(1, m_min.isValid() ? m_min.year() : fallback);
}

int AgDatePickerPanel::maxYearBound() const
{
    const int fallback = QDate::currentDate().year() + 100;
    return qMin(9999, m_max.isValid() ? m_max.year() : fallback);
}

int AgDatePickerPanel::minPageIndex() const
{
    return minYearBound() / 12;
}

int AgDatePickerPanel::maxPageIndex() const
{
    return maxYearBound() / 12;
}

int AgDatePickerPanel::pageOfSettled() const
{
    return m_yearPageStart / 12;
}

void AgDatePickerPanel::startYearScrollTo(int page)
{
    page = qBound(minPageIndex(), page, maxPageIndex());
    m_yearPageStart = page * 12; // head 区间文本立即切换到目标页
    if (qFuzzyCompare(m_scrollPos, qreal(page)))
        return;
    m_scrollAnim->stop();
    m_scrollAnim->setStartValue(m_scrollPos);
    m_scrollAnim->setEndValue(QVariant(qreal(page)));
    m_scrollAnim->start();
}

void AgDatePickerPanel::jumpToYearPage(int page)
{
    page = qBound(minPageIndex(), page, maxPageIndex());
    m_scrollAnim->stop();
    m_scrollPos = page;
    m_yearPageStart = page * 12;
    update();
}

// ---------------------------------------------------------------------------
// 日期数据

QDate AgDatePickerPanel::dayGridStart() const
{
    const QDate first(m_year, m_month, 1);
    return first.addDays(-(first.dayOfWeek() - 1)); // 周一开头
}

QDate AgDatePickerPanel::dayAtCell(int row, int col) const
{
    return dayGridStart().addDays(row * 7 + col);
}

bool AgDatePickerPanel::isDayCellSelectable(const QDate &date) const
{
    return isDateSelectable(date);
}

bool AgDatePickerPanel::navButtonEnabled(int index) const
{
    switch (index) {
    case 0: return m_year > 1;
    case 1: return QDate(m_year, m_month, 1) > QDate(1, 1, 1);
    case 2: return QDate(m_year, m_month, 1) < QDate(9999, 12, 1);
    default: return m_year < 9999;
    }
}

bool AgDatePickerPanel::scrollUpEnabled() const
{
    return m_view == YearView ? pageOfSettled() > minPageIndex()
                              : m_monthViewYear > minYearBound();
}

bool AgDatePickerPanel::scrollDownEnabled() const
{
    return m_view == YearView ? pageOfSettled() < maxPageIndex()
                              : m_monthViewYear < maxYearBound();
}

bool AgDatePickerPanel::monthHeadYearClickable() const
{
    return true; // 进入年份页选择年份（SelectYear 粒度时不会出现在 MonthView）
}

// ---------------------------------------------------------------------------
// footer 内置快捷项

QString AgDatePickerPanel::footerShortcutText() const
{
    switch (m_mode) {
    case AgDatePicker::SelectMonth: return QStringLiteral(u"当月");
    case AgDatePicker::SelectYear: return QStringLiteral(u"今年");
    default: return QStringLiteral(u"今天");
    }
}

QDate AgDatePickerPanel::footerShortcutDate() const
{
    const QDate today = QDate::currentDate();
    switch (m_mode) {
    case AgDatePicker::SelectMonth: return QDate(today.year(), today.month(), 1);
    case AgDatePicker::SelectYear: return QDate(today.year(), 1, 1);
    default: return today;
    }
}

bool AgDatePickerPanel::footerShortcutEnabled() const
{
    const QDate today = QDate::currentDate();
    switch (m_mode) {
    case AgDatePicker::SelectMonth: return isMonthSelectable(today.year(), today.month());
    case AgDatePicker::SelectYear: return isYearSelectable(today.year());
    default: return isDateSelectable(today);
    }
}

// ---------------------------------------------------------------------------
// 绘制

void AgDatePickerPanel::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(m_style->font());

    switch (m_view) {
    case DayView: paintDayView(painter); break;
    case MonthView: paintMonthView(painter); break;
    case YearView: paintYearView(painter); break;
    }
    paintFooter(painter);
}

void AgDatePickerPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_footerWidget)
        m_footerWidget->setGeometry(footerRect());
}

void AgDatePickerPanel::drawChevron(QPainter &painter, const QRect &rect,
                                    int direction, bool doubled, const QColor &color) const
{
    QPen pen(color, 1.6);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    const qreal s = qMin(rect.width(), rect.height()) * (doubled ? 0.16 : 0.2);
    const qreal cx = rect.center().x();
    const qreal cy = rect.center().y();
    const qreal offset = s * 1.1;

    for (int i = 0; i <= (doubled ? 1 : 0); ++i) {
        QPainterPath path;
        const qreal o = doubled ? (i == 0 ? offset / 2 : -offset / 2) : 0.0;
        switch (direction) {
        case ArrowLeft:
            path.moveTo(cx + o + s * 0.55, cy - s);
            path.lineTo(cx + o - s * 0.55, cy);
            path.lineTo(cx + o + s * 0.55, cy + s);
            break;
        case ArrowRight:
            path.moveTo(cx + o - s * 0.55, cy - s);
            path.lineTo(cx + o + s * 0.55, cy);
            path.lineTo(cx + o - s * 0.55, cy + s);
            break;
        case ArrowUp:
            path.moveTo(cx - s, cy + o + s * 0.55);
            path.lineTo(cx, cy + o - s * 0.55);
            path.lineTo(cx + s, cy + o + s * 0.55);
            break;
        default: // ArrowDown
            path.moveTo(cx - s, cy + o - s * 0.55);
            path.lineTo(cx, cy + o + s * 0.55);
            path.lineTo(cx + s, cy + o - s * 0.55);
            break;
        }
        painter.drawPath(path);
    }
}

void AgDatePickerPanel::paintHeadNavButton(QPainter &painter, const QRect &rect,
                                           int direction, bool doubled, bool hovered,
                                           bool enabled) const
{
    if (hovered && enabled && m_style->navHoverBackgroundColor().isValid()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_style->navHoverBackgroundColor());
        painter.drawRoundedRect(QRectF(rect), rect.width() / 2.0, rect.height() / 2.0);
    }
    drawChevron(painter, rect, direction, doubled,
                enabled ? m_style->navIconColor() : m_style->navIconDisabledColor());
}

void AgDatePickerPanel::paintGridCell(QPainter &painter, const QRect &rect,
                                      const QString &text, const QColor &textColor,
                                      bool selected, bool hovered, bool enabled) const
{
    if (selected) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_style->cellSelectedBackgroundColor());
        painter.drawRoundedRect(QRectF(rect).adjusted(1.0, 1.0, -1.0, -1.0),
                                m_style->cellRadius(), m_style->cellRadius());
    } else if (hovered && enabled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_style->cellHoverBackgroundColor());
        painter.drawRoundedRect(QRectF(rect).adjusted(1.0, 1.0, -1.0, -1.0),
                                m_style->cellRadius(), m_style->cellRadius());
    }
    painter.setPen(selected ? m_style->cellSelectedForegroundColor() : textColor);
    painter.drawText(rect, Qt::AlignCenter, text);
}

void AgDatePickerPanel::paintFocusRing(QPainter &painter, const QRect &rect, bool onFilled) const
{
    // 实底格（选中 / 范围端点）上用白圈，普通格上用主题色圈，保证任意底色可见
    QPen pen(onFilled ? m_style->cellSelectedForegroundColor()
                      : m_style->cellSelectedBackgroundColor(),
             1.6);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    if (onFilled) {
        // 实底格内侧描一圈（留在色块内）
        painter.drawRoundedRect(QRectF(rect).adjusted(1.8, 1.8, -1.8, -1.8),
                                qMax<qreal>(0.0, m_style->cellRadius() - 1.0),
                                qMax<qreal>(0.0, m_style->cellRadius() - 1.0));
    } else {
        // 普通格外侧扩一圈
        painter.drawRoundedRect(QRectF(rect).adjusted(-0.8, -0.8, 0.8, 0.8),
                                m_style->cellRadius() + 1.0, m_style->cellRadius() + 1.0);
    }
}

void AgDatePickerPanel::paintDayView(QPainter &painter)
{
    const AgDatePickerStyle *s = m_style;

    // head：<< < | xxxx年xx月（分段可点击）| > >>
    const int navDirs[4] = { ArrowLeft, ArrowLeft, ArrowRight, ArrowRight };
    const bool navDoubled[4] = { true, false, false, true };
    for (int i = 0; i < 4; ++i) {
        if (!(m_navMask & (1 << i)))
            continue;
        paintHeadNavButton(painter, navButtonRect(i), navDirs[i], navDoubled[i],
                           m_hoverKind == HoverNav && m_hoverIndex == i,
                           navButtonEnabled(i));
    }
    QRect yearRect, monthRect;
    titleRects(&yearRect, &monthRect);
    const bool titleHover = m_titleNavEnabled;
    painter.setPen(titleHover && m_hoverKind == HoverTitleYear ? s->headTitleHoverColor()
                                                               : s->headTitleColor());
    painter.drawText(yearRect, Qt::AlignCenter,
                     QString::number(m_year) + QStringLiteral(u"年"));
    painter.setPen(titleHover && m_hoverKind == HoverTitleMonth ? s->headTitleHoverColor()
                                                                : s->headTitleColor());
    painter.drawText(monthRect, Qt::AlignCenter,
                     QString::number(m_month) + QStringLiteral(u"月"));

    // 星期行（周一 ~ 周日）
    painter.setPen(s->weekdayForegroundColor());
    for (int col = 0; col < 7; ++col) {
        const QRect cell = dayCellRect(0, col);
        const QRect rect(cell.left(), weekdayRowRect().top(), cell.width(),
                         weekdayRowRect().height());
        painter.drawText(rect, Qt::AlignCenter, weekdayLabel(col));
    }

    // 范围带：起点 ~ 预览（或已提交）终点之间（不含端点）的淡色背景，逐行整段绘制
    QDate bandFrom, bandTo;
    if (m_rangeMode && m_rangeStart.isValid()) {
        const QDate ref = m_rangePreviewEnd.isValid() ? m_rangePreviewEnd : m_rangeEnd;
        if (ref.isValid()) {
            bandFrom = qMin(m_rangeStart, ref);
            bandTo = qMax(m_rangeStart, ref);
        }
    }
    if (bandFrom.isValid() && bandTo > bandFrom) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(s->cellInRangeBackgroundColor());
        for (int row = 0; row < 6; ++row) {
            int leftCol = -1;
            int rightCol = -1;
            for (int col = 0; col < 7; ++col) {
                const QDate date = dayAtCell(row, col);
                if (date > bandFrom && date < bandTo) {
                    if (leftCol < 0)
                        leftCol = col;
                    rightCol = col;
                }
            }
            if (leftCol < 0)
                continue;
            const QRect first = dayCellRect(row, leftCol);
            const QRect last = dayCellRect(row, rightCol);
            painter.drawRect(QRect(first.left(), first.top(),
                                   last.right() - first.left(), first.height()));
        }
    }

    // 6 周日期（前后月补齐灰显、可选）
    const QDate today = QDate::currentDate();
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 7; ++col) {
            const QDate date = dayAtCell(row, col);
            const int index = row * 7 + col;
            const bool selectable = isDayCellSelectable(date);
            const bool selected = !m_rangeMode && m_selected.isValid() && date == m_selected;
            const bool isStart = m_rangeMode && m_rangeStart.isValid() && date == m_rangeStart;
            const bool isEnd = m_rangeMode && m_rangeEnd.isValid() && date == m_rangeEnd;
            const bool isPreviewEnd = m_rangeMode && m_rangePreviewEnd.isValid()
                                      && m_rangePreviewEnd != m_rangeStart
                                      && date == m_rangePreviewEnd;
            QColor textColor = s->cellForegroundColor();
            if (!selectable)
                textColor = s->cellDisabledForegroundColor();
            else if (isStart || isEnd)
                textColor = s->cellSelectedForegroundColor();
            else if (date == today)
                textColor = s->todayForegroundColor();
            else if (date.month() != m_month || date.year() != m_year)
                textColor = s->cellOtherMonthForegroundColor();

            const QRect cell = dayCellRect(row, col);
            if (isStart || isEnd) {
                // 范围端点：主题色实底圆角块
                painter.setPen(Qt::NoPen);
                painter.setBrush(s->cellSelectedBackgroundColor());
                painter.drawRoundedRect(QRectF(cell).adjusted(1.0, 1.0, -1.0, -1.0),
                                        s->cellRadius(), s->cellRadius());
            } else if (isPreviewEnd) {
                // 预览终点：淡色圆角块（跟随范围带同色）
                painter.setPen(Qt::NoPen);
                painter.setBrush(s->cellInRangeBackgroundColor());
                painter.drawRoundedRect(QRectF(cell).adjusted(0.5, 0.5, -0.5, -0.5),
                                        s->cellRadius(), s->cellRadius());
            } else {
                paintGridCell(painter, cell, QString::number(date.day()), textColor,
                              selected, m_hoverKind == HoverDayCell && m_hoverIndex == index,
                              selectable);
                if (m_focusDate.isValid() && date == m_focusDate)
                    paintFocusRing(painter, cell, selected);
                continue;
            }
            painter.setPen(textColor);
            painter.drawText(cell, Qt::AlignCenter, QString::number(date.day()));
            if (m_focusDate.isValid() && date == m_focusDate)
                paintFocusRing(painter, cell, true);
        }
    }
}

void AgDatePickerPanel::paintMonthView(QPainter &painter)
{
    const AgDatePickerStyle *s = m_style;

    // head：左 "xxxx年"（可点击进入年份页）+ 右侧上下箭头切换年份
    const QRect yearRect = monthHeadYearRect();
    painter.setPen(m_hoverKind == HoverMonthHeadYear ? s->headTitleHoverColor()
                                                     : s->headTitleColor());
    painter.drawText(yearRect, Qt::AlignVCenter | Qt::AlignLeft,
                     QString::number(m_monthViewYear) + QStringLiteral(u"年"));
    paintHeadNavButton(painter, scrollButtonRect(true), ArrowUp, false,
                       m_hoverKind == HoverScrollUp, scrollUpEnabled());
    paintHeadNavButton(painter, scrollButtonRect(false), ArrowDown, false,
                       m_hoverKind == HoverScrollDown, scrollDownEnabled());

    // 4 列 x 3 行：12 个月
    for (int index = 0; index < 12; ++index) {
        const int month = index + 1;
        const bool selectable = isMonthSelectable(m_monthViewYear, month);
        const bool selected = m_selected.isValid() && m_selected.year() == m_monthViewYear
                              && m_selected.month() == month;
        const QRect cell = monthCellRect(index / 4, index % 4);
        paintGridCell(painter, cell,
                      QString::number(month) + QStringLiteral(u"月"),
                      selectable ? s->cellForegroundColor() : s->cellDisabledForegroundColor(),
                      selected, m_hoverKind == HoverMonthCell && m_hoverIndex == index,
                      selectable);
        if (m_focusIndex == index)
            paintFocusRing(painter, cell, selected);
    }
}

void AgDatePickerPanel::paintYearView(QPainter &painter)
{
    const AgDatePickerStyle *s = m_style;

    // head：左 当前页年份区间文本 + 右侧上下箭头翻页（每次一屏）
    const QFontMetrics fm(s->font());
    const QString rangeText = QStringLiteral("%1 ~ %2")
                                  .arg(QString::number(m_yearPageStart),
                                       QString::number(m_yearPageStart + 11));
    painter.setPen(s->headLabelColor());
    painter.drawText(QRect(m_style->panelHorizontalPadding() + 2, headRect().top(),
                           fm.horizontalAdvance(rangeText) + 8, headRect().height()),
                     Qt::AlignVCenter | Qt::AlignLeft, rangeText);
    paintHeadNavButton(painter, scrollButtonRect(true), ArrowUp, false,
                       m_hoverKind == HoverScrollUp, scrollUpEnabled());
    paintHeadNavButton(painter, scrollButtonRect(false), ArrowDown, false,
                       m_hoverKind == HoverScrollDown, scrollDownEnabled());

    // 当前页与相邻页按滚动位置平移绘制（无滚动条）
    const QRect body = bodyRect();
    painter.save();
    painter.setClipRect(body);
    const int page = int(qFloor(m_scrollPos));
    const int stride = body.height();
    paintYearPage(painter, page, qRound((page - m_scrollPos) * stride));
    paintYearPage(painter, page + 1, qRound((page + 1 - m_scrollPos) * stride));
    painter.restore();
}

void AgDatePickerPanel::paintYearPage(QPainter &painter, int page, int yOffset)
{
    if (page < 0 || page > 9999 / 12)
        return;
    for (int index = 0; index < 12; ++index) {
        const int year = page * 12 + index;
        if (year < 1 || year > 9999)
            continue;
        const bool selectable = isYearSelectable(year);
        const bool selected = m_selected.isValid() && m_selected.year() == year;
        const QRect cell = monthCellRect(index / 4, index % 4).translated(0, yOffset);
        paintGridCell(painter, cell,
                      QString::number(year),
                      selectable ? m_style->cellForegroundColor()
                                 : m_style->cellDisabledForegroundColor(),
                      selected, m_hoverKind == HoverYearCell && m_hoverIndex == index
                                    && page == pageOfSettled(),
                      selectable);
        if (page == pageOfSettled() && m_focusIndex == index)
            paintFocusRing(painter, cell, selected);
    }
}

void AgDatePickerPanel::paintFooter(QPainter &painter)
{
    if (!m_footerVisible)
        return;
    const QRect footer = footerRect();

    if (m_style->footerTopLineColor().isValid()) {
        painter.setPen(QPen(m_style->footerTopLineColor(), 1.0));
        painter.drawLine(QPointF(m_style->panelHorizontalPadding() / 2.0, footer.top() + 0.5),
                         QPointF(width() - m_style->panelHorizontalPadding() / 2.0,
                                 footer.top() + 0.5));
    }
    if (m_footerWidget)
        return; // 完全自定义 footer

    for (int slot = 0; slot < 3; ++slot) {
        const QRect rect = footerLinkRect(slot);
        if (rect.isNull())
            continue;
        const bool custom = m_footerLinks[slot].custom;
        const bool enabled = custom ? true : footerShortcutEnabled();
        const bool hovered = m_hoverKind == HoverFooterLink && m_hoverIndex == slot;
        QColor color = m_style->footerLinkColor();
        if (!enabled)
            color = m_style->footerLinkDisabledColor();
        else if (hovered)
            color = m_style->footerLinkHoverColor();
        painter.setPen(color);
        const QString text = custom ? m_footerLinks[slot].text
                                    : (slot == 1 ? footerShortcutText() : QString());
        painter.drawText(rect, Qt::AlignCenter, text);
    }
}

// ---------------------------------------------------------------------------
// 交互

void AgDatePickerPanel::showMonth(int year, int month)
{
    year = qBound(1, year, 9999);
    month = qBound(1, month, 12);
    if (year == m_year && month == m_month)
        return;
    m_year = year;
    m_month = month;
    emit monthShownChanged(m_year, m_month);
    update();
}

void AgDatePickerPanel::shiftMonth(int delta)
{
    const QDate base(m_year, m_month, 1);
    const QDate target = base.addMonths(delta);
    if (target.isValid())
        showMonth(target.year(), target.month());
}

void AgDatePickerPanel::shiftMonthViewYear(int delta)
{
    const int target = qBound(minYearBound(), m_monthViewYear + delta, maxYearBound());
    if (target == m_monthViewYear)
        return;
    m_monthViewYear = target;
    update();
}

void AgDatePickerPanel::updateHover(const QPoint &pos)
{
    int kind = HoverNone;
    int index = -1;
    bool enabled = false;

    if (m_footerVisible && !m_footerWidget && footerRect().contains(pos)) {
        for (int slot = 0; slot < 3; ++slot) {
            if (footerLinkRect(slot).contains(pos)) {
                kind = HoverFooterLink;
                index = slot;
                enabled = m_footerLinks[slot].custom ? true : footerShortcutEnabled();
                break;
            }
        }
    } else if (headRect().contains(pos)) {
        switch (m_view) {
        case DayView:
            for (int i = 0; i < 4; ++i) {
                if (!(m_navMask & (1 << i)))
                    continue;
                if (navButtonRect(i).contains(pos)) {
                    kind = HoverNav;
                    index = i;
                    enabled = navButtonEnabled(i);
                    break;
                }
            }
            if (kind == HoverNone && m_titleNavEnabled) {
                if (titleYearRect().contains(pos)) {
                    kind = HoverTitleYear;
                    enabled = true;
                } else if (titleMonthRect().contains(pos)) {
                    kind = HoverTitleMonth;
                    enabled = true;
                }
            }
            break;
        case MonthView:
            if (monthHeadYearRect().contains(pos)) {
                kind = HoverMonthHeadYear;
                enabled = monthHeadYearClickable();
            } else if (scrollButtonRect(true).contains(pos)) {
                kind = HoverScrollUp;
                enabled = scrollUpEnabled();
            } else if (scrollButtonRect(false).contains(pos)) {
                kind = HoverScrollDown;
                enabled = scrollDownEnabled();
            }
            break;
        case YearView:
            if (scrollButtonRect(true).contains(pos)) {
                kind = HoverScrollUp;
                enabled = scrollUpEnabled();
            } else if (scrollButtonRect(false).contains(pos)) {
                kind = HoverScrollDown;
                enabled = scrollDownEnabled();
            }
            break;
        }
    } else if (bodyRect().contains(pos)) {
        switch (m_view) {
        case DayView:
            for (int row = 0; row < 6 && kind == HoverNone; ++row) {
                for (int col = 0; col < 7; ++col) {
                    if (dayCellRect(row, col).contains(pos)) {
                        kind = HoverDayCell;
                        index = row * 7 + col;
                        enabled = isDayCellSelectable(dayAtCell(row, col));
                        break;
                    }
                }
            }
            break;
        case MonthView:
            for (int i = 0; i < 12; ++i) {
                if (monthCellRect(i / 4, i % 4).contains(pos)) {
                    kind = HoverMonthCell;
                    index = i;
                    enabled = isMonthSelectable(m_monthViewYear, i + 1);
                    break;
                }
            }
            break;
        case YearView:
            if (m_scrollAnim->state() != QAbstractAnimation::Running) {
                for (int i = 0; i < 12; ++i) {
                    if (monthCellRect(i / 4, i % 4).contains(pos)) {
                        kind = HoverYearCell;
                        index = i;
                        enabled = isYearSelectable(m_yearPageStart + i);
                        break;
                    }
                }
            }
            break;
        }
    }

    if (kind != m_hoverKind || index != m_hoverIndex || enabled != m_hoverExtra) {
        m_hoverKind = kind;
        m_hoverIndex = index;
        m_hoverExtra = enabled;
        update();
    }

    // 悬停日期变化通知（范围预览用；移出日期格时发无效 QDate）
    QDate hoverDate;
    if (m_hoverKind == HoverDayCell && m_hoverExtra)
        hoverDate = dayAtCell(m_hoverIndex / 7, m_hoverIndex % 7);
    if (hoverDate != m_hoveredDate) {
        m_hoveredDate = hoverDate;
        emit dateHovered(m_hoveredDate);
    }
}

void AgDatePickerPanel::activateAt(const QPoint &pos)
{
    // footer 快捷链接
    if (m_footerVisible && !m_footerWidget && footerRect().contains(pos)) {
        for (int slot = 0; slot < 3; ++slot) {
            if (!footerLinkRect(slot).contains(pos))
                continue;
            if (m_footerLinks[slot].custom) {
                if (m_footerLinks[slot].callback)
                    m_footerLinks[slot].callback();
            } else if (slot == 1 && footerShortcutEnabled()) {
                emit dateSelected(footerShortcutDate());
            }
            return;
        }
        return;
    }

    if (headRect().contains(pos)) {
        switch (m_view) {
        case DayView:
            for (int i = 0; i < 4; ++i) {
                if (!(m_navMask & (1 << i)))
                    continue;
                if (navButtonRect(i).contains(pos) && navButtonEnabled(i)) {
                    switch (i) {
                    case 0: shiftMonth(-12); break;
                    case 1: shiftMonth(-1); break;
                    case 2: shiftMonth(1); break;
                    default: shiftMonth(12); break;
                    }
                    return;
                }
            }
            if (m_titleNavEnabled && titleYearRect().contains(pos)) {
                // 进入年份页（直接定位到当前年所在页，不播动画）
                m_view = YearView;
                m_focusIndex = -1;
                jumpToYearPage(m_year / 12);
            } else if (m_titleNavEnabled && titleMonthRect().contains(pos)) {
                m_view = MonthView;
                m_focusIndex = -1;
                m_monthViewYear = m_year;
                update();
            }
            break;
        case MonthView:
            if (monthHeadYearRect().contains(pos)) {
                m_view = YearView;
                m_focusIndex = -1;
                jumpToYearPage(m_monthViewYear / 12);
                update();
            } else if (scrollButtonRect(true).contains(pos) && scrollUpEnabled()) {
                shiftMonthViewYear(-1);
            } else if (scrollButtonRect(false).contains(pos) && scrollDownEnabled()) {
                shiftMonthViewYear(1);
            }
            break;
        case YearView:
            if (scrollButtonRect(true).contains(pos) && scrollUpEnabled())
                scrollYearsBy(-1);
            else if (scrollButtonRect(false).contains(pos) && scrollDownEnabled())
                scrollYearsBy(1);
            break;
        }
        return;
    }

    if (!bodyRect().contains(pos))
        return;

    switch (m_view) {
    case DayView: {
        const QDate date = dateAt(pos);
        if (date.isValid())
            emit dateSelected(date);
        break;
    }
    case MonthView: {
        const int month = monthAt(pos);
        if (month >= 1)
            activateMonth(month);
        break;
    }
    case YearView: {
        const int year = yearAt(pos);
        if (year >= 1)
            activateYear(year);
        break;
    }
    }
}

void AgDatePickerPanel::activateMonth(int month)
{
    if (month < 1 || month > 12 || !isMonthSelectable(m_monthViewYear, month))
        return;
    if (m_mode == AgDatePicker::SelectMonth) {
        emit dateSelected(QDate(m_monthViewYear, month, 1));
        return;
    }
    const int year = m_monthViewYear;
    m_view = DayView;
    m_focusDate = QDate(year, month, 1);
    showMonth(year, month);
    update();
}

void AgDatePickerPanel::activateYear(int year)
{
    if (year < 1 || year > 9999 || !isYearSelectable(year))
        return;
    if (m_mode == AgDatePicker::SelectYear) {
        emit dateSelected(QDate(year, 1, 1));
        return;
    }
    if (m_mode == AgDatePicker::SelectMonth) {
        m_view = MonthView;
        m_monthViewYear = year;
        m_focusIndex = -1;
        update();
        return;
    }
    // 选日粒度：选定年份后直接回到对应年月的日历页（月份保持不变）
    m_year = qBound(1, year, 9999);
    m_view = DayView;
    update();
}

void AgDatePickerPanel::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    activateAt(event->pos());
}

void AgDatePickerPanel::mouseMoveEvent(QMouseEvent *event)
{
    updateHover(event->pos());
    QWidget::mouseMoveEvent(event);
}

void AgDatePickerPanel::leaveEvent(QEvent *event)
{
    if (m_hoverKind != HoverNone) {
        m_hoverKind = HoverNone;
        m_hoverIndex = -1;
        m_hoverExtra = false;
        update();
    }
    if (m_hoveredDate.isValid()) {
        m_hoveredDate = QDate();
        emit dateHovered(QDate());
    }
    QWidget::leaveEvent(event);
}

void AgDatePickerPanel::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() > 0 ? -1 : 1; // 上滚 = 更早
    switch (m_view) {
    case DayView: shiftMonth(steps); break;
    case MonthView: shiftMonthViewYear(steps); break;
    case YearView: scrollYearsBy(steps); break;
    }
    event->accept();
}
