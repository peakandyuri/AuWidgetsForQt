#ifndef AUCELLRENDERER_H
#define AUCELLRENDERER_H

#include "AuGrid_global.h"

#include <QVariant>
#include <QPainter>
#include <QColor>
#include <QPoint>

class AuColumnDef;
class AuGridModel;
class AuGridStyle;

/**
 * @brief 单元格绘制时的上下文状态，随 paint() 一并传给渲染器。
 */
struct AuCellRendererContext
{
    bool hovered = false;                  ///< 鼠标是否悬停在该单元格上
    bool selected = false;                 ///< 是否为当前选中格
    Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter;  ///< 最终对齐（列对齐 + 样式覆盖后的结果）

    // 树形支持：仅树形表格的首个可见数据列由控件填充以下字段，
    // 其余格子均为默认值（不占位、不绘制箭头），非树场景行为不变。
    bool treeRow = false;                  ///< 该格是否位于树形表格的树列（保留缩进 + 箭头占位）
    int treeDepth = 0;                     ///< 行层级（根 = 0），据此缩进
    bool treeHasChildren = false;          ///< 行是否有子行（决定是否绘制展开箭头）
    bool treeExpanded = false;             ///< 行当前是否展开（箭头指向）
    bool treeExpanderHovered = false;      ///< 鼠标是否悬停在展开箭头热区上
};

/**
 * @brief 单元格渲染器抽象基类：决定一格内容如何绘制、如何参与行高与复制。
 *
 * 通过 AuGridWidget::registerCellRenderer(name, renderer) 注册，
 * 再由 AuColumnDef::setCellRenderer(name) 按列引用；未指定的列使用
 * AuDefaultCellRenderer，checkbox 列强制使用内置 AuCheckBoxCellRenderer。
 * 渲染器以 QSharedPointer 持有，可跨多个控件共享同一实例。
 *
 * 重绘时机：悬停状态变化、鼠标进出单元格（cellMouseEnter/Leave/Move）都会
 * 触发控件重绘相应区域；渲染器可维护内部状态（如按钮悬停）实现丰富交互。
 *
 * 最小实现只需重写带 AuCellRendererContext 的 paint()。
 */
class AUGRID_EXPORT AuCellRenderer
{
public:
    virtual ~AuCellRenderer();

    /// 基础绘制入口（纯虚）。textColor 为默认文字色；rect 为单元格矩形。
    virtual void paint(QPainter *painter,
                       const QRect &rect,
                       const QVariant &value,
                       const AuColumnDef &columnDef,
                       int row,
                       int column,
                       const AuGridModel *model,
                       const AuGridStyle *style,
                       const QColor &textColor) const = 0;

    // 带 hover/selected 状态的绘制入口，默认转调旧接口以保持兼容。
    /// 控件实际调用的绘制入口：携带悬停 / 选中 / 对齐等上下文。
    virtual void paint(QPainter *painter,
                       const QRect &rect,
                       const QVariant &value,
                       const AuColumnDef &columnDef,
                       int row,
                       int column,
                       const AuGridModel *model,
                       const AuGridStyle *style,
                       const QColor &textColor,
                       const AuCellRendererContext &context) const
    {
        Q_UNUSED(context)
        paint(painter, rect, value, columnDef, row, column, model, style, textColor);
    }

    // 鼠标进入/离开/在单元格内移动。cellPos 为单元格左上角为 (0,0) 的相对坐标。
    /// 鼠标进入单元格（可记录热区状态用于重绘）。
    virtual void cellMouseEnter(int row, int column, const QPoint &cellPos) { Q_UNUSED(row) Q_UNUSED(column) Q_UNUSED(cellPos) }
    /// 鼠标离开单元格（清除悬停状态）。
    virtual void cellMouseLeave(int row, int column) { Q_UNUSED(row) Q_UNUSED(column) }
    /// 鼠标在单元格内移动。
    virtual void cellMouseMove(int row, int column, const QPoint &cellPos) { Q_UNUSED(row) Q_UNUSED(column) Q_UNUSED(cellPos) }

    // 返回 true 表示当前单元格需要显示手型光标。
    /// 该位置是否显示手型光标（如按钮热区上返回 true）。
    virtual bool wantsHoverCursor(int row, int column, const QPoint &cellPos) const { Q_UNUSED(row) Q_UNUSED(column) Q_UNUSED(cellPos) return false; }

    // 返回单元格期望高度；返回 -1 表示使用表格默认行高。
    /// 单元格期望高度，参与行高计算（取各列最大值）；-1 = 使用默认行高。
    /// WordWrap 文本即通过此接口实现自动行高。
    virtual int preferredHeight(const QRect &rect,
                                const QVariant &value,
                                const AuColumnDef &columnDef,
                                int row,
                                int column,
                                const AuGridModel *model,
                                const AuGridStyle *style) const { Q_UNUSED(rect) Q_UNUSED(value) Q_UNUSED(columnDef) Q_UNUSED(row) Q_UNUSED(column) Q_UNUSED(model) Q_UNUSED(style) return -1; }

    /// 带树形上下文的重载：控件优先调用；默认转调旧接口保持兼容。
    /// 树形首列的渲染器应据此扣减缩进 / 箭头占位后再测量换行宽度。
    virtual int preferredHeight(const QRect &rect,
                                const QVariant &value,
                                const AuColumnDef &columnDef,
                                int row,
                                int column,
                                const AuGridModel *model,
                                const AuGridStyle *style,
                                const AuCellRendererContext &context) const
    {
        Q_UNUSED(context)
        return preferredHeight(rect, value, columnDef, row, column, model, style);
    }

    // 返回该单元格复制到剪贴板的内容。
    // 返回无效的 QVariant 表示该渲染器不提供复制内容，此时不会复制到剪贴板。
    /// Ctrl+C 复制内容；返回无效 QVariant 表示不提供复制。
    virtual QVariant copyContent(const QVariant &value,
                                 const AuColumnDef &columnDef,
                                 int row,
                                 int column,
                                 const AuGridModel *model) const
    {
        Q_UNUSED(value) Q_UNUSED(columnDef) Q_UNUSED(row) Q_UNUSED(column) Q_UNUSED(model)
        return QVariant();
    }
};

#endif // AUCELLRENDERER_H
