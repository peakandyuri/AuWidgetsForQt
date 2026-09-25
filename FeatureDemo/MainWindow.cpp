#include "MainWindow.h"

#include "pages/GridPages.h"
#include "pages/SelectPages.h"
#include "pages/DatePickerPages.h"
#include "pages/PlotPages.h"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabBar>
#include <QVBoxLayout>
#include <QWidget>

namespace {

/// 左侧列表的条目：key + 标题 + 一句话说明。
struct WidgetEntry {
    QString key;
    QString title;
    QString subtitle;
};

QVector<WidgetEntry> widgetEntries()
{
    return {
        { QStringLiteral("grid"),
          QStringLiteral(u"AgGrid"),
          QStringLiteral(u"数据表格\n行号 / 复选框 / 选择 / 拖拽 / 排序…") },
        { QStringLiteral("select"),
          QStringLiteral(u"AgSelect"),
          QStringLiteral(u"下拉选择\n单选 / 多选 chips / 分组 / 过滤…") },
        { QStringLiteral("datepicker"),
          QStringLiteral(u"AgDatePicker"),
          QStringLiteral(u"日期选择\n日 / 月 / 年 / 范围 / footer…") },
        { QStringLiteral("plot"),
          QStringLiteral(u"AgPlot"),
          QStringLiteral(u"曲线绘制\n双轴 / 网格 / 点线面 / 拖动缩放…") },
    };
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral(u"AgWidgets 功能细化演示 — 一个功能一个示例"));

    m_pages.insert(QStringLiteral("grid"), makeGridPages());
    m_pages.insert(QStringLiteral("select"), makeSelectPages());
    m_pages.insert(QStringLiteral("datepicker"), makeDatePickerPages());
    m_pages.insert(QStringLiteral("plot"), makePlotPages());
    m_widgetKeys << QStringLiteral("grid") << QStringLiteral("select") << QStringLiteral("datepicker")
                 << QStringLiteral("plot");

    QWidget *central = new QWidget(this);
    QHBoxLayout *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(12);

    buildLeftPanel();
    buildRightPanel();

    rootLayout->addWidget(m_widgetList);
    rootLayout->addWidget(m_rightHost, 1);

    setCentralWidget(central);

    connect(m_widgetList, &QListWidget::currentRowChanged, this, &MainWindow::switchWidget);
    m_widgetList->blockSignals(true);
    m_widgetList->setCurrentRow(0);
    m_widgetList->blockSignals(false);
    switchWidget(0);

    resize(1280, 800);
}

void MainWindow::buildLeftPanel()
{
    m_widgetList = new QListWidget(this);
    m_widgetList->setFixedWidth(200);

    const QVector<WidgetEntry> entries = widgetEntries();
    for (const WidgetEntry &entry : entries) {
        QListWidgetItem *item = new QListWidgetItem(
            QStringLiteral(u"%1\n%2").arg(entry.title, entry.subtitle), m_widgetList);
        item->setData(Qt::UserRole, entry.key);
        item->setSizeHint(QSize(188, 64));
    }
    m_widgetList->setSpacing(6);
    m_widgetList->setStyleSheet(
        QStringLiteral(u"QListWidget { background: #f5f6f8; border: 1px solid #d8dbe0;"
                       u" border-radius: 6px; padding: 6px; }"
                       u"QListWidget::item { border-radius: 4px; padding: 6px; }"
                       u"QListWidget::item:selected { background: #2f6fdb; color: white; }"));
}

void MainWindow::buildRightPanel()
{
    QWidget *right = new QWidget(this);
    m_rightHost = right;
    QVBoxLayout *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    // ---- 顶部：功能 TabBar（唯一一个，切控件时重建 Tab）----
    m_tabBar = new QTabBar(right);
    m_tabBar->setExpanding(false);
    m_tabBar->setUsesScrollButtons(true);
    m_tabBar->setElideMode(Qt::ElideNone);
    m_tabBar->setDrawBase(true);
    m_tabBar->setStyleSheet(
        QStringLiteral(u"QTabBar::tab { padding: 6px 14px; margin-right: 2px; }"));
    connect(m_tabBar, &QTabBar::currentChanged, this, &MainWindow::switchPage);
    rightLayout->addWidget(m_tabBar);

    // ---- 功能说明 ----
    m_description = new QLabel(right);
    m_description->setWordWrap(true);
    m_description->setStyleSheet(
        QStringLiteral(u"color: #444; background: #eef3fc; border: 1px solid #d4e0f5;"
                       u" border-radius: 4px; padding: 6px 10px;"));
    rightLayout->addWidget(m_description);

    // ---- 下部：左演示 / 右代码 ----
    QSplitter *splitter = new QSplitter(Qt::Horizontal, right);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    m_demoHost = new QWidget(splitter);
    m_demoLayout = new QVBoxLayout(m_demoHost);
    m_demoLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *codePanel = new QWidget(splitter);
    QVBoxLayout *codeLayout = new QVBoxLayout(codePanel);
    codeLayout->setContentsMargins(0, 0, 0, 0);
    codeLayout->setSpacing(4);

    QHBoxLayout *codeHeader = new QHBoxLayout;
    m_codeTitle = new QLabel(QStringLiteral(u"实现代码"), codePanel);
    m_codeTitle->setStyleSheet(QStringLiteral(u"color: #666; font-weight: bold;"));
    QPushButton *copyButton = new QPushButton(QStringLiteral(u"复制代码"), codePanel);
    connect(copyButton, &QPushButton::clicked, this, &MainWindow::copyCode);
    codeHeader->addWidget(m_codeTitle);
    codeHeader->addStretch(1);
    codeHeader->addWidget(copyButton);
    codeLayout->addLayout(codeHeader);

    m_codeView = new QPlainTextEdit(codePanel);
    m_codeView->setReadOnly(true);
    m_codeView->setLineWrapMode(QPlainTextEdit::NoWrap);
    QFont codeFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    codeFont.setPointSizeF(9.5);
    m_codeView->setFont(codeFont);
    m_codeView->setStyleSheet(
        QStringLiteral(u"QPlainTextEdit { background: #fbfbfd; border: 1px solid #d8dbe0;"
                       u" border-radius: 4px; padding: 6px; }"));
    codeLayout->addWidget(m_codeView, 1);

    splitter->addWidget(m_demoHost);
    splitter->addWidget(codePanel);
    splitter->setSizes({760, 480});

    rightLayout->addWidget(splitter, 1);
}

void MainWindow::switchWidget(int index)
{
    if (index < 0 || index >= m_widgetKeys.size())
        return;

    const QVector<FeaturePage> &pages = m_pages.value(m_widgetKeys.at(index));

    QSignalBlocker blocker(m_tabBar);
    while (m_tabBar->count() > 0)
        m_tabBar->removeTab(0);
    for (const FeaturePage &page : pages)
        m_tabBar->addTab(page.title);

    loadPage(0);
    m_tabBar->setCurrentIndex(0);
}

void MainWindow::switchPage(int index)
{
    loadPage(index);
}

void MainWindow::loadPage(int index)
{
    if (m_widgetList->currentRow() < 0 || m_widgetList->currentRow() >= m_widgetKeys.size())
        return;
    const QVector<FeaturePage> pages = m_pages.value(m_widgetKeys.at(m_widgetList->currentRow()));
    if (index < 0 || index >= pages.size())
        return;

    const FeaturePage &page = pages.at(index);

    // 演示实例为临时对象：切页即卸载上一个（不缓存、不持久持有）。
    // 用 deleteLater 而非同步 delete：本函数可能在鼠标事件分发中被调用，
    // 此前页面可能还开着弹层 / 编辑器，同步析构不安全。
    if (m_demoWidget) {
        m_demoLayout->removeWidget(m_demoWidget);
        m_demoWidget->hide();
        m_demoWidget->deleteLater();
        m_demoWidget = nullptr;
    }

    m_demoWidget = page.createDemo();
    if (m_demoWidget)
        m_demoLayout->addWidget(m_demoWidget);

    m_description->setText(page.description);
    m_codeView->setPlainText(page.code);
}

void MainWindow::copyCode()
{
    QApplication::clipboard()->setText(m_codeView->toPlainText());
}
