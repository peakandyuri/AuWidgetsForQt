#ifndef HEADCHECKBOXRENDERER_H
#define HEADCHECKBOXRENDERER_H

#include "AgHeaderRenderer.h"

#include <QPointer>

class AgColumnDef;
class AgGridModel;
class AgGridStyle;

/**
 * @brief 示例：数据列复选框的"表头全选"渲染器。
 *
 * 在列头右侧绘制一个三态复选框（全选 / 半选 / 全不选，状态由该列数据
 * 实时聚合），点击后把当前全部可见行切换为全选或清空。
 *
 * 只用到了库的内置能力，无需改动库：
 *  - AgGridWidget::registerHeaderRenderer("headerCheckBox", ...) 注册后，
 *    由 AgColumnDef::setHeaderRenderer("headerCheckBox") 按列引用；
 *  - hitTest() 声明复选框热区，热区内的点击不会触发表头排序；
 *  - headerMouseClick() 中遍历 model->setCellValue() 写全选 / 清空；
 *  - wantsHoverCursor() 在热区上显示手型光标。
 *
 * 多个复选框数据列可共用同一个渲染器实例（回调按列参数区分），
 * 使用前需调用 setModel() 告知要操作的数据模型。
 */
class HeaderCheckBoxRenderer : public AgHeaderRenderer
{
public:
    /// 设置要操作的数据模型（headerMouseClick 里遍历写值用）。
    void setModel(AgGridModel *model);

    void paint(QPainter *painter,
               const QRect &rect,
               const AgColumnDef &columnDef,
               int column,
               int level,
               const AgGridModel *model,
               const AgGridStyle *style,
               const QColor &textColor,
               const AgHeaderRendererContext &context) const override;

    bool wantsHoverCursor(int column, int level, const QRect &rect, const QPoint &headerPos) const override;
    bool hitTest(int column, int level, const QRect &rect, const QPoint &headerPos) const override;
    void headerMouseClick(int column, int level, const QRect &rect, const QPoint &headerPos) override;

private:
    QPointer<AgGridModel> m_model;   ///< headerMouseClick 中写值的目标模型
};

#endif // HEADCHECKBOXRENDERER_H
