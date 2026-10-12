#ifndef AUPLOT_H
#define AUPLOT_H

#include "AuPlot_global.h"
#include "AuPlotAxis.h"
#include "AuPlotCurve.h"
#include "AuPlotItem.h"
#include "AuPlotLegend.h"
#include "AuPlotStyle.h"
#include "AuPlotTypes.h"

#include <QImage>
#include <QList>
#include <QPair>
#include <QPointF>
#include <QRectF>
#include <QVector>
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
 * 图例：setName() 非空的图元自动进入内置图例（AuPlot::legend() 调整位置 /
 * 交互），点击条目默认可切换曲线显隐（隐藏条目置灰保留，可点击恢复）；
 * itemVisibilityChanged() / legendEntryClicked() 信号供外部图例面板同步。
 *
 * 标题：setTitle() 图表标题（顶部居中），AuPlotAxis::setTitle() 轴标题
 * （常用于单位），布局自动为标题预留空间。
 *
 * 交互（需求 9，拖动 / 缩放 / 框选默认关闭）：
 *  - setDragEnabled(true) 拖动平移；setDragDirections() 可精细化只允许横向
 *    或纵向拖动；
 *  - setZoomEnabled(true) 鼠标滚轮缩放（以光标为中心）；setZoomDirections()
 *    可只缩放横向或纵向；
 *  - setRubberBandZoomEnabled(true) 框选缩放（默认 Ctrl + 左键拖出矩形，
 *    修饰键可改）；框选同样受 zoomDirections 约束，Esc / 右键取消；
 *  - 双击绘图区恢复所有轴自适应量程（setDoubleClickResetEnabled 可关），
 *    也可代码调用 resetAxes()；
 *  - 拖动 / 缩放过的轴自动脱离数据自适应量程（setAutoRange(true) 可恢复）；
 *  - 轴设置 setRange() / setLimits() 后，拖动与缩放被钳制在该范围内（需求 15）。
 *
 * 导出：toImage() 按当前或指定尺寸渲染为图片（支持缩放倍数与透明背景）。
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
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(bool legendVisible READ legendVisible WRITE setLegendVisible)
    Q_PROPERTY(bool rubberBandZoomEnabled READ rubberBandZoomEnabled WRITE setRubberBandZoomEnabled)
    Q_PROPERTY(bool doubleClickResetEnabled READ doubleClickResetEnabled WRITE setDoubleClickResetEnabled)

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

    // ---- 标题 ----
    /// 图表标题（顶部居中）；空（默认）不显示、不占空间。
    QString title() const;
    void setTitle(const QString &title);

    // ---- 图例 ----
    /// 图例配置（控件持有，永不为空）；显示名来自 AuPlotItem::setName()。
    AuPlotLegend *legend() const;
    /// 图例整体显隐（等价 legend()->setVisible，属性化便于设计器配置）。
    bool legendVisible() const;
    void setLegendVisible(bool on);

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

    // ---- 框选缩放与视图恢复 ----
    /// 框选缩放（默认关）：按住 rubberBandModifiers 修饰键 + 左键拖出矩形，
    /// 松开后把矩形范围设为可见范围（与 zoomDirections 一致的方向生效）。
    bool rubberBandZoomEnabled() const;
    void setRubberBandZoomEnabled(bool on);
    /// 触发框选的键盘修饰键（默认 Qt::ControlModifier；设为 Qt::NoModifier
    /// 则普通左键拖动即框选，此时拖动平移在其上不可用）。
    Qt::KeyboardModifiers rubberBandModifiers() const;
    void setRubberBandModifiers(Qt::KeyboardModifiers modifiers);
    /// 所有轴恢复数据自适应量程（清除 setRange / 拖动缩放造成的固定量程，
    /// setLimits 硬边界保留）。
    void resetAxes();
    /// 双击绘图区是否恢复自适应量程（默认开）。
    bool doubleClickResetEnabled() const;
    void setDoubleClickResetEnabled(bool on);

    // ---- 导出 ----
    /// 渲染为图片。size 为空（默认）时用控件当前尺寸；导出会临时按目标
    /// 尺寸重排（可见控件会闪动后恢复）。devicePixelRatio 支持高清导出
    /// （如 2.0）；transparentBackground = true 时背景透明。
    QImage toImage(const QSize &size = QSize(), qreal devicePixelRatio = 1.0,
                   bool transparentBackground = false) const;

    // ---- 追踪器与十字线（默认关闭） ----
    /// 追踪器：鼠标在绘图区内移动时，以鼠标 x 为基准同时吸附所有可见曲线上
    /// 最近的数据点（升序曲线二分查找，百万级数据无压力）——每条曲线画一个
    /// 空心圆标记（曲线色），所有曲线的 x / y 值合并在同一个 tooltip 里逐行
    /// 显示（行首色点标识曲线），tooltip 锚定鼠标位置，可视区域放不下时自动
    /// 翻转到左侧 / 下方并钳制在控件内；值文本使用绑定轴的刻度格式化，自定义
    /// formatter 同样生效。移出绘图区后自动隐藏。trackerChanged() 信号实时
    /// 通知各曲线的吸附值。
    bool trackerEnabled() const;
    void setTrackerEnabled(bool on);
    /// 追踪线（鼠标 x）离开某条曲线的数据 x 范围后，是否继续吸附该曲线的
    /// 最近端点：默认 false——离开后该曲线的追踪点标记与 tip 中的数值行一并
    /// 隐藏（所有曲线都离开时整个 tip 隐藏）；true 时保留吸附端点的旧行为。
    bool trackerSnapOutOfRange() const;
    void setTrackerSnapOutOfRange(bool on);
    /// 十字线（追踪参考线）：横贯绘图区的点线跟随鼠标，最多横竖各一条，
    /// 方向可用 setCrosshairDirections() 单独开启 / 关闭，移出后隐藏。
    bool crosshairEnabled() const;
    void setCrosshairEnabled(bool on);
    /// 十字线方向（默认横 + 竖）；设为 Qt::Horizontal / Qt::Vertical 只显示一条。
    Qt::Orientations crosshairDirections() const;
    void setCrosshairDirections(Qt::Orientations directions);

    /// 请求重绘（配置变化一般自动刷新，外部直接改范围后可调用）。
    void replot();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// 追踪吸附点变化：某条可见曲线的吸附点变化时发出该曲线及其新吸附点；
    /// curve 为 nullptr 表示鼠标已离开所有曲线 / 绘图区。
    void trackerChanged(AuPlotCurve *curve, const QPointF &dataPoint);
    /// 图元可见性变化（代码 setVisible / 图例点击切换都会发出）。
    void itemVisibilityChanged(AuPlotItem *item, bool visible);
    /// 点击图例条目（无论是否切换可见性都会发出；item 为对应图元）。
    void legendEntryClicked(AuPlotItem *item);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    friend class AuPlotItem;
    friend class AuPlotCurve;
    friend class AuPlotPoints;
    friend class AuPlotLine;
    friend class AuPlotArea;
    friend class AuPlotRefLine;

    /// 图例条目（绘制时记录，供点击命中测试）。
    struct LegendEntry {
        AuPlotItem *item;
        QRectF rect;   // 控件局部坐标（含图标 + 文字的完整命中区）
    };

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
    /// 图元可见性变化（setVisible / 图例点击）：清理追踪态并广播信号。
    void handleItemVisibility(AuPlotItem *item, bool visible);

    void drawGrid(QPainter *painter, const QRectF &rect, const AuPlotStyle *st);
    void drawAxes(QPainter *painter, const QRectF &rect, const AuPlotStyle *st);
    void drawCrosshair(QPainter *painter);
    void drawTracker(QPainter *painter);
    void drawTitle(QPainter *painter, const AuPlotStyle *st);
    void drawLegend(QPainter *painter, const AuPlotStyle *st);
    void drawRubberBand(QPainter *painter);
    /// 根据鼠标位置更新十字线与追踪器状态（必要时重绘并发出 trackerChanged）。
    void updateTracker(const QPoint &pos);

    /// 布局图例：计算所有条目的命中矩形与条带背景矩形（控件局部坐标）。
    /// 无可见图例（关闭 / 无命名图元）时返回 false。布局与绘制共用，
    /// calculatePlotRect 用它预留边距。
    bool layoutLegend(QVector<LegendEntry> *entries, QRectF *bandRect) const;
    /// 图表标题条带高度（未设置标题时为 0）。
    int titleStripHeight() const;

    /// 拖动平移一条轴：脱离自适应量程、按边界钳制并触发重绘。
    void panAxis(AuPlotAxis *axis, double dataDelta);
    /// 以 anchor（数据值）为中心缩放一条轴：脱离自适应量程、钳制并触发重绘。
    void zoomAxis(AuPlotAxis *axis, qreal factor, double anchor);
    /// 把框选矩形（控件像素）应用为各可见轴的新可见范围。
    void applyRubberBandZoom(const QRect &rect);

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
    bool m_trackerSnapOutOfRange = false;   ///< 离开曲线数据范围后仍吸附端点（默认隐藏）
    bool m_crosshairEnabled = false;
    Qt::Orientations m_crosshairDirections = Qt::Horizontal | Qt::Vertical;
    bool m_trackerValid = false;            ///< 当前有吸附点可显示
    bool m_crosshairValid = false;          ///< 十字线可见（鼠标在绘图区内）
    /// 各可见曲线的吸附结果（曲线 + 点索引，按图元顺序），与 trackerChanged 对应。
    QVector<QPair<AuPlotCurve *, int>> m_trackerHits;
    QPoint m_trackerPos;                    ///< 追踪 tooltip 锚定的鼠标位置
    QPoint m_crosshairPos;                  ///< 十字线位置（控件局部坐标）

    // 标题 / 图例
    QString m_title;
    AuPlotLegend *m_legend = nullptr;
    QVector<LegendEntry> m_legendEntries;   ///< 最近一次绘制记录的图例条目命中区

    // 框选缩放
    bool m_rubberBandZoomEnabled = false;
    Qt::KeyboardModifiers m_rubberBandModifiers = Qt::ControlModifier;
    bool m_rubberBandActive = false;        ///< 正在拖出框选矩形
    QPoint m_rubberBandOrigin;              ///< 框选起点（控件局部坐标）
    QRect m_rubberBandRect;                 ///< 当前框选矩形（钳制在绘图区内）

    // 双击恢复 / 导出
    bool m_doubleClickResetEnabled = true;
    mutable bool m_exportTransparent = false;   ///< toImage 期间跳过背景填充
};

#endif // AUPLOT_H
