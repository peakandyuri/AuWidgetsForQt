#ifndef AUSELECTSTYLE_H
#define AUSELECTSTYLE_H

#include "AuSelect_global.h"

#include <QColor>
#include <QFont>
#include <QVector>

class AuSelect;

/**
 * @brief AuSelect 样式：编辑框、下拉面板、下拉项、标签块（多选 chips）、箭头的全部视觉属性。
 *
 * 三种使用方式：
 *  1. 全局样式：AuSelectStyle::setGlobalStyle() 设置后，所有未显式 setStyle() 的
 *     AuSelect 自动使用，且全局样式变化时已注册控件自动刷新；
 *  2. 控件独立样式：AuSelect::setStyle(new AuSelectStyle())（控件接管所有权），
 *     随后可对实例逐项微调，例如 select->style()->setEditorBorderRadius(2);
 *  3. 内置主题工厂：createLightStyle() / createDarkStyle()，配合 setSizeMode()
 *     在小 / 中 / 大三种尺寸预设间切换（重置高度、间距、圆角等尺寸类字段，颜色保持）。
 *
 * 颜色字段为无效 QColor 时表示不绘制对应元素（如关闭 chip 边框、关闭命中高亮）。
 */
class AUSELECT_EXPORT AuSelectStyle
{
public:
    /// 尺寸档位：小 / 中 / 大，决定编辑框高度、选项高度、间距等尺寸类预设值。
    enum class SizeMode {
        Small,   ///< 紧凑
        Medium,  ///< 适中（默认）
        Large    ///< 宽松
    };

    AuSelectStyle();
    virtual ~AuSelectStyle();

    // 内置主题工厂（所有权归调用者）
    static AuSelectStyle *createLightStyle(SizeMode mode = SizeMode::Medium);
    static AuSelectStyle *createDarkStyle(SizeMode mode = SizeMode::Medium);

    // 全局样式：未显式设置样式的 AuSelect 自动使用
    static AuSelectStyle *globalStyle();
    static void setGlobalStyle(AuSelectStyle *style);   // 所有权归调用者

    /// 当前尺寸档位。
    SizeMode sizeMode() const;
    /// 切换尺寸档位：编辑框高度与字体、选项高度、间距、圆角等尺寸类字段
    /// 重置为该档位预设（颜色保持不变）。字体档位：小 12px / 中 14px / 大 16px。
    void setSizeMode(SizeMode mode);

    /// 编辑框 / chips / 下拉项共用的字体（随尺寸档位重置为预设字号，可手动覆盖）。
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
    void setEditorActiveBorderColor(const QColor &color);
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

    // ---- 多选标签块（chip） ----
    QColor chipBackgroundColor() const;
    void setChipBackgroundColor(const QColor &color);
    QColor chipHoverBackgroundColor() const;
    void setChipHoverBackgroundColor(const QColor &color);
    QColor chipForegroundColor() const;
    void setChipForegroundColor(const QColor &color);
    QColor chipBorderColor() const;                     ///< 无效 QColor = 不绘制边框
    void setChipBorderColor(const QColor &color);
    QColor chipRemoveIconColor() const;
    void setChipRemoveIconColor(const QColor &color);
    QColor chipRemoveIconHoverColor() const;
    void setChipRemoveIconHoverColor(const QColor &color);
    int chipHeight() const;
    void setChipHeight(int height);
    int chipSpacing() const;                            ///< chip 之间与换行行距
    void setChipSpacing(int spacing);

    // ---- 下拉面板 ----
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
    int panelMaxHeight() const;                         ///< 面板内容区最大高度
    void setPanelMaxHeight(int height);

    // ---- 下拉项 ----
    int itemHeight() const;
    void setItemHeight(int height);
    int itemSpacing() const;                            ///< 相邻选项的垂直间距
    void setItemSpacing(int spacing);
    int itemHorizontalPadding() const;                  ///< 选项文字区左右内边距
    void setItemHorizontalPadding(int padding);
    int itemIndentWidth() const;                        ///< 分组选项相对组名的缩进
    void setItemIndentWidth(int width);
    int itemBorderRadius() const;
    void setItemBorderRadius(int radius);
    QColor itemForegroundColor() const;
    void setItemForegroundColor(const QColor &color);
    QColor itemDisabledForegroundColor() const;
    void setItemDisabledForegroundColor(const QColor &color);
    QColor itemHoverBackgroundColor() const;
    void setItemHoverBackgroundColor(const QColor &color);
    QColor itemSelectedBackgroundColor() const;
    void setItemSelectedBackgroundColor(const QColor &color);
    QColor itemSelectedForegroundColor() const;
    void setItemSelectedForegroundColor(const QColor &color);
    QColor itemHoverSelectedBackgroundColor() const;    ///< 悬停已选中项时的背景（天蓝）
    void setItemHoverSelectedBackgroundColor(const QColor &color);
    QColor itemCheckmarkColor() const;                  ///< 多选勾标记颜色
    void setItemCheckmarkColor(const QColor &color);
    QColor matchHighlightForegroundColor() const;       ///< 过滤命中文字颜色（无效色 = 关闭高亮）
    void setMatchHighlightForegroundColor(const QColor &color);

    // ---- 组名 ----
    int groupHeaderHeight() const;
    void setGroupHeaderHeight(int height);
    QColor groupHeaderForegroundColor() const;
    void setGroupHeaderForegroundColor(const QColor &color);

    // ---- 箭头 ----
    QColor arrowColor() const;
    void setArrowColor(const QColor &color);
    int arrowSlotWidth() const;                         ///< 编辑框右侧箭头槽位宽度
    void setArrowSlotWidth(int width);

    // ---- 清除图标（箭头槽位，悬停已选项时切换显示）----
    QColor clearIconCircleColor() const;                ///< 清除图标实心圆底色
    void setClearIconCircleColor(const QColor &color);
    QColor clearIconCircleHoverColor() const;           ///< 悬停清除图标实心圆底色
    void setClearIconCircleHoverColor(const QColor &color);
    QColor clearIconCrossColor() const;                 ///< 清除图标 × 颜色
    void setClearIconCrossColor(const QColor &color);

private:
    friend class AuSelect;
    static void registerGlobalObserver(AuSelect *widget);
    static void unregisterGlobalObserver(AuSelect *widget);
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

    // chip
    QColor m_chipBackgroundColor;
    QColor m_chipHoverBackgroundColor;
    QColor m_chipForegroundColor;
    QColor m_chipBorderColor;
    QColor m_chipRemoveIconColor;
    QColor m_chipRemoveIconHoverColor;
    int m_chipHeight;
    int m_chipSpacing;

    // 面板
    QColor m_panelBackgroundColor;
    QColor m_panelBorderColor;
    int m_panelBorderRadius;
    QColor m_panelShadowColor;
    int m_panelShadowRadius;
    int m_panelMaxHeight;

    // 下拉项
    int m_itemHeight;
    int m_itemSpacing;
    int m_itemHorizontalPadding;
    int m_itemIndentWidth;
    int m_itemBorderRadius;
    QColor m_itemForegroundColor;
    QColor m_itemDisabledForegroundColor;
    QColor m_itemHoverBackgroundColor;
    QColor m_itemSelectedBackgroundColor;
    QColor m_itemSelectedForegroundColor;
    QColor m_itemHoverSelectedBackgroundColor;
    QColor m_itemCheckmarkColor;
    QColor m_matchHighlightForegroundColor;

    // 组名
    int m_groupHeaderHeight;
    QColor m_groupHeaderForegroundColor;

    // 箭头
    QColor m_arrowColor;
    int m_arrowSlotWidth;

    // 清除图标
    QColor m_clearIconCircleColor;
    QColor m_clearIconCircleHoverColor;
    QColor m_clearIconCrossColor;

    static AuSelectStyle *s_globalStyle;
    static QVector<AuSelect *> s_observers;
};

#endif // AUSELECTSTYLE_H
