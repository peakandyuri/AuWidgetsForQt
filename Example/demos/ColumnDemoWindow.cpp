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
                                     u"列拖拽重排（直接拖动表头，任意层级分组列可整体拖动）；"
                                     u"最后两列演示单元格 tooltip（内容超出列宽被裁剪时悬停显示完整内容，"
                                     u"列定义 setTooltipEnabled(true) 手动开启）。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
{
    m_model = new AgGridModel(this);

    // tooltip 演示：两列内容相同、宽度相同，仅 tooltipEnabled 不同
    QVector<AgColumnDef> defs = makeStandardColumns();
    AgColumnDef noteOn("note", QStringLiteral(u"备注(tooltip开)"));
    noteOn.setWidth(150);
    noteOn.setTextWrapMode(AgColumnDef::TextWrapMode::NoWrapTruncate);
    noteOn.setTooltipEnabled(true);
    defs.append(noteOn);
    AgColumnDef noteOff("noteOff", QStringLiteral(u"备注(tooltip关)"));
    noteOff.setWidth(150);
    noteOff.setTextWrapMode(AgColumnDef::TextWrapMode::NoWrapTruncate);
    defs.append(noteOff);
    m_model->setColumnDefs(defs);

    QVector<QVariantMap> rows = makeFlatRows(100);
    for (int r = 0; r < rows.size(); ++r) {
        const QString longText = QStringLiteral(u"第 %1 行备注：这段文本明显超出列宽，悬停可查看完整内容").arg(r + 1);
        rows[r].insert(QStringLiteral("note"), longText);
        rows[r].insert(QStringLiteral("noteOff"), longText);
    }
    m_model->setRowData(rows);

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
    addNote("悬停最后两列对比 tooltip：内容超出列宽被裁剪时，开启的列悬停约 0.7 秒显示完整内容，"
            "列宽足够或未开启的列不显示；滚动/点击后自动消失。");
}
