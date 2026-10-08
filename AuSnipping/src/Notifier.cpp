#include "Notifier.h"

#include <QApplication>
#include <QCoreApplication>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>

#if defined(Q_OS_UNIX) && defined(AUSNIP_WITH_DBUS)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

#include "NotificationHub.h"
#endif

namespace {

// ---------- 托运气泡（Windows 主路径 / Linux 无 D-Bus 时的回退） ----------

std::function<void()> g_trayAction;     // 等待用户点击气泡时执行的回调

void trayNotify(const QString &title, const QString &body, bool *ok)
{
    *ok = false;
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    *ok = true;

    static QSystemTrayIcon *tray = nullptr;
    if (!tray) {
        tray = new QSystemTrayIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
        tray->setToolTip(QStringLiteral("AuSnipping"));
        // 点击气泡时执行挂起的回调（如打开保存的文件）
        QObject::connect(tray, &QSystemTrayIcon::messageClicked, [] {
            if (g_trayAction) {
                const std::function<void()> action = g_trayAction;
                g_trayAction = nullptr;
                action();
            }
        });
    }
    tray->show();
    tray->showMessage(title, body, QSystemTrayIcon::Information, 6000);
    // 托盘图标只为承载通知，超时后自动隐藏，不常驻状态栏
    QSystemTrayIcon *trayIcon = tray;
    QTimer::singleShot(12000, trayIcon, [trayIcon] { trayIcon->hide(); });
}

#if defined(Q_OS_UNIX) && defined(AUSNIP_WITH_DBUS)

// ---------- D-Bus 桌面通知（Linux 主路径），支持点击通知执行动作 ----------

NotificationHub *hub()
{
    static NotificationHub hub;
    return &hub;
}

// 发送带“打开”动作的通知，返回通知 id（失败返回 0）
void sendDBusNotify(const QString &title, const QString &body,
                    const std::function<void()> &onClicked)
{
    static bool signalsConnected = false;
    if (!signalsConnected) {
        signalsConnected = true;
        QDBusConnection bus = QDBusConnection::sessionBus();
        bus.connect(QStringLiteral("org.freedesktop.Notifications"),
                    QStringLiteral("/org/freedesktop/Notifications"),
                    QStringLiteral("org.freedesktop.Notifications"),
                    QStringLiteral("ActionInvoked"),
                    hub(), SLOT(actionInvoked(uint, QString)));
        bus.connect(QStringLiteral("org.freedesktop.Notifications"),
                    QStringLiteral("/org/freedesktop/Notifications"),
                    QStringLiteral("org.freedesktop.Notifications"),
                    QStringLiteral("NotificationClosed"),
                    hub(), SLOT(notificationClosed(uint, uint)));
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("/org/freedesktop/Notifications"),
        QStringLiteral("org.freedesktop.Notifications"),
        QStringLiteral("Notify"));
    msg.setArguments({QVariant(QStringLiteral("AuSnipping")),
                      QVariant::fromValue<uint>(0u),
                      QVariant(QString()),
                      QVariant(title),
                      QVariant(body),
                      QVariant(QStringList{QStringLiteral("default"), QStringLiteral("打开")}),
                      QVariantMap(),
                      QVariant::fromValue<int>(6000)});

    QDBusPendingCallWatcher *watcher =
            new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg));
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, watcher,
                     [onClicked](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        QDBusPendingReply<uint> reply = *self;
        if (!reply.isError())
            hub()->setPending(reply.value(), onClicked);
    });
}

#endif // Q_OS_UNIX && AUSNIP_WITH_DBUS

} // namespace

void Notifier::notify(const QString &title, const QString &body,
                      const std::function<void()> &onClicked)
{
#if defined(Q_OS_UNIX) && defined(AUSNIP_WITH_DBUS)
    static int dbusAvailable = -1;
    if (dbusAvailable < 0) {
        QDBusConnection bus = QDBusConnection::sessionBus();
        dbusAvailable = (bus.isConnected()
                         && bus.interface()->isServiceRegistered(
                                QStringLiteral("org.freedesktop.Notifications"))) ? 1 : 0;
    }
    if (dbusAvailable == 1) {
        sendDBusNotify(title, body, onClicked);
        return;
    }
#endif
    bool delivered = false;
    trayNotify(title, body, &delivered);
    g_trayAction = delivered ? onClicked : std::function<void()>();
}
