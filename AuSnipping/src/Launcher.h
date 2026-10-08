#pragma once

#include <QPixmap>
#include <QWidget>

class QCheckBox;
class QKeySequenceEdit;
class QLabel;
class QPushButton;
class QRadioButton;
class QShortcut;
class QSpinBox;
class GlobalHotkey;

// 启动器窗口（主界面）：选择截图模式与选项（延迟 / 光标捕获 / 立即保存 /
// 快捷键），点击“开始截图”或按下快捷键触发。
//
// 开始截图时先把窗口设为全透明再隐藏，等待其完全消失（避开合成器的
// 隐藏淡出动画）后才抓屏，避免把半透明残影截进画面。
// 编辑窗关闭（或点“继续截图”）后回到本界面，可再次开始截图。
class Launcher : public QWidget
{
    Q_OBJECT

public:
    enum Mode { Fullscreen = 0, Region = 1, Window = 2 };

    explicit Launcher(bool quitAfter = false, QWidget *parent = nullptr);

    // 供命令行直接触发某模式（quitAfter 时结束后自动退出）
    void startMode(Mode mode);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Mode currentMode() const;
    void buildUi();
    void saveUiSettings();
    void applyHotkeySettings();
    void triggerCapture();
    void beginCapture(Mode mode);
    void showCountdown(Mode mode, int seconds);
    void doGrab(Mode mode);
    void handleCaptured(const QPixmap &image);
    void postCapture(const QPixmap &image);
    void showLauncher();
    QString autoSavePath() const;

    bool m_quitAfter = false;
    bool m_capturing = false;
    int m_editorsOpen = 0;          // 打开中的编辑窗口数（编辑期间可再触发截图叠开多个）

    QRadioButton *m_modeRadios[3] = { nullptr, nullptr, nullptr };
    QPushButton *m_startBtn = nullptr;
    QSpinBox *m_delaySpin = nullptr;
    QCheckBox *m_cursorCheck = nullptr;
    QCheckBox *m_autoSaveCheck = nullptr;
    QKeySequenceEdit *m_hotkeyEdit = nullptr;
    QCheckBox *m_globalHotkeyCheck = nullptr;
    QLabel *m_hotkeyStatus = nullptr;
    QShortcut *m_inAppShortcut = nullptr;
    GlobalHotkey *m_hotkey = nullptr;
};
