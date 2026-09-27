#ifndef AUPLOT_H
#define AUPLOT_H

#include "AuPlot_global.h"
#include "AuPlotAxis.h"
#include "AuPlotCurve.h"
#include "AuPlotItem.h"
#include "AuPlotStyle.h"
#include "AuPlotTypes.h"

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QWidget>

/**
 * @brief AuPlot 曲线绘制控件：双轴坐标 + 背景网格 + 曲线 / 点 / 线 / 面图元 +
 *        拖动缩放交互，支持大数据量。
 *
 * 坐标轴布局（需求 1）：
 *  - 左侧 Y1（yAxis）、底部 X1（xAxis）默认显示，即默认单轴；
 *  - 顶部 X2（xAxis2）、右侧 Y2（yAxis2）默认隐藏；setDualAxisEnabled(true)
 *    同时开启两条，也可用 xAxis2()->setVisible(true) 单独开启某条；
 *  - 每条轴独立配置范围、刻度、刻度值、网格（见 AuPlotAxis），轴刻度默认
 *    按数据值显示，可自定义显示值或隐藏数值；
 *  - 自适应量程按绑定到该轴的图元数据计算，X2 / Y2 没有绑定数据时跟随
 *    X1 / Y1 的范围。
 *
 * 图元（需求 10）：addItem() 加入 AuPlotCurve（曲线）/ AuPlotPoints（点）/
 * AuPlotLine（线）/ AuPlotArea（面），坐标一律按绑定轴的数据值，zLevel()
 * 控制层级（默认曲线 0，负值在曲线下方，正值在上方，先画层级低的）。
 * 控件接管图元所有权。
 *
 * 交互（需求 9，默认全部关闭）：
 *  - setDragEnabled(true) 拖动平移；setDragDirections() 可精细化只允许横向
 *    或纵向拖动；
 *  - setZoomEnabled(true) 鼠标滚轮缩放（以光标为中心）；setZoomDirections()
 *    可只缩放横向或纵向；
 *  - 拖动 / 缩放过的轴自动脱离数据自适应量程（setAutoRange(true) 可恢复）；
 *  - 轴设置 setRange() / setLimits() 后，拖动与缩放被钳制在该范围内（需求 15）。
 *
 * 样式（需求 14）：默认白底；背景支持透明与任意颜色（含透明度），轴色、
 * 刻度值、网格、曲线 / 点 / 线 / 面的颜色均独立可控（见 AuPlotStyle 与各图元）。
 */
class AUPLOT_EXPORT AuPlot : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool dualAxisEnabled READ dualAxisEnabled WRITE setDualAxisEnabled)
    Q_PROPERTY(bool dragEnabled READ dragEnabled WRITE setDragEnabled)
    Q_PROPERTY(bool zoomEnabled READ zoomEnabled WRITE setZoomEnabled)

public:
    explicit AuPlot(QWidget *parent = nullptr);
    ~AuPlot() override;

    // ---- 坐标轴 ----
    /// 底部 X1 轴。
    AuPlotAxis *xAxis() const;
    /// 左侧 Y1 轴。
    AuPlotAxis *yAxis() const;
    /// 顶部 X2 轴（默认隐藏）。
    AuPlotAxis *xAxis2() const;
    /// 右侧 Y2 轴（默认隐藏）。
    AuPlotAxis *yAxis2() const;
    /// 是否处于双轴显示（X2 + Y2 同时可见）。
    bool dualAxisEnabled() const;
    /// 开启 / 关闭双轴：同时显示或隐藏顶部 X2 与右侧 Y2。
    void setDualAxisEnabled(bool on);

    // ---- 图元管理（所有权归 AuPlot） ----
    /// 加入图元（曲线 / 点 / 线 / 面），接管所有权；图元 zLevel 决定绘制顺序。
    void addItem(AuPlotItem *item);
    /// 移除并 delete 图元。
    void removeItem(AuPlotItem *item);
    /// 移除并 delete 全部图元。
    void clearItems();
    QList<AuPlotItem *> items() const;

    // ---- 坐标映射 ----
    /// 主绘图区（坐标轴围成的区域），控件局部坐标。
    QRectF plotRect() const;
    /// 数据 x -> 像素（axis 指定用哪条 X 轴的范围）。
    qreal xToPixel(double x, AuPlotAxisPos axis = AuPlotAxisPos::X1) const;
    /// 数据 y -> 像素（axis 指定用哪条 Y 轴的范围）。
    qreal yToPixel(double y, AuPlotAxisPos axis = AuPlotAxisPos::Y1) const;
    /// 像素 -> 数据 x。
    double pixelToX(qreal pixelX, AuPlotAxisPos axis = AuPlotAxisPos::X1) const;
    /// 像素 -> 数据 y。
    double pixelToY(qreal pixelY, AuPlotAxisPos axis = AuPlotAxisPos::Y1) const;

    // ---- 样式 ----
    /// 生效样式：显式 setStyle 的 > 全局样式 > 内置默认（白底）。
    AuPlotStyle *style() const;
    /// 设置独立样式（接管所有权；传 nullptr 恢复默认 / 全局）。
    void setStyle(AuPlotStyle *style);

    // ---- 交互（默认全部关闭，见需求 9） ----
    bool dragEnabled() const;
    void setDragEnabled(bool on);
    /// 拖动方向（默认横 + 纵）；可设为 Qt::Horizontal / Qt::Vertical 精细化控制。
    Qt::Orientations dragDirections() const;
    void setDragDirections(Qt::Orientations directions);
    bool zoomEnabled() const;
    void setZoomEnabled(bool on);
    /// 缩放方向（默认横 + 纵）。
    Qt::Orientations zoomDirections() const;
    void setZoomDirections(Qt::Orientations directions);
    /// 每格滚轮的缩放步长（>1，默认 1.2：向上滚放大，值越大一次缩放越多）。
    qreal zoomFactor() const;
    void setZoomFactor(qreal factor);

    // ---- 追踪器与十字线（默认关闭） ----
    /// 追踪器：鼠标在绘图区内移动时，吸附到最近曲线的数据点（升序曲线二分查找，
    /// 百万级数据无压力），绘制空心圆标记 + 引导线 + 显示 x / y 值的 tooltip
    /// （值文本使用绑定轴的刻度格式化，自定义 formatter 同样生效）；移出绘图区
    /// 或所有曲线后自动隐藏。trackerChanged() 信号实时通知当前吸附值。
    bool trackerEnabled() const;
    void setTrackerEnabled(bool on);
    /// 十字线：横贯绘图区的点线（横 + 竖）跟随鼠标移动，移出后隐藏。
    bool crosshairEnabled() const;
    void setCrosshairEnabled(bool on);

    /// 请求重绘（配置变化一般自动刷新，外部直接改范围后可调用）。
    void replot();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// 追踪器吸附点变化（curve 为 nullptr 表示鼠标已离开所有曲线 / 绘图区）。
    void trackerChanged(AuPlotCurve *curve, const QPointF &dataPoint);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    friend class AuPlotItem;
    friend class AuPlotCurve;
    friend class AuPlotPoints;
    friend class AuPlotLine;
    friend class AuPlotArea;

    AuPlotAxis *axisAt(AuPlotAxisPos pos) const;
    /// 生效样式指针（不会为空）。
    AuPlotStyle *effectiveStyle() const;
    /// 按边距（刻度值宽度自适应）计算主绘图区。
    QRectF calculatePlotRect() const;
    /// 把自适应轴的范围贴合到图元数据范围（数据变化后自动调用）。
    void fitAutoRanges();
    /// 给未显式设置颜色的图元按调色板取色。
    void assignDefaultColor(AuPlotItem *item);
    /// 图元 zLevel 变化后把它移到 m_items 中正确的排序位置。
    void reinsertItem(AuPlotItem *item);

    void drawGrid(QPainter *painter, const QRectF &rect, const AuPlotStyle *st);
    void drawAxes(QPainter *painter, const QRectF &rect, const AuPlotStyle *st);
    void drawCrosshair(QPainter *painter);
    void drawTracker(QPainter *painter);
    /// 根据鼠标位置更新十字线与追踪器状态（必要时重绘并发出 trackerChanged）。
    void updateTracker(const QPoint &pos);

    /// 拖动平移一条轴：脱离自适应量程、按边界钳制并触发重绘。
    void panAxis(AuPlotAxis *axis, double dataDelta);
    /// 以 anchor（数据值）为中心缩放一条轴：脱离自适应量程、钳制并触发重绘。
    void zoomAxis(AuPlotAxis *axis, qreal factor, double anchor);

    AuPlotAxis *m_xAxis = nullptr;
    AuPlotAxis *m_yAxis = nullptr;
    AuPlotAxis *m_xAxis2 = nullptr;
    AuPlotAxis *m_yAxis2 = nullptr;

    QList<AuPlotItem *> m_items;            ///< 已按 zLevel 升序
    AuPlotStyle *m_customStyle = nullptr;   ///< 用户显式设置（接管所有权）
    AuPlotStyle *m_defaultStyle = nullptr;  ///< 无自定义且无全局样式时使用（接管所有权）

    QRectF m_plotRect;
    bool m_dragging = false;
    QPoint m_lastMousePos;
    bool m_dragEnabled = false;
    Qt::Orientations m_dragDirections = Qt::Horizontal | Qt::Vertical;
    bool m_zoomEnabled = false;
    Qt::Orientations m_zoomDirections = Qt::Horizontal | Qt::Vertical;
    qreal m_zoomFactor = 1.2;

    bool m_trackerEnabled = false;
    bool m_crosshairEnabled = false;
    bool m_trackerValid = false;            ///< 当前有吸附点可显示
    bool m_crosshairValid = false;          ///< 十字线可见（鼠标在绘图区内）
    AuPlotCurve *m_trackerCurve = nullptr;  ///< 当前吸附的曲线
    int m_trackerIndex = -1;                ///< 吸附点索引
    QPoint m_crosshairPos;                  ///< 十字线位置（控件局部坐标）
};

#endif // AUPLOT_H
