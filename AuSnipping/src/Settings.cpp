#include "Settings.h"

#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {

QSettings *settings()
{
    static QSettings s(QStringLiteral("AuSnipping"), QStringLiteral("AuSnipping"));
    return &s;
}

} // namespace

int AppSettings::mode()
{
    int v = settings()->value(QStringLiteral("capture/mode"), 1).toInt();
    if (v < 0 || v > 2)
        v = 1;
    return v;
}

void AppSettings::setMode(int mode)
{
    settings()->setValue(QStringLiteral("capture/mode"), qBound(0, mode, 2));
}

int AppSettings::delaySeconds()
{
    int v = settings()->value(QStringLiteral("capture/delaySeconds"), 0).toInt();
    if (v < 0 || v > 5)
        v = 0;
    return v;
}

void AppSettings::setDelaySeconds(int seconds)
{
    settings()->setValue(QStringLiteral("capture/delaySeconds"), qBound(0, seconds, 5));
}

bool AppSettings::captureCursor()
{
    return settings()->value(QStringLiteral("capture/captureCursor"), false).toBool();
}

void AppSettings::setCaptureCursor(bool on)
{
    settings()->setValue(QStringLiteral("capture/captureCursor"), on);
}

bool AppSettings::autoSaveEnabled()
{
    return settings()->value(QStringLiteral("capture/autoSave"), false).toBool();
}

void AppSettings::setAutoSaveEnabled(bool on)
{
    settings()->setValue(QStringLiteral("capture/autoSave"), on);
}

QString AppSettings::hotkey()
{
    return settings()->value(QStringLiteral("hotkey/sequence"), QStringLiteral("Alt+S")).toString();
}

void AppSettings::setHotkey(const QString &sequence)
{
    settings()->setValue(QStringLiteral("hotkey/sequence"), sequence);
}

bool AppSettings::globalHotkeyEnabled()
{
    return settings()->value(QStringLiteral("hotkey/global"), true).toBool();
}

void AppSettings::setGlobalHotkeyEnabled(bool on)
{
    settings()->setValue(QStringLiteral("hotkey/global"), on);
}

QString AppSettings::lastSaveDir()
{
    return settings()->value(QStringLiteral("save/lastDir")).toString();
}

void AppSettings::setLastSaveDir(const QString &dir)
{
    settings()->setValue(QStringLiteral("save/lastDir"), dir);
}

QString AppSettings::defaultSaveDir()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (dir.isEmpty())
        dir = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    return dir;
}

QString AppSettings::defaultFileName()
{
    // 名称按当前时间：20261007_153012.png
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))
            + QStringLiteral(".png");
}
