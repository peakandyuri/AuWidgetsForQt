#include "DemoWindowBase.h"

#include "AgColumnDef.h"
#include "AgGridModel.h"

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

QVector<AgColumnDef> DemoWindowBase::makeStandardColumns()
{
    QVector<AgColumnDef> columns;

    AgColumnDef colA("col0", "A");
    colA.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colA.setWidth(90);
    colA.setSortable(true);
    columns.append(colA);

    AgColumnDef colB("col1", "B");
    colB.setWidthMode(AgColumnDef::ColumnWidthMode::AutoFit);
    colB.setMinWidth(60);
    colB.setEditable(true);
    colB.setSortable(true);
    columns.append(colB);

    AgColumnDef multi("Multi");
    multi.setHeaderName("Multi");
    QVector<AgColumnDef> children;
    AgColumnDef colC("col2", "C");
    colC.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colC.setWidth(120);
    colC.setResizable(false);
    colC.setEditable(true);
    AgColumnDef colD("col3", "D");
    colD.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
    colD.setWidth(120);
    colD.setEditable(true);
    children.append(colC);
    children.append(colD);
    multi.setChildren(children);
    columns.append(multi);

    AgColumnDef colE("col4", "E");
    colE.setWidthMode(AgColumnDef::ColumnWidthMode::Flex);
    colE.setFlexWeight(2);
    colE.setMinWidth(100);
    colE.setSortable(true);
    columns.append(colE);

    AgColumnDef colF("col5", "F");
    colF.setWidthMode(AgColumnDef::ColumnWidthMode::Flex);
    colF.setFlexWeight(1);
    colF.setMinWidth(100);
    columns.append(colF);

    AgColumnDef colG("col6", "Enabled");
    colG.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
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

AgGridModel *DemoWindowBase::makeStandardModel(int rowCount, QObject *parent)
{
    AgGridModel *model = new AgGridModel(parent);
    model->setColumnDefs(makeStandardColumns());
    model->setRowData(makeFlatRows(rowCount));
    return model;
}
