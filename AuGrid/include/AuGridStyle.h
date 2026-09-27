#ifndef AUGRIDSTYLE_H
#define AUGRIDSTYLE_H

#include "AuGrid_global.h"

#include <QVariant>
#include <QColor>
#include <QFont>

class AuGridModel;
class AuGridWidget;

/**
 * @brief 表格样式：所有视觉属性按"角色（StyleRole）→ QVariant 值"的查询表组织。
 *
 * 三种使用粒度：
 *  1. 全局样式：AuGridStyle::setGlobalStyle() 设置后，所有未显式 setStyle() 的
 *     控件自动使用，且全局样式变化时已注册控件自动刷新；
 *  2. 控件独立样式：AuGridWidget::setStyle()（控件接管所有权）；
 *  3. 按行列上下文：value()/color()/integer() 等接受 (model, row, column) 参数，
 *     子类可按单元格位置返回不同值（如负值标红）。
 *
 * 颜色类角色返回"无效 QColor"表示不绘制该元素（例如关闭斑马纹）。
 *
 * 内置主题：createLightStyle() / createDarkStyle() 工厂；AuDarkGridStyle 为暗色实现类。
 *
 * 自定义示例：
 * @code
 *   class MyStyle : public AuGridStyle {
 *   public:
 *       QVariant value(StyleRole role, const AuGridModel *model,
 *                      int row, int column) const override {
 *           if (role == CellForegroundColor && column == 1
 *               && model && model->cellValue(row, 1).toInt() < 0)
 *               return QColor(Qt::red);
 *           return AuGridStyle::value(role, model, row, column);
 *       }
 *   };
 *   grid->setStyle(new MyStyle());
 * @endcode
 */
class AUGRID_EXPORT AuGridStyle
{
public:
    /// 密度档位：影响表头高、行高、字体、内边距等尺寸类默认值。
    enum class SizeMode {
        Small,   ///< 紧凑
        Medium,  ///< 适中
        Large    ///< 宽松
    };

    /**
     * 样式角色（键）。分组的完整说明见 docs/AuGrid-帮助文档.md 第 16 节。
     * 标注 [上下文] 的键在查询时会传入 (model, row, column)，可按单元格返回不同值。
     */
    enum StyleRole {
        // 全局尺寸
        HeaderHeight,               ///< 单级表头高度（像素）
        RowHeight,                  ///< 默认行高（像素；WordWrap 行可更高）
        ScrollBarExtent,            ///< 滚动条厚度
        CornerRadius,               ///< 表格面板圆角半径（0 = 直角）

        // 表头
        HeaderBackgroundColor,      ///< 表头背景色
        HeaderForegroundColor,      ///< 表头文字色
        HeaderBorderColor,          ///< 表头边框 / 停靠区分隔线颜色
        HeaderFont,                 ///< 表头字体（列定义 headerFont 可覆盖）
        HeaderAlignment,            ///< 表头对齐（列定义 headerAlignment 可覆盖）
        SortIndicatorColor,         ///< 排序指示三角颜色（无效色 = 跟随 HeaderForegroundColor）

        // 单元格 / 网格线
        CellBorderColor,            ///< 单元格边框色 [上下文]
        GridLineColor,              ///< 网格线颜色
        RowBackgroundColor,         ///< 行背景色
        SelectedRowBackgroundColor, ///< 选中行背景色
        AlternateRowBackgroundColor,///< 斑马纹背景色（无效色 = 关闭斑马纹）
        HoverRowBackgroundColor,    ///< 悬停行背景色（合并单元格为整体高亮）
        FocusedCellBorderColor,     ///< 焦点格边框颜色（无效色 = 不绘制）[上下文]
        FocusedCellBorderWidth,     ///< 焦点格边框宽度 [上下文]
        CellHorizontalPadding,      ///< 单元格水平内边距 [上下文]
        CellVerticalPadding,        ///< 单元格垂直内边距 [上下文]

        // 滚动条
        ScrollBarSliderColor,       ///< 滚动条滑块颜色
        ScrollBarTrackBackgroundColor, ///< 滚动条滑轨背景色

        // 字体与对齐
        Font,                       ///< 单元格字体 [上下文]
        DefaultTextAlignment,       ///< 默认文字对齐 [上下文]

        // 行列上下文相关
        CellBackgroundColor,        ///< 单元格自定义背景（有效时由渲染器绘制并叠加 hover）[上下文]
        CellForegroundColor,        ///< 单元格自定义前景色 [上下文]
        CellTextAlignment,          ///< 单元格对齐覆盖（0 = 不覆盖）[上下文]

        // 外边框
        BorderColor,                ///< 表格最外层边框颜色

        // 复选框列
        CheckboxSize,               ///< 复选框边长
        CheckboxCheckedColor,       ///< 选中填充色
        CheckboxUncheckedColor,     ///< 未选中填充色
        CheckboxBorderColor,        ///< 复选框边框色
        CheckboxCheckmarkColor,     ///< 对勾颜色

        // 行拖拽
        DragHandleIconColor,        ///< 拖拽手柄圆点颜色
        DragHandleHoveredIconColor, ///< 悬停时圆点颜色
        DragHandleDotSize,          ///< 圆点直径
        DragHandleBackgroundColor,  ///< 手柄格背景色
        DragHandleHoveredBackgroundColor, ///< 悬停时手柄格背景色
        DraggedRowOverlayColor,     ///< 拖动中行上的半透明覆盖色

        // 行号列
        RowNumberFontBold,          ///< 行号是否加粗
        RowNumberForegroundColor,   ///< 行号文字颜色

        // 加载遮罩
        LoadingOverlayColor,        ///< 加载遮罩背景色（通常带透明度；无效色 = 不绘制蒙层）
        LoadingSpinnerColor,        ///< 加载圆环颜色
        LoadingSpinnerSize,         ///< 加载圆环直径（像素）
        LoadingTextColor,           ///< 加载文字颜色
        LoadingText,                ///< 默认加载文字（setLoadingText 未设置或传空时生效；样式置空 = 只显示圆环）

        // 树形表格
        TreeIndentWidth,            ///< 树形层级缩进宽度（每级像素）
        TreeExpanderSize,           ///< 展开箭头热区边长（像素）
        TreeExpanderColor,          ///< 展开箭头颜色
        TreeExpanderHoveredColor    ///< 悬停时展开箭头颜色
    };

    AuGridStyle();
    virtual ~AuGridStyle();

    /// 核心查询入口：返回角色对应的值。子类重写以自定义样式；
    /// 未知角色返回 defaultValue(role)。model/row/column 用于上下文样式。
    virtual QVariant value(StyleRole role,
                           const AuGridModel *model = nullptr,
                           int row = -1,
                           int column = -1) const;

    /// 类型化读取便捷模板。
    template <typename T>
    T valueAs(StyleRole role, const AuGridModel *model = nullptr, int row = -1, int column = -1) const
    {
        return value(role, model, row, column).template value<T>();
    }

    /// 以 QColor 读取；角色值不是颜色或未设置时返回无效 QColor。
    QColor color(StyleRole role, const AuGridModel *model = nullptr, int row = -1, int column = -1) const;
    /// 以 int 读取。
    int integer(StyleRole role, const AuGridModel *model = nullptr, int row = -1, int column = -1) const;
    /// 读取单元格字体（角色 Font）。
    QFont font(const AuGridModel *model = nullptr, int row = -1, int column = -1) const;
    /// 读取默认文字对齐（角色 DefaultTextAlignment）。
    Qt::Alignment alignment(const AuGridModel *model = nullptr, int row = -1, int column = -1) const;

    /// 当前密度档位。
    SizeMode sizeMode() const;
    /// 切换密度档位（尺寸类默认值随之变化）。
    void setSizeMode(SizeMode mode);

    /// 深拷贝当前样式（子类必须重写以正确拷贝自有状态）。
    virtual AuGridStyle *clone() const;

    // 全局样式：未显式设置样式的 AuGridWidget 会自动使用
    /// 读取全局样式（始终返回有效实例）。
    static AuGridStyle *globalStyle();
    /// 设置全局样式（所有权归调用者）；已注册的控件自动刷新。
    static void setGlobalStyle(AuGridStyle *style);

    // 内置主题工厂
    /// 创建浅色主题实例（所有权归调用者）。
    static AuGridStyle *createLightStyle();
    /// 创建暗色主题实例（所有权归调用者）。
    static AuGridStyle *createDarkStyle();

protected:
    /// 各角色的默认值；子类可重写以覆盖部分角色默认值。
    virtual QVariant defaultValue(StyleRole role) const;

    SizeMode m_sizeMode;   ///< 当前密度档位

private:
    friend class AuGridWidget;
    /// 控件注册 / 注销为全局样式观察者（构造 / 析构时自动调用）。
    static void registerGlobalStyleObserver(AuGridWidget *widget);
    static void unregisterGlobalStyleObserver(AuGridWidget *widget);
    /// 通知全部观察者控件全局样式已变化。
    static void notifyGlobalStyleChanged();

    static AuGridStyle *s_globalStyle;          ///< 全局样式实例
    static AuGridStyle *s_defaultLightStyle;    ///< 内置浅色默认样式
    static QVector<AuGridWidget *> s_observers; ///< 全局样式观察控件列表
};

#endif // AUGRIDSTYLE_H
