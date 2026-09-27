#ifndef AUDEFAULTCELLRENDERER_H
#define AUDEFAULTCELLRENDERER_H

#include "AuCellRenderer.h"

/**
 * @brief 默认文本渲染器：未指定 cellRenderer 的列使用。
 *
 * 绘制行为：
 *  - 按列定义 textWrapMode 决定换行 / 截断 / 溢出；
 *  - 对齐取列 alignment 与样式 CellTextAlignment 覆盖后的结果；
 *  - 样式 CellBackgroundColor 有效时先铺自定义背景（并按需叠加 hover 色），
 *    CellForegroundColor 有效时替代 textColor；
 *  - WordWrap 模式下 preferredHeight() 返回内容所需高度，实现自动行高；
 *  - copyContent() 返回原始值（QVariant::toString）。
 */
class AUGRID_EXPORT AuDefaultCellRenderer : public AuCellRenderer
{
public:
    void paint(QPainter *painter,
               const QRect &rect,
               const QVariant &value,
               const AuColumnDef &columnDef,
               int row,
               int column,
               const AuGridModel *model,
               const AuGridStyle *style,
               const QColor &textColor) const override;

    void paint(QPainter *painter,
               const QRect &rect,
               const QVariant &value,
               const AuColumnDef &columnDef,
               int row,
               int column,
               const AuGridModel *model,
               const AuGridStyle *style,
               const QColor &textColor,
               const AuCellRendererContext &context) const override;

    /// WordWrap 模式返回文本换行所需高度，其余返回 -1（默认行高）。
    int preferredHeight(const QRect &rect,
                        const QVariant &value,
                        const AuColumnDef &columnDef,
                        int row,
                        int column,
                        const AuGridModel *model,
                        const AuGridStyle *style) const override;

    /// 带树形上下文的测量：树形首列扣减缩进 / 箭头占位后再计算换行宽度。
    int preferredHeight(const QRect &rect,
                        const QVariant &value,
                        const AuColumnDef &columnDef,
                        int row,
                        int column,
                        const AuGridModel *model,
                        const AuGridStyle *style,
                        const AuCellRendererContext &context) const override;

    /// 返回原始值的字符串形式。
    QVariant copyContent(const QVariant &value,
                         const AuColumnDef &columnDef,
                         int row,
                         int column,
                         const AuGridModel *model) const override;
};

#endif // AUDEFAULTCELLRENDERER_H
