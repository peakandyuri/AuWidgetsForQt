#pragma once

#include <functional>
#include <QString>

namespace Notifier {

// 发送一条桌面通知。
//   Linux：优先走 D-Bus（org.freedesktop.Notifications，银河麒麟/UKUI 均支持），
//          通知带“打开”动作，用户点击通知时触发 onClicked（如打开保存的文件）；
//          不依赖托盘图标。
//   Windows：使用托运气泡，点击气泡触发 onClicked；托盘图标仅在通知期间
//          短暂出现，随后自动隐藏。
//   D-Bus 不可用时回退托运气泡（此时点击可能无效）。
void notify(const QString &title, const QString &body,
            const std::function<void()> &onClicked = std::function<void()>());

} // namespace Notifier
