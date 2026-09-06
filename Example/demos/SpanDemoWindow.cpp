#include "SpanDemoWindow.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"
#include "AgGridWidget.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

SpanDemoWindow::SpanDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"合并单元格"),
                     QStringLiteral(u"按可见行 / 叶子列坐标合并区域（类似 QTableView::setSpan）；"
                                     u"hover / 选中 / 网格线 / 焦点框均按合并区域整体处理。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
{
    QVector<AgColumnDef> columns;
    for (int c = 0; c < 4; ++c) {
        AgColumnDef col(QString("col%1").arg(c), QString("COL%1").arg(c));
        col.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
        col.setWidth(140);
        columns.append(col);
    }

    m_model = new AgGridModel(this);
    m_model->setColumnDefs(columns);
    m_model->setRowData(makeFlatRows(8));

    m_grid = new AgGridWidget(this);
    m_grid->setModel(m_model);

    // 合并示例
    auto applyDemoSpans = [this]() {
        m_grid->setSpan(0, 0, 3, 1);   // 纵向合并：第 0~2 行的第 0 列
        m_grid->setSpan(0, 2, 1, 2);   // 横向合并：第 0 行的第 2~3 列
        m_grid->setSpan(5, 1, 2, 2);   // 2x2 合并：第 5~6 行、第 1~2 列
    };
    applyDemoSpans();

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *applyBtn = new QPushButton(QStringLiteral(u"应用合并示例"), this);
    QPushButton *clearBtn = new QPushButton(QStringLiteral(u"清除合并"), this);
    QPushButton *queryBtn = new QPushButton(QStringLiteral(u"查询 span"), this);
    buttons->addWidget(applyBtn);
    buttons->addWidget(clearBtn);
    buttons->addWidget(queryBtn);
    buttons->addStretch(1);

    connect(applyBtn, &QPushButton::clicked, this, applyDemoSpans);
    connect(clearBtn, &QPushButton::clicked, this, [this]() {
        m_grid->clearSpans();
    });
    connect(queryBtn, &QPushButton::clicked, this, [this]() {
        qDebug() << "hasSpans=" << m_grid->hasSpans()
                 << " rowSpan(0,0)=" << m_grid->rowSpan(0, 0)
                 << " columnSpan(0,2)=" << m_grid->columnSpan(0, 2);
    });

    m_contentLayout->addLayout(buttons);
    m_contentLayout->addWidget(m_grid, 1);

    addNote("合并区域不跨停靠边界；点击 / 右键 / 编辑都作用在锚点格。");
    addNote("排序 / 过滤 / 行移动 / 树形展开折叠会改变可见行映射，合并自动清空。");
}
