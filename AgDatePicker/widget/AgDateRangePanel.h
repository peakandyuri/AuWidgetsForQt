#ifndef AGDATERANGEPANEL_H
#define AGDATERANGEPANEL_H

#include "AgDatePicker_global.h"
#include "AgDatePickerStyle.h"

#include <QWidget>
#include <QDate>

class AgDatePickerPanel;

/**
 * @brief AgDatePicker 日期范围选择面板（弹层内容部件）：左右两个日历并排，
 * 右侧比左侧晚一个月，任一侧翻月 / 滚轮时另一侧自动联动，始终保持一个月差。
 *
 * 交互流程（状态机由 AgDatePicker 驱动）：
 *  - 弹出时展示已提交范围（起点 / 终点实底端点 + 中间淡色范围带）；
 *  - 第一次点击 / 回车选中起点（beginPicking），此后悬停或键盘移动焦点时实时
 *    预览起点到该日期的范围（setPreviewEnd）；起点之前 / 之后的日期均可选，
 *    选了之前的日期时由上层自动交换（第一次选择变为结束日期）；
 *  - 第二次点击 / 回车确定终点（上层提交并收起面板）。
 *
 * 面板内部：两个 AgDatePickerPanel 均为范围模式（单一选中高亮关闭）、隐藏
 * footer、关闭 head 年 / 月九宫格跳转；左侧只显示 "<<" "<" 箭头，右侧只显示
 * ">" ">>" 箭头。键盘焦点（方向键移动、回车激活）由持有焦点日期的一侧承载。
 */
class AGDATEPICKER_EXPORT AgDateRangePanel : public QWidget
{
    Q_OBJECT

public:
    explicit AgDateRangePanel(QWidget *parent = nullptr);
    ~AgDateRangePanel() override;

    // ---- 样式（非持有，转发给两个子日历）----
    void setStyle(AgDatePickerStyle *style);

    // ---- 已提交范围（无效 QDate = 无）----
    void setSelectedRange(const QDate &start, const QDate &end);

    // ---- 可选范围限制（无效 QDate = 不限，转发给两个子日历）----
    void setDateRange(const QDate &min, const QDate &max);

    // ---- 弹出 / 选点状态 ----
    /// 弹出复位：定位到起点（无值时当前月），展示已提交范围。
    void resetView();
    /// 第一次选中起点：进入选终点状态（清除旧范围带，仅显示起点端点）。
    void beginPicking(const QDate &start);
    /// 退出选终点状态：恢复展示已提交范围（Esc 取消 / 外部收起时调用）。
    void endPicking();
    /// 选终点过程中的预览终点（悬停日期或键盘焦点日期；无效 QDate = 无预览）。
    void setPreviewEnd(const QDate &date);

    // ---- 键盘（委托给持有焦点日期的一侧日历）----
    QDate focusDate() const;
    void setFocusDate(const QDate &date);
    bool navigateKey(int key);
    void activateFocus();

    QSize sizeHint() const override;

Q_SIGNALS:
    /// 点击 / 回车选中日期（第一次 = 起点，第二次 = 终点，由上层状态机解释）。
    void dateSelected(const QDate &date);
    /// 悬停可选日期变化（无效 QDate = 移出；选终点时用于范围预览）。
    void dateHovered(const QDate &date);

private:
    AgDatePickerPanel *focusPanel() const;      ///< 焦点日期所在（或应属）的一侧
    void syncPeer(AgDatePickerPanel *sender);   ///< 保持右侧 = 左侧 + 1 月

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    AgDatePickerStyle *m_style;
    AgDatePickerPanel *m_left;
    AgDatePickerPanel *m_right;
    AgDatePickerPanel *m_focusOwner;        ///< 当前持有键盘焦点日期的一侧

    QDate m_committedStart;                 ///< 已提交范围（endPicking 恢复用）
    QDate m_committedEnd;
    bool m_picking;
};

#endif // AGDATERANGEPANEL_H
