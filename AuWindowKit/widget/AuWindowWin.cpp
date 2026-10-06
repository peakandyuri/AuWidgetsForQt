#include "AuWindow.h"
#include "AuWindowStyle.h"

#include <QCursor>
#include <QWindow>

// ============================================================================
// AuWindow 的 Windows 平台实现。全部 Win32 / DWM 原生代码集中在本文件，
// AuWindow.cpp 只保留跨平台逻辑（经 platformXxx() 钩子进入本文件）。
// 其它平台编译本文件为空翻译单元。
// ============================================================================

#ifdef Q_OS_WIN

#include <qt_windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <uxtheme.h>   // MARGINS（DwmExtendFrameIntoClientArea 参数类型）
#ifndef SM_CXPADDEDBORDER
   // Vista 引入的常量；旧版 MinGW 头文件在默认 WINVER（<0x0600）下未声明
#  define SM_CXPADDEDBORDER 92
#endif
#ifndef WM_NCUAHDRAWCAPTION
   // 未公开的主题重绘消息（Chromium 惯例命名），旧头文件未定义
#  define WM_NCUAHDRAWCAPTION 0x00AE
#  define WM_NCUAHDRAWFRAME 0x00AF
#endif

namespace {

// 可见窗口矩形：Win10 的 WS_POPUP|WS_THICKFRAME 窗口，GetWindowRect 返回的
// 矩形含 ~7px 不可见调整边框（DWM 只渲染内芯）；DWMWA_EXTENDED_FRAME_BOUNDS
// 给出真实可见边界。命中测试 / 边框热区必须以可见矩形为基准，否则热区会
// 落在不可见条带上（边框无光标、无法缩放）。取不到时回退窗口矩形。
RECT auVisibleFrameBounds(HWND hwnd)
{
    RECT rc;
    typedef HRESULT(WINAPI * DwmGetWindowAttributePtr)(HWND, DWORD, PVOID, DWORD);
    static DwmGetWindowAttributePtr pDwa = []() {
        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        return dwm ? reinterpret_cast<DwmGetWindowAttributePtr>(
                         GetProcAddress(dwm, "DwmGetWindowAttribute"))
                   : nullptr;
    }();
    if (!pDwa || FAILED(pDwa(hwnd, 9 /* DWMWA_EXTENDED_FRAME_BOUNDS */,
                             &rc, sizeof(rc)))) {
        GetWindowRect(hwnd, &rc);
    }
    return rc;
}

} // namespace

/// Windows 内容区容器：Qt 在某些时序下（实测：显示带属主关系的子窗体后）
/// 会把本容器提升为原生子窗口（WS_CHILD），其矩形覆盖整个客户区——包括
/// 边框缩放热区。系统命中测试先询问该原生 child，其默认回答 HTCLIENT 会让
/// 窗体本身永远收不到边框命中（表现为：边框无缩放光标、无法拖拽缩放，
/// 但标题栏拖动 / 内容区点击不受影响，任务栏缩略图亦受扰）。此处覆盖
/// nativeEvent：光标落在窗体边框热区内时返回 HTTRANSPARENT，让系统穿透
/// 本 child 继续向父（窗体）做命中测试，边框判定交还给窗体的
/// WM_NCHITTEST 处理。容器保持非原生时本函数不会被调用，零影响。
class AuClientArea : public QWidget
{
public:
    explicit AuClientArea(AuWindow *window)
        : QWidget(window), m_window(window)
    {
    }

protected:
    bool nativeEvent(const QByteArray &eventType, void *message, AuNativeEventResult *result) override
    {
        if (eventType == QByteArrayLiteral("windows_generic_MSG")) {
            MSG *msg = static_cast<MSG *>(message);
            if (msg->message == WM_NCHITTEST) {
                HWND win = reinterpret_cast<HWND>(m_window->winId());
                if (!IsZoomed(win) && !m_window->isFullScreen() && !m_window->isMaximized()) {
                    // 以窗体的 DWM 可见矩形为基准（本控件横向铺满客户区）：
                    // 处于左右 / 底边框热区内时返回 HTTRANSPARENT，让系统穿透
                    // 本原生 child 继续命中父窗体（不处理则本 child 会吞掉
                    // 边框命中，边框缩放失效——Qt 在显示属主子窗体后会把
                    // 本容器提升为原生 child，实测为可复现的固定行为）。
                    // 热区用系统标准边框厚（不加宽：过宽会侵入标题栏 /
                    // 操作按钮区，把标题栏拖动误判为缩放）
                    const int frame = GetSystemMetrics(SM_CXSIZEFRAME)
                                      + GetSystemMetrics(SM_CXPADDEDBORDER);
                    const RECT rc = auVisibleFrameBounds(win);
                    const QPoint global(GET_X_LPARAM(msg->lParam),
                                        GET_Y_LPARAM(msg->lParam));
                    const QPoint local(global.x() - rc.left, global.y() - rc.top);
                    const int w = rc.right - rc.left;
                    const int h = rc.bottom - rc.top;
                    if (local.x() < frame
                        || local.x() >= w - frame
                        || local.y() >= h - frame) {
                        *result = HTTRANSPARENT;
                        return true;
                    }
                }
            }
        }
        return QWidget::nativeEvent(eventType, message, result);
    }

private:
    AuWindow *m_window;
};

// ---- 平台钩子 ----

void AuWindow::platformInit()
{
    // 保留 WS_THICKFRAME 等原生样式并剥离 WS_CAPTION（见 applyWindowStyles），
    // 在 nativeEvent 中去掉可见边框，以换取 DWM 阴影 / Snap / 原生动画，
    // 且杜绝 DWM 向客户区顶部重绘原生标题栏。手动拖动路径的边缘吸附（为
    // X11 无系统 Snap 场景设计）在 Windows 上必须关闭：拖顶/贴边由系统
    // 原生 Snap 处理，本模块的 setGeometry 贴靠会与系统最大化/还原状态
    // 互相污染（无法还原、边框热区失效）。
    m_edgeSnapEnabled = false;
    m_client = new AuClientArea(this);   // 边框热区 HTTRANSPARENT（见类注释）
}

void AuWindow::platformWinIdChanged()
{
    if (winId()) {
        applyWindowStyles();
        applyWindowRoundedCorners();
        applyTaskbarVisibility();   // 属主窗口需 APPWINDOW 才有任务栏按钮
    }
}

void AuWindow::platformFirstShow() {}

void AuWindow::platformResized()
{
    applyWindowRoundedCorners(); // Win10 窗口区域需跟随尺寸
}

void AuWindow::platformStateChanged()
{
    // 最大化/还原瞬间 Qt 的 resize 优化会把旧帧缓冲 blit 进新缓冲，只重绘
    // 新暴露的边条——标题栏等几何变化未覆盖到的旧像素会残留（未最大化时
    // 的标题栏残影）。relayout + 同步全量重绘立即清除；再排队一次全量重绘
    // 兜底（StateChange 与 Resize 两种到达顺序都能覆盖）。
    repaint();
    QMetaObject::invokeMethod(this, [this] { repaint(); }, Qt::QueuedConnection);
}

void AuWindow::platformRadiusChanged()
{
    applyWindowRoundedCorners();
}

int AuWindow::platformEffectiveRadius(int radius) const
{
    // 生效半径：Win10 无系统圆角（直角保 DWM 阴影），强制 0——否则绘制层
    // 会在不透明窗口上画圆角路径，圆角外露出调色板底色（暗色主题下的
    // 白角）。Win11 返回真实值，圆角由 DWM 裁剪（客户端按方形绘制即可）。
    return m_dwmCornerOk ? radius : 0;
}

void AuWindow::platformButtonVisibilityChanged()
{
    // WS_MAXIMIZEBOX 跟随最大化按钮可见性（决定 Aero Snap / 双击最大化
    // 是否可用）；需在原生窗口存在后重新应用样式
    if (winId())
        applyWindowStyles();
}

void AuWindow::platformStyleChanged()
{
    applyWindowRoundedCorners();   // DWM 边框线明暗跟随窗体背景色
}

bool AuWindow::platformSetTaskbarVisible(bool visible)
{
    Q_UNUSED(visible)   // applyTaskbarVisibility 读取 m_taskbarVisible
    applyTaskbarVisibility();
    return true;
}

bool AuWindow::platformStartSystemMove()
{
    // 标题栏拖动一律交给系统移动循环（SC_MOVE）。到达本函数说明按下落在
    // HTCLIENT 区（标题栏上的自定义组件），系统不会自发进入移动循环；先
    // 释放进程鼠标捕获，再重放一次 HTCAPTION 按下，让 DefWindowProc 启动
    // 原生拖动——Aero Snap / Snap Assist / 还原矩形全部由系统维护。Qt 5.12
    // 无 startSystemMove，若走 Qt 手动 move() 拖动会完全绕过系统吸附（无
    // Assist、还原矩形污染），且手动路径自带的边缘吸附会在 Windows 上与
    // 系统最大化状态互相污染（实测：拖顶后无法还原、边框热区失效、焦点
    // 切换闪现经典边框）。
    if (windowHandle())
        windowHandle()->setMouseGrabEnabled(false);
    if (m_titleBar)
        m_titleBar->releaseMouse();
    ReleaseCapture();
    m_titleBar->resetDragState();
    PostMessageW(reinterpret_cast<HWND>(winId()), WM_NCLBUTTONDOWN, HTCAPTION,
                 MAKELPARAM(QCursor::pos().x(), QCursor::pos().y()));
    return true;
}

bool AuWindow::platformStartSystemResize(Qt::Edges edges)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    return m_systemMoveEnabled && windowHandle()
           && windowHandle()->startSystemResize(edges);
#else
    Q_UNUSED(edges)
    return false;
#endif
}

void AuWindow::platformMouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event)   // X11 专属的系统缩放宽限期回退判定，Windows 不涉及
}

// Win11 DWM 圆角 / Win10 窗口区域由系统裁剪窗体形状，客户端圆角浮层无用
bool AuWindow::platformClipsWindowShape() const
{
    return true;
}

bool AuWindow::platformIsComposited() const
{
    return true;
}

// Windows 上拖动一律原生转发（WM_NCHITTEST），最大化还原由系统完成，
// 不经此钩子
bool AuWindow::platformStartMaximizeRestoreDrag()
{
    return false;
}

// ---- Windows：原生无边框实现 ----

void AuWindow::applyWindowStyles()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    // 保留 WS_THICKFRAME（系统缩放热区 / DWM 阴影 / Aero Snap）等原生样式，
    // 剥离 WS_CAPTION：caption 框架走 DWM 合成路径绘制，WM_NCACTIVATE 的
    // lParam=-1 只能拦住 DefWindowProc 的重绘、拦不住 DWM 自身的经典边框
    // 呈现（实测：带 caption 时启动闪经典边框、焦点切换持续闪烁；无 caption
    // 时两者皆无）。保留 Qt 默认的 WS_POPUP 基底。原生矩形与可见边界的
    // ~7px 差异（不可见调整边框）由命中测试按 DWM 可见矩形对齐解决。
    // Snap / 拖动 / 双击最大化由 WM_NCHITTEST 的 HTCAPTION 提供。
    //
    // WS_MAXIMIZEBOX 动态判定：系统 Aero Snap（拖顶最大化 / 贴边半屏）与
    // 双击标题栏最大化只对带最大化框的窗口生效。固定尺寸（min==max）或
    // 隐藏了最大化按钮的窗体不置位——拖拽到屏幕边缘不触发任何吸附。
    LONG_PTR wanted = WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX;
    const bool resizable = minimumSize() != maximumSize();
    if (resizable && m_titleBar && m_titleBar->isButtonVisible(AuTitleBar::ButtonMaximize))
        wanted |= WS_MAXIMIZEBOX;
    const LONG_PTR updated = (style & ~(WS_CAPTION | WS_MAXIMIZEBOX)) | wanted;
    if (updated == style)
        return;
    SetWindowLongPtrW(hwnd, GWL_STYLE, updated);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                     | SWP_NOACTIVATE);
    // 无 caption 的窗口 DWM 默认不绘制框架阴影；向客户区扩展 1px 边框
    // 强制 DWM 恢复完整框架渲染（外阴影 + 系统细边框，Chromium 同款手法）
    typedef HRESULT(WINAPI * DwmExtendFrameIntoClientAreaPtr)(HWND, const MARGINS *);
    static DwmExtendFrameIntoClientAreaPtr pExtend = []() {
        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        return dwm ? reinterpret_cast<DwmExtendFrameIntoClientAreaPtr>(
                         GetProcAddress(dwm, "DwmExtendFrameIntoClientArea"))
                   : nullptr;
    }();
    if (pExtend) {
        const MARGINS margins = {0, 0, 0, 1};
        pExtend(hwnd, &margins);
    }
}

void AuWindow::applyWindowRoundedCorners()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;
    // 注意读原始半径（m_cornerRadii）：windowRadius() 在 Win10 上会强制
    // 返回生效值 0，此处需要真实设置值来判定 DWM 偏好
    const int maxR = qMax(qMax(m_cornerRadii[0], m_cornerRadii[1]),
                          qMax(m_cornerRadii[2], m_cornerRadii[3]));
    const int pref = maxR <= 0 ? 1        // DWMWCP_DONOTROUND
                     : maxR <= 5 ? 3      // DWMWCP_ROUNDSMALL（4px）
                                 : 2;     // DWMWCP_ROUND（8px）
    typedef HRESULT(WINAPI * DwmSetWindowAttributePtr)(HWND, DWORD, LPCVOID, DWORD);
    static DwmSetWindowAttributePtr pDwmSetWindowAttribute = []() {
        HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
        return dwm ? reinterpret_cast<DwmSetWindowAttributePtr>(
                         GetProcAddress(dwm, "DwmSetWindowAttribute"))
                   : nullptr;
    }();
    if (pDwmSetWindowAttribute
        && SUCCEEDED(pDwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */,
                                            &pref, sizeof(pref)))) {
        m_dwmCornerOk = true;   // Win11+：系统圆角 + 配套 DWM 阴影
    } else {
        // Win10 及以下：放弃窗口区域（SetWindowRgn）圆角。区域会把窗口转入
        // 区域框架模式——DWM 停止渲染该窗口框架（阴影消失）、激活切换时
        // 退回经典 GDI 边框（闪现原始边框）、吸附记账退化。取舍（已与使用
        // 方确认）：保系统原生阴影，圆角降级为直角。setWindowRadius() /
        // setCornerRadius() 在 Win10 上无视觉效果（Win11 仍生效）。
        m_dwmCornerOk = false;
    }
    // DWM 框架元素（边框线等）跟随窗体明暗主题：暗色窗体下 DWM 边框线也
    // 是暗色，避免暗色界面四周出现一圈突兀的浅色描边。Win10 1809+ /
    // Win11 支持；不支持时静默忽略。随样式切换重调本函数即可刷新。
    if (pDwmSetWindowAttribute) {
        const QColor bg = style()->windowBackgroundColor();
        const BOOL dark = bg.lightness() < 128;
        pDwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */,
                               &dark, sizeof(dark));
    }
}

void AuWindow::applyTaskbarVisibility()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd)
        return;
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    // WS_EX_APPWINDOW：有属主（owner）的窗口默认不进任务栏（系统对话框规
    // 则）；置上该扩展样式强制获得任务栏按钮（同时具备缩略图预览）。
    // 隐藏任务栏时清掉它并置 WS_EX_TOOLWINDOW（连 Alt+Tab 一并隐藏）。
    LONG_PTR wanted = m_taskbarVisible ? WS_EX_APPWINDOW : WS_EX_TOOLWINDOW;
    LONG_PTR clear = m_taskbarVisible ? WS_EX_TOOLWINDOW : WS_EX_APPWINDOW;
    if (((exStyle & wanted) == wanted) && ((exStyle & clear) == 0))
        return;
    exStyle = (exStyle & ~clear) | wanted;
    SetWindowLongPtrW(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER
                     | SWP_NOACTIVATE);
}

bool AuWindow::handleNcCalcSize(void *message, AuNativeEventResult *result)
{
    MSG *msg = static_cast<MSG *>(message);
    *result = 0;
    if (!msg->wParam)
        return true;
    NCCALCSIZE_PARAMS *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
    const HWND hwnd = msg->hwnd;
    if (IsZoomed(hwnd)) {
        // 最大化时窗口矩形比工作区大一圈边框，把客户区缩回工作区
        const int frameX = GetSystemMetrics(SM_CXSIZEFRAME)
                           + GetSystemMetrics(SM_CXPADDEDBORDER);
        const int frameY = GetSystemMetrics(SM_CYSIZEFRAME)
                           + GetSystemMetrics(SM_CXPADDEDBORDER);
        params->rgrc[0].left += frameX;
        params->rgrc[0].top += frameY;
        params->rgrc[0].right -= frameX;
        params->rgrc[0].bottom -= frameY;
        // 自动隐藏的任务栏所在边额外让出 2px，避免误触唤出
        APPBARDATA state = {sizeof(state)};
        if ((SHAppBarMessage(ABM_GETSTATE, &state) & ABS_AUTOHIDE) != 0) {
            APPBARDATA pos = {sizeof(pos)};
            SHAppBarMessage(ABM_GETTASKBARPOS, &pos);
            switch (pos.uEdge) {
            case ABE_LEFT:   params->rgrc[0].left += 2; break;
            case ABE_TOP:    params->rgrc[0].top += 2; break;
            case ABE_RIGHT:  params->rgrc[0].right -= 2; break;
            case ABE_BOTTOM: params->rgrc[0].bottom -= 2; break;
            default: break;
            }
        }
    }
    return true; // 返回 0：移除全部可见非客户区（保留 DWM 阴影 / Snap / 动画）
}

bool AuWindow::handleNcHitTest(void *message, AuNativeEventResult *result)
{
    MSG *msg = static_cast<MSG *>(message);
    const HWND hwnd = msg->hwnd;
    const QPoint global(GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
    // 以 DWM 可见矩形为基准（见 auVisibleFrameBounds）：原生矩形含不可见
    // 调整边框，直接用它判热区会把缩放热区挪到不可见条带上
    const RECT rc = auVisibleFrameBounds(hwnd);
    const int winW = rc.right - rc.left;
    const int winH = rc.bottom - rc.top;
    const QPoint local(global.x() - rc.left, global.y() - rc.top);
    LRESULT hit = HTCLIENT;

    // 边框缩放热区（最大化 / 全屏时禁用；固定尺寸的轴不提供热区——
    // 固定尺寸对话框不应出现缩放光标）
    if (!IsZoomed(hwnd) && !isFullScreen()) {
        // 热区用系统标准边框厚，不加宽：过宽会侵入标题栏 / 操作按钮区，
        // 把标题栏拖动误判为缩放（吸附后标题栏出现上下拉伸光标即此因）
        const int frame = GetSystemMetrics(SM_CXSIZEFRAME)
                          + GetSystemMetrics(SM_CXPADDEDBORDER);
        const QSize minS = minimumSize();
        const QSize maxS = maximumSize();
        const bool hResizable = minS.width() != maxS.width();
        const bool vResizable = minS.height() != maxS.height();
        const bool left = hResizable && local.x() < frame;
        const bool right = hResizable && local.x() >= winW - frame;
        const bool top = vResizable && local.y() < frame;
        const bool bottom = vResizable && local.y() >= winH - frame;
        if (top && left)
            hit = HTTOPLEFT;
        else if (top && right)
            hit = HTTOPRIGHT;
        else if (bottom && left)
            hit = HTBOTTOMLEFT;
        else if (bottom && right)
            hit = HTBOTTOMRIGHT;
        else if (top)
            hit = HTTOP;
        else if (bottom)
            hit = HTBOTTOM;
        else if (left)
            hit = HTLEFT;
        else if (right)
            hit = HTRIGHT;
    }

    // 标题栏空白区交给系统作为标题栏：原生拖动 / 边缘分屏（Aero Snap）/ 双击最大化
    if (hit == HTCLIENT && m_titleBar && m_titleBar->isVisible()) {
        const QPoint tbPos = m_titleBar->mapFromGlobal(global);
        if (m_titleBar->rect().contains(tbPos) && m_titleBar->hitTestCaption(tbPos))
            hit = HTCAPTION;
    }

    *result = static_cast<long>(hit);
    return true;
}

bool AuWindow::handleMinMaxInfo(void *message, AuNativeEventResult *result)
{
    MSG *msg = static_cast<MSG *>(message);
    MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);

    // 无 WS_CAPTION 的弹出式窗口，系统默认最大化到整个显示器：盖住任务栏，
    // 且四周不外溢边框——与 WM_NCCALCSIZE 的最大化内缩不匹配（客户区缩小
    // 一圈边框后，顶部/四周会露出一圈无人绘制的残留条带）。这里改写最大化
    // 目标为工作区，并按有 caption 窗口的惯例让边框外溢到屏幕外：客户区经
    // NCCALCSIZE 内缩后恰好铺满工作区，任务栏保持可见、残留消失。
    HMONITOR monitor = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info;
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        const int frameX = GetSystemMetrics(SM_CXSIZEFRAME)
                           + GetSystemMetrics(SM_CXPADDEDBORDER);
        const int frameY = GetSystemMetrics(SM_CYSIZEFRAME)
                           + GetSystemMetrics(SM_CXPADDEDBORDER);
        mmi->ptMaxSize.x = (info.rcWork.right - info.rcWork.left) + 2 * frameX;
        mmi->ptMaxSize.y = (info.rcWork.bottom - info.rcWork.top) + 2 * frameY;
        mmi->ptMaxPosition.x = info.rcWork.left - frameX;
        mmi->ptMaxPosition.y = info.rcWork.top - frameY;
    }

    // 原生边框缩放遵守窗体最小/最大宽高（最大跟踪尺寸不钳制时，固定尺寸
    // 窗口仍可被系统缩放循环拉大——热区判定之外的第二道保险）
    mmi->ptMinTrackSize.x = qMax<LONG>(minimumWidth(), mmi->ptMinTrackSize.x);
    mmi->ptMinTrackSize.y = qMax<LONG>(minimumHeight(), mmi->ptMinTrackSize.y);
    mmi->ptMaxTrackSize.x = qMin<LONG>(maximumWidth(), mmi->ptMaxTrackSize.x);
    mmi->ptMaxTrackSize.y = qMin<LONG>(maximumHeight(), mmi->ptMaxTrackSize.y);
    *result = 0;
    return true;
}

bool AuWindow::nativeEvent(const QByteArray &eventType, void *message, AuNativeEventResult *result)
{
    if (eventType != QByteArrayLiteral("windows_generic_MSG"))
        return QWidget::nativeEvent(eventType, message, result);
    MSG *msg = static_cast<MSG *>(message);
    switch (msg->message) {
    case WM_NCACTIVATE:
        // 激活/失活统一传 lParam=-1：DefWindowProc 更新激活状态但跳过非
        // 客户区重绘。无 WS_CAPTION 时系统仍会在激活切换瞬间以传统路径
        // 呈现一帧经典框架（DWM 合成侧，日志证实无 WM_NCPAINT 到达窗口），
        // -1 让 USER32 连这一帧也不画。早期版本曾怀疑它破坏吸附记账，
        // 现已证实当时真凶是手动拖动路径（已改为原生转发，见
        // platformStartSystemMove），本处理与吸附不再冲突。
        *result = DefWindowProcW(msg->hwnd, msg->message, msg->wParam, -1);
        return true;
    case WM_NCPAINT:
    case WM_NCUAHDRAWCAPTION:   // 未公开的主题消息：视觉样式开启时经此
    case WM_NCUAHDRAWFRAME:     // 重绘标题栏 / 边框（Chromium 同款处理）
        // 客户区已覆盖整个窗口，任何非客户区绘制都是幽灵残迹，直接吞掉
        *result = 0;
        return true;
    case WM_NCMOUSEMOVE:
        // 悬停标题栏空白区时系统会重绘原生按钮悬停态（幽灵来源）；
        // 吞掉不影响拖动 / Snap（由 WM_NCLBUTTONDOWN 系消息驱动）
        *result = 0;
        return true;
    case WM_NCCALCSIZE:
        return handleNcCalcSize(message, result);
    case WM_NCHITTEST:
        return handleNcHitTest(message, result);
    case WM_GETMINMAXINFO:
        return handleMinMaxInfo(message, result);
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
        // 直接交系统处理：原生标题栏拖动 / Snap 布局 / 双击最大化 / 边框缩放
        *result = DefWindowProcW(msg->hwnd, msg->message, msg->wParam, msg->lParam);
        return true;
    default:
        break;
    }
    return QWidget::nativeEvent(eventType, message, result);
}

// ---- 仅 Windows 有意义的成员，其余成员见各平台文件 ----

// 窗体形状由系统管理（Win11 DWM 圆角 / Win10 直角），客户侧不做 mask 裁剪
void AuWindow::updateMask() {}

void AuWindow::applyPositionViaWM(const QPoint &pos)
{
    move(pos);
}

// X11 专属的抓取恢复轮询，Windows 不涉及
void AuWindow::startGrabRecoveryPoll() {}
void AuWindow::pollGrabRecovery() {}

#endif // Q_OS_WIN
