#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "AgGridStyle.h"
#include "BasicDemoWindow.h"
#include "ColumnDemoWindow.h"
#include "HeaderCheckDemoWindow.h"
#include "LoadingDemoWindow.h"
#include "RowDragDemoWindow.h"
#include "ScrollDemoWindow.h"
#include "SelectionCheckDemoWindow.h"
#include "SortFilterDemoWindow.h"
#include "SpanDemoWindow.h"
#include "StyleDemoWindow.h"
#include "SyncDemoWindow.h"
#include "TreeDemoWindow.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral(u"AgGrid 功能示例"));

    // 全局样式：所有未显式 setStyle() 的示例窗口自动应用（亮暗切换见"样式与外观"示例）
    AgGridStyle::setGlobalStyle(AgGridStyle::createLightStyle());

    QLabel *titleLabel = new QLabel(QStringLiteral(u"AgGrid for Qt 功能示例"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);

    QLabel *descLabel = new QLabel(
        QStringLiteral(u"按功能拆分的独立示例窗口：点击按钮打开对应功能演示。各示例窗口可同时打开、独立操作。"), this);
    descLabel->setAlignment(Qt::AlignCenter);

    QWidget *buttonPanel = new QWidget(this);
    QGridLayout *buttonGrid = new QGridLayout(buttonPanel);
    buttonGrid->setSpacing(12);

    struct DemoEntry {
        QString key;
        QString text;
        std::function<QDialog *()> create;
    };

    const QVector<DemoEntry> demos = {
        { "basic", QStringLiteral(u"基础功能\n列宽模式 / 多级表头 / 编辑校验 / 自定义渲染器"),
          [this]() { return static_cast<QDialog *>(new BasicDemoWindow(this)); } },
        { "selection", QStringLiteral(u"选中与勾选\n行号列 / 复选框列 / 行可选回调"),
          [this]() { return static_cast<QDialog *>(new SelectionCheckDemoWindow(this)); } },
        { "headercheck", QStringLiteral(u"表头全选\n数据列复选框的表头全选 / 半选组件"),
          [this]() { return static_cast<QDialog *>(new HeaderCheckDemoWindow(this)); } },
        { "sortfilter", QStringLiteral(u"排序与过滤\n表头排序 / 多规则排序 / 过滤回调"),
          [this]() { return static_cast<QDialog *>(new SortFilterDemoWindow(this)); } },
        { "column", QStringLiteral(u"列操作\n停靠 / 显隐动画 / 列拖拽"),
          [this]() { return static_cast<QDialog *>(new ColumnDemoWindow(this)); } },
        { "rowdrag", QStringLiteral(u"行拖拽重排\n整组拖动 / 动画 / 自动滚动"),
          [this]() { return static_cast<QDialog *>(new RowDragDemoWindow(this)); } },
        { "span", QStringLiteral(u"合并单元格\n行列合并 / 交互语义"),
          [this]() { return static_cast<QDialog *>(new SpanDemoWindow(this)); } },
        { "scroll", QStringLiteral(u"大数据量与定位\n10 万行虚拟化 / scrollToRow"),
          [this]() { return static_cast<QDialog *>(new ScrollDemoWindow(this)); } },
        { "tree", QStringLiteral(u"树形表格\n层级缩进 / 展开折叠 / 复选框级联三态"),
          [this]() { return static_cast<QDialog *>(new TreeDemoWindow(this)); } },
        { "sync", QStringLiteral(u"多表格同步\n滚动 / 列宽同步（数据对比）"),
          [this]() { return static_cast<QDialog *>(new SyncDemoWindow(this)); } },
        { "style", QStringLiteral(u"样式与外观\n全局主题 / 尺寸模式 / 上下文样式"),
          [this]() { return static_cast<QDialog *>(new StyleDemoWindow(this)); } },
        { "loading", QStringLiteral(u"加载遮罩\nsetLoading / 模拟异步请求"),
          [this]() { return static_cast<QDialog *>(new LoadingDemoWindow(this)); } },
    };

    const int columns = 3;
    for (int i = 0; i < demos.size(); ++i) {
        addDemoButton(buttonGrid, i / columns, i % columns,
                      demos.at(i).key, demos.at(i).text, demos.at(i).create);
    }
    buttonGrid->setRowStretch(demos.size() / columns + 1, 1);
    buttonGrid->setColumnStretch(columns, 1);

    QLabel *footerLabel = new QLabel(
        QStringLiteral(u"更多细节见 docs/AgGrid-帮助文档.md"), this);
    footerLabel->setAlignment(Qt::AlignCenter);
    footerLabel->setWordWrap(true);

    ui->verticalLayout->addWidget(titleLabel);
    ui->verticalLayout->addWidget(descLabel);
    ui->verticalLayout->addWidget(buttonPanel, 1);
    ui->verticalLayout->addWidget(footerLabel);

    resize(760, 620);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::addDemoButton(QGridLayout *grid, int row, int column,
                               const QString &key, const QString &buttonText,
                               const std::function<QDialog *()> &create)
{
    QPushButton *button = new QPushButton(buttonText, this);
    button->setMinimumHeight(72);
    connect(button, &QPushButton::clicked, this, [this, key, create]() {
        QPointer<QDialog> demo = m_openDemos.value(key);
        if (!demo) {
            demo = create();
            demo->setAttribute(Qt::WA_DeleteOnClose);
            m_openDemos.insert(key, demo);
        }
        demo->show();
        demo->raise();
        demo->activateWindow();
    });
    grid->addWidget(button, row, column);
}
