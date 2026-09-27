#include "RowHeightDemoWindow.h"

#include "AuColumnDef.h"
#include "AuGridModel.h"
#include "AuGridWidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

RowHeightDemoWindow::RowHeightDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"自适应行高"),
                     QStringLiteral(u"列定义 textWrapMode=WordWrap 时按列宽自动换行，"
                                     u"行高取全部列测量结果的最大值（RowHeight 样式值为最小行高）；"
                                     u"列宽变化后行高实时重算。"),
                     parent)
    , m_grid(nullptr)
    , m_model(nullptr)
{
    m_model = new AuGridModel(this);
    applyWrapMode(true);

    // 描述列长短不一（含显式 \n），各行高度按内容自适应
    QVector<QVariantMap> rows;
    rows.append({{"name", "订单 A1001"},
                 {"count", 12},
                 {"desc", QStringLiteral(u"正常一行长度的描述文本。")}});
    rows.append({{"name", "订单 A1002"},
                 {"count", 3},
                 {"desc", QStringLiteral(u"这段描述比较长：客户要求全部使用加厚包装箱，并在每个箱体外部"
                                         u"粘贴易碎标签，物流方式指定为顺丰陆运，预计本周五之前发出，"
                                         u"发货后请把运单号同步到售后群。")}});
    rows.append({{"name", "订单 A1003"},
                 {"count", 128},
                 {"desc", QStringLiteral(u"多行备注：\n1. 附质检报告扫描件；\n2. 优先安排早班发货；\n3. 收货人为仓库夜班值班组长。")}});
    rows.append({{"name", "订单 B2001"},
                 {"count", 7},
                 {"desc", QStringLiteral(u"短备注。")}});
    rows.append({{"name", "订单 B2002"},
                 {"count", 56},
                 {"desc", QStringLiteral(u"超长文本用于观察滚动与行高：本项目零件涉及三批次到货，"
                                         u"第一批为标准件已入库，第二批为定制件预计下周二到货，到货后需"
                                         u"逐件核对激光编号并拍照留存；第三批为备品备件，暂存三号货架，"
                                         u"待客户确认安装计划后再统一调拨，如有变更以采购邮件为准。")}});
    rows.append({{"name", "订单 B2003"},
                 {"count", 24},
                 {"desc", QStringLiteral(u"两行左右：合同编号 HT-2026-0918，付款方式为月结三十天。")}});
    m_model->setRowData(rows);

    m_grid = new AuGridWidget(this);
    m_grid->setModel(m_model);
    m_grid->setRowNumberColumnVisible(true);

    // 换行模式切换：对比 WordWrap（行高自适应）与 NoWrapTruncate（固定行高 + 省略号）
    QHBoxLayout *modeButtons = new QHBoxLayout;
    QPushButton *wrapBtn = new QPushButton(QStringLiteral(u"描述列：自动换行（自适应行高）"), this);
    QPushButton *truncateBtn = new QPushButton(QStringLiteral(u"描述列：单行截断（固定行高）"), this);
    wrapBtn->setCheckable(true);
    truncateBtn->setCheckable(true);
    wrapBtn->setChecked(true);
    modeButtons->addWidget(wrapBtn);
    modeButtons->addWidget(truncateBtn);
    modeButtons->addStretch(1);

    auto syncModeUi = [wrapBtn, truncateBtn](bool wordWrap) {
        wrapBtn->setChecked(wordWrap);
        truncateBtn->setChecked(!wordWrap);
    };
    connect(wrapBtn, &QPushButton::clicked, this, [this, syncModeUi]() {
        applyWrapMode(true);
        syncModeUi(true);
    });
    connect(truncateBtn, &QPushButton::clicked, this, [this, syncModeUi]() {
        applyWrapMode(false);
        syncModeUi(false);
    });

    // 描述列宽切换：WordWrap 下拖动列宽或程序改宽，行高都会跟随重算
    QHBoxLayout *widthButtons = new QHBoxLayout;
    QPushButton *narrowBtn = new QPushButton(QStringLiteral(u"描述列宽 220"), this);
    QPushButton *wideBtn = new QPushButton(QStringLiteral(u"描述列宽 520"), this);
    widthButtons->addWidget(narrowBtn);
    widthButtons->addWidget(wideBtn);
    widthButtons->addStretch(1);
    connect(narrowBtn, &QPushButton::clicked, this, [this]() { m_model->setColumnWidth("desc", 220); });
    connect(wideBtn, &QPushButton::clicked, this, [this]() { m_model->setColumnWidth("desc", 520); });

    m_contentLayout->addLayout(modeButtons);
    m_contentLayout->addLayout(widthButtons);
    m_contentLayout->addWidget(m_grid, 1);

    addNote("WordWrap 列：按列宽自动换行，行高 = max(RowHeight 样式值, 各列内容测量高度)；"
            "其余换行模式不参与撑高（返回默认行高）。");
    addNote("拖动表头边缘调整描述列宽，行高实时重算；列宽越窄行越高，越宽行越矮。");
    addNote("自定义渲染器同样可实现 preferredHeight() 返回内容所需高度参与行高计算，"
            "返回 -1 表示不撑高（使用默认行高）。");
    addNote("换行 / 截断切换通过 setColumnDefs 更新列定义实现；表头截断模式下列 tooltip "
            "可配合 setCellTooltipTextCallback 显示完整内容。");
}

void RowHeightDemoWindow::applyWrapMode(bool wordWrap)
{
    QVector<AuColumnDef> columns;
    AuColumnDef colName("name", QStringLiteral(u"名称"));
    colName.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
    colName.setWidth(120);
    colName.setTextWrapMode(AuColumnDef::TextWrapMode::NoWrapTruncate);
    columns.append(colName);

    AuColumnDef colCount("count", QStringLiteral(u"数量"));
    colCount.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
    colCount.setWidth(80);
    colCount.setAlignment(Qt::AlignCenter);
    columns.append(colCount);

    AuColumnDef colDesc("desc", QStringLiteral(u"描述"));
    colDesc.setWidthMode(AuColumnDef::ColumnWidthMode::Flex);
    colDesc.setTextWrapMode(wordWrap ? AuColumnDef::TextWrapMode::WordWrap
                                     : AuColumnDef::TextWrapMode::NoWrapTruncate);
    columns.append(colDesc);

    m_model->setColumnDefs(columns);
}
