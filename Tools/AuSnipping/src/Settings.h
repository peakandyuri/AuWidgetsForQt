#pragma once

#include <QString>

// 全局设置（QSettings 持久化）：
// 截图模式、延迟时间、光标捕获、立即保存、快捷键、上次保存目录
class AppSettings
{
public:
    static int mode();                      // 截图模式（0 全屏 / 1 区域 / 2 窗口）
    static void setMode(int mode);

    static int delaySeconds();              // 0 表示不延迟，1~5 秒
    static void setDelaySeconds(int seconds);

    static bool captureCursor();
    static void setCaptureCursor(bool on);

    static bool autoSaveEnabled();          // 截图后立即保存（不弹编辑窗口）
    static void setAutoSaveEnabled(bool on);

    static QString hotkey();                // 开始截图快捷键（PortableText），默认 Alt+S
    static void setHotkey(const QString &sequence);

    static bool globalHotkeyEnabled();      // 快捷键是否全局生效
    static void setGlobalHotkeyEnabled(bool on);

    static QString lastSaveDir();           // 上一次选择的保存目录
    static void setLastSaveDir(const QString &dir);

    static QString defaultSaveDir();        // 默认保存目录（当前用户图片目录）
    static QString defaultFileName();       // 按当前时间生成的默认文件名
};
