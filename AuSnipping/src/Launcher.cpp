#include "Launcher.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDir>
#include <QEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QShortcut>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include "Capturer.h"
#include "EditWindow.h"
#include "GlobalHotkey.h"
#include "Notifier.h"
#include "Settings.h"
#include "SnipOverlay.h"

namespace {

const char *kModeTexts[3] = { "全屏", "区域", "窗口" };
const char *kModeTips[3] = {
    "把整个屏幕直接复制到剪贴板",
    "在屏幕上拖动框选截图区域",
    "自动捕获光标下的窗口"
};

} // namespace

Launcher::Launcher(bool quitAfter, QWidget *parent)
    : QWidget(parent)
    , m_quitAfter(quitAfter)
{
    setWindowTitle(QStringLiteral("AuSnipping 截图"));
    buildUi();
    applyHotkeySettings();
}

void Launcher::buildUi()
{
    // ---- 模式选择 + 开始截图 ----
    auto *modeRow = new QHBoxLayout;
    modeRow->setSpacing(10);
    for (int i = 0; i < 3; ++i) {
        auto *radio = new QRadioButton(QString::fromUtf8(kModeTexts[i]), this);
        radio->setToolTip(QString::fromUtf8(kModeTips[i]));
        radio->setChecked(AppSettings::mode() == i);
        modeRow->addWidget(radio);
        m_modeRadios[i] = radio;
    }
    modeRow->addStretch();

    m_startBtn = new QPushButton(tr("开始截图"), this);
    m_startBtn->setMinimumHeight(38);
    m_startBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1e88e5; color: white; font-weight: bold;"
        " font-size: 14px; padding: 0 28px; border: none; border-radius: 4px; }"
        "QPushButton:hover { background: #1976d2; }"
        "QPushButton:pressed { background: #1565c0; }"));
    connect(m_startBtn, &QPushButton::clicked, this, &Launcher::triggerCapture);

    // ---- 选项 ----
    auto *optRow = new QHBoxLayout;
    optRow->setSpacing(12);
    optRow->addWidget(new QLabel(tr("延迟:"), this));
    m_delaySpin = new QSpinBox(this);
    m_delaySpin->setRange(0, 5);
    m_delaySpin->setSuffix(QStringLiteral(" 秒"));
    m_delaySpin->setSpecialValueText(tr("无"));
    m_delaySpin->setValue(AppSettings::delaySeconds());
    optRow->addWidget(m_delaySpin);

    m_cursorCheck = new QCheckBox(tr("捕获鼠标光标"), this);
    m_cursorCheck->setChecked(AppSettings::captureCursor());
    optRow->addWidget(m_cursorCheck);

    m_autoSaveCheck = new QCheckBox(tr("立即保存"), this);
    m_autoSaveCheck->setToolTip(tr("开启后截图直接保存到图片目录并发送通知，"
                                   "不再弹出编辑窗口"));
    m_autoSaveCheck->setChecked(AppSettings::autoSaveEnabled());
    optRow->addWidget(m_autoSaveCheck);
    optRow->addStretch();

    // ---- 快捷键 ----
    auto *hotkeyRow = new QHBoxLayout;
    hotkeyRow->setSpacing(8);
    hotkeyRow->addWidget(new QLabel(tr("快捷键:"), this));
    m_hotkeyEdit = new QKeySequenceEdit(this);
    m_hotkeyEdit->setKeySequence(QKeySequence(AppSettings::hotkey()));
    m_hotkeyEdit->setFixedWidth(130);
    m_hotkeyEdit->installEventFilter(this);     // 编辑时临时停用快捷键，便于录入
    connect(m_hotkeyEdit, &QKeySequenceEdit::keySequenceChanged,
            this, [this] { applyHotkeySettings(); });
    hotkeyRow->addWidget(m_hotkeyEdit);

    m_globalHotkeyCheck = new QCheckBox(tr("全局生效"), this);
    m_globalHotkeyCheck->setToolTip(tr("全局生效时，在任何应用前台按下快捷键都能开始截图\n"
                                       "（Windows / X11 支持；Wayland 下仅窗口内生效）"));
    m_globalHotkeyCheck->setChecked(AppSettings::globalHotkeyEnabled());
    connect(m_globalHotkeyCheck, &QCheckBox::toggled, this, [this] { applyHotkeySettings(); });
    hotkeyRow->addWidget(m_globalHotkeyCheck);
    hotkeyRow->addStretch();

    m_hotkeyStatus = new QLabel(this);
    m_hotkeyStatus->setStyleSheet(QStringLiteral("color: #666;"));

    auto *tip = new QLabel(tr("截图自动复制到剪贴板。\n"
                              "未开启“立即保存”时，会打开编辑窗口进行标注。\n"
                              "开启后保存到当前用户的图片目录，文件名按当前时间。"), this);
    tip->setStyleSheet(QStringLiteral("color: #666;"));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);
    mainLayout->addLayout(modeRow);
    mainLayout->addWidget(m_startBtn);
    mainLayout->addLayout(optRow);
    mainLayout->addLayout(hotkeyRow);
    mainLayout->addWidget(m_hotkeyStatus);
    mainLayout->addWidget(tip);

    m_hotkey = new GlobalHotkey(this);
    connect(m_hotkey, &GlobalHotkey::triggered, this, &Launcher::triggerCapture);

    m_inAppShortcut = new QShortcut(this);
    connect(m_inAppShortcut, &QShortcut::activated, this, &Launcher::triggerCapture);

    setFixedWidth(440);
}

Launcher::Mode Launcher::currentMode() const
{
    for (int i = 0; i < 3; ++i) {
        if (m_modeRadios[i] && m_modeRadios[i]->isChecked())
            return Mode(i);
    }
    return Region;
}

void Launcher::startMode(Mode mode)
{
    beginCapture(mode);
}

bool Launcher::eventFilter(QObject *obj, QEvent *event)
{
    // 录入新快捷键期间临时停用快捷键（含全局注册），避免按键被拦截
    if (obj == m_hotkeyEdit) {
        if (event->type() == QEvent::FocusIn) {
            m_inAppShortcut->setEnabled(false);
            m_hotkey->unregister();
        } else if (event->type() == QEvent::FocusOut) {
            m_inAppShortcut->setEnabled(true);
            applyHotkeySettings();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void Launcher::saveUiSettings()
{
    AppSettings::setMode(int(currentMode()));
    AppSettings::setDelaySeconds(m_delaySpin->value());
    AppSettings::setCaptureCursor(m_cursorCheck->isChecked());
    AppSettings::setAutoSaveEnabled(m_autoSaveCheck->isChecked());
}

void Launcher::applyHotkeySettings()
{
    const QKeySequence sequence = m_hotkeyEdit ? m_hotkeyEdit->keySequence() : QKeySequence();
    AppSettings::setHotkey(sequence.toString(QKeySequence::PortableText));
    if (m_globalHotkeyCheck)
        AppSettings::setGlobalHotkeyEnabled(m_globalHotkeyCheck->isChecked());
    if (m_inAppShortcut)
        m_inAppShortcut->setKey(sequence);

    if (sequence.isEmpty()) {
        m_hotkey->unregister();
        m_hotkeyStatus->setText(tr("未设置快捷键"));
        return;
    }
    const QString name = sequence.toString(QKeySequence::NativeText);
    if (!m_globalHotkeyCheck || !m_globalHotkeyCheck->isChecked()) {
        m_hotkey->unregister();
        m_hotkeyStatus->setText(tr("全局未启用：仅本窗口内 %1 生效").arg(name));
    } else if (m_hotkey->registerSequence(sequence)) {
        m_hotkeyStatus->setText(tr("全局快捷键 %1 已生效").arg(name));
    } else {
        m_hotkeyStatus->setText(tr("全局不可用（%1），仅本窗口内 %2 生效")
                                .arg(m_hotkey->lastError(), name));
    }
}

void Launcher::triggerCapture()
{
    if (m_capturing) {
        // 连续截图会话进行中（编辑窗口打开）：把编辑窗口带到前台，
        // 提示先关闭它（关闭后会自动回到上一个截图界面）
        if (m_editor) {
            m_editor->raise();
            m_editor->activateWindow();
        }
        return;
    }
    saveUiSettings();
    beginCapture(currentMode());
}

void Launcher::beginCapture(Mode mode)
{
    if (m_capturing)
        return;
    m_capturing = true;

    // 先把窗口设为全透明再隐藏：合成器来不及呈现隐藏淡出动画，
    // 避免半透明残影被截进画面；随后再等一拍确保完全消失。
    setWindowOpacity(0.0);
    hide();

    const int settleMs = 150;
    const int delay = AppSettings::delaySeconds();
    if (delay > 0)
        QTimer::singleShot(settleMs, this, [this, mode, delay] { showCountdown(mode, delay); });
    else
        QTimer::singleShot(settleMs, this, [this, mode] { doGrab(mode); });
}

void Launcher::showCountdown(Mode mode, int seconds)
{
    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();

    // 输入穿透的小提示条，不影响延迟期间使用其它应用
    auto *cd = new QLabel(nullptr);
    cd->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                       | Qt::Tool | Qt::WindowTransparentForInput);
    cd->setAttribute(Qt::WA_DeleteOnClose);
    // 主窗口隐藏期间它是唯一可见顶层窗口，关闭时不能让 Qt 退出程序
    cd->setAttribute(Qt::WA_QuitOnClose, false);
    cd->setStyleSheet(QStringLiteral(
        "background: #e53935; color: white; font-size: 20px; font-weight: bold;"
        "padding: 12px 28px; border-radius: 8px;"));
    cd->setText(QStringLiteral("%1 秒后将截图…").arg(seconds));
    cd->adjustSize();
    if (screen) {
        const QRect geo = screen->geometry();
        cd->move(geo.center().x() - cd->width() / 2, geo.top() + 80);
    }
    cd->show();

    auto *timer = new QTimer(cd);
    timer->setInterval(1000);
    int remain = seconds;
    connect(timer, &QTimer::timeout, cd, [cd, timer, this, mode, remain]() mutable {
        --remain;
        if (remain <= 0) {
            timer->stop();
            cd->setWindowOpacity(0.0);  // 同样先全透明再关闭，避免残影
            cd->close();
            QTimer::singleShot(120, this, [this, mode] { doGrab(mode); });
        } else {
            cd->setText(QStringLiteral("%1 秒后将截图…").arg(remain));
        }
    });
    timer->start();
}

void Launcher::doGrab(Mode mode)
{
    const ScreenGrab grab = Capturer::grab();
    if (!grab.valid || grab.screenImage.isNull()) {
        m_capturing = false;
        QMessageBox::warning(this, QStringLiteral("AuSnipping"), tr("抓取屏幕失败。"));
        if (m_quitAfter)
            qApp->quit();
        else
            showLauncher();
        return;
    }

    if (mode == Fullscreen) {
        // 全屏截图：直接复制整个屏幕
        QPixmap image = grab.desktop;
        Capturer::composeCursor(image, grab, AppSettings::captureCursor(), grab.virtualTopLeft);
        postCapture(image);
        return;
    }

    auto *overlay = new SnipOverlay(grab,
                                    mode == Window ? SnipOverlay::WindowMode
                                                   : SnipOverlay::RegionMode,
                                    AppSettings::captureCursor());
    connect(overlay, &SnipOverlay::captured, this, &Launcher::handleCaptured);
    connect(overlay, &SnipOverlay::cancelled, this, [this] {
        m_capturing = false;
        if (m_quitAfter)
            qApp->quit();
        else
            showLauncher();
    });
    m_overlay = overlay;
    overlay->show();
}

void Launcher::handleCaptured(const QPixmap &image)
{
    // 遮罩层只是隐藏（选区与冻结画面保留），会话是否结束由 postCapture 决定
    postCapture(image);
}

QString Launcher::autoSavePath() const
{
    const QString dir = AppSettings::defaultSaveDir();
    QDir().mkpath(dir);
    QString path = dir + QLatin1Char('/') + AppSettings::defaultFileName();
    if (QFileInfo::exists(path)) {
        const QString base = path.left(path.length() - 4);  // 去掉 ".png"
        for (int i = 1; QFileInfo::exists(path); ++i)
            path = QStringLiteral("%1_%2.png").arg(base).arg(i);
    }
    return path;
}

void Launcher::postCapture(const QPixmap &image)
{
    // 截图后先放进内存剪贴板
    QGuiApplication::clipboard()->setPixmap(image);

    // 区域 / 窗口截图存在遮罩会话：编辑窗关闭或保存完成后回到遮罩继续截图，
    // 直到用户按 Esc 结束；全屏截图没有遮罩会话，结束后回主界面
    const bool overlaySession = !m_overlay.isNull();

    if (AppSettings::autoSaveEnabled()) {
        const QString path = autoSavePath();
        const bool ok = image.save(path);
        Notifier::notify(ok ? tr("截图已保存") : tr("截图保存失败"),
                        tr("图片已存储在 %1").arg(path));
        if (overlaySession)
            returnToOverlay();
        else if (m_quitAfter)
            qApp->quit();
        else {
            m_capturing = false;
            showLauncher();
        }
        return;
    }

    auto *editor = new EditWindow(image);
    connect(editor, &QObject::destroyed, this, [this] {
        --m_editorsOpen;
        const bool continueRequested = m_editorContinue;
        m_editorContinue = false;   // 单次消费，避免影响后续编辑窗的关闭路径
        if (!m_overlay.isNull()) {
            returnToOverlay();      // 连续截图：编辑窗关闭后回到上一个截图界面
        } else if (continueRequested) {
            // 全屏模式没有遮罩会话：直接开始下一张全屏截图
            m_capturing = false;
            beginCapture(Fullscreen);
        } else if (m_editorsOpen <= 0) {
            m_capturing = false;
            if (m_quitAfter)
                qApp->quit();
            else
                showLauncher();
        }
    });
    connect(editor, &EditWindow::continueCapture, this, [this, editor] {
        // 从编辑界面直接继续截图：不询问保存（画面已实时同步在剪贴板中）
        m_editorContinue = true;
        editor->deleteLater();
    });
    ++m_editorsOpen;
    m_editor = editor;
    editor->show();

    if (!overlaySession)
        m_capturing = false;        // 全屏截图：允许立即再次触发
}

void Launcher::returnToOverlay()
{
    if (m_overlay.isNull())
        return;
    m_capturing = true;             // 会话继续，快捷键不开启新截图
    m_overlay->show();              // showEvent 会重新置顶并 grabKeyboard
    m_overlay->activateWindow();
}

void Launcher::showLauncher()
{
    setWindowOpacity(1.0);
    show();
    raise();
    activateWindow();
}
