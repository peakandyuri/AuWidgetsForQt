#include "Capturer.h"

#include <QCursor>
#include <QDebug>
#include <QGuiApplication>
#include <QLibrary>
#include <QMargins>
#include <QPainter>
#include <QScreen>
#include <QWindow>
#include <QtMath>
#include <functional>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef HAVE_X11
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

namespace {

// 逻辑矩形 → 设备像素矩形（下/右取整，保证不丢像素）
QRect deviceRect(const QRectF &logical, qreal dpr)
{
    const int left = qFloor(logical.left() * dpr);
    const int top = qFloor(logical.top() * dpr);
    const int right = qCeil(logical.right() * dpr);
    const int bottom = qCeil(logical.bottom() * dpr);
    return QRect(left, top, right - left, bottom - top);
}

// QRectF::translate 只接受 QPoint，这里补一个 QPointF 版本
QRectF translateRect(const QRectF &rect, const QPointF &offset)
{
    return rect.translated(offset.x(), offset.y());
}

#ifdef Q_OS_WIN

// ---------- Windows：运行期解析 dwmapi，避免硬链接依赖 ----------

typedef HRESULT (WINAPI *DwmGetWindowAttributePtr)(HWND, DWORD, PVOID, DWORD);

#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif
#ifndef DWMWA_EXTENDED_FRAME_BOUNDS
#define DWMWA_EXTENDED_FRAME_BOUNDS 9
#endif

DwmGetWindowAttributePtr dwmGetWindowAttribute()
{
    static DwmGetWindowAttributePtr ptr = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        QLibrary lib(QStringLiteral("dwmapi.dll"));
        if (lib.load())
            ptr = reinterpret_cast<DwmGetWindowAttributePtr>(lib.resolve("DwmGetWindowAttribute"));
    }
    return ptr;
}

struct EnumContext {
    POINT pt = { 0, 0 };        // 物理像素坐标（Windows 屏幕坐标系，主屏左上角为原点）
    qreal dpr = 1.0;
    QSet<WId> exclude;
    bool found = false;
    QRectF rect;                // 命中窗口的物理像素矩形
};

// EnumWindows 按 Z 序自顶向下枚举，首个命中即最顶层窗口
BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lParam)
{
    EnumContext *ctx = reinterpret_cast<EnumContext *>(lParam);

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId())
        return TRUE;                                // 跳过本进程窗口（遮罩层等）
    if (!IsWindowVisible(hwnd) || IsIconic(hwnd))
        return TRUE;
    if (ctx->exclude.contains(reinterpret_cast<WId>(hwnd)))
        return TRUE;
    if (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TRANSPARENT)
        return TRUE;                                // 点击穿透窗口不作为候选

    DwmGetWindowAttributePtr dwm = dwmGetWindowAttribute();
    if (dwm) {
        DWORD cloaked = 0;
        if (SUCCEEDED(dwm(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) && cloaked)
            return TRUE;                            // 被遮蔽（挂起）的 UWP 窗口
    }

    RECT rc;
    if (!GetWindowRect(hwnd, &rc))
        return TRUE;
    if (rc.right - rc.left < 4 || rc.bottom - rc.top < 4)
        return TRUE;
    if (!PtInRect(&rc, ctx->pt))
        return TRUE;

    RECT use = rc;
    if (dwm) {
        RECT frame = { 0, 0, 0, 0 };
        if (SUCCEEDED(dwm(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame)))
                && frame.right > frame.left && frame.bottom > frame.top) {
            use = frame;                            // 去掉 DWM 不可见的阴影边距
        }
    }
    ctx->rect = QRectF(qreal(use.left), qreal(use.top),
                       qreal(use.right - use.left), qreal(use.bottom - use.top));
    ctx->found = true;
    return FALSE;
}

QPixmap grabCursorWin(QPointF *hotspot)
{
    QPixmap result;
    *hotspot = QPointF();

    CURSORINFO ci;
    memset(&ci, 0, sizeof(ci));
    ci.cbSize = sizeof(ci);
    if (!GetCursorInfo(&ci) || !(ci.flags & CURSOR_SHOWING))
        return result;

    ICONINFO ii;
    memset(&ii, 0, sizeof(ii));
    if (!GetIconInfo(ci.hCursor, &ii))
        return result;

    // 32bpp DIB → QImage（GetDIBits 会把单色掩码展开成 0/0xFFFFFF）
    auto bitmapToImage = [](HBITMAP bitmap) -> QImage {
        if (!bitmap)
            return QImage();
        BITMAP bm;
        if (!GetObject(bitmap, sizeof(bm), &bm))
            return QImage();
        QImage img(qAbs(bm.bmWidth), qAbs(bm.bmHeight), QImage::Format_ARGB32);
        BITMAPINFOHEADER bi;
        memset(&bi, 0, sizeof(bi));
        bi.biSize = sizeof(bi);
        bi.biWidth = img.width();
        bi.biHeight = -img.height();                // 自顶向下
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        HDC hdc = GetDC(nullptr);
        const BOOL ok = GetDIBits(hdc, bitmap, 0, img.height(), img.bits(),
                                  reinterpret_cast<BITMAPINFO *>(&bi), DIB_RGB_COLORS);
        ReleaseDC(nullptr, hdc);
        return ok ? img : QImage();
    };

    const QImage color = ii.hbmColor ? bitmapToImage(ii.hbmColor) : QImage();
    const QImage mask = ii.hbmMask ? bitmapToImage(ii.hbmMask) : QImage();

    QImage out;
    if (!color.isNull()) {
        out = color;
        // 旧式彩色光标无 alpha 通道：用 AND 掩码生成透明度（掩码位 0 → 不透明）
        bool hasAlpha = false;
        for (int y = 0; y < out.height() && !hasAlpha; ++y) {
            for (int x = 0; x < out.width(); ++x) {
                if (qAlpha(out.pixel(x, y)) != 0) {
                    hasAlpha = true;
                    break;
                }
            }
        }
        if (!hasAlpha && !mask.isNull() && mask.size() == out.size()) {
            for (int y = 0; y < out.height(); ++y) {
                for (int x = 0; x < out.width(); ++x) {
                    const QRgb c = out.pixel(x, y);
                    const bool transparent = (mask.pixel(x, y) & 0x00ffffff) != 0;
                    out.setPixel(x, y, qRgba(qRed(c), qGreen(c), qBlue(c),
                                             transparent ? 0 : 255));
                }
            }
        }
    } else if (!mask.isNull() && mask.height() >= 2) {
        // 单色光标：掩码高度为 2 倍，上半是 AND，下半是 XOR
        const int w = mask.width();
        const int h = mask.height() / 2;
        out = QImage(w, h, QImage::Format_ARGB32);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const bool andBit = (mask.pixel(x, y) & 0x00ffffff) != 0;
                const bool xorBit = (mask.pixel(x, y + h) & 0x00ffffff) != 0;
                if (andBit && xorBit)
                    out.setPixel(x, y, qRgba(0, 0, 0, 200));        // 反色，简化处理
                else if (andBit)
                    out.setPixel(x, y, qRgba(0, 0, 0, 0));          // 透明
                else if (xorBit)
                    out.setPixel(x, y, qRgba(255, 255, 255, 255));  // 白
                else
                    out.setPixel(x, y, qRgba(0, 0, 0, 255));        // 黑
            }
        }
    }

    if (ii.hbmColor)
        DeleteObject(ii.hbmColor);
    if (ii.hbmMask)
        DeleteObject(ii.hbmMask);

    if (!out.isNull()) {
        result = QPixmap::fromImage(out);
        *hotspot = QPointF(qreal(ii.xHotspot), qreal(ii.yHotspot));
    }
    return result;
}

#elif defined(HAVE_X11)

// ---------- X11 ----------

// 与 Xfixes.h 中 XFixesCursorImage 布局完全一致，避免构建期依赖 xfixes 头文件。
// 注意字段宽度必须照抄真实头文件（x/y 为 short，宽高与热点为 unsigned short），
// 若误写成 int，64 位下会整体错位，宽高读到热点/填充字节，光标获取必然失败。
typedef struct {
    short x, y;                 // 光标位置（根坐标，未使用）
    unsigned short width, height;
    unsigned short xhot, yhot;
    unsigned long cursor_serial;
    unsigned long *pixels;      // ARGB（小端下低 32 位有效）
} AuXFixesCursorImage;

typedef AuXFixesCursorImage *(*XFixesGetCursorImagePtr)(Display *);

// QLibrary 必须常驻：局部对象析构会 dlclose 卸库，函数指针随之悬空
QLibrary &xFixesLibrary()
{
    static QLibrary library;
    static bool tried = false;
    if (!tried) {
        tried = true;
        for (const char *so : { "libXfixes.so.3", "libXfixes.so" }) {
            library.setFileName(QLatin1String(so));
            if (library.load())
                break;
        }
        if (!library.isLoaded())
            qWarning("AuSnipping: 无法加载 libXfixes（请安装 libxfixes3），本次截图不含光标");
    }
    return library;
}

XFixesGetCursorImagePtr xFixesGetCursorImage()
{
    static XFixesGetCursorImagePtr ptr = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        QLibrary &library = xFixesLibrary();
        if (library.isLoaded())
            ptr = reinterpret_cast<XFixesGetCursorImagePtr>(
                library.resolve("XFixesGetCursorImage"));
    }
    return ptr;
}

// XOpenDisplay(NULL) 与 Qt 使用同一 DISPLAY，仅用于只读查询窗口树
Display *x11Display()
{
    static Display *display = nullptr;
    static bool tried = false;
    if (!tried) {
        tried = true;
        display = XOpenDisplay(nullptr);
    }
    return display;
}

// EWMH _NET_FRAME_EXTENTS：WM 叠在客户端窗口四周的装饰厚度（左、右、上、下）。
// 客户端窗口矩形加上它才是用户看到的、带标题栏的完整窗口框。
QMargins x11FrameExtents(Display *display, Window window)
{
    QMargins extents;
    const Atom atom = XInternAtom(display, "_NET_FRAME_EXTENTS", True);
    if (atom == None)
        return extents;

    Atom type = None;
    int format = 0;
    unsigned long count = 0, after = 0;
    unsigned char *data = nullptr;
    if (XGetWindowProperty(display, window, atom, 0, 4, False, XA_CARDINAL,
                           &type, &format, &count, &after, &data) == Success
            && type == XA_CARDINAL && format == 32 && count >= 4 && data) {
        // format 为 32 的属性按 long 排布（64 位平台上每项 8 字节）
        const long *values = reinterpret_cast<const long *>(data);
        extents = QMargins(int(values[0]), int(values[2]),
                           int(values[1]), int(values[3]));
    }
    if (data)
        XFree(data);
    return extents;
}

// 找出包含指定点（全局逻辑坐标）的最顶层可视窗口。
// 优先使用 EWMH 堆叠客户端列表（自顶向下），命中后按 _NET_FRAME_EXTENTS
// 把 WM 装饰（标题栏/边框）补回，窗口截图因此能带上标题栏；
// 点恰在标题栏上等客户端列表覆盖不到的情况，回退到 X 窗口树，
// 取包含点的最外层可视窗口（窗口管理器包裹出的框架窗口本身已含装饰）。
QRectF x11WindowAt(qreal dpr, const QPointF &point, const QSet<WId> &exclude)
{
    Display *display = x11Display();
    if (!display)
        return QRectF();
    Window root = DefaultRootWindow(display);

    // 可视窗口换算到根坐标后的逻辑矩形；ok 为 false 表示不可视 / InputOnly
    auto visibleRect = [&](Window w, bool *ok) -> QRectF {
        if (ok)
            *ok = false;
        XWindowAttributes a;
        if (!XGetWindowAttributes(display, w, &a))
            return QRectF();
        if (a.map_state != IsViewable || a.c_class == InputOnly)
            return QRectF();
        int rx = 0, ry = 0;
        Window child = None;
        XTranslateCoordinates(display, w, root, 0, 0, &rx, &ry, &child);
        if (ok)
            *ok = true;
        return QRectF(qreal(rx) / dpr, qreal(ry) / dpr,
                      qreal(a.width) / dpr, qreal(a.height) / dpr);
    };

    auto containsPoint = [&point](const QRectF &logical) -> bool {
        return logical.width() >= 4.0 && logical.height() >= 4.0
               && logical.contains(point);
    };

    Atom stacking = XInternAtom(display, "_NET_CLIENT_LIST_STACKING", True);
    if (stacking != None) {
        Atom type = None;
        int format = 0;
        unsigned long count = 0, after = 0;
        unsigned char *data = nullptr;
        if (XGetWindowProperty(display, root, stacking, 0, 8192, False, XA_WINDOW,
                               &type, &format, &count, &after, &data) == Success
                && type == XA_WINDOW && format == 32 && data && count > 0) {
            const Window *wins = reinterpret_cast<const Window *>(data);
            QRectF result;
            for (unsigned long i = count; i > 0 && result.isNull(); --i) {
                const Window w = wins[i - 1];
                if (exclude.contains(static_cast<WId>(w)))
                    continue;
                bool ok = false;
                const QRectF client = visibleRect(w, &ok);
                if (!ok || !containsPoint(client))
                    continue;
                const QMargins ext = x11FrameExtents(display, w);
                result = client.adjusted(-ext.left() / dpr, -ext.top() / dpr,
                                         ext.right() / dpr, ext.bottom() / dpr);
            }
            XFree(data);
            if (!result.isNull())
                return result;
        }
    }

    // 自顶向下遍历 X 窗口树，取包含点的最外层可视窗口（框架已含装饰）；
    // 命中自己（exclude）的窗口时继续深入，相当于“透过”它看下面的窗口
    std::function<QRectF(Window)> searchTree = [&](Window w) -> QRectF {
        Window rootReturn = None, parentReturn = None;
        Window *children = nullptr;
        unsigned int count = 0;
        if (!XQueryTree(display, w, &rootReturn, &parentReturn, &children, &count)
                || !children)
            return QRectF();

        QRectF found;
        for (int i = int(count) - 1; i >= 0 && found.isNull(); --i) {
            const Window c = children[i];
            bool ok = false;
            const QRectF logical = visibleRect(c, &ok);
            if (!ok || !containsPoint(logical))
                continue;   // 子窗口被父窗口裁剪，父窗口不含点则子树同样不含
            if (exclude.contains(static_cast<WId>(c)))
                found = searchTree(c);
            else
                found = logical;
        }
        XFree(children);
        return found;
    };
    return searchTree(root);
}

QPixmap grabCursorX11(QPointF *hotspot)
{
    QPixmap result;
    *hotspot = QPointF();
    if (QGuiApplication::platformName() != QLatin1String("xcb")) {
        qWarning("AuSnipping: 非 X11(xcb) 平台，无法捕获光标");
        return result;
    }
    Display *display = x11Display();
    XFixesGetCursorImagePtr getCursor = xFixesGetCursorImage();
    if (!display) {
        qWarning("AuSnipping: XOpenDisplay 失败，无法捕获光标");
        return result;
    }
    if (!getCursor) {
        qWarning("AuSnipping: 无法取得 XFixesGetCursorImage，无法捕获光标");
        return result;
    }

    AuXFixesCursorImage *ci = getCursor(display);
    if (!ci || ci->width == 0 || ci->height == 0) {
        qWarning("AuSnipping: XFixesGetCursorImage 返回空（X 服务可能未启用 XFixes 2.0）");
        if (ci)
            XFree(ci);
        return result;
    }

    QImage img(int(ci->width), int(ci->height), QImage::Format_ARGB32);
    const int count = img.width() * img.height();
    for (int i = 0; i < count; ++i) {
        const quint32 p = quint32(ci->pixels[i] & 0xffffffffUL);
        img.setPixel(i % img.width(), i / img.width(),
                     qRgba(int((p >> 16) & 0xff), int((p >> 8) & 0xff),
                           int(p & 0xff), int((p >> 24) & 0xff)));
    }

    // 热点要在 XFree 之前读取（XFree 之后 ci 即失效）
    *hotspot = QPointF(qreal(ci->xhot), qreal(ci->yhot));
    XFree(ci);

    result = QPixmap::fromImage(img);
    return result;
}

#endif // platform

QPixmap grabCursor(QPointF *hotspot)
{
#if defined(Q_OS_WIN)
    return grabCursorWin(hotspot);
#elif defined(HAVE_X11)
    return grabCursorX11(hotspot);
#else
    qWarning("AuSnipping: 当前平台不支持捕获光标");
    *hotspot = QPointF();
    return QPixmap();
#endif
}

// 通用兜底：仅在 Qt 自己的顶层窗口中查找（Wayland / 未启用 X11 时仍可用）
QRectF qtWindowAt(const QPointF &point, const QSet<WId> &exclude)
{
    QRectF best;
    qreal bestArea = 0.0;
    const QList<QWindow *> windows = QGuiApplication::topLevelWindows();
    for (QWindow *w : windows) {
        if (!w || !w->isVisible() || (w->windowStates() & Qt::WindowMinimized))
            continue;
        if (exclude.contains(w->winId()))
            continue;
        const QRectF geo(w->frameGeometry());
        if (!geo.contains(point))
            continue;
        const qreal area = geo.width() * geo.height();
        if (best.isNull() || area < bestArea) {
            bestArea = area;
            best = geo;
        }
    }
    return best;
}

} // namespace

ScreenGrab Capturer::grab(QScreen *target, bool withCursor)
{
    ScreenGrab g;
    QScreen *screen = target ? target : QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return g;
    g.screen = screen;

    QPixmap desktop = screen->grabWindow(0);
    if (desktop.isNull())
        return g;

    g.desktop = desktop;
    g.dpr = desktop.devicePixelRatio() > 0 ? desktop.devicePixelRatio()
                                           : screen->devicePixelRatio();
    if (g.dpr <= 0)
        g.dpr = 1.0;
    g.screenGeometry = screen->geometry();
    const QScreen *primary = QGuiApplication::primaryScreen();
    g.virtualTopLeft = primary ? QPointF(primary->virtualGeometry().topLeft()) : QPointF(0, 0);

    // 目标屏幕区域（设备像素），供遮罩层 1:1 显示
    QRectF screenLocal(g.screenGeometry);
    screenLocal = translateRect(screenLocal, -g.virtualTopLeft);
    g.screenImage = desktop.copy(deviceRect(screenLocal, g.dpr)
                                         .intersected(desktop.rect())).toImage();
    if (g.screenImage.isNull())
        return g;

    if (withCursor) {
        g.cursorPos = QCursor::pos();
        QPointF hotspot;
        g.cursor = grabCursor(&hotspot);
        if (!g.cursor.isNull()) {
            g.cursor.setDevicePixelRatio(g.dpr);
            g.cursorHotspot = hotspot;
        }
    }
    g.valid = true;
    return g;
}

QPixmap Capturer::cropScreenRect(const ScreenGrab &g, const QRectF &localLogical)
{
    if (!g.valid || localLogical.isEmpty())
        return QPixmap();
    QRectF global = localLogical.translated(g.screenGeometry.topLeft());
    global = translateRect(global, -g.virtualTopLeft);
    const QRect dev = deviceRect(global, g.dpr).intersected(g.desktop.rect());
    if (dev.isEmpty())
        return QPixmap();
    QPixmap out = g.desktop.copy(dev);
    out.setDevicePixelRatio(g.dpr);
    return out;
}

QPixmap Capturer::cropDesktopDeviceRect(const ScreenGrab &g, const QRect &deviceRectIn)
{
    if (!g.valid)
        return QPixmap();
    const QRect dev = deviceRectIn.intersected(g.desktop.rect());
    if (dev.isEmpty())
        return QPixmap();
    QPixmap out = g.desktop.copy(dev);
    out.setDevicePixelRatio(g.dpr);
    return out;
}

NativeWindowHit Capturer::windowUnderPoint(const ScreenGrab &g,
                                           const QPointF &globalLogical,
                                           const QSet<WId> &exclude)
{
    NativeWindowHit hit;
    if (!g.valid)
        return hit;

    QRectF found;

#if defined(Q_OS_WIN)
    EnumContext ctx;
    ctx.pt.x = int(qRound(globalLogical.x() * g.dpr));
    ctx.pt.y = int(qRound(globalLogical.y() * g.dpr));
    ctx.dpr = g.dpr;
    ctx.exclude = exclude;
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&ctx));
    if (ctx.found)
        found = QRectF(ctx.rect.left() / g.dpr, ctx.rect.top() / g.dpr,
                       ctx.rect.width() / g.dpr, ctx.rect.height() / g.dpr);
#elif defined(HAVE_X11)
    if (QGuiApplication::platformName() == QLatin1String("xcb"))
        found = x11WindowAt(g.dpr, globalLogical, exclude);
#endif

    if (found.isNull() || found.isEmpty())
        found = qtWindowAt(globalLogical, exclude);
    if (found.isNull() || found.isEmpty())
        return hit;

    hit.valid = true;
    hit.logicalGlobal = found;
    QRectF desktopLocal = translateRect(found, -g.virtualTopLeft);
    hit.deviceDesktop = deviceRect(desktopLocal, g.dpr).intersected(g.desktop.rect());
    return hit;
}

void Capturer::composeCursor(QPixmap &image, const ScreenGrab &g, bool enabled,
                             const QPointF &imageGlobalOrigin)
{
    composeCursorAt(image, g, enabled, imageGlobalOrigin, g.cursorPos);
}

void Capturer::composeCursorAt(QPixmap &image, const ScreenGrab &g, bool enabled,
                               const QPointF &imageGlobalOrigin,
                               const QPointF &cursorPosGlobal)
{
    if (!enabled || !g.valid || image.isNull() || g.cursor.isNull())
        return;

    const qreal dpr = image.devicePixelRatio() > 0 ? image.devicePixelRatio() : 1.0;
    const QPointF hotspotLogical = g.cursorHotspot / g.dpr;
    const QPointF drawTopLeft = cursorPosGlobal - hotspotLogical - imageGlobalOrigin;

    const QRectF imageRect(0, 0, qreal(image.width()) / dpr, qreal(image.height()) / dpr);
    const QRectF cursorRect(drawTopLeft,
                            QSizeF(qreal(g.cursor.width()) / g.dpr,
                                   qreal(g.cursor.height()) / g.dpr));
    if (!cursorRect.intersects(imageRect)) {
        qWarning() << "[AuSnipping] 光标超出图像范围，未合成: 光标矩形" << cursorRect
                   << "图像矩形" << imageRect << "dpr" << dpr;
        return;
    }

    QPainter p(&image);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawPixmap(drawTopLeft, g.cursor);
    p.end();
}
