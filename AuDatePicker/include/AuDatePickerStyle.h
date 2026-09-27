#ifndef AUDATEPICKERSTYLE_H
#define AUDATEPICKERSTYLE_H

#include "AuDatePicker_global.h"

#include <QColor>
#include <QFont>
#include <QVector>

class AuDatePicker;

/**
 * @brief AuDatePicker 样式：编辑框、日历图标 / 清除图标、弹层面板、面板头部、
 * 星期行、日期 / 月份 / 年份格子、footer 快捷链接的全部视觉属性。
 *
 * 三种使用方式（与 AuSelectStyle 一致）：
 *  1. 全局样式：AuDatePickerStyle::setGlobalStyle() 设置后，所有未显式 setStyle()
 *     的 AuDatePicker 自动使用，且全局样式变化时已注册控件自动刷新；
 *  2. 控件独立样式：AuDatePicker::setStyle(new AuDatePickerStyle())（控件接管所有权），
 *     随后可对实例逐项微调，例如 picker->style()->setEditorHeight(40);
 *  3. 内置主题工厂：createLightStyle() / createDarkStyle()，配合 setSizeMode()
 *     在小 / 中 / 大三种尺寸预设间切换（重置高度、格子尺寸、间距、圆角等尺寸类
 *     字段，颜色保持）。
 *
 * 颜色字段为无效 QColor 时表示不绘制对应元素（如关闭 footer 顶部分隔线）。
 */
class AUDATEPICKER_EXPORT AuDatePickerStyle
{
public:
    /// 尺寸档位：小 / 中 / 大，决定编辑框高度、日历格子尺寸、面板高度等尺寸类预设值。
    enum class SizeMode {
        Small,   ///< 紧凑
        Medium,  ///< 适中（默认）
        Large    ///< 宽松
    };

    AuDatePickerStyle();
    virtual ~AuDatePickerStyle();

    // 内置主题工厂（所有权归调用者）
    static AuDatePickerStyle *createLightStyle(SizeMode mode = SizeMode::Medium);
    static AuDatePickerStyle *createDarkStyle(SizeMode mode = SizeMode::Medium);

    // 全局样式：未显式设置样式的 AuDatePicker 自动使用
    static AuDatePickerStyle *globalStyle();
    static void setGlobalStyle(AuDatePickerStyle *style);   // 所有权归调用者

    /// 当前尺寸档位。
    SizeMode sizeMode() const;
    /// 切换尺寸档位：编辑框高度与字体、日历格子、面板头部 / footer 高度等尺寸类
    /// 字段重置为该档位预设（颜色保持不变）。字体档位：小 12px / 中 14px / 大 16px。
    void setSizeMode(SizeMode mode);

    /// 编辑框与面板共用的字体（随尺寸档位重置为预设字号，可手动覆盖）。
    QFont font() const;
    void setFont(const QFont &font);

    // ---- 编辑框 ----
    int editorHeight() const;                           ///< <=0 表示跟随尺寸档位默认值
    void setEditorHeight(int height);
    QColor editorBackgroundColor() const;
    void setEditorBackgroundColor(const QColor &color);
    QColor editorForegroundColor() const;
    void setEditorForegroundColor(const QColor &color);
    QColor editorPlaceholderColor() const;
    void setEditorPlaceholderColor(const QColor &color);
    QColor editorBorderColor() const;
    void setEditorBorderColor(const QColor &color);
    QColor editorHoverBorderColor() const;
    void setEditorHoverBorderColor(const QColor &color);
    QColor editorActiveBorderColor() const;             ///< 获得焦点 / 面板展开时
    void setActiveBorderColor(const QColor &color);
    QColor editorFocusBorderColor() const;              ///< 键盘焦点（Tab 切换）边框色
    void setEditorFocusBorderColor(const QColor &color);
    QColor editorFocusBackgroundColor() const;          ///< 键盘焦点内部淡色底（无效色 = 不绘制）
    void setEditorFocusBackgroundColor(const QColor &color);
    QColor editorDisabledBackgroundColor() const;
    void setEditorDisabledBackgroundColor(const QColor &color);
    QColor editorDisabledForegroundColor() const;
    void setEditorDisabledForegroundColor(const QColor &color);
    QColor editorDisabledBorderColor() const;
    void setEditorDisabledBorderColor(const QColor &color);
    int editorBorderRadius() const;
    void setEditorBorderRadius(int radius);
    int editorBorderWidth() const;
    void setEditorBorderWidth(int width);
    int editorHorizontalPadding() const;
    void setEditorHorizontalPadding(int padding);
    int iconSlotWidth() const;                          ///< 编辑框右侧图标槽位宽度
    void setIconSlotWidth(int width);

    // ---- 日历 / 清除图标 ----
    QColor calendarIconColor() const;                   ///< 编辑框右侧日历图标颜色
    void setCalendarIconColor(const QColor &color);
    QColor clearIconCircleColor() const;                ///< 清除图标实心圆底色
    void setClearIconCircleColor(const QColor &color);
    QColor clearIconCircleHoverColor() const;           ///< 悬停清除图标实心圆底色
    void setClearIconCircleHoverColor(const QColor &color);
    QColor clearIconCrossColor() const;                 ///< 清除图标 × 颜色
    void setClearIconCrossColor(const QColor &color);

    // ---- 弹层面板 ----
    QColor panelBackgroundColor() const;
    void setPanelBackgroundColor(const QColor &color);
    QColor panelBorderColor() const;                    ///< 无效 QColor = 不绘制边框
    void setPanelBorderColor(const QColor &color);
    int panelBorderRadius() const;
    void setPanelBorderRadius(int radius);
    QColor panelShadowColor() const;
    void setPanelShadowColor(const QColor &color);
    int panelShadowRadius() const;
    void setPanelShadowRadius(int radius);
    int panelHorizontalPadding() const;                 ///< 面板内容左右内边距
    void setPanelHorizontalPadding(int padding);

    // ---- 面板头部（年月导航行）----
    int headHeight() const;                             ///< 头部行高
    void setHeadHeight(int height);
    int navButtonSize() const;                          ///< 头部图标按钮边长
    void setNavButtonSize(int size);
    QColor navIconColor() const;                        ///< 单 / 双箭头、上下箭头图标颜色
    void setNavIconColor(const QColor &color);
    QColor navIconDisabledColor() const;                ///< 图标按钮禁用（到达滚动边界）
    void setNavIconDisabledColor(const QColor &color);
    QColor navHoverBackgroundColor() const;             ///< 图标按钮悬停背景（无效色 = 无悬停）
    void setNavHoverBackgroundColor(const QColor &color);
    QColor headTitleColor() const;                      ///< "xxxx年xx月" 标题颜色
    void setHeadTitleColor(const QColor &color);
    QColor headTitleHoverColor() const;                 ///< 标题年份 / 月份分段悬停颜色
    void setHeadTitleHoverColor(const QColor &color);
    QColor headLabelColor() const;                      ///< 年份页头部 "2020 ~ 2031" 文本颜色
    void setHeadLabelColor(const QColor &color);

    // ---- 星期行 ----
    int weekdayRowHeight() const;
    void setWeekdayRowHeight(int height);
    QColor weekdayForegroundColor() const;
    void setWeekdayForegroundColor(const QColor &color);

    // ---- 日期 / 月份 / 年份格子 ----
    int dayCellWidth() const;
    void setDayCellWidth(int width);
    int dayCellHeight() const;
    void setDayCellHeight(int height);
    int cellSpacing() const;                            ///< 格子之间的间距
    void setCellSpacing(int spacing);
    int cellRadius() const;                             ///< 格子悬停 / 选中背景圆角
    void setCellRadius(int radius);
    QColor cellForegroundColor() const;                 ///< 当月日期 / 月份 / 年份普通文本
    void setCellForegroundColor(const QColor &color);
    QColor cellOtherMonthForegroundColor() const;       ///< 前后月补齐日期（灰色）
    void setCellOtherMonthForegroundColor(const QColor &color);
    QColor cellDisabledForegroundColor() const;         ///< 范围外禁用文本
    void setCellDisabledForegroundColor(const QColor &color);
    QColor cellHoverBackgroundColor() const;            ///< 可选格子悬停背景
    void setCellHoverBackgroundColor(const QColor &color);
    QColor cellSelectedBackgroundColor() const;         ///< 选中格子背景
    void setCellSelectedBackgroundColor(const QColor &color);
    QColor cellSelectedForegroundColor() const;         ///< 选中格子上文字颜色
    void setCellSelectedForegroundColor(const QColor &color);
    QColor todayForegroundColor() const;                ///< "今天" 日期文本强调色
    void setTodayForegroundColor(const QColor &color);
    QColor cellInRangeBackgroundColor() const;          ///< 范围选择中起止之间格子的淡色背景
    void setCellInRangeBackgroundColor(const QColor &color);

    // ---- footer ----
    int footerHeight() const;
    void setFooterHeight(int height);
    QColor footerTopLineColor() const;                  ///< footer 顶部分隔线（无效色 = 不绘制）
    void setFooterTopLineColor(const QColor &color);
    QColor footerLinkColor() const;                     ///< 快捷链接文本颜色
    void setFooterLinkColor(const QColor &color);
    QColor footerLinkHoverColor() const;
    void setFooterLinkHoverColor(const QColor &color);
    QColor footerLinkDisabledColor() const;             ///< 快捷目标不在可选范围时
    void setFooterLinkDisabledColor(const QColor &color);

private:
    friend class AuDatePicker;
    static void registerGlobalObserver(AuDatePicker *widget);
    static void unregisterGlobalObserver(AuDatePicker *widget);
    static void notifyGlobalStyleChanged();

    void applySizeModePreset(SizeMode mode);            ///< 将尺寸类字段重置为档位预设

    SizeMode m_sizeMode;
    QFont m_font;

    // 编辑框
    int m_editorHeight;
    QColor m_editorBackgroundColor;
    QColor m_editorForegroundColor;
    QColor m_editorPlaceholderColor;
    QColor m_editorBorderColor;
    QColor m_editorHoverBorderColor;
    QColor m_editorActiveBorderColor;
    QColor m_editorFocusBorderColor;
    QColor m_editorFocusBackgroundColor;
    QColor m_editorDisabledBackgroundColor;
    QColor m_editorDisabledForegroundColor;
    QColor m_editorDisabledBorderColor;
    int m_editorBorderRadius;
    int m_editorBorderWidth;
    int m_editorHorizontalPadding;
    int m_iconSlotWidth;

    // 日历 / 清除图标
    QColor m_calendarIconColor;
    QColor m_clearIconCircleColor;
    QColor m_clearIconCircleHoverColor;
    QColor m_clearIconCrossColor;

    // 面板
    QColor m_panelBackgroundColor;
    QColor m_panelBorderColor;
    int m_panelBorderRadius;
    QColor m_panelShadowColor;
    int m_panelShadowRadius;
    int m_panelHorizontalPadding;

    // 面板头部
    int m_headHeight;
    int m_navButtonSize;
    QColor m_navIconColor;
    QColor m_navIconDisabledColor;
    QColor m_navHoverBackgroundColor;
    QColor m_headTitleColor;
    QColor m_headTitleHoverColor;
    QColor m_headLabelColor;

    // 星期行
    int m_weekdayRowHeight;
    QColor m_weekdayForegroundColor;

    // 格子
    int m_dayCellWidth;
    int m_dayCellHeight;
    int m_cellSpacing;
    int m_cellRadius;
    QColor m_cellForegroundColor;
    QColor m_cellOtherMonthForegroundColor;
    QColor m_cellDisabledForegroundColor;
    QColor m_cellHoverBackgroundColor;
    QColor m_cellSelectedBackgroundColor;
    QColor m_cellSelectedForegroundColor;
    QColor m_todayForegroundColor;
    QColor m_cellInRangeBackgroundColor;

    // footer
    int m_footerHeight;
    QColor m_footerTopLineColor;
    QColor m_footerLinkColor;
    QColor m_footerLinkHoverColor;
    QColor m_footerLinkDisabledColor;

    static AuDatePickerStyle *s_globalStyle;
    static QVector<AuDatePicker *> s_observers;
};

#endif // AUDATEPICKERSTYLE_H
