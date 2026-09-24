#ifndef AGDATEPICKERPANEL_H
#define AGDATEPICKERPANEL_H

#include "AgDatePicker_global.h"
#include "AgDatePicker.h"

#include <QWidget>
#include <QDate>
#include <functional>

class QVariantAnimation;

/**
 * @brief AgDatePicker 的日历面板（弹层内容部件，也可独立内嵌使用）。
 *
 * 三个页面（View）：
 *  - DayView：head（"<<"/"<" + "xxxx年xx月" 分段可点击 + ">"/">>"）+ 星期行 +
 *    6 周日期（前后月补齐灰显、可点击）+ footer；
 *  - MonthView：4 列 x 3 行显示 12 个月，head 左侧年份（可点击进入 YearView）、
 *    右侧上下箭头 / 鼠标滚动切换年份（格子不变）；
 *  - YearView：4 列 x 3 行 / 页显示 12 个年份，head 左侧当前页年份区间文本、
 *    右侧上下箭头与鼠标滚轮翻页（每次一屏，带滚动动画、不显示滚动条）。
 *
 * 选择粒度（AgDatePicker::SelectionMode）决定初始页面与点击格子的最终语义：
 * SelectDay / SelectMonth 时 YearView 点击年份进入 MonthView、MonthView 点击月份
 * 进入（或选定）对应年月；SelectYear 时 YearView 点击年份即选定。
 *
 * 范围限制（min/max）：范围外日期 / 月份 / 年份灰显、无悬停效果、不可点击，
 * footer 内置快捷项目标不在范围内时同样禁用。年份页滚动范围亦收敛到可选年份。
 *
 * 键盘导航（焦点由 AgDatePicker 编辑框承载，方向键 / 回车经 navigateKey /
 * activateFocus 驱动）：日历页上下左右移动焦点日期（自动翻月），PgUp / PgDn
 * 切换月份；月份 / 年份页在九宫格内移动焦点；回车激活焦点格（等价点击）。
 *
 * 范围选择模式（setRangeSelectionMode，配合 AgDateRangePanel 双日历组合使用）：
 * 起点与终点之间的日期绘制淡色范围带（setRangeStart / setRangeEnd），选终点
 * 过程中可通过 setRangePreviewEnd 实时预览范围；单一选中高亮在该模式下关闭。
 */
class AGDATEPICKER_EXPORT AgDatePickerPanel : public QWidget
{
    Q_OBJECT

public:
    /// 面板当前页面。
    enum View {
        DayView,    ///< 日历（星期 + 日期）
        MonthView,  ///< 月份九宫格
        YearView    ///< 年份九宫格（可滚动）
    };

    explicit AgDatePickerPanel(QWidget *parent = nullptr);
    ~AgDatePickerPanel() override;

    // ---- 样式（非持有，通常为 AgDatePickerStyle::globalStyle() 或控件独立样式）----
    void setStyle(AgDatePickerStyle *style);
    AgDatePickerStyle *style() const;

    // ---- 选择粒度与当前值 ----
    void setSelectionMode(AgDatePicker::SelectionMode mode);
    AgDatePicker::SelectionMode selectionMode() const;
    void setSelectedDate(const QDate &date);
    QDate selectedDate() const;

    // ---- 可选范围（无效 QDate = 不限）----
    void setDateRange(const QDate &min, const QDate &max);
    QDate minimumDate() const;
    QDate maximumDate() const;
    bool isDateSelectable(const QDate &date) const;      ///< 日期粒度可选
    bool isMonthSelectable(int year, int month) const;   ///< 月份粒度可选（与范围有交集）
    bool isYearSelectable(int year) const;                ///< 年份粒度可选（与范围有交集）

    // ---- 键盘导航 ----
    QDate focusDate() const;                 ///< 日历页焦点日期（无效 = 无焦点）
    void setFocusDate(const QDate &date);    ///< 设置焦点（不在当前 42 格内时自动翻月）
    /// 方向键 / PgUp / PgDn 移动焦点；键被处理返回 true。日历页移动日期（左右 ±1 天、
    /// 上下 ±7 天、PgUp/PgDn ±1 月），月 / 年页移动九宫格焦点（跨年滚动）。
    bool navigateKey(int key);
    /// 激活当前焦点格（等价点击）：日历页发出 dateSelected，月 / 年页执行对应跳转。
    void activateFocus();

    // ---- 范围选择（供 AgDateRangePanel 双日历组合）----
    void setRangeSelectionMode(bool on);     ///< 范围模式：关闭单一选中高亮
    bool isRangeSelectionMode() const;
    void setRangeStart(const QDate &date);   ///< 范围起点（无效 = 无）
    void setRangeEnd(const QDate &date);     ///< 范围终点（无效 = 无）
    void setRangePreviewEnd(const QDate &date); ///< 选终点过程中的预览终点（悬停 / 键盘）
    /// head 年 / 月分段是否可点击进入九宫格页（双日历组合时关闭）。
    void setTitleNavigationEnabled(bool enabled);
    /// DayView 箭头显示掩码（bit0 "<<" bit1 "<" bit2 ">" bit3 ">>"，默认 0xF；
    /// 双日历组合时左右两侧只保留外侧箭头）。
    void setNavButtonsMask(int mask);

    // ---- footer ----
    void setFooterVisible(bool visible);
    bool isFooterVisible() const;
    /// 配置快捷链接槽位（slot 0=左 1=中 2=右）；custom=false 时中间槽位恢复内置
    /// "今天 / 当月 / 今年"。回调为空则仅展示。
    void setFooterLink(int slot, const QString &text,
                       const std::function<void()> &callback, bool custom);
    void resetFooterLink(int slot);
    /// 完全自定义 footer 部件（reparent 进面板，覆盖整个 footer 区）；nullptr 恢复内置。
    void setFooterWidget(QWidget *widget);
    QWidget *footerWidget() const;

    // ---- 页面状态 ----
    View view() const;
    int displayedYear() const;       ///< Day：当前年；Month：当前年；Year：当前页首年
    int displayedMonth() const;      ///< Day：当前月（1-12）；其余页面无意义返回 0
    /// 弹出时复位：按粒度回到初始页面，无选中值时定位到当前日期所在月。
    void resetView();
    /// 返回上一级视图（年 / 月选择页 → 日历页，或月粒度的年页 → 月页）；
    /// 已在初始页时返回 false（由调用方收起面板）。
    bool goBack();
    /// 年份页滚动翻页（带动画，自动收敛到可选范围；Day/Month 页面无效果）。
    void scrollYearsBy(int pages);
    /// DayView 定位到指定年月（自动钳制 QDate 范围；双日历组合联动翻月用）。
    void showMonth(int year, int month);

    QSize sizeHint() const override;

    // ---- 命中测试 / 几何辅助（自动化测试亦使用）----
    QDate dateAt(const QPoint &pos) const;          ///< 可选日期则返回，否则无效 QDate
    int monthAt(const QPoint &pos) const;           ///< 1-12，非可选返回 -1
    int yearAt(const QPoint &pos) const;            ///< 非可选返回 -1
    QRect dateCellRect(const QDate &date) const;    ///< 日期格（含前后月补齐），无效返回空
    QRect monthCellRect(int month) const;
    QRect yearCellRect(int year) const;
    QRect titleYearRect() const;                    ///< DayView head "xxxx年" 分段
    QRect titleMonthRect() const;                   ///< DayView head "xx月" 分段
    QRect footerLinkRect(int slot) const;

Q_SIGNALS:
    /// 最终确定选择（点击可选日期 / 月份 / 年份，或 footer 内置快捷项）。
    void dateSelected(const QDate &date);
    /// DayView 实际切换显示月之后发出（双日历组合据此联动翻月）。
    void monthShownChanged(int year, int month);
    /// 悬停可选日期变化（移出日期格 / 离开面板时为无效 QDate；范围预览用）。
    void dateHovered(const QDate &date);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct FooterLinkData {
        QString text;
        std::function<void()> callback;
        bool custom = false;
    };

    // 布局
    QRect headRect() const;
    QRect bodyRect() const;                  ///< head 与 footer 之间（月/年格子整体区域）
    QRect footerRect() const;                ///< footer 隐藏时为空矩形
    QRect weekdayRowRect() const;            ///< DayView 星期行
    QRect dayGridRect() const;               ///< DayView 6 周日期区
    QRect gridCellRect(const QRect &grid, int rows, int cols, int row, int col) const;
    QRect dayCellRect(int row, int col) const;
    QRect monthCellRect(int row, int col) const;   ///< row 0-2，col 0-3
    QRect yearCellRect(int row, int col) const;   ///< 相对当前滚动页

    // DayView head
    QRect navButtonRect(int index) const;    ///< 0="<<" 1="<" 2=">" 3=">>"
    void titleRects(QRect *yearRect, QRect *monthRect) const;
    // Month/YearView head 右侧上下箭头（up=true 上箭头）
    QRect scrollButtonRect(bool up) const;
    QRect monthHeadYearRect() const;         ///< MonthView head 年份文本（可点击）

    // 年份页滚动
    int yearPageCount() const;               ///< 每页年数（12）
    int minYearBound() const;                ///< 可滚动年份下界（范围或当前年 -100）
    int maxYearBound() const;                ///< 可滚动年份上界（范围或当前年 +100）
    int minPageIndex() const;
    int maxPageIndex() const;
    int pageOfSettled() const;               ///< 当前静止页索引
    void startYearScrollTo(int page);        ///< 动画滚动到目标页
    void jumpToYearPage(int page);           ///< 直接定位到目标页（无动画）

    // 日期数据
    QDate dayGridStart() const;              ///< 42 格首日（周一开头）
    QDate dayAtCell(int row, int col) const;

    // 状态判定
    bool isDayCellSelectable(const QDate &date) const;   ///< 范围内（含前后月补齐日）
    bool navButtonEnabled(int index) const;  ///< DayView 箭头（QDate 边界内即可用）
    bool scrollUpEnabled() const;            ///< Month/YearView 上箭头
    bool scrollDownEnabled() const;
    bool monthHeadYearClickable() const;     ///< MonthView 年份文本是否可进年份页

    // footer 内置快捷项
    QString footerShortcutText() const;      ///< "今天" / "当月" / "今年"
    QDate footerShortcutDate() const;        ///< 今天 / 当月 / 今年
    bool footerShortcutEnabled() const;

    // 绘制
    void drawChevron(QPainter &painter, const QRect &rect, int direction,
                     bool doubled, const QColor &color) const;
    void paintDayView(QPainter &painter);
    void paintMonthView(QPainter &painter);
    void paintYearView(QPainter &painter);
    void paintYearPage(QPainter &painter, int page, int yOffset);
    void paintGridCell(QPainter &painter, const QRect &rect, const QString &text,
                       const QColor &textColor, bool selected, bool hovered,
                       bool enabled) const;
    /// 在格子外围绘制键盘焦点圈（onFilled = 实底格上改用对比白圈）。
    void paintFocusRing(QPainter &painter, const QRect &rect, bool onFilled) const;
    void paintHeadNavButton(QPainter &painter, const QRect &rect, int direction,
                            bool doubled, bool hovered, bool enabled) const;
    void paintFooter(QPainter &painter);

    // 交互
    void updateHover(const QPoint &pos);
    void activateAt(const QPoint &pos);
    void activateMonth(int month);             ///< 月份页格子激活（选定或回日历页）
    void activateYear(int year);               ///< 年份页格子激活（选定或逐级回退）
    void shiftMonth(int delta);
    void shiftMonthViewYear(int delta);

    AgDatePickerStyle *m_style;

    AgDatePicker::SelectionMode m_mode;
    View m_view;

    QDate m_selected;
    QDate m_min;
    QDate m_max;

    // 键盘焦点：日历页为焦点日期，月 / 年页为九宫格索引（-1 = 无）
    QDate m_focusDate;
    int m_focusIndex;

    // 范围选择状态（m_rangeMode 时生效）
    bool m_rangeMode;
    QDate m_rangeStart;
    QDate m_rangeEnd;
    QDate m_rangePreviewEnd;
    QDate m_hoveredDate;                       ///< 上次发出 dateHovered 的日期

    int m_navMask;                             ///< DayView 箭头显示掩码
    bool m_titleNavEnabled;                    ///< head 年 / 月分段可点击

    int m_year;          ///< DayView 当前显示年
    int m_month;         ///< DayView 当前显示月（1-12）
    int m_monthViewYear; ///< MonthView 当前年
    int m_yearPageStart; ///< YearView 静止页首年（12 的倍数）
    qreal m_scrollPos;   ///< YearView 可视滚动位置（页为单位）
    QVariantAnimation *m_scrollAnim;

    bool m_footerVisible;
    FooterLinkData m_footerLinks[3];
    QWidget *m_footerWidget;

    // 悬停状态：kind + 相关索引（格子行 * 列 / footer 槽位等）
    int m_hoverKind;
    int m_hoverIndex;
    bool m_hoverExtra;   ///< 箭头按钮等：是否处于可交互（启用）状态
};

#endif // AGDATEPICKERPANEL_H
