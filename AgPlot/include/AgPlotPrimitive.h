#ifndef AGPLOT_PRIMITIVE_H
#define AGPLOT_PRIMITIVE_H

#include "AgPlotItem.h"
#include "AgPlotTypes.h"

#include <QColor>
#include <QPointF>
#include <QVector>

class QPainter;
class AgPlot;

/**
 * @brief 独立"点"图元：只绘制标记点，不连线、不填充（需求 10/11）。
 *
 * 典型用途：标注采样点、离群点高亮、自由散点。坐标按绑定的 XY 轴数值，
 * 层级 setZLevel() 控制与曲线的上下关系。
 */
class AGPLOT_EXPORT AgPlotPoints : public AgPlotItem
{
public:
    AgPlotPoints();
    ~AgPlotPoints() override;

    void setPoints(const QVector<QPointF> &points);
    void addPoint(double x, double y);
    void clearPoints();
    int pointCount() const;

    AgPlotPointStyle pointStyle() const;
    void setPointStyle(AgPlotPointStyle style);
    /// 点直径 / 边长（像素），默认 6。
    int pointSize() const;
    void setPointSize(int size);
    /// 点颜色：未设置（无效）时加入 AgPlot 后自动取样式调色板颜色。
    QColor pointColor() const;
    void setPointColor(const QColor &color);

protected:
    friend class AgPlot;
    void paint(QPainter *painter, const AgPlot *plot) override;
    QRectF computeBounds() const override;

private:
    QVector<QPointF> m_points;
    AgPlotPointStyle m_pointStyle = AgPlotPointStyle::Circle;
    int m_pointSize = 6;
    QColor m_pointColor;
};

/**
 * @brief 独立"线"图元：折线（两点即线段），只有线、无点标记与填充（需求 10/12）。
 *
 * 线型支持实线 / 虚线 / 点线，虚线间隔可用 setDashPattern() 精细控制。
 * 典型用途：参考线、阈值线、连线标注等。
 */
class AGPLOT_EXPORT AgPlotLine : public AgPlotItem
{
public:
    AgPlotLine();
    ~AgPlotLine() override;

    /// 折线顶点（>= 2 个才绘制）。
    void setPoints(const QVector<QPointF> &points);
    void addPoint(double x, double y);
    void clearPoints();
    int pointCount() const;

    /// 线颜色：未设置（无效）时加入 AgPlot 后自动取样式调色板颜色。
    QColor lineColor() const;
    void setLineColor(const QColor &color);
    qreal lineWidth() const;
    void setLineWidth(qreal width);
    Qt::PenStyle lineStyle() const;
    void setLineStyle(Qt::PenStyle style);
    QVector<qreal> dashPattern() const;
    void setDashPattern(const QVector<qreal> &pattern);
    qreal dashOffset() const;
    void setDashOffset(qreal offset);

protected:
    friend class AgPlot;
    void paint(QPainter *painter, const AgPlot *plot) override;
    QRectF computeBounds() const override;

private:
    QVector<QPointF> m_points;
    QColor m_lineColor;
    qreal m_lineWidth = 1.5;
    Qt::PenStyle m_lineStyle = Qt::SolidLine;
    QVector<qreal> m_dashPattern;
    qreal m_dashOffset = 0.0;
};

/**
 * @brief 独立"面"图元：多边形区域（点序自动闭合）（需求 10/13）。
 *
 * 内部填充色支持透明度（setFillColor），边框可选（默认实线，也可虚线 / 点线）。
 * 典型用途：背景区域带、正常区间着色、多边形标注等。
 */
class AGPLOT_EXPORT AgPlotArea : public AgPlotItem
{
public:
    AgPlotArea();
    ~AgPlotArea() override;

    /// 多边形顶点（绘制时自动闭合首尾）。
    void setPoints(const QVector<QPointF> &points);
    void addPoint(double x, double y);
    void clearPoints();
    int pointCount() const;

    /// 填充色（支持透明度），默认蓝色半透明；无效 = 只画边框不填充。
    QColor fillColor() const;
    void setFillColor(const QColor &color);
    bool borderVisible() const;
    void setBorderVisible(bool on);
    QColor borderColor() const;
    void setBorderColor(const QColor &color);
    qreal borderWidth() const;
    void setBorderWidth(qreal width);
    Qt::PenStyle borderStyle() const;
    void setBorderStyle(Qt::PenStyle style);
    QVector<qreal> borderDashPattern() const;
    void setBorderDashPattern(const QVector<qreal> &pattern);

protected:
    friend class AgPlot;
    void paint(QPainter *painter, const AgPlot *plot) override;
    QRectF computeBounds() const override;

private:
    QVector<QPointF> m_points;
    QColor m_fillColor;
    bool m_borderVisible = true;
    QColor m_borderColor;
    qreal m_borderWidth = 1.5;
    Qt::PenStyle m_borderStyle = Qt::SolidLine;
    QVector<qreal> m_borderDashPattern;
};

/**
 * @brief 参考线 / 标线图元：横贯整个绘图区的水平线或垂直线。
 *
 * 典型用途：阈值线、报警线、时间标记线。与 AgPlotLine（两点线段 / 折线）不同，
 * RefLine 只需一个数据值即可绘制全幅线：Horizontal 在绑定 Y 轴的 value 处
 * 横贯左右，Vertical 在绑定 X 轴的 value 处纵贯上下。
 *
 * 默认虚线；层级 setZLevel() 照常生效。注意：参考线不参与自适应量程
 * （避免一条阈值线把视图拉得很远），数据轴自适应时只统计曲线 / 点 / 线 / 面。
 */
class AGPLOT_EXPORT AgPlotRefLine : public AgPlotItem
{
public:
    enum class Orientation {
        Horizontal, ///< 水平线（value 为绑定 Y 轴上的 y 值）
        Vertical    ///< 垂直线（value 为绑定 X 轴上的 x 值）
    };

    AgPlotRefLine();
    /// 便利构造：指定方向（所有权在使用时交给 AgPlot::addItem）。
    explicit AgPlotRefLine(Orientation orientation);
    ~AgPlotRefLine() override;

    Orientation orientation() const;
    void setOrientation(Orientation orientation);
    /// 线所在的数据值（水平线 = y 值，垂直线 = x 值）。
    double value() const;
    void setValue(double value);

    /// 线颜色：未设置（无效）时加入 AgPlot 后自动取样式调色板颜色。
    QColor lineColor() const;
    void setLineColor(const QColor &color);
    qreal lineWidth() const;
    void setLineWidth(qreal width);
    /// 线型，默认 Qt::DashLine（虚线）。
    Qt::PenStyle lineStyle() const;
    void setLineStyle(Qt::PenStyle style);
    QVector<qreal> dashPattern() const;
    void setDashPattern(const QVector<qreal> &pattern);
    qreal dashOffset() const;
    void setDashOffset(qreal offset);

protected:
    friend class AgPlot;
    void paint(QPainter *painter, const AgPlot *plot) override;
    QRectF computeBounds() const override;

private:
    Orientation m_orientation = Orientation::Horizontal;
    double m_value = 0.0;
    QColor m_lineColor;
    qreal m_lineWidth = 1.5;
    Qt::PenStyle m_lineStyle = Qt::DashLine;
    QVector<qreal> m_dashPattern;
    qreal m_dashOffset = 0.0;
};

#endif // AGPLOT_PRIMITIVE_H
