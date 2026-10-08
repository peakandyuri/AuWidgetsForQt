#include "GlobalHotkey.h"

#include <QCoreApplication>
#include <QGuiApplication>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif
#endif

#ifdef HAVE_X11
#include <QSocketNotifier>
#include <X11/Xlib.h>
#endif

namespace {

// 从 QKeySequence 取出单键与修饰键（兼容 Qt5 的 int 与 Qt6 的 QKeyCombination）
bool splitSequence(const QKeySequence &sequence, Qt::Key *keyOut, Qt::KeyboardModifiers *modsOut)
{
    if (sequence.isEmpty())
        return false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QKeyCombination combination = sequence[0];
    *keyOut = combination.key();
    *modsOut = combination.keyboardModifiers();
#else
    const int combined = int(sequence[0]);
    *modsOut = Qt::KeyboardModifiers(combined & int(Qt::KeyboardModifierMask));
    *keyOut = Qt::Key(combined & ~int(Qt::KeyboardModifierMask));
#endif
    return *keyOut != Qt::Key_unknown && *keyOut != Qt::Key(0);
}

#ifdef Q_OS_WIN

// Qt::Key → Windows 虚拟键码（覆盖常用单键）
bool qtKeyToVirtualKey(Qt::Key key, UINT *vkOut)
{
    const int k = int(key);
    if ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9')) {
        *vkOut = UINT(k);
        return true;
    }
    if (k >= int(Qt::Key_F1) && k <= int(Qt::Key_F24)) {
        *vkOut = UINT(VK_F1 + (k - int(Qt::Key_F1)));
        return true;
    }
    switch (key) {
    case Qt::Key_Space:       *vkOut = VK_SPACE;     return true;
    case Qt::Key_Comma:       *vkOut = VK_OEM_COMMA; return true;
    case Qt::Key_Period:      *vkOut = VK_OEM_PERIOD; return true;
    case Qt::Key_Minus:       *vkOut = VK_OEM_MINUS; return true;
    case Qt::Key_Equal:       *vkOut = VK_OEM_PLUS;  return true;
    case Qt::Key_Semicolon:   *vkOut = VK_OEM_1;     return true;
    case Qt::Key_Slash:       *vkOut = VK_OEM_2;     return true;
    case Qt::Key_Backspace:   *vkOut = VK_BACK;      return true;
    case Qt::Key_Tab:         *vkOut = VK_TAB;       return true;
    case Qt::Key_Return:      *vkOut = VK_RETURN;    return true;
    case Qt::Key_Escape:      *vkOut = VK_ESCAPE;    return true;
    case Qt::Key_Print:       *vkOut = VK_SNAPSHOT;  return true;
    case Qt::Key_Insert:      *vkOut = VK_INSERT;    return true;
    case Qt::Key_Delete:      *vkOut = VK_DELETE;    return true;
    case Qt::Key_Home:        *vkOut = VK_HOME;      return true;
    case Qt::Key_End:         *vkOut = VK_END;       return true;
    case Qt::Key_PageUp:      *vkOut = VK_PRIOR;     return true;
    case Qt::Key_PageDown:    *vkOut = VK_NEXT;      return true;
    case Qt::Key_Left:        *vkOut = VK_LEFT;      return true;
    case Qt::Key_Up:          *vkOut = VK_UP;        return true;
    case Qt::Key_Right:       *vkOut = VK_RIGHT;     return true;
    case Qt::Key_Down:        *vkOut = VK_DOWN;      return true;
    default:                  return false;
    }
}

quint32 qtModsToWinMods(Qt::KeyboardModifiers mods)
{
    quint32 m = 0;
    if (mods & Qt::AltModifier)
        m |= MOD_ALT;
    if (mods & Qt::ControlModifier)
        m |= MOD_CONTROL;
    if (mods & Qt::ShiftModifier)
        m |= MOD_SHIFT;
    if (mods & Qt::MetaModifier)
        m |= MOD_WIN;
    return m;
}

#elif defined(HAVE_X11)

// 捕获 XGrabKey 的 BadAccess 错误（组合键被其它程序抢占）
bool g_x11GrabError = false;

int grabErrorHandler(Display *, XErrorEvent *)
{
    g_x11GrabError = true;
    return 0;
}

// Qt::Key → X keysym（覆盖常用单键）
bool qtKeyToKeysym(Qt::Key key, KeySym *symOut)
{
    const int k = int(key);
    if (k >= int(Qt::Key_A) && k <= int(Qt::Key_Z)) {
        *symOut = KeySym('a' + (k - int(Qt::Key_A)));
        return true;
    }
    if (k >= int(Qt::Key_0) && k <= int(Qt::Key_9)) {
        *symOut = KeySym('0' + (k - int(Qt::Key_0)));
        return true;
    }
    if (k >= int(Qt::Key_F1) && k <= int(Qt::Key_F24)) {
        *symOut = KeySym(0xffbe + (k - int(Qt::Key_F1)));
        return true;
    }
    switch (key) {
    case Qt::Key_Space:       *symOut = 0x20;   return true;
    case Qt::Key_Comma:       *symOut = 0x2c;   return true;
    case Qt::Key_Minus:       *symOut = 0x2d;   return true;
    case Qt::Key_Period:      *symOut = 0x2e;   return true;
    case Qt::Key_Slash:       *symOut = 0x2f;   return true;
    case Qt::Key_Semicolon:   *symOut = 0x3b;   return true;
    case Qt::Key_Apostrophe:  *symOut = 0x27;   return true;
    case Qt::Key_BracketLeft: *symOut = 0x5b;   return true;
    case Qt::Key_BracketRight: *symOut = 0x5d;  return true;
    case Qt::Key_Backslash:   *symOut = 0x5c;   return true;
    case Qt::Key_Equal:       *symOut = 0x3d;   return true;
    case Qt::Key_Print:       *symOut = 0xff61; return true;
    case Qt::Key_Insert:      *symOut = 0xff63; return true;
    case Qt::Key_Delete:      *symOut = 0xffff; return true;
    case Qt::Key_Home:        *symOut = 0xff50; return true;
    case Qt::Key_End:         *symOut = 0xff57; return true;
    case Qt::Key_PageUp:      *symOut = 0xff55; return true;
    case Qt::Key_PageDown:    *symOut = 0xff56; return true;
    case Qt::Key_Left:        *symOut = 0xff51; return true;
    case Qt::Key_Up:          *symOut = 0xff52; return true;
    case Qt::Key_Right:       *symOut = 0xff53; return true;
    case Qt::Key_Down:        *symOut = 0xff54; return true;
    case Qt::Key_Escape:      *symOut = 0xff1b; return true;
    case Qt::Key_Tab:         *symOut = 0xff09; return true;
    case Qt::Key_Return:      *symOut = 0xff0d; return true;
    case Qt::Key_Backspace:   *symOut = 0xff08; return true;
    default:                  return false;
    }
}

unsigned int qtModsToX11Mods(Qt::KeyboardModifiers mods)
{
    unsigned int m = 0;
    if (mods & Qt::ShiftModifier)
        m |= ShiftMask;
    if (mods & Qt::ControlModifier)
        m |= ControlMask;
    if (mods & Qt::AltModifier)
        m |= Mod1Mask;
    if (mods & Qt::MetaModifier)
        m |= Mod4Mask;
    return m;
}

#endif // platform

} // namespace

#ifdef HAVE_X11
struct GlobalHotkey::X11State
{
    Display *display = nullptr;
    Window root = None;
    int keycode = 0;
    unsigned int mods = 0;
    QSocketNotifier *notifier = nullptr;
};
#endif

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    QCoreApplication::instance()->installNativeEventFilter(this);
#endif
}

GlobalHotkey::~GlobalHotkey()
{
    unregister();
#ifdef Q_OS_WIN
    QCoreApplication::instance()->removeNativeEventFilter(this);
#endif
}

bool GlobalHotkey::registerSequence(const QKeySequence &sequence)
{
    platformUnregister();
    m_active = false;
    m_lastError.clear();

    if (sequence.isEmpty())
        return true;    // 视为清空快捷键

    Qt::Key key = Qt::Key_unknown;
    Qt::KeyboardModifiers mods = Qt::NoModifier;
    if (!splitSequence(sequence, &key, &mods)) {
        m_lastError = tr("不支持的按键组合");
        return false;
    }
    m_key = key;
    m_mods = mods;

    if (!platformRegister()) {
        m_key = Qt::Key_unknown;
        return false;
    }
    m_active = true;
    m_repeatGuard.start();
    return true;
}

void GlobalHotkey::unregister()
{
    platformUnregister();
    m_active = false;
    m_key = Qt::Key_unknown;
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr * /*result*/)
#else
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, long * /*result*/)
#endif
{
#ifdef Q_OS_WIN
    if (m_active && eventType == QByteArrayLiteral("windows_generic_MSG")) {
        MSG *msg = static_cast<MSG *>(message);
        if (msg && msg->message == WM_HOTKEY && UINT(msg->wParam) == kHotkeyId)
            debounceEmit();
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
#endif
    return false;
}

void GlobalHotkey::debounceEmit()
{
    // 抑制按住不放的自动重复触发
    if (m_repeatGuard.isValid() && m_repeatGuard.elapsed() < 300)
        return;
    m_repeatGuard.restart();
    emit triggered();
}

bool GlobalHotkey::platformRegister()
{
#if defined(Q_OS_WIN)
    UINT vk = 0;
    if (!qtKeyToVirtualKey(m_key, &vk)) {
        m_lastError = tr("不支持的按键组合");
        return false;
    }
    const UINT mods = qtModsToWinMods(m_mods) | MOD_NOREPEAT;
    if (!RegisterHotKey(nullptr, kHotkeyId, mods, vk)) {
        m_lastError = tr("注册失败，可能被其它程序占用");
        return false;
    }
    return true;

#elif defined(HAVE_X11)
    if (QGuiApplication::platformName() != QLatin1String("xcb")) {
        m_lastError = tr("当前桌面（Wayland）不支持全局热键");
        return false;
    }

    X11State *st = new X11State;
    st->display = XOpenDisplay(nullptr);
    if (!st->display) {
        m_lastError = tr("无法连接 X 服务");
        delete st;
        return false;
    }
    st->root = DefaultRootWindow(st->display);

    KeySym sym = 0;
    if (!qtKeyToKeysym(m_key, &sym)) {
        m_lastError = tr("不支持的按键组合");
        XCloseDisplay(st->display);
        delete st;
        return false;
    }
    const int keycode = XKeysymToKeycode(st->display, sym);
    if (keycode == 0) {
        m_lastError = tr("当前键盘布局不支持该按键");
        XCloseDisplay(st->display);
        delete st;
        return false;
    }
    const unsigned int mods = qtModsToX11Mods(m_mods);

    // 同时抓取 CapsLock / NumLock 变体，保证大小写与数字锁状态不影响触发
    g_x11GrabError = false;
    XErrorHandler oldHandler = XSetErrorHandler(grabErrorHandler);
    XGrabKey(st->display, keycode, mods, st->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(st->display, keycode, mods | LockMask, st->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(st->display, keycode, mods | Mod2Mask, st->root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(st->display, keycode, mods | LockMask | Mod2Mask, st->root, True, GrabModeAsync, GrabModeAsync);
    XSync(st->display, False);
    XSetErrorHandler(oldHandler);
    if (g_x11GrabError) {
        XUngrabKey(st->display, keycode, mods, st->root);
        XUngrabKey(st->display, keycode, mods | LockMask, st->root);
        XUngrabKey(st->display, keycode, mods | Mod2Mask, st->root);
        XUngrabKey(st->display, keycode, mods | LockMask | Mod2Mask, st->root);
        XSync(st->display, False);
        XCloseDisplay(st->display);
        delete st;
        m_lastError = tr("注册失败，可能被其它程序占用");
        return false;
    }

    XSelectInput(st->display, st->root, KeyPressMask);
    st->keycode = keycode;
    st->mods = mods;
    st->notifier = new QSocketNotifier(ConnectionNumber(st->display), QSocketNotifier::Read, this);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    // Qt 5.15 起新增 activated(QSocketDescriptor, QSocketNotifier::Type) 并弃用 activated(int)。
    // 部分发行版构建 Qt 时禁用了弃用接口（QT_DISABLE_DEPRECATED_BEFORE），
    // 此时头文件里 activated(int) 不存在，Qt6 起则彻底移除；
    // 两者并存时也必须显式取新签名消歧，故 5.15+ 统一用这一分支
    void (QSocketNotifier::*activatedNew)(QSocketDescriptor, QSocketNotifier::Type,
                                          QSocketNotifier::QPrivateSignal)
            = &QSocketNotifier::activated;
    connect(st->notifier, activatedNew, this, [this, st] {
#else
    void (QSocketNotifier::*activatedInt)(int) = &QSocketNotifier::activated;
    connect(st->notifier, activatedInt, this, [this, st](int) {
#endif
        while (XPending(st->display) > 0) {
            XEvent event;
            XNextEvent(st->display, &event);
            if (event.type == KeyPress
                    && int(event.xkey.keycode) == st->keycode
                    && (event.xkey.state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask)) == st->mods) {
                debounceEmit();
            }
        }
    });

    m_x11 = st;
    return true;

#else
    m_lastError = tr("当前平台不支持全局热键");
    return false;
#endif
}

void GlobalHotkey::platformUnregister()
{
#ifdef Q_OS_WIN
    UnregisterHotKey(nullptr, kHotkeyId);
#endif

#ifdef HAVE_X11
    if (m_x11) {
        delete m_x11->notifier;     // 先停掉通知器，避免关闭连接时被触发
        XUngrabKey(m_x11->display, m_x11->keycode, m_x11->mods, m_x11->root);
        XUngrabKey(m_x11->display, m_x11->keycode, m_x11->mods | LockMask, m_x11->root);
        XUngrabKey(m_x11->display, m_x11->keycode, m_x11->mods | Mod2Mask, m_x11->root);
        XUngrabKey(m_x11->display, m_x11->keycode, m_x11->mods | LockMask | Mod2Mask, m_x11->root);
        XSync(m_x11->display, False);
        XSelectInput(m_x11->display, m_x11->root, 0);
        XCloseDisplay(m_x11->display);
        delete m_x11;
        m_x11 = nullptr;
    }
#endif
}
