#ifndef AGPLOT_STYLE_H
#define AGPLOT_STYLE_H

#include "AgPlot_global.h"

#include <QColor>
#include <QFont>
#include <QVector>

/**
 * @brief AgPlot 样式：背景、坐标轴 / 刻度 / 网格的视觉属性与曲线默认调色板。
 *
 * 三种使用方式（与 AgDatePickerStyle 一致）：
 *  1. 全局样式：AgPlotStyle::setGlobalStyle() 设置后，所有未显式 setStyle()
 *     的 AgPlot 自动使用；
 *  2. 控件独立样式：AgPlot::setStyle(new AgPlotStyle())（控件接管所有权），
 *     随后可对实例逐项微调；
 *  3. 内置主题工厂：createLightStyle()（默认白底）/ createDarkStyle()。
 *
 * 颜色约定：
 *  - backgroundColor() 无效（未设置）时背景透明，透出父部件内容；
 *  - 背景色支持带透明度（如 QColor(255,255,255,200)）实现半透明；
 *  - gridColor / axisColor / tickLabelColor 独立可控；
 *  - curvePalette 为新加入曲线的默认取色调色板（按顺序轮转）。
 */
class AGPLOT_EXPORT AgPlotStyle
{
public:
    AgPlotStyle();
    virtual ~AgPlotStyle();

    // 内置主题工厂（所有权归调用者）
    static AgPlotStyle *createLightStyle();
    static AgPlotStyle *createDarkStyle();

    // 全局样式：未显式设置样式的 AgPlot 自动使用
    static AgPlotStyle *globalStyle();
    static void setGlobalStyle(AgPlotStyle *style);   // 所有权归调用者

    // ---- 整体背景 ----
    /// 控件背景色，默认白色；无效颜色 = 透明（透出父部件），可带透明度。
    QColor backgroundColor() const;
    void setBackgroundColor(const QColor &color);
    /// 主绘图区背景色：无效（默认）时不单独填充（与控件背景一致）。
    QColor plotAreaBackground() const;
    void setPlotAreaBackground(const QColor &color);

    // ---- 坐标轴 ----
    /// 轴线与刻度线颜色（轴上可单独覆盖，见 AgPlotAxis::setAxisColor）。
    QColor axisColor() const;
    void setAxisColor(const QColor &color);
    /// 刻度数值文字颜色。
    QColor tickLabelColor() const;
    void setTickLabelColor(const QColor &color);
    /// 刻度数值字体。
    QFont tickLabelFont() const;
    void setTickLabelFont(const QFont &font);
    /// 大刻度线长度（像素，默认 5）。
    int tickLength() const;
    void setTickLength(int length);
    /// 小刻度线长度（像素，默认 3）。
    int minorTickLength() const;
    void setMinorTickLength(int length);
    /// 是否在轴上绘制小刻度线（默认开）。
    bool minorTicksVisible() const;
    void setMinorTicksVisible(bool on);

    // ---- 背景网格（轴上可逐轴覆盖线型 / 颜色 / 线宽） ----
    QColor gridColor() const;
    void setGridColor(const QColor &color);
    /// 全局网格线型默认（虚线）；逐轴可用 AgPlotAxis::setGridStyle 覆盖。
    Qt::PenStyle gridStyle() const;
    void setGridStyle(Qt::PenStyle style);
    qreal gridWidth() const;
    void setGridWidth(qreal width);

    // ---- 曲线默认调色板 ----
    /// 新加入的曲线 / 点 / 线未显式设置颜色时按此调色板轮转取色。
    QVector<QColor> curvePalette() const;
    void setCurvePalette(const QVector<QColor> &colors);
    /// 内部使用：轮转取下一个调色板颜色。
    QColor nextCurveColor();

    // ---- 追踪器 tooltip / 十字线 ----
    /// 追踪器 tooltip 背景色（可带透明度），默认白色 92% 不透明。
    QColor trackerBackgroundColor() const;
    void setTrackerBackgroundColor(const QColor &color);
    QColor trackerBorderColor() const;
    void setTrackerBorderColor(const QColor &color);
    /// 追踪器 tooltip 文字颜色。
    QColor trackerTextColor() const;
    void setTrackerTextColor(const QColor &color);
    /// 追踪器 tooltip 字体（默认应用字体）。
    QFont trackerFont() const;
    void setTrackerFont(const QFont &font);
    /// 十字线 / 追踪器引导线颜色（默认 #999999）。
    QColor crosshairColor() const;
    void setCrosshairColor(const QColor &color);
    /// 十字线线型（默认 Qt::DotLine 点线）。
    Qt::PenStyle crosshairStyle() const;
    void setCrosshairStyle(Qt::PenStyle style);

private:
    QColor m_backgroundColor;
    QColor m_plotAreaBackground;
    QColor m_axisColor;
    QColor m_tickLabelColor;
    QFont m_tickLabelFont;
    int m_tickLength = 5;
    int m_minorTickLength = 3;
    bool m_minorTicksVisible = true;
    QColor m_gridColor;
    Qt::PenStyle m_gridStyle = Qt::DashLine;
    qreal m_gridWidth = 1.0;
    QVector<QColor> m_curvePalette;
    int m_paletteIndex = 0;
    QColor m_trackerBackgroundColor;
    QColor m_trackerBorderColor;
    QColor m_trackerTextColor;
    QFont m_trackerFont;
    QColor m_crosshairColor;
    Qt::PenStyle m_crosshairStyle = Qt::DotLine;
};

#endif // AGPLOT_STYLE_H
