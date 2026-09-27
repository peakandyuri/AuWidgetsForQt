#include "MainWindow.h"
#include "AgWindowStyle.h"
#include "demos/DialogWindows.h"
#include "demos/MyWin.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
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
    : AgWindow(parent)
{
    setWindowTitle(QStringLiteral("AgWindowKit 窗体演示"));
    setWindowRadius(0);
    setMinimumSize(640, 480);

    // ---- 标题栏自定义组件：三槽位演示 ----
    QLabel *versionLabel = new QLabel(QStringLiteral("v1.0.0"));
    versionLabel->setStyleSheet(QStringLiteral("color:#888; font-size:12px;"));
    addTitleBarWidget(versionLabel, AgTitleBar::SlotAfterTitle, false); // 纯展示：不抢占鼠标

    QLabel *clockLabel = new QLabel;
    clockLabel->setStyleSheet(QStringLiteral("color:#888; font-size:12px;"));
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [clockLabel]() {
        clockLabel->setText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")));
    });
    timer->start(1000);
    clockLabel->setText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")));
    addTitleBarWidget(clockLabel, AgTitleBar::SlotCenter, false);      // 居中：纯展示

    QPushButton *feedbackButton = new QPushButton(QStringLiteral("反馈"));
    feedbackButton->setStyleSheet(QStringLiteral(
        "QPushButton { background:#2f7fe0; color:white; border:none; border-radius:3px;"
        "  padding:2px 10px; font-size:12px; }"
        "QPushButton:hover { background:#3d8ceb; }"));
    feedbackButton->connect(feedbackButton, &QPushButton::clicked, this, []() {
        QMessageBox::information(nullptr, QStringLiteral("反馈"),
                                 QStringLiteral("抢占鼠标的标题栏按钮：点击不受拖动影响"));
    });
    addTitleBarWidget(feedbackButton, AgTitleBar::SlotBeforeButtons);  // 靠右：可交互组件，抢占鼠标

    buildContent();
    connect(this, &AgWindow::closeIntercepted, this, &MainWindow::onCloseIntercepted);
}

void MainWindow::buildContent()
{
    QTabWidget *tabs = new QTabWidget;
    tabs->addTab(buildWindowControlPage(), QStringLiteral("窗体特性"));
    tabs->addTab(buildDialogPage(), QStringLiteral("Dialog 模态"));
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
            this, &AgWindow::setWindowRadius);
    radiusRow->addWidget(radiusSpin);
    radiusRow->addStretch(1);
    pageLayout->addLayout(radiusRow);

    QHBoxLayout *buttonRow = new QHBoxLayout;
    buttonRow->addWidget(new QLabel(QStringLiteral("标题栏按钮：")));
    const QList<QPair<AgTitleBar::Button, QString>> buttons = {
        { AgTitleBar::ButtonMinimize, QStringLiteral("最小化") },
        { AgTitleBar::ButtonMaximize, QStringLiteral("最大化") },
        { AgTitleBar::ButtonClose, QStringLiteral("关闭") }
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
    connect(taskbarBox, &QCheckBox::toggled, this, &AgWindow::setTaskbarVisible);
    miscRow->addWidget(taskbarBox);

    QCheckBox *interceptBox = new QCheckBox(QStringLiteral("拦截关闭（点 X 不直接退出）"));
    connect(interceptBox, &QCheckBox::toggled, this, &AgWindow::setCloseInterceptionEnabled);
    miscRow->addWidget(interceptBox);
    miscRow->addStretch(1);
    pageLayout->addLayout(miscRow);

    QHBoxLayout *snapRow = new QHBoxLayout;
    QCheckBox *snapBox = new QCheckBox(QStringLiteral("边缘吸附（拖动中靠近屏幕上/左/右缘出现预览，松手贴靠）"));
    snapBox->setChecked(isEdgeSnapEnabled());
    connect(snapBox, &QCheckBox::toggled, this, &AgWindow::setEdgeSnapEnabled);
    snapRow->addWidget(snapBox);
    snapRow->addStretch(1);
    pageLayout->addLayout(snapRow);

    pageLayout->addWidget(makeHint(
        QStringLiteral("说明：标题栏可拖动，拖到屏幕上/左/右边缘触发系统分屏与最大化；"
                       "边框可缩放（尊重最小宽高）；双击标题栏空白切换最大化；"
                       "标题栏上方三个自定义组件分别演示靠左/居中/靠右槽位。"), this));

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

    QPushButton *windowModal = new QPushButton(QStringLiteral("父窗口模态（exec，Esc 关闭）"));
    connect(windowModal, &QPushButton::clicked, this, [this]() {
        DialogWindow dialog(AgWindow::Modality::WindowModal, this);
        const int code = dialog.exec();
        QMessageBox::information(this, QStringLiteral("exec 返回"),
                                 code == AgWindow::Accepted
                                     ? QStringLiteral("返回：Accepted（确定）")
                                     : QStringLiteral("返回：Rejected（取消/Esc/点X）"));
    });
    pageLayout->addWidget(windowModal);

    QPushButton *appModal = new QPushButton(QStringLiteral("应用级模态（exec，无父窗口）"));
    connect(appModal, &QPushButton::clicked, this, [this]() {
        DialogWindow dialog(AgWindow::Modality::ApplicationModal, nullptr);
        dialog.exec();
    });
    pageLayout->addWidget(appModal);

    QPushButton *modeless = new QPushButton(QStringLiteral("非模态 Dialog（show）"));
    connect(modeless, &QPushButton::clicked, this, [this]() {
        DialogWindow *dialog = new DialogWindow(AgWindow::Modality::WindowModal, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    });
    pageLayout->addWidget(modeless);

    pageLayout->addWidget(makeHint(
        QStringLiteral("模态行为由操作系统保证：Windows 上模态期间点击父窗口会发出系统提示音、"
                       "父窗口标题栏闪烁；本窗体使用系统 API（窗口所有权 + 系统模态），"
                       "效果与原生对话框一致。"), this));

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
        QStringLiteral("MyWin 继承 AgWindow 并统一设定主题/圆角/按钮；"
                       "FromUiWindow 再继承 MyWin，构造中直接 ui->setupUi(this)："
                       "Qt Designer 设计的根布局会被自动转移到标题栏下方内容区，"
                       "无需手工指定 centralWidget。"), this));

    pageLayout->addStretch(1);
    return page;
}

void MainWindow::switchGlobalTheme()
{
    const bool toDark = (m_themeButton->text() == QStringLiteral("切换暗黑主题"));
    AgWindowStyle::setGlobalStyle(toDark ? AgWindowStyle::createDarkStyle()
                                         : AgWindowStyle::createLightStyle());
    m_themeButton->setText(toDark ? QStringLiteral("切换明亮主题")
                                  : QStringLiteral("切换暗黑主题"));
    // 同步内容区控件配色，演示整体效果（实际工程可搭配 QSS/调色板）
    QTabWidget *tabs = qobject_cast<QTabWidget *>(centralWidget());
    if (tabs)
        tabs->setStyleSheet(toDark ? QStringLiteral("background:#252526; color:#ddd;")
                                   : QString());
}

void MainWindow::onCloseIntercepted()
{
    const QMessageBox::StandardButton answer =
        QMessageBox::question(this, QStringLiteral("确认退出"),
                              QStringLiteral("关闭已被拦截，确定要退出吗？"));
    if (answer == QMessageBox::Yes)
        forceClose();
}
