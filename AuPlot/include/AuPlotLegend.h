#ifndef AUPLOT_LEGEND_H
#define AUPLOT_LEGEND_H

#include "AuPlot_global.h"
#include "AuPlotTypes.h"

class AuPlot;

/**
 * @brief AuPlot 图例配置：位置、交互与条目外观的全部开关。
 *
 * 图例由 AuPlot 内置绘制（无需创建/管理对象），通过 AuPlot::legend() 拿到本
 * 配置对象逐项调整（改配置自动重绘）；显示名来自 AuPlotItem::setName()，
 * 名字非空的图元才会出现在图例里。
 *
 * 交互：clickToToggleEnabled()（默认开）时点击条目切换对应图元的可见性——
 * 隐藏的图元在图例中仍保留条目（置灰），可再次点击恢复；点击同时发出
 * AuPlot::legendEntryClicked() 与 AuPlot::itemVisibilityChanged() 信号，
 * 需要自制外部图例面板时依赖这两个信号同步状态。
 *
 * 布局：Top / Bottom 横向排列、放不下自动换行；Left / Right 纵向排列、
 * 自动分列；Floating 系列在绘图区角落单列悬浮。maxRows() 限制换行行数 /
 * 分列列数（0 = 不限制，条目极多时会持续挤占绘图区，请酌情设置）。
 *
 * 颜色 / 字体在 AuPlotStyle 中配置（legendFont / legendTextColor /
 * legendDimmedTextColor / legendBackgroundColor / legendBorderColor）。
 */
class AUPLOT_EXPORT AuPlotLegend
{
public:
    AuPlotLegend();

    bool isVisible() const;
    void setVisible(bool on);

    AuPlotLegendPos position() const;
    void setPosition(AuPlotLegendPos pos);

    /// 点击条目切换对应图元可见性（默认开）；关闭后点击只发信号，不切换。
    bool clickToToggleEnabled() const;
    void setClickToToggleEnabled(bool on);

    /// 图例图标边长（像素，默认 14）。
    int iconSize() const;
    void setIconSize(int size);

    /// 换行行数 / 分列列数上限（默认 0 = 不限制；悬浮模式同时受绘图区高度约束）。
    int maxRows() const;
    void setMaxRows(int rows);

private:
    friend class AuPlot;
    /// 仅供 AuPlot 内部使用：挂接宿主控件，配置变化自动请求重绘。
    void attachPlot(AuPlot *plot) { m_plot = plot; }

    AuPlot *m_plot = nullptr;   /// 宿主控件（attachPlot 设置），不拥有
    bool m_visible = true;
    AuPlotLegendPos m_position = AuPlotLegendPos::Top;
    bool m_clickToToggle = true;
    int m_iconSize = 14;
    int m_maxRows = 0;
};

#endif // AUPLOT_LEGEND_H
