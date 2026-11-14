#include "MainWindow.h"
#include "AuMessageBox.h"
#include "AuMessageBoxStyle.h"
#include "AuWindowStyle.h"
#include "demos/DialogWindows.h"
#include "demos/MyWin.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

namespace {
/// 简单的演示页容器：垂直布局 + 统一留白。
QVBoxLayout *makePageLayout(QWidget *parent)
{
    QVBoxLayout *lay = new QVBoxLayout(parent);
    lay->setContentsMargins(24, 24, 24, 24);
    lay->setSpacing(12);
    return lay;
}

QLabel *makeHint(const QString &text, QWidget *parent)
{
    QLabel *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color: #888;"));
    return label;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : AuWindow(parent)
{
    setWindowTitle(QStringLiteral("AuWindowKit 窗体演示"));
    setWindowRadius(0);
    setMinimumSize(640, 480);

    // ---- 标题栏自定义组件：三槽位演示 ----
    QLabel *versionLabel = new QLabel(QStringLiteral("v1.0.0"));
    versionLabel->setStyleSheet(QStringLiteral("color:#888; font-size:12px;"));
    addTitleBarWidget(versionLabel, AuTitleBar::SlotAfterTitle, false); // 纯展示：不抢占鼠标

    QLabel *clockLabel = new QLabel;
    clockLabel->setStyleSheet(QStringLiteral("color:#888; font-size:12px;"));
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [clockLabel]() {
        clockLabel->setText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")));
    });
    timer->start(1000);
    clockLabel->setText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")));
    addTitleBarWidget(clockLabel, AuTitleBar::SlotCenter, false);      // 居中：纯展示

    QPushButton *feedbackButton = new QPushButton(QStringLiteral("反馈"));
    feedbackButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:#2f7fe0; color:white; border:none; border-radius:3px;"
        "  padding:2px 10px; font-size:12px; }"
        "QPushButton:hover { background:#3d8ceb; }"));
    feedbackButton->connect(feedbackButton, &QPushButton::clicked, this, []() {
        AuMessageBox::information(nullptr, QStringLiteral("反馈"),
                                  QStringLiteral("抢占鼠标的标题栏按钮：点击不受拖动影响"));
    });
    addTitleBarWidget(feedbackButton, AuTitleBar::SlotBeforeButtons);  // 靠右：可交互组件，抢占鼠标

    buildContent();
    connect(this, &AuWindow::closeIntercepted, this, &MainWindow::onCloseIntercepted);
}

void MainWindow::buildContent()
{
    QTabWidget *tabs = new QTabWidget;
    tabs->addTab(buildWindowControlPage(), QStringLiteral("窗体特性"));
    tabs->addTab(buildDialogPage(), QStringLiteral("Dialog 模态"));
    tabs->addTab(buildMessageBoxPage(), QStringLiteral("消息框"));
    tabs->addTab(buildInheritPage(), QStringLiteral("继承与 ui 文件"));
    setCentralWidget(tabs);
}

QWidget *MainWindow::buildWindowControlPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *pageLayout = makePageLayout(page);

    m_themeButton = new QPushButton(QStringLiteral("切换暗黑主题"));
    connect(m_themeButton, &QPushButton::clicked, this, &MainWindow::switchGlobalTheme);
    pageLayout->addWidget(m_themeButton);

    QHBoxLayout *radiusRow = new QHBoxLayout;
    radiusRow->addWidget(new QLabel(QStringLiteral("窗体圆角(px)：")));
    QSpinBox *radiusSpin = new QSpinBox;
    radiusSpin->setRange(0, 40);
    radiusSpin->setValue(0);
    connect(radiusSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &AuWindow::setWindowRadius);
    radiusRow->addWidget(radiusSpin);
    radiusRow->addStretch(1);
    pageLayout->addLayout(radiusRow);

    // 四角独立圆角：可做出"仅顶部圆角""顶部大、底部小"等形态
    QHBoxLayout *cornerRow = new QHBoxLayout;
    cornerRow->addWidget(new QLabel(QStringLiteral("四角独立(左上/右上/左下/右下)：")));
    QList<QSpinBox *> cornerSpins;
    const QList<Qt::Corner> corners = { Qt::TopLeftCorner, Qt::TopRightCorner,
                                        Qt::BottomLeftCorner, Qt::BottomRightCorner };
    for (Qt::Corner corner : corners) {
        QSpinBox *spin = new QSpinBox;
        spin->setRange(0, 40);
        spin->setValue(0);
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this,
                [this, corner](int value) { setCornerRadius(corner, value); });
        cornerSpins.append(spin);
        cornerRow->addWidget(spin);
    }
    // 统一圆角变化时同步四个独立角（阻断信号避免回环）
    connect(radiusSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [cornerSpins](int value) {
                for (QSpinBox *spin : cornerSpins) {
                    spin->blockSignals(true);
                    spin->setValue(value);
                    spin->blockSignals(false);
                }
            });
    cornerRow->addStretch(1);
    pageLayout->addLayout(cornerRow);

    QHBoxLayout *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(new QLabel(QStringLiteral("标题栏按钮：")));
    const QList<QPair<AuTitleBar::Button, QString>> buttons = {
        { AuTitleBar::ButtonMinimize, QStringLiteral("最小化") },
        { AuTitleBar::ButtonMaximize, QStringLiteral("最大化") },
        { AuTitleBar::ButtonClose, QStringLiteral("关闭") }
    };
    for (const auto &pair : buttons) {
        QCheckBox *box = new QCheckBox(pair.second);
        box->setChecked(true);
        connect(box, &QCheckBox::toggled, this, [this, pair](bool on) {
            setButtonVisible(pair.first, on);
        });
        buttonRow->addWidget(box);
    }
    buttonRow->addStretch(1);
    pageLayout->addLayout(buttonRow);

    QHBoxLayout *miscRow = new QHBoxLayout;
    QCheckBox *taskbarBox = new QCheckBox(QStringLiteral("显示任务栏图标（含缩略图）"));
    taskbarBox->setChecked(true);
    connect(taskbarBox, &QCheckBox::toggled, this, &AuWindow::setTaskbarVisible);
    miscRow->addWidget(taskbarBox);

    QCheckBox *interceptBox = new QCheckBox(QStringLiteral("拦截关闭（点 X 不直接退出）"));
    connect(interceptBox, &QCheckBox::toggled, this, &AuWindow::setCloseInterceptionEnabled);
    miscRow->addWidget(interceptBox);
    miscRow->addStretch(1);
    pageLayout->addLayout(miscRow);

    QHBoxLayout *snapRow = new QHBoxLayout;
    QCheckBox *snapBox = new QCheckBox(QStringLiteral("边缘吸附（拖动中靠近屏幕上/左/右缘出现预览，松手贴靠）"));
    snapBox->setChecked(isEdgeSnapEnabled());
    connect(snapBox, &QCheckBox::toggled, this, &AuWindow::setEdgeSnapEnabled);
    snapRow->addWidget(snapBox);
    snapRow->addStretch(1);
    pageLayout->addLayout(snapRow);

    // 窗口间磁吸：拖近其它顶层窗口（X11 含全系统窗口）时自动贴合边缘。
    // 仅手动拖动通道生效，开启后拖动自动改走手动通道。
    QHBoxLayout *winSnapRow = new QHBoxLayout;
    QCheckBox *winSnapBox = new QCheckBox(QStringLiteral("窗口间吸附（拖动中靠近其它窗口自动磁吸贴合边缘）"));
    winSnapBox->setChecked(isWindowSnapEnabled());
    connect(winSnapBox, &QCheckBox::toggled, this, &AuWindow::setWindowSnapEnabled);
    winSnapRow->addWidget(winSnapBox);
    winSnapRow->addStretch(1);
    pageLayout->addLayout(winSnapRow);

    // 边框阴影：X11 上由 WM 的 border-only 装饰在窗外合成（运行时开关即时
    // 生效，由无装饰切回会重映射一次）；Windows 由 DWM 原生提供（本开关不
    // 影响）；圆角窗体（radius>0）在 X11 上无 WM 阴影（矩形装饰会破坏圆角）。
    QHBoxLayout *shadowRow = new QHBoxLayout;
    QCheckBox *shadowBox = new QCheckBox(QStringLiteral("边框阴影（X11 由 WM 合成，运行时开关生效；圆角窗体无 WM 阴影）"));
    shadowBox->setChecked(isBorderShadowEnabled());
    connect(shadowBox, &QCheckBox::toggled, this, &AuWindow::setBorderShadowEnabled);
    shadowRow->addWidget(shadowBox);
    shadowRow->addStretch(1);
    pageLayout->addLayout(shadowRow);

    // 系统通道 = 把拖动/缩放交给窗口管理器（Qt 5.15 startSystemMove/Resize），
    // 吸附即 WM 原生 Quick Tiling；代价是接管后本进程鼠标事件管线依赖 WM 实现，
    // 实测 UKUI/KWin 系 WM 上存在几何脱节风险（见 setSystemWindowMoveEnabled 文档）。
    // 默认走 Qt 手动通道：模块内置吸附预览（蓝色高亮）即为该路径的系统级替代实现。
    QHBoxLayout *sysRow = new QHBoxLayout;
    QCheckBox *sysMoveBox = new QCheckBox(QStringLiteral("使用系统拖动/缩放通道（WM 原生贴靠；当前 WM 上若拖动异常请关闭）"));
    sysMoveBox->setChecked(isSystemWindowMoveEnabled());
    connect(sysMoveBox, &QCheckBox::toggled, this, &AuWindow::setSystemWindowMoveEnabled);
    sysRow->addWidget(sysMoveBox);
    sysRow->addStretch(1);
    pageLayout->addLayout(sysRow);

    pageLayout->addWidget(makeHint(
        QStringLiteral("说明：标题栏可拖动，拖到屏幕上/左/右边缘触发系统分屏与最大化；"
                       "边框可缩放（尊重最小/最大宽高）；双击标题栏空白切换最大化；"
                       "标题栏上方三个自定义组件分别演示靠左/居中/靠右槽位；"
                       "四角独立圆角支持仅顶部 / 单角等任意组合；"
                       "Dialog 页提供固定尺寸与限制最大尺寸的窗体验证入口。"), this));

    QCheckBox *hideBarBox = new QCheckBox(QStringLiteral("隐藏标题栏（无边框纯内容模式）"));
    connect(hideBarBox, &QCheckBox::toggled, this, [this](bool on) {
        titleBar()->setVisible(!on);
        relayout();
    });
    pageLayout->addWidget(hideBarBox);

    pageLayout->addStretch(1);
    return page;
}

QWidget *MainWindow::buildDialogPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *pageLayout = makePageLayout(page);

    QPushButton *windowModal = new QPushButton(QStringLiteral("父窗口模态（exec，Esc 关闭，回车确定）"));
    connect(windowModal, &QPushButton::clicked, this, [this]() {
        DialogWindow dialog(AuWindow::Modality::WindowModal, this);
        const int code = dialog.exec();
        AuMessageBox::information(this, QStringLiteral("exec 返回"),
                                  code == AuWindow::Accepted
                                      ? QStringLiteral("返回：Accepted（确定）")
                                      : QStringLiteral("返回：Rejected（取消/Esc/点X）"));
    });
    pageLayout->addWidget(windowModal);

    QPushButton *appModal = new QPushButton(QStringLiteral("应用级模态（exec，无父窗口）"));
    connect(appModal, &QPushButton::clicked, this, [this]() {
        DialogWindow dialog(AuWindow::Modality::ApplicationModal, nullptr);
        dialog.exec();
    });
    pageLayout->addWidget(appModal);

    QPushButton *modeless = new QPushButton(QStringLiteral("非模态 Dialog（show）"));
    connect(modeless, &QPushButton::clicked, this, [this]() {
        DialogWindow *dialog = new DialogWindow(AuWindow::Modality::WindowModal, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    pageLayout->addWidget(modeless);

    QPushButton *fixedSize = new QPushButton(QStringLiteral("固定尺寸窗体（setFixedSize，不可缩放）"));
    connect(fixedSize, &QPushButton::clicked, this, [this]() {
        AuWindow dialog(this);
        dialog.setDialogMode(true);
        dialog.setModality(Modality::WindowModal);
        dialog.setWindowTitle(QStringLiteral("固定尺寸对话框"));
        dialog.setWindowRadius(8);
        dialog.setButtonVisible(AuTitleBar::ButtonMaximize, false);
        dialog.setButtonVisible(AuTitleBar::ButtonMinimize, false);
        QLabel *label = new QLabel(QStringLiteral("setFixedSize(440, 260)：\n"
                                                   "窗体不可缩放，边缘不显示缩放手柄；\n"
                                                   "标题栏与关闭按钮在任何状态下都应跟随窗体宽度。"));
        label->setWordWrap(true);
        QVBoxLayout *lay = new QVBoxLayout(&dialog);
        lay->setContentsMargins(24, 20, 24, 16);
        lay->addWidget(label);
        lay->addStretch(1);
        QPushButton *ok = new QPushButton(QStringLiteral("确定"));
        connect(ok, &QPushButton::clicked, &dialog, &AuWindow::accept);
        ok->setDefault(true);   // 回车触发默认按钮（QDialog 语义）
        lay->addWidget(ok, 0, Qt::AlignRight);
        // 布局直接装在窗体上：AuWindow 自动劫持到标题栏下方内容区
        dialog.setFixedSize(440, 260);
        dialog.exec();
    });
    pageLayout->addWidget(fixedSize);

    QPushButton *limitedSize = new QPushButton(QStringLiteral("限制最大尺寸窗体（380×220 ~ 520×300）"));
    connect(limitedSize, &QPushButton::clicked, this, [this]() {
        AuWindow dialog(this);
        dialog.setDialogMode(true);
        dialog.setModality(Modality::WindowModal);
        dialog.setWindowTitle(QStringLiteral("限制尺寸对话框"));
        dialog.setWindowRadius(8);
        dialog.setButtonVisible(AuTitleBar::ButtonMaximize, false);
        dialog.setButtonVisible(AuTitleBar::ButtonMinimize, false);
        QLabel *label = new QLabel(QStringLiteral("380×220 ~ 520×300：\n"
                                                   "缩放被钳制在最大尺寸内，标题栏关闭按钮应始终跟随。"));
        label->setWordWrap(true);
        QVBoxLayout *lay = new QVBoxLayout(&dialog);
        lay->setContentsMargins(24, 20, 24, 16);
        lay->addWidget(label);
        lay->addStretch(1);
        QPushButton *ok = new QPushButton(QStringLiteral("确定"));
        connect(ok, &QPushButton::clicked, &dialog, &AuWindow::accept);
        ok->setDefault(true);   // 回车触发默认按钮（QDialog 语义）
        lay->addWidget(ok, 0, Qt::AlignRight);
        // 布局直接装在窗体上：AuWindow 自动劫持到标题栏下方内容区
        dialog.setMinimumSize(380, 220);
        dialog.setMaximumSize(520, 300);
        dialog.resize(440, 260);
        dialog.exec();
    });
    pageLayout->addWidget(limitedSize);

    pageLayout->addWidget(makeHint(
        QStringLiteral("模态行为由操作系统保证：Windows 上模态期间点击父窗口会发出系统提示音、"
                       "父窗口标题栏闪烁；本窗体使用系统 API（窗口所有权 + 系统模态），"
                       "效果与原生对话框一致。"), this));

    pageLayout->addStretch(1);
    return page;
}

QWidget *MainWindow::buildMessageBoxPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *pageLayout = makePageLayout(page);

    pageLayout->addWidget(makeHint(
        QStringLiteral("AuMessageBox：QMessageBox 用法的统一风格版本。图标、正文、"
                       "按钮全部自绘，跨平台观感一致；窗体骨架（标题栏/圆角/阴影/模态）"
                       "继承 AuWindow。本体样式随 AuMessageBoxStyle 全局样式联动，"
                       "可切到暗黑主题查看整体效果。"), this));

    QPushButton *infoButton = new QPushButton(QStringLiteral("information（信息，仅确定）"));
    connect(infoButton, &QPushButton::clicked, this, [this]() {
        AuMessageBox::information(this, QStringLiteral("操作完成"),
                                  QStringLiteral("文件已成功导出到指定目录。"));
    });
    pageLayout->addWidget(infoButton);

    QPushButton *warnButton = new QPushButton(QStringLiteral("warning（警告，是/否，读返回值）"));
    connect(warnButton, &QPushButton::clicked, this, [this]() {
        const AuMessageBox::StandardButton choice =
            AuMessageBox::warning(this, QStringLiteral("覆盖确认"),
                                  QStringLiteral("目标文件已存在，是否覆盖？"),
                                  AuMessageBox::Yes | AuMessageBox::No);
        AuMessageBox::information(this, QStringLiteral("返回值"),
                                  choice == AuMessageBox::Yes
                                      ? QStringLiteral("返回值：Yes（是）")
                                      : QStringLiteral("返回值：No（否 / Esc / 点X）"));
    });
    pageLayout->addWidget(warnButton);

    QPushButton *criticalButton = new QPushButton(QStringLiteral("critical（错误，关闭）"));
    connect(criticalButton, &QPushButton::clicked, this, [this]() {
        AuMessageBox::critical(this, QStringLiteral("保存失败"),
                               QStringLiteral("磁盘空间不足，写入已中断。"),
                               AuMessageBox::Close);
    });
    pageLayout->addWidget(criticalButton);

    QPushButton *questionButton = new QPushButton(QStringLiteral("question（询问，是/否/取消，默认按钮=是）"));
    connect(questionButton, &QPushButton::clicked, this, [this]() {
        const AuMessageBox::StandardButton choice =
            AuMessageBox::question(this, QStringLiteral("删除确认"),
                                   QStringLiteral("确定要删除选中的 3 个文件吗？"),
                                   AuMessageBox::Yes | AuMessageBox::No | AuMessageBox::Cancel,
                                   AuMessageBox::Yes);
        AuMessageBox::information(this, QStringLiteral("返回值"),
                                  choice == AuMessageBox::Yes ? QStringLiteral("是")
                                  : choice == AuMessageBox::No ? QStringLiteral("否")
                                  : QStringLiteral("取消"));
    });
    pageLayout->addWidget(questionButton);

    QPushButton *instanceButton = new QPushButton(QStringLiteral("实例用法（补充文本 + 实例级改按钮文案）"));
    connect(instanceButton, &QPushButton::clicked, this, [this]() {
        AuMessageBox box(this);
        box.setWindowTitle(QStringLiteral("软件更新"));
        box.setIcon(AuMessageBox::Question);
        box.setText(QStringLiteral("发现新版本 2.3.1，是否立即更新？"));
        box.setInformativeText(QStringLiteral("更新大小 45 MB，更新过程需要重启应用。"));
        box.setStandardButtons(AuMessageBox::Ok | AuMessageBox::Cancel);
        box.setButtonText(AuMessageBox::Ok, QStringLiteral("立即更新"));
        box.setButtonText(AuMessageBox::Cancel, QStringLiteral("下次再说"));
        box.exec();
    });
    pageLayout->addWidget(instanceButton);

    QPushButton *customButton = new QPushButton(QStringLiteral("自定义内容（“不再提示”勾选框）"));
    connect(customButton, &QPushButton::clicked, this, [this]() {
        AuMessageBox box(this);
        box.setWindowTitle(QStringLiteral("打开链接"));
        box.setIcon(AuMessageBox::Information);
        box.setText(QStringLiteral("检测到剪贴板中有链接，是否直接打开？"));
        QCheckBox *noPrompt = new QCheckBox(QStringLiteral("不再提示"));
        box.setCustomContent(noPrompt);
        box.setStandardButtons(AuMessageBox::Ok | AuMessageBox::Cancel);
        box.exec();
        if (noPrompt->isChecked())
            AuMessageBox::information(this, QStringLiteral("勾选状态"),
                                      QStringLiteral("“不再提示”已勾选（业务可据此持久化）。"));
    });
    pageLayout->addWidget(customButton);

    QPushButton *globalTextButton = new QPushButton(QStringLiteral("全局改按钮文案（setDefaultButtonText 切换中/英）"));
    connect(globalTextButton, &QPushButton::clicked, this, [this]() {
        m_englishButtonText = !m_englishButtonText;
        // 空字符串 = 恢复内置中文默认；全局覆盖对所有消息框即时生效
        AuMessageBox::setDefaultButtonText(AuMessageBox::Ok,
                                           m_englishButtonText ? QStringLiteral("OK") : QString());
        AuMessageBox::setDefaultButtonText(AuMessageBox::Cancel,
                                           m_englishButtonText ? QStringLiteral("Cancel") : QString());
        AuMessageBox::information(this, QStringLiteral("全局文案"),
                                  m_englishButtonText
                                      ? QStringLiteral("全部消息框的 确定/取消 已切换为英文。")
                                      : QStringLiteral("已恢复内置中文默认文案。"));
    });
    pageLayout->addWidget(globalTextButton);

    pageLayout->addStretch(1);
    return page;
}

QWidget *MainWindow::buildInheritPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *pageLayout = makePageLayout(page);

    QPushButton *openMyWin = new QPushButton(QStringLiteral("打开 MyWin 风格窗体（继承定制基类）"));
    connect(openMyWin, &QPushButton::clicked, this, [this]() {
        MyWin *win = new MyWin(this);
        QLabel *label = new QLabel(QStringLiteral(
            "所有继承 MyWin 的窗口自动获得暗黑主题、圆角与标题栏定制。\n"
            "内容区按普通 QWidget 方式使用：setCentralWidget 或 setupUi(this) 均可。"));
        label->setWordWrap(true);
        label->setStyleSheet(QStringLiteral("color:#ddd;"));
        QVBoxLayout *lay = new QVBoxLayout;
        lay->setContentsMargins(24, 20, 24, 20);
        lay->addWidget(label);
        lay->addStretch(1);
        QWidget *content = new QWidget;
        content->setLayout(lay);
        win->setCentralWidget(content);
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->show();
    });
    pageLayout->addWidget(openMyWin);

    QPushButton *openFromUi = new QPushButton(QStringLiteral("打开 FromUiWindow（MyWin 子类 + .ui 文件 setupUi(this)）"));
    connect(openFromUi, &QPushButton::clicked, this, [this]() {
        FromUiWindow *win = new FromUiWindow(this);
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->show();
    });
    pageLayout->addWidget(openFromUi);

    pageLayout->addWidget(makeHint(
        QStringLiteral("MyWin 继承 AuWindow 并统一设定主题/圆角/按钮；"
                       "FromUiWindow 再继承 MyWin，构造中直接 ui->setupUi(this)："
                       "Qt Designer 设计的根布局会被自动转移到标题栏下方内容区，"
                       "无需手工指定 centralWidget。"), this));

    pageLayout->addStretch(1);
    return page;
}

void MainWindow::switchGlobalTheme()
{
    const bool toDark = (m_themeButton->text() == QStringLiteral("切换暗黑主题"));
    // 全局样式只负责窗体自绘部分（标题栏 / 窗体背景 / 边框色 / 阴影色）；
    // 内容区控件配色由业务自行控制（QSS / QPalette），库与演示均不代管。
    // AuMessageBox 本体（正文 / 图标 / 按钮）另有一套全局样式，需同步切换。
    AuWindowStyle::setGlobalStyle(toDark ? AuWindowStyle::createDarkStyle()
                                         : AuWindowStyle::createLightStyle());
    AuMessageBoxStyle::setGlobalStyle(toDark ? AuMessageBoxStyle::createDarkStyle()
                                             : AuMessageBoxStyle::createLightStyle());
    m_themeButton->setText(toDark ? QStringLiteral("切换明亮主题")
                                  : QStringLiteral("切换暗黑主题"));
}

void MainWindow::onCloseIntercepted()
{
    const AuMessageBox::StandardButton answer =
        AuMessageBox::question(this, QStringLiteral("确认退出"),
                               QStringLiteral("关闭已被拦截，确定要退出吗？"));
    if (answer == AuMessageBox::Yes)
        forceClose();
}
