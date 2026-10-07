#include "Notifier.h"

#include <QApplication>
#include <QStyle>
#include <QSystemTrayIcon>

#if defined(Q_OS_UNIX) && defined(AUSNIP_WITH_DBUS)
#include <QDBusConnection>
#include <QDBusMessage>
#endif

namespace {

void trayNotify(const QString &title, const QString &body)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    static QSystemTrayIcon *tray = nullptr;
    if (!tray) {
        tray = new QSystemTrayIcon(QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
        tray->show();
    }
    tray->showMessage(title, body, QSystemTrayIcon::Information, 4000);
}

} // namespace

void Notifier::notify(const QString &title, const QString &body)
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
                          QVariant(QStringList()),
                          QVariantMap(),
                          QVariant::fromValue<int>(4000)});
        QDBusConnection::sessionBus().asyncCall(msg);
        return;
    }
#endif
    trayNotify(title, body);
}
