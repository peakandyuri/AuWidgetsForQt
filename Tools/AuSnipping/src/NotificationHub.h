#pragma once

#include <functional>
#include <QObject>

// D-Bus 桌面通知的信号接收器（org.freedesktop.Notifications）。
// 接收 ActionInvoked / NotificationClosed，在用户点击通知（default 动作）
// 时触发挂起的回调（如打开保存的文件）。仅在 Linux 的 D-Bus 通知路径使用。
class NotificationHub : public QObject
{
    Q_OBJECT

public:
    void setPending(uint id, const std::function<void()> &action)
    {
        m_id = id;
        m_action = action;
    }

public slots:
    void actionInvoked(uint id, const QString &actionKey)
    {
        if (id == m_id && m_action && actionKey == QLatin1String("default")) {
            const std::function<void()> action = m_action;
            m_action = nullptr;
            action();
        }
    }

    void notificationClosed(uint id, uint /*reason*/)
    {
        if (id == m_id)
            m_action = nullptr;
    }

private:
    uint m_id = 0;
    std::function<void()> m_action;
};
