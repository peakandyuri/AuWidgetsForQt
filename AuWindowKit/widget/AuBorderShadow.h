#ifndef AUBORDERSHADOW_H
#define AUBORDERSHADOW_H

//
// AuWindow 边框阴影渲染器（模块内部实现细节，非公共 API）。
//
// 算法来源（麒麟 peony / qt5-ukui-platformtheme 同款）：
//   - peony   libpeony-qt/effects/border-shadow-effect.cpp：圆角矩形填
//     阴影色后经 Qt 私有导出函数 qt_blurImage（QGraphicsBlurEffect 内核，
//     Q_WIDGETS_EXPORT，Qt5/Qt6 均可用）做高斯模糊，裁剪到窗口边框环绘制；
//   - qt5-ukui-platformtheme ConfigShadowHelper（KWindowShadow 前身）：
//     将一次模糊的结果按九宫格切为 8 块贴图——4 块角贴图原尺寸摆放、
//     4 块 1px 边条拉伸填充，因此缩放窗口零重绘成本，也无 peony 主窗口
//     因 resize 反复全图模糊导致的撕裂。
//
// 贴图布局（与 KWin 对 _KDE_NET_WM_SHADOW 的九宫格摆放精确对齐）：
//
//   ┌────────┬─┬────────┐   图幅 = 2*maxRadius + 2*blur + 1（+dpr 缩放）
//   │ 角 TL  │↑│ 角 TR  │   内部窗口矩形位于 (blur, blur)，边长
//   ├────────┼─┼────────┤   2*maxRadius + 1；1px 中心条供拉伸。
//   │←       │ │       →│
//   ├────────┼─┼────────┤
//   │ 角 BL  │↓│ 角 BR  │
//   └────────┴─┴────────┘
//
// 角贴图尺寸 (blur + radius)²、边条 1px × (blur + maxRadius)，与
// peony/ukui 的切法逐像素一致；客户端绘制（AuWindow::paintEvent）与
// X11 WM 侧上传（AuWindowX11 的 _KDE_NET_WM_SHADOW 通道）共用本图。

#include <QColor>
#include <QImage>
#include <QRect>
#include <QRectF>

namespace AuWindowDetail {

/// 渲染一张模糊阴影九宫格贴图（四角独立半径，dpr 为设备像素比缩放）。
/// blur 为高斯模糊半径（同时也是窗口矩形到贴图边缘的渐隐距离）；
/// color 一般带 alpha（默认标定值见 AuWindowStyle::shadowColor）。
QImage auRenderBorderShadowTile(const QColor &color, int blur,
                                int radiusTopLeft, int radiusTopRight,
                                int radiusBottomLeft, int radiusBottomRight,
                                qreal dpr);

/// 九宫格参数：角贴图边长（blur + maxRadius）与内窗矩形偏移（blur）。
/// 客户端绘制 / X11 上传共用，保证两通道对齐一致。
inline int auBorderShadowCornerSize(int blur, int maxRadius) { return blur + maxRadius; }
inline int auBorderShadowInnerOffset(int blur) { return blur; }

} // namespace AuWindowDetail

#endif // AUBORDERSHADOW_H
