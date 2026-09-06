#include "ScrollDemoWindow.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"
#include "AgGridWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ScrollDemoWindow::ScrollDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"大数据量与滚动定位"),
                     QStringLiteral(u"10 万行数据虚拟化渲染：只绘制可视区（含缓冲）内的行。"
                                     u"演示 scrollToRow / scrollToCell 各种定位提示。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
    , m_rowSpin(nullptr)
    , m_columnSpin(nullptr)
{
    const int rowCount = 100000;

    m_model = new AgGridModel(this);
    m_model->setColumnDefs(makeStandardColumns());
    m_model->setRowData(makeFlatRows(rowCount));

    m_grid = new AgGridWidget(this);
    m_grid->setModel(m_model);
    m_grid->setVisibleRowBuffer(5);   // 视口外上下各多渲染 5 行，滚动更平滑

    m_rowSpin = new QSpinBox(this);
    m_rowSpin->setRange(0, rowCount - 1);
    m_rowSpin->setValue(0);
    m_columnSpin = new QSpinBox(this);
    m_columnSpin->setRange(0, 6);
    m_columnSpin->setValue(0);

    QHBoxLayout *positionRow = new QHBoxLayout;
    positionRow->addWidget(new QLabel(QStringLiteral(u"行:"), this));
    positionRow->addWidget(m_rowSpin);
    positionRow->addWidget(new QLabel(QStringLiteral(u"列:"), this));
    positionRow->addWidget(m_columnSpin);

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *rowTopBtn = new QPushButton(QStringLiteral(u"行顶"), this);
    QPushButton *rowCenterBtn = new QPushButton(QStringLiteral(u"行中"), this);
    QPushButton *rowBottomBtn = new QPushButton(QStringLiteral(u"行底"), this);
    QPushButton *cellVisibleBtn = new QPushButton(QStringLiteral(u"格可见"), this);
    QPushButton *cellCenterBtn = new QPushButton(QStringLiteral(u"格居中"), this);
    buttons->addWidget(rowTopBtn);
    buttons->addWidget(rowCenterBtn);
    buttons->addWidget(rowBottomBtn);
    buttons->addWidget(cellVisibleBtn);
    buttons->addWidget(cellCenterBtn);
    buttons->addStretch(1);

    connect(rowTopBtn, &QPushButton::clicked, this, [this]() {
        m_grid->scrollToRow(m_rowSpin->value(), AgGridWidget::ScrollHint::PositionTop);
    });
    connect(rowCenterBtn, &QPushButton::clicked, this, [this]() {
        m_grid->scrollToRow(m_rowSpin->value(), AgGridWidget::ScrollHint::PositionCenter);
    });
    connect(rowBottomBtn, &QPushButton::clicked, this, [this]() {
        m_grid->scrollToRow(m_rowSpin->value(), AgGridWidget::ScrollHint::PositionBottom);
    });
    connect(cellVisibleBtn, &QPushButton::clicked, this, [this]() {
        m_grid->scrollToCell(m_rowSpin->value(), m_columnSpin->value(), AgGridWidget::ScrollHint::EnsureVisible);
    });
    connect(cellCenterBtn, &QPushButton::clicked, this, [this]() {
        m_grid->scrollToCell(m_rowSpin->value(), m_columnSpin->value(), AgGridWidget::ScrollHint::PositionCenter);
    });

    m_contentLayout->addLayout(positionRow);
    m_contentLayout->addLayout(buttons);
    m_contentLayout->addWidget(m_grid, 1);

    addNote("拖动右侧滚动条在 10 万行间快速滑动，验证虚拟化渲染性能。");
    addNote("可见行缓冲（setVisibleRowBuffer）越大滚动越平滑，绘制开销略增。");
}
