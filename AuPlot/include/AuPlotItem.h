#ifndef AUPLOT_ITEM_H
#define AUPLOT_ITEM_H

#include "AuPlot_global.h"
#include "AuPlotTypes.h"

#include <QRectF>
#include <QString>

class QPainter;
class AuPlot;

/**
 * @brief AuPlot 图元基类：曲线、点集、独立线、独立面都由它派生。
 *
 * 层级（zLevel）约定：数值越小越先绘制（越靠底层）。默认曲线为 0，面的 zLevel
 * 设为 1 会盖在默认曲线上，设为负数则在默认曲线下方；AuPlot 绘制时按 zLevel
 * 从小到大排序（同层级按加入顺序）。
 *
 * 生命周期：通过 AuPlot::addItem() 加入后由 AuPlot 接管所有权（removeItem /
 * clearItems 时销毁）；一个图元同一时间只属于一个 AuPlot。
 *
 * 坐标：图元数据点一律使用数据坐标（对应绑定的 X / Y 轴的数值范围），绘制时
 * 由 AuPlot 换算为像素；绑定轴默认 X1 + Y1，双轴模式下可绑定 X2 / Y2。
 *
 * 图例：setName() 设置显示名后图元自动进入 AuPlot 内置图例（名字为空不进）；
 * 自定义图元子类覆盖 drawLegendIcon() 即可定义图例图标外观，未覆盖时图标
 * 为空。可见性变化（含图例点击切换）通过 AuPlot::itemVisibilityChanged() 通知。
 */
class AUPLOT_EXPORT AuPlotItem
{
public:
    explicit AuPlotItem();
    virtual ~AuPlotItem();

    /// 绘制层级：默认 0，越小越靠底层（先画）。
    int zLevel() const;
    void setZLevel(int z);

    bool isVisible() const;
    void setVisible(bool on);

    /// 图例显示名；空（默认）表示不进图例。
    QString name() const;
    void setName(const QString &name);

    /// 数据绑定的 X 轴（默认底部 X1）。
    AuPlotAxisPos xAxis() const;
    void setXAxis(AuPlotAxisPos pos);
    /// 数据绑定的 Y 轴（默认左侧 Y1）。
    AuPlotAxisPos yAxis() const;
    void setYAxis(AuPlotAxisPos pos);

    /// 当前数据的包围盒（数据坐标）；无数据时返回 isNull() 矩形。
    /// 自动量程（AuPlotAxis::autoRange）用它计算轴范围；结果有缓存，
    /// 数据变化时子类调用 invalidateBounds() 使其失效。
    QRectF dataBounds() const;

protected:
    friend class AuPlot;

    /// 绘制接口：调用时 painter 已裁剪到主绘图区并打开抗锯齿，
    /// 数据坐标 -> 像素用 plot->xToPixel() / plot->yToPixel()。
    virtual void paint(QPainter *painter, const AuPlot *plot) = 0;
    /// 计算数据包围盒（子类实现，只管算不管缓存）。
    virtual QRectF computeBounds() const = 0;
    /// 绘制图例图标（iconRect 为可用矩形）；默认不绘制任何内容，
    /// 自定义图元子类可覆盖（线型图元建议画一条中线 + 标记采样）。
    virtual void drawLegendIcon(QPainter *painter, const QRectF &iconRect);
    /// 数据变化后由子类调用：包围盒失效并请求所属 AuPlot 重绘。
    void invalidateBounds();
    /// 所属 AuPlot（未加入前为 nullptr）。
    AuPlot *attachedPlot() const;

private:
    AuPlot *m_plot = nullptr;
    int m_zLevel = 0;
    bool m_visible = true;
    QString m_name;
    AuPlotAxisPos m_xAxis = AuPlotAxisPos::X1;
    AuPlotAxisPos m_yAxis = AuPlotAxisPos::Y1;
    mutable QRectF m_bounds;
    mutable bool m_boundsValid = false;
};

#endif // AUPLOT_ITEM_H
