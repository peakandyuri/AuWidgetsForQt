#ifndef AUTIMEPANEL_H
#define AUTIMEPANEL_H

#include "AuDatePicker_global.h"
#include "AuDatePickerStyle.h"

#include <QWidget>
#include <QTime>

class QVariantAnimation;

/**
 * @brief 时 / 分 / 秒列滚动选择面板（AuDatePicker 弹层时间部分的内嵌部件，也可独立内嵌使用）。
 *
 * 各列以列表形式展示（列头 "时 / 分 / 秒"），鼠标滚轮在对应列上滚动（带动画），
 * 点击条目选定；当前选中条目以主题色背景高亮，窗口外条目灰显、无悬停、不可点击。
 *
 * 级联限制规则（窗口为"每日生效"的时刻区间 [minTime, maxTime]，不支持跨午夜）：
 *  - 某小时可选 ⇔ 该小时内存在窗口内的合法时间；
 *  - 某分钟可选 ⇔ 当前小时下，该分钟内存在窗口内的合法时间；
 *  - 某秒可选 ⇔ (时, 分, 秒) 本身在窗口内。
 * 因此限制大于 12:30 时：0 ~ 11 时整体不可选，选中 12 时后 0 ~ 29 分不可选，
 * 选中 13 时后全部分钟可选。点击使更细字段非法时自动钳位到该列范围内最近的合法值。
 *
 * 无滚动条：条目区随滚轮偏移（QVariantAnimation 动画，收敛到两端），
 * 打开 / 设值时自动滚动使选中条目居中。
 *
 * min / max 传无效 QTime 表示不限；两者同时有效且 min > max 时自动交换。
 * setSelectedTime() 允许设置窗口外的值（用于展示"非法预设"：条目照常定位高亮，
 * 但确定按钮由上层按 isTimeValid() 置灰），传无效 QTime 清除选中高亮。
 */
class AUDATEPICKER_EXPORT AuTimePanel : public QWidget
{
    Q_OBJECT

public:
    /// 显示粒度（列数）。NoTime 表示不显示时间列。
    enum Granularity {
        NoTime = 0,             ///< 不显示时间列
        HoursOnly,              ///< 仅时
        HoursMinutes,           ///< 时 + 分
        HoursMinutesSeconds     ///< 时 + 分 + 秒
    };

    explicit AuTimePanel(QWidget *parent = nullptr);
    ~AuTimePanel() override;

    // ---- 样式（非持有，通常为 AuDatePickerStyle::globalStyle() 或控件独立样式）----
    void setStyle(AuDatePickerStyle *style);
    AuDatePickerStyle *style() const;

    // ---- 粒度与当前值 ----
    void setGranularity(Granularity granularity);
    Granularity granularity() const;
    void setSelectedTime(const QTime &time);   ///< 程序设置：定位并高亮（可为窗口外值）
    QTime selectedTime() const;

    // ---- 可选窗口（无效 QTime = 不限）----
    void setTimeRange(const QTime &min, const QTime &max);
    QTime minimumTime() const;
    QTime maximumTime() const;
    bool isTimeValid(const QTime &time) const;              ///< 时刻是否落在窗口内
    /// 指定条目当前是否可选（级联判定；column: 0 时 / 1 分 / 2 秒）。
    bool isItemSelectable(int column, int value) const;

    // ---- 命中测试 / 几何辅助（自动化测试亦使用）----
    QTime timeAt(const QPoint &pos) const;                  ///< 命中条目返回其时间（未命中返回无效 QTime）
    QRect timeItemRect(int column, int value) const;        ///< 条目矩形（不可见 / 不存在返回空）

    QSize sizeHint() const override;

Q_SIGNALS:
    /// 用户点击条目选定时间（含跨列钳位后的最终值）。
    void timeChanged(const QTime &time);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    // 数据
    int columnCount() const;                     ///< 当前粒度下的列数（0-3）
    int itemCount(int column) const;             ///< 列条目数（24 / 60 / 60）
    QString columnHeader(int column) const;      ///< "时" / "分" / "秒"
    int selectedValue(int column) const;         ///< 当前选中在该列的值（未选中返回 -1）

    // 布局
    int headerHeight() const;                    ///< 列头行高（与日历星期行一致）
    QRect headerRect() const;
    QRect listRect() const;                      ///< 条目滚动区
    int columnsWidth() const;                    ///< 全部列总宽（含列间距）
    int columnsLeft() const;                     ///< 首列 x（整体水平居中）
    QRect columnRect(int column) const;          ///< 列条带（含列头与条目区全高）
    QRectF itemRect(int column, int value) const; ///< 条目绘制矩形（含滚动偏移，浮点平滑动画）

    // 滚动
    int maxScrollOffset(int column) const;       ///< 最大像素偏移（收敛用）
    void scrollToItem(int column, int value, bool animated); ///< 将条目滚入可视区（尽量居中）
    void startScrollTo(int column, qreal target);
    void clampOffsets();

    // 选择 / 悬停
    int columnAtX(int x) const;                  ///< x 就近映射到列（条带外收敛到首 / 末列）
    bool itemAt(const QPoint &pos, int *column, int *value) const;
    void selectAt(const QPoint &pos);
    /// 保持该列及更粗字段不变，将候选时间钳位到窗口内的最近合法值（无可行值返回无效 QTime）。
    QTime clampWithinColumn(int column, const QTime &candidate) const;
    void updateHover(const QPoint &pos);

    AuDatePickerStyle *m_style;
    Granularity m_granularity;

    QTime m_selected;
    QTime m_min;
    QTime m_max;

    qreal m_offsets[3];                          ///< 各列滚动像素偏移（向下增大）
    QVariantAnimation *m_scrollAnims[3];
    int m_hoverColumn;                           ///< 悬停列（-1 无）
    int m_hoverValue;                            ///< 悬停条目值（-1 无）
};

#endif // AUTIMEPANEL_H
