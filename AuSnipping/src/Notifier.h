#pragma once

#include <QString>

namespace Notifier {

// 发送一条桌面通知。
// Linux 优先走 D-Bus（org.freedesktop.Notifications，银河麒麟/UKUI 均支持），
// 失败回退到系统托运气泡；Windows 使用系统托运气泡。
void notify(const QString &title, const QString &body);

} // namespace Notifier
