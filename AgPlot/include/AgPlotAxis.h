#ifndef AGPLOT_AXIS_H
#define AGPLOT_AXIS_H

#include "AgPlot_global.h"

#include <QtCore/qglobal.h>
#include <QColor>
#include <QObject>
#include <QVector>
#include <functional>

/**
 * @brief AgPlot 单条坐标轴：范围、大/小刻度、刻度值显示与背景网格的全部配置。
 *
 * AgPlot 内部固定持有 4 条轴（AgPlot::xAxis / yAxis / xAxis2 / yAxis2），生命周期
 * 由 AgPlot 管理；直接改轴配置后无需手动重绘（AgPlot 自动刷新）。
 *
 * 范围语义（对应"设置了最小值和最大值后，缩放与拖动只能在此范围内"）：
 *  - 默认 autoRange：范围随绑定到该轴的图元数据自适应（两侧留白 setRangePadding）；
 *  - setRange(min, max)：固定为该范围，同时该范围成为交互（拖动 / 缩放）边界；
 *  - setLimits(min, max)：不固定当前范围，只给交互加硬边界，自适应结果同样
 *    被夹在边界内；clearLimits() 清除；
 *  - setAutoRange(true) 重新回到自适应（清除 setRange 的固定边界，保留 setLimits）。
 *
 * 刻度语义：
 *  - 大刻度：显示数值的刻度；小刻度：相邻大刻度之间平分的刻度（不显示数值，
 *    如大刻度 1~10、setMinorTickCount(4) 时小刻度为 2/4/6/8）；
 *  - setTickCount(n)（n>0）：固定大刻度数量为 n（首尾各一个，均分范围）；
 *    n<=0 自动：按轴像素长度以"好数"步长（1/2/5 x 10^n）生成；
 *  - setTickLabelsVisible(false)：不显示刻度数值（只保留刻度线 / 网格）；
 *  - setLabelFormatter()：自定义刻度显示值（如 1~7 显示"周一"~"周日"），
 *    默认按数据值格式化（自动精度，setLabelPrecision 可强制小数位）。
 *
 * 网格语义（对应"横向 / 纵向 / 两者，默认虚线，可配置实线或点线"）：
 *  - X 轴的网格是纵向线、Y 轴的网格是横向线；横 / 纵开关、线型、颜色逐轴独立；
 *  - 默认 X1 / Y1 网格开（虚线浅灰），X2 / Y2 网格默认关。
 */
class AGPLOT_EXPORT AgPlotAxis : public QObject
{
    Q_OBJECT

public:
    /// 一般不直接构造：使用 AgPlot::xAxis()/yAxis()/xAxis2()/yAxis2() 拿到的实例。
    explicit AgPlotAxis(QObject *parent = nullptr);
    ~AgPlotAxis() override;

    // ---- 可见性 ----
    bool isVisible() const;
    /// 隐藏轴后不再绘制轴线 / 刻度 / 刻度值（网格由 gridVisible 独立控制，
    /// 但轴隐藏时网格也不绘制）；主绘图区不再为它预留边距。
    void setVisible(bool visible);

    // ---- 数值范围 ----
    /// 当前可见范围下限。
    double minimum() const;
    /// 当前可见范围上限。
    double maximum() const;
    /// 固定范围为 [min, max]（同时作为拖动 / 缩放边界，见类注释）。
    void setRange(double min, double max);
    bool autoRange() const;
    /// 切换数据自适应（默认开）。开启时清除 setRange 的固定边界，保留 setLimits。
    /// 注意：拖动 / 缩放交互会自动把该轴切出自适应（手动量程），需要恢复时
    /// 再调用 setAutoRange(true)。
    void setAutoRange(bool on = true);
    bool hasLimits() const;
    double limitMinimum() const;
    double limitMaximum() const;
    /// 设置交互硬边界 [min, max]（不改变当前范围）。
    void setLimits(double min, double max);
    void clearLimits();
    /// 自适应量程时两侧留白比例（0 ~ 0.45，默认 0.05；数据只有单点时按 +/-1 扩展）。
    qreal rangePadding() const;
    void setRangePadding(qreal padding);

    // ---- 刻度 ----
    /// 大刻度数量：>0 固定数量（含首尾均分），<=0 按轴像素长度自动（默认）。
    int tickCount() const;
    void setTickCount(int count);
    /// 相邻大刻度之间的小刻度个数（默认 4，即大刻度间均分出 4 个小刻度）。
    int minorTickCount() const;
    void setMinorTickCount(int count);
    bool tickLabelsVisible() const;
    /// 隐藏刻度数值（刻度线与网格保留），适合只看趋势的场景。
    void setTickLabelsVisible(bool on);
    /// 自定义刻度显示值；空（默认）时按数据值 + 自动精度格式化。
    void setLabelFormatter(std::function<QString(double)> formatter);
    bool hasLabelFormatter() const;
    /// 把刻度值转为显示文本：优先自定义 formatter，否则数值格式化。
    QString formatTickLabel(double value) const;
    /// 默认数值格式化的小数位（>=0 生效；默认 -1 按刻度步长自动决定）。
    int labelPrecision() const;
    void setLabelPrecision(int precision);

    // ---- 背景网格 ----
    bool gridVisible() const;
    void setGridVisible(bool on);
    /// 网格线型：默认 Qt::DashLine（虚线），可改 Qt::SolidLine / Qt::DotLine 等。
    Qt::PenStyle gridStyle() const;
    void setGridStyle(Qt::PenStyle style);
    /// 网格颜色：无效时使用 AgPlotStyle::gridColor()。
    QColor gridColor() const;
    void setGridColor(const QColor &color);
    /// 网格线宽：<=0 使用 AgPlotStyle::gridWidth()。
    qreal gridWidth() const;
    void setGridWidth(qreal width);
    /// 小刻度网格线（更密的次级网格），默认关。
    bool minorGridVisible() const;
    void setMinorGridVisible(bool on);

    // ---- 刻度取值 ----
    /// 大刻度值列表。pixelLength 为该轴方向的绘图区像素长度（自动刻度按它定数量）。
    QVector<double> tickValues(qreal pixelLength) const;
    /// 小刻度值列表（相邻大刻度之间均分，不含大刻度本身）。
    QVector<double> minorTickValues(qreal pixelLength) const;

    // ---- 轴颜色覆盖 ----
    /// 轴线 / 刻度线颜色：无效时使用 AgPlotStyle::axisColor()。双轴时给 Y2
    /// 设置独立颜色可与 Y1 区分。
    QColor axisColor() const;
    void setAxisColor(const QColor &color);

signals:
    /// 可见范围变化（setRange / 拖动 / 缩放 / 自适应量程）。
    void rangeChanged(double minimum, double maximum);

private:
    friend class AgPlot;
    friend class AgPlotRenderer;

    /// 仅供 AgPlot 内部使用：直接改当前范围（不重设 setRange 的交互边界快照）。
    void setCurrentRange(double min, double max);
    /// 交互钳制：把 [mn, mx] 夹回交互边界内（边界 = limits 优先，否则 setRange 快照）。
    bool clampToBounds(double &mn, double &mx) const;
    /// 交互边界下限（无边界时返回 0，配合 hasInteractionBounds 使用）。
    bool hasInteractionBounds(double &boundMin, double &boundMax) const;

    void generateTicks(qreal pixelLength) const;

    bool m_visible = true;

    double m_min = 0.0;
    double m_max = 10.0;
    bool m_autoRange = true;
    bool m_fixedRange = false;          ///< setRange() 后为真（范围固定 + 交互边界）
    double m_boundMin = 0.0;            ///< setRange() 时的范围快照（交互边界）
    double m_boundMax = 10.0;
    bool m_hasLimits = false;
    double m_limitMin = 0.0;
    double m_limitMax = 10.0;
    qreal m_padding = 0.05;

    int m_tickCount = 0;                ///< 0 = 自动
    int m_minorCount = 4;
    bool m_tickLabelsVisible = true;
    std::function<QString(double)> m_formatter;
    int m_labelPrecision = -1;

    bool m_gridVisible = true;
    Qt::PenStyle m_gridStyle = Qt::DashLine;
    QColor m_gridColor;                 ///< 无效 → 全局样式
    qreal m_gridWidth = 0.0;            ///< <=0 → 全局样式
    bool m_minorGridVisible = false;

    QColor m_axisColor;                 ///< 无效 → 全局样式

    // 刻度缓存：范围或像素长度变化时重新生成
    mutable QVector<double> m_majorTicks;
    mutable QVector<double> m_minorTicks;
    mutable qreal m_cachedPixelLength = -1.0;
    mutable bool m_rangeDirty = true;
    mutable double m_lastStep = 1.0;
};

#endif // AGPLOT_AXIS_H
