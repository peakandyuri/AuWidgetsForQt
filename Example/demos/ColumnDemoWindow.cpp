#include "ColumnDemoWindow.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"
#include "AgGridWidget.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

ColumnDemoWindow::ColumnDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"列操作"),
                     QStringLiteral(u"列停靠（Pin 左 / 右）、列显隐（按叶子索引或分组 field）、"
                                     u"列拖拽重排（直接拖动表头，任意层级分组列可整体拖动）。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
{
    m_model = new AgGridModel(this);
    m_model->setColumnDefs(makeStandardColumns());
    m_model->setRowData(makeFlatRows(100));

    m_grid = new AgGridWidget(this);
    m_grid->setModel(m_model);

    QHBoxLayout *pinButtons = new QHBoxLayout;
    QPushButton *pinLeftBtn = new QPushButton(QStringLiteral(u"A 列停靠左侧"), this);
    QPushButton *pinRightBtn = new QPushButton(QStringLiteral(u"Enabled 停靠右侧"), this);
    QPushButton *unpinBtn = new QPushButton(QStringLiteral(u"全部取消停靠"), this);
    pinButtons->addWidget(pinLeftBtn);
    pinButtons->addWidget(pinRightBtn);
    pinButtons->addWidget(unpinBtn);
    pinButtons->addStretch(1);

    connect(pinLeftBtn, &QPushButton::clicked, this, [this]() {
        m_grid->pinColumn(0, AgColumnDef::PinMode::Left);
    });
    connect(pinRightBtn, &QPushButton::clicked, this, [this]() {
        m_grid->pinColumn(QStringLiteral("col6"), AgColumnDef::PinMode::Right);
    });
    connect(unpinBtn, &QPushButton::clicked, this, [this]() {
        for (int c = 0; c < m_model->columnCount(); ++c)
            m_grid->pinColumn(c, AgColumnDef::PinMode::None);
    });

    QHBoxLayout *visButtons = new QHBoxLayout;
    QPushButton *hideColBtn = new QPushButton(QStringLiteral(u"隐藏 C 列"), this);
    QPushButton *showColBtn = new QPushButton(QStringLiteral(u"显示 C 列"), this);
    QPushButton *hideGroupBtn = new QPushButton(QStringLiteral(u"隐藏 Multi 分组"), this);
    QPushButton *showGroupBtn = new QPushButton(QStringLiteral(u"显示 Multi 分组"), this);
    QPushButton *hideLastBtn = new QPushButton(QStringLiteral(u"隐藏最右列"), this);
    QPushButton *showLastBtn = new QPushButton(QStringLiteral(u"显示最右列"), this);
    visButtons->addWidget(hideColBtn);
    visButtons->addWidget(showColBtn);
    visButtons->addWidget(hideGroupBtn);
    visButtons->addWidget(showGroupBtn);
    visButtons->addWidget(hideLastBtn);
    visButtons->addWidget(showLastBtn);
    visButtons->addStretch(1);

    connect(hideColBtn, &QPushButton::clicked, this, [this]() {
        m_grid->setColumnVisible(2, false);
    });
    connect(showColBtn, &QPushButton::clicked, this, [this]() {
        m_grid->setColumnVisible(2, true);
    });
    connect(hideGroupBtn, &QPushButton::clicked, this, [this]() {
        m_grid->setColumnVisible(QStringLiteral("Multi"), false);
    });
    connect(showGroupBtn, &QPushButton::clicked, this, [this]() {
        m_grid->setColumnVisible(QStringLiteral("Multi"), true);
    });
    connect(hideLastBtn, &QPushButton::clicked, this, [this]() {
        m_grid->setColumnVisible(m_model->columnCount() - 1, false);
    });
    connect(showLastBtn, &QPushButton::clicked, this, [this]() {
        m_grid->setColumnVisible(m_model->columnCount() - 1, true);
    });

    m_contentLayout->addLayout(pinButtons);
    m_contentLayout->addLayout(visButtons);
    m_contentLayout->addWidget(m_grid, 1);

    addNote("列显隐带位移动画；隐藏列不改变叶子列索引（索引即建列顺序）。");
    addNote("分组列显隐按分组 field 一次性作用于其全部子列。");
    addNote("拖动表头可重排列（含整组 Multi）；拖到左右边缘区域可配合停靠。");
}
