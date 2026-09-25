#ifndef AGPLOT_CURVE_H
#define AGPLOT_CURVE_H

#include "AgPlotItem.h"
#include "AgPlotTypes.h"

#include <QColor>
#include <QPointF>
#include <QVector>

class QPainter;
class AgPlot;

/**
 * @brief 曲线图元：数据点 + 折线（实线 / 虚线 / 点线）+ 可选点标记 + 可选基线填充。
 *
 * 线（需求 12）：setLineStyle() 用 Qt::PenStyle 选实线 / 虚线 / 点线等；
 * setDashPattern() 可自定义虚线间隔（单位为线宽，如 {4, 2} = 画 4 空 2），
 * 设置后优先于 lineStyle；setDashOffset() 控制虚线相位。
 *
 * 点（需求 11）：setPointStyle() 从内置样式里选（实心 / 空心圆、十字架、
 * 带圈十字架等），AgPlotPointStyle::None 表示不绘制；大数据量时点标记自动
 * 按像素抽稀，不会一个像素叠几十个点。
 *
 * 面（需求 13）：setFillVisible(true) 后曲线与基线（setFillBaseline，默认 0）
 * 之间填充 setFillColor()（支持透明度）。
 *
 * 大数据量（需求 8）：x 升序的数据自动用二分查找定位可见区间，点数远超像素数
 * 时按像素列做 min/max 抽稀（每列保留首、最小、最大、末 4 个点），百万级数据
 * 依旧流畅，且极值峰谷形状不丢。
 *
 * 层级（需求 10）：默认 zLevel = 0；setZLevel() 控制与其它图元的上下关系。
 */
class AGPLOT_EXPORT AgPlotCurve : public AgPlotItem
{
public:
    AgPlotCurve();
    ~AgPlotCurve() override;

    // ---- 数据 ----
    /// 整体替换数据点（内部自动检测 x 是否升序）。
    void setPoints(const QVector<QPointF> &points);
    void addPoint(double x, double y);
    void clearPoints();
    int pointCount() const;
    QPointF pointAt(int index) const;
    /// x 是否升序（自动检测；升序时可见区间用二分查找，大数据量更快）。
    bool isXAscending() const;
    /// 返回 x 最接近给定值的数据点索引（升序二分 / 乱序线性扫描）；无数据返回 -1。
    /// 追踪器（AgPlot::setTrackerEnabled）用它吸附光标附近的点。
    int nearestIndexForX(double x) const;

    // ---- 线 ----
    bool lineVisible() const;
    void setLineVisible(bool on);
    /// 线颜色：未设置（无效）时加入 AgPlot 后自动取样式调色板颜色。
    QColor lineColor() const;
    void setLineColor(const QColor &color);
    qreal lineWidth() const;
    void setLineWidth(qreal width);
    /// 线型：Qt::SolidLine（默认）/ Qt::DashLine 虚线 / Qt::DotLine 点线等。
    Qt::PenStyle lineStyle() const;
    void setLineStyle(Qt::PenStyle style);
    /// 自定义虚线间隔（画-空交替，单位为线宽倍数）；非空时优先于 lineStyle。
    QVector<qreal> dashPattern() const;
    void setDashPattern(const QVector<qreal> &pattern);
    /// 虚线起始相位（单位为线宽倍数）。
    qreal dashOffset() const;
    void setDashOffset(qreal offset);

    // ---- 点标记 ----
    AgPlotPointStyle pointStyle() const;
    void setPointStyle(AgPlotPointStyle style);
    /// 点直径 / 边长（像素），默认 6。
    int pointSize() const;
    void setPointSize(int size);
    /// 点颜色：默认与线同色。
    QColor pointColor() const;
    void setPointColor(const QColor &color);

    // ---- 面填充 ----
    bool fillVisible() const;
    void setFillVisible(bool on);
    /// 填充色（支持透明度），默认蓝色半透明。
    QColor fillColor() const;
    void setFillColor(const QColor &color);
    /// 填充基线的 y 值（默认 0）。
    double fillBaseline() const;
    void setFillBaseline(double y);

protected:
    friend class AgPlot;
    void paint(QPainter *painter, const AgPlot *plot) override;
    QRectF computeBounds() const override;

private:
    QVector<QPointF> m_points;
    bool m_xAscending = true;
    bool m_lineVisible = true;
    QColor m_lineColor;
    qreal m_lineWidth = 1.5;
    Qt::PenStyle m_lineStyle = Qt::SolidLine;
    QVector<qreal> m_dashPattern;
    qreal m_dashOffset = 0.0;
    AgPlotPointStyle m_pointStyle = AgPlotPointStyle::None;
    int m_pointSize = 6;
    QColor m_pointColor;
    bool m_fillVisible = false;
    QColor m_fillColor;
    double m_fillBaseline = 0.0;
};

#endif // AGPLOT_CURVE_H
