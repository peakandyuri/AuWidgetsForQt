#include "DemoWindowBase.h"

#include "AuColumnDef.h"
#include "AuGridModel.h"

#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

DemoWindowBase::DemoWindowBase(const QString &title, const QString &description, QWidget *parent)
    : QDialog(parent)
    , m_contentLayout(nullptr)
    , m_noteLayout(nullptr)
{
    setWindowTitle(title);
    resize(1080, 640);

    QVBoxLayout *outer = new QVBoxLayout(this);

    QLabel *titleLabel = new QLabel(title, this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    outer->addWidget(titleLabel);

    QLabel *descLabel = new QLabel(description, this);
    descLabel->setWordWrap(true);
    outer->addWidget(descLabel);

    m_contentLayout = new QVBoxLayout;
    m_contentLayout->setSpacing(8);
    outer->addLayout(m_contentLayout, 1);

    m_noteLayout = new QVBoxLayout;
    m_noteLayout->setSpacing(2);
    outer->addLayout(m_noteLayout);
}

void DemoWindowBase::addNote(const QString &text)
{
    QLabel *note = new QLabel(text, this);
    note->setWordWrap(true);
    QPalette pal = note->palette();
    QColor grey = pal.color(QPalette::WindowText);
    grey.setAlpha(160);
    pal.setColor(QPalette::WindowText, grey);
    note->setPalette(pal);
    m_noteLayout->addWidget(note);
}

QVector<AuColumnDef> DemoWindowBase::makeStandardColumns()
{
    QVector<AuColumnDef> columns;

    AuColumnDef colA("col0", "A");
    colA.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
    colA.setWidth(90);
    colA.setSortable(true);
    columns.append(colA);

    AuColumnDef colB("col1", "B");
    colB.setWidthMode(AuColumnDef::ColumnWidthMode::AutoFit);
    colB.setMinWidth(60);
    colB.setEditable(true);
    colB.setSortable(true);
    columns.append(colB);

    AuColumnDef multi("Multi");
    multi.setHeaderName("Multi");
    QVector<AuColumnDef> children;
    AuColumnDef colC("col2", "C");
    colC.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
    colC.setWidth(120);
    colC.setResizable(false);
    colC.setEditable(true);
    AuColumnDef colD("col3", "D");
    colD.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
    colD.setWidth(120);
    colD.setEditable(true);
    children.append(colC);
    children.append(colD);
    multi.setChildren(children);
    columns.append(multi);

    AuColumnDef colE("col4", "E");
    colE.setWidthMode(AuColumnDef::ColumnWidthMode::Flex);
    colE.setFlexWeight(2);
    colE.setMinWidth(100);
    colE.setSortable(true);
    columns.append(colE);

    AuColumnDef colF("col5", "F");
    colF.setWidthMode(AuColumnDef::ColumnWidthMode::Flex);
    colF.setFlexWeight(1);
    colF.setMinWidth(100);
    columns.append(colF);

    AuColumnDef colG("col6", "Enabled");
    colG.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
    colG.setWidth(90);
    colG.setCheckbox(true);
    colG.setEditable(true);
    columns.append(colG);

    // 列级表头样式覆盖示例
    columns[0].setHeaderFont(QFont("Arial", 12, QFont::Bold));
    columns[0].setHeaderForegroundColor(Qt::red);
    columns[0].setHeaderAlignment(Qt::AlignRight | Qt::AlignVCenter);

    return columns;
}

QVector<QVariantMap> DemoWindowBase::makeFlatRows(int rowCount)
{
    const QStringList comboOptions = { "Alpha", "Beta", "Gamma", "Delta" };
    QVector<QVariantMap> rows;
    rows.reserve(rowCount);
    for (int r = 0; r < rowCount; ++r) {
        QVariantMap row;
        for (int c = 0; c < 7; ++c) {
            if (c == 3)
                row.insert(QString("col%1").arg(c), comboOptions.at(r % comboOptions.size()));
            else if (c == 6)
                row.insert(QString("col%1").arg(c), (r % 2 == 0));
            else
                row.insert(QString("col%1").arg(c), QString("R%1C%2").arg(r).arg(c));
        }
        rows.append(row);
    }
    return rows;
}

AuGridModel *DemoWindowBase::makeStandardModel(int rowCount, QObject *parent)
{
    AuGridModel *model = new AuGridModel(parent);
    model->setColumnDefs(makeStandardColumns());
    model->setRowData(makeFlatRows(rowCount));
    return model;
}
