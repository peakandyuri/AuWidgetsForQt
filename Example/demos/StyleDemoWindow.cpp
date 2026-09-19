#include "StyleDemoWindow.h"

#include "AgGridModel.h"
#include "AgGridStyle.h"
#include "AgGridWidget.h"
#include "DemoStyle.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

StyleDemoWindow::StyleDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"样式与外观"),
                     QStringLiteral(u"三种样式粒度：全局样式（所有默认控件）、控件独立样式（setStyle 接管所有权）、"
                                     u"按行列上下文动态返回的样式值（DemoStyle，如按内容着色 / 改字体对齐）。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
{
    m_model = new AgGridModel(this);
    m_model->setColumnDefs(makeStandardColumns());
    m_model->setRowData(makeFlatRows(60));

    m_grid = new AgGridWidget(this);
    m_grid->setModel(m_model);

    QHBoxLayout *themeRow = new QHBoxLayout;
    QPushButton *lightBtn = new QPushButton(QStringLiteral(u"全局亮色"), this);
    QPushButton *darkBtn = new QPushButton(QStringLiteral(u"全局暗色"), this);
    themeRow->addWidget(lightBtn);
    themeRow->addWidget(darkBtn);
    themeRow->addStretch(1);
    connect(lightBtn, &QPushButton::clicked, this, []() {
        AgGridStyle::setGlobalStyle(AgGridStyle::createLightStyle());
    });
    connect(darkBtn, &QPushButton::clicked, this, []() {
        AgGridStyle::setGlobalStyle(AgGridStyle::createDarkStyle());
    });

    QHBoxLayout *modeRow = new QHBoxLayout;
    QPushButton *sizeSmallBtn = new QPushButton(QStringLiteral(u"小"), this);
    QPushButton *sizeMediumBtn = new QPushButton(QStringLiteral(u"中"), this);
    QPushButton *sizeLargeBtn = new QPushButton(QStringLiteral(u"大"), this);
    sizeSmallBtn->setCheckable(true);
    sizeMediumBtn->setCheckable(true);
    sizeLargeBtn->setCheckable(true);
    sizeSmallBtn->setChecked(true);
    modeRow->addWidget(sizeSmallBtn);
    modeRow->addWidget(sizeMediumBtn);
    modeRow->addWidget(sizeLargeBtn);
    modeRow->addStretch(1);

    auto updateSizeButtons = [sizeSmallBtn, sizeMediumBtn, sizeLargeBtn](AgGridWidget::SizeMode mode) {
        sizeSmallBtn->setChecked(mode == AgGridWidget::SizeMode::Small);
        sizeMediumBtn->setChecked(mode == AgGridWidget::SizeMode::Medium);
        sizeLargeBtn->setChecked(mode == AgGridWidget::SizeMode::Large);
    };
    connect(sizeSmallBtn, &QPushButton::clicked, this, [this, updateSizeButtons]() {
        m_grid->setSizeMode(AgGridWidget::SizeMode::Small);
        updateSizeButtons(m_grid->sizeMode());
    });
    connect(sizeMediumBtn, &QPushButton::clicked, this, [this, updateSizeButtons]() {
        m_grid->setSizeMode(AgGridWidget::SizeMode::Medium);
        updateSizeButtons(m_grid->sizeMode());
    });
    connect(sizeLargeBtn, &QPushButton::clicked, this, [this, updateSizeButtons]() {
        m_grid->setSizeMode(AgGridWidget::SizeMode::Large);
        updateSizeButtons(m_grid->sizeMode());
    });

    QHBoxLayout *styleRow = new QHBoxLayout;
    QPushButton *demoStyleBtn = new QPushButton(QStringLiteral(u"应用 DemoStyle（本表独立样式）"), this);
    QPushButton *defaultStyleBtn = new QPushButton(QStringLiteral(u"恢复默认样式"), this);
    QCheckBox *vLinesBox = new QCheckBox(QStringLiteral(u"显示列边框线（左右）"), this);
    vLinesBox->setChecked(true);
    styleRow->addWidget(demoStyleBtn);
    styleRow->addWidget(defaultStyleBtn);
    styleRow->addWidget(vLinesBox);
    styleRow->addStretch(1);
    connect(demoStyleBtn, &QPushButton::clicked, this, [this]() {
        m_grid->setStyle(new DemoStyle());   // 独立样式：控件接管所有权
    });
    connect(defaultStyleBtn, &QPushButton::clicked, this, [this]() {
        m_grid->setStyle(AgGridStyle::globalStyle()->clone());
    });
    connect(vLinesBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_grid->setVerticalGridLinesVisible(checked);
    });

    m_contentLayout->addLayout(themeRow);
    m_contentLayout->addLayout(modeRow);
    m_contentLayout->addLayout(styleRow);
    m_contentLayout->addWidget(m_grid, 1);

    addNote("全局主题影响所有未显式 setStyle() 的示例窗口；独立样式只影响本表格。");
    addNote("DemoStyle 演示按行列上下文动态样式：第 7n 行加粗、col1 每 5 行标红、含 99 的格变绿等。");
    addNote("尺寸模式改变行高 / 表头高 / 字体 / 内边距等尺寸类样式键的默认值。");
    addNote("取消勾选\"显示列边框线\"后，数据列只保留行方向（上下）边框；行号列 / 复选框列 / 手柄列仍带完整边框。");
}
