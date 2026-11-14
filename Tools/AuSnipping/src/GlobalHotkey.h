#pragma once

#include <QAbstractNativeEventFilter>
#include <QElapsedTimer>
#include <QKeySequence>
#include <QObject>
#include <QString>

// 全局热键（在任意应用前台时都能触发“开始截图”）：
//   - Windows：RegisterHotKey + 原生消息过滤（WM_HOTKEY）
//   - X11（麒麟 UKUI 等）：XGrabKey + QSocketNotifier，
//     使用自建的 X 连接监听根窗口按键，不干扰 Qt 自身的事件
//   - Wayland 或注册失败：isActive() 为 false，界面给出提示，
//     由窗口内快捷键（QShortcut）兜底
class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    // 注册全局热键；空序列视为注销并返回 true
    bool registerSequence(const QKeySequence &sequence);
    void unregister();

    bool isActive() const { return m_active; }
    QString lastError() const { return m_lastError; }

signals:
    void triggered();

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
#endif

private:
    bool platformRegister();
    void platformUnregister();
    void debounceEmit();

    bool m_active = false;
    QString m_lastError;
    QElapsedTimer m_repeatGuard;
    Qt::Key m_key = Qt::Key_unknown;
    Qt::KeyboardModifiers m_mods = Qt::NoModifier;

#ifdef Q_OS_WIN
    enum : quint32 { kHotkeyId = 0xA0511 };
#endif
#ifdef HAVE_X11
    struct X11State;
    X11State *m_x11 = nullptr;
#endif
};
