#ifndef AUHEADERRENDERER_H
#define AUHEADERRENDERER_H

#include "AuGrid_global.h"

#include <QPainter>
#include <QColor>
#include <QPoint>
#include <QRect>

class AuColumnDef;
class AuGridModel;
class AuGridStyle;

/**
 * @brief 表头节点绘制时的上下文状态。
 */
struct AuHeaderRendererContext
{
    bool hovered = false;              ///< 鼠标是否悬停在该表头节点上
    int column = -1;                   ///< 该表头节点首个叶子列索引
    int level = 0;                     ///< 1 起始的层级（与 headerRightClicked 的 level 语义一致）
    bool isGroup = false;              ///< 是否分组节点（跨多列的父表头）
    bool sorted = false;               ///< 该列是否为当前排序列
    Qt::SortOrder sortOrder = Qt::AscendingOrder;  ///< 当前排序方向
    QPoint headerPos = QPoint(-1, -1); ///< 鼠标相对表头节点左上角的坐标；无效时为 (-1,-1)
};

/**
 * @brief 表头渲染器抽象基类：自定义表头节点外观与点击热区。
 *
 * 通过 AuGridWidget::registerHeaderRenderer(name, renderer) 注册，
 * 再由 AuColumnDef::setHeaderRenderer(name) 按列引用；未指定的列绘制
 * 标准表头（背景 + 文字 + 排序箭头）。表头渲染器可以为列头添加按钮等
 * 交互元素：hitTest() 命中的区域点击不会触发列排序，而是在
 * headerMouseClick() 中回调。
 *
 * 示例见 Example/CustomAuGrid/HeaderButtonRenderer。
 */
class AUGRID_EXPORT AuHeaderRenderer
{
public:
    virtual ~AuHeaderRenderer();

    /// 绘制表头节点（纯虚）。rect 为节点矩形；column 为该节点首个叶子列索引。
    virtual void paint(QPainter *painter,
                       const QRect &rect,
                       const AuColumnDef &columnDef,
                       int column,
                       int level,
                       const AuGridModel *model,
                       const AuGridStyle *style,
                       const QColor &textColor,
                       const AuHeaderRendererContext &context) const = 0;

    // 鼠标进入/离开/在表头节点内移动。rect 为节点矩形（窗口坐标），headerPos 为相对节点左上角的坐标。
    /// 鼠标进入表头节点。
    virtual void headerMouseEnter(int column, int level, const QRect &rect, const QPoint &headerPos) { Q_UNUSED(column) Q_UNUSED(level) Q_UNUSED(rect) Q_UNUSED(headerPos) }
    /// 鼠标离开表头节点。
    virtual void headerMouseLeave(int column, int level) { Q_UNUSED(column) Q_UNUSED(level) }
    /// 鼠标在表头节点内移动。
    virtual void headerMouseMove(int column, int level, const QRect &rect, const QPoint &headerPos) { Q_UNUSED(column) Q_UNUSED(level) Q_UNUSED(rect) Q_UNUSED(headerPos) }

    // 返回 true 表示该位置需要显示手型光标。
    /// 该位置是否显示手型光标。
    virtual bool wantsHoverCursor(int column, int level, const QRect &rect, const QPoint &headerPos) const { Q_UNUSED(column) Q_UNUSED(level) Q_UNUSED(rect) Q_UNUSED(headerPos) return false; }

    // 返回 true 表示鼠标命中了一个可点击区域（如按钮）；此时点击不会触发列头排序。
    /// 命中可点击区域（如按钮）；返回 true 时该处点击不触发排序。
    virtual bool hitTest(int column, int level, const QRect &rect, const QPoint &headerPos) const { Q_UNUSED(column) Q_UNUSED(level) Q_UNUSED(rect) Q_UNUSED(headerPos) return false; }

    // 鼠标在可点击区域上按下并释放时回调。
    /// 可点击区域上完成一次点击（按下并释放）。
    virtual void headerMouseClick(int column, int level, const QRect &rect, const QPoint &headerPos) { Q_UNUSED(column) Q_UNUSED(level) Q_UNUSED(rect) Q_UNUSED(headerPos) }
};

#endif // AUHEADERRENDERER_H
