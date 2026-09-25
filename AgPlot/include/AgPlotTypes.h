#ifndef AGPLOT_TYPES_H
#define AGPLOT_TYPES_H

#include "AgPlot_global.h"

/**
 * @brief 坐标轴位置。
 *
 * AgPlot 固定四条轴：底部 X1（默认 X 轴）、左侧 Y1（默认 Y 轴）始终可用；
 * 顶部 X2、右侧 Y2 通过 setDualAxisEnabled(true) 或轴的 setVisible() 开启。
 * 图元（曲线 / 点 / 线 / 面）通过 AgPlotAxisPos 指定自己绑定哪条 X 轴与 Y 轴。
 */
enum class AgPlotAxisPos {
    X1,   ///< 底部 X1 轴（默认 X 轴）
    Y1,   ///< 左侧 Y1 轴（默认 Y 轴）
    X2,   ///< 顶部 X2 轴（双轴）
    Y2    ///< 右侧 Y2 轴（双轴）
};

/**
 * @brief "点"标记的内置样式。
 *
 * 实心类样式以 color 填充；空心类样式以 color 描边（不填充）；
 * Cross / XCross / RingedCross 以 color 画线。size 统一表示直径或边长（像素）。
 */
enum class AgPlotPointStyle {
    None,           ///< 不绘制点
    Circle,         ///< 实心圆
    HollowCircle,   ///< 空心圆圈
    Square,         ///< 实心方块
    HollowSquare,   ///< 空心方块
    Diamond,        ///< 实心菱形
    HollowDiamond,  ///< 空心菱形
    Triangle,       ///< 实心三角
    HollowTriangle, ///< 空心三角
    Cross,          ///< 十字架（+）
    XCross,         ///< 叉号（×）
    RingedCross     ///< 带圈的十字架（外圈 + 内部十字）
};

#endif // AGPLOT_TYPES_H
