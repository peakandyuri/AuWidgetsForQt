#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "AgGridModel.h"
#include "AgGridWidget.h"
#include "AgGridSyncGroup.h"
#include "AgGridStyle.h"
#include "AgColumnDef.h"
#include "AgComboBoxCellEditor.h"
#include "AgCellEditValidator.h"

#include "ButtonRenderer.h"
#include "HeaderButtonRenderer.h"
#include "DemoValidator.h"

#include <QTimer>
#include <QDebug>
#include <QToolBar>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QLayout>
#include <QSplitter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_gridWidget(nullptr)
    , m_gridModel(nullptr)
    , m_gridWidgetB(nullptr)
    , m_gridModelB(nullptr)
    , m_syncGroup(nullptr)
{
    ui->setupUi(this);

    // 设置全局样式：所有未显式调用 setStyle() 的 AgGridWidget 都会自动应用
    AgGridStyle::setGlobalStyle(AgGridStyle::createLightStyle());
    // 切换到暗黑主题只需改成：AgGridStyle::setGlobalStyle(AgGridStyle::createDarkStyle());

    const int columnCount = 7;
    const int rowCount = 10000;
    const QStringList comboOptions = { "Alpha", "Beta", "Gamma", "Delta" };

    // 构建一份相同数据的模型，供左右两个表格使用（数据对比场景）
    auto makeModel = [this, columnCount, rowCount, comboOptions]() {
        AgGridModel *model = new AgGridModel(this);

        QVector<AgColumnDef> columns;

        AgColumnDef colA("col0", "A");
        colA.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
        colA.setWidth(80);
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
        colD.setCellEditor("combo");
        children.append(colC);
        children.append(colD);
        multi.setChildren(children);
        columns.append(multi);

        AgColumnDef colE("col4", "E");
        colE.setWidthMode(AgColumnDef::ColumnWidthMode::Flex);
        colE.setFlexWeight(2);
        colE.setMinWidth(100);
        colE.setSortable(true);
        colE.setHeaderRenderer("headerButton");
        columns.append(colE);

        AgColumnDef colF("col5", "F");
        colF.setWidthMode(AgColumnDef::ColumnWidthMode::Flex);
        colF.setFlexWeight(1);
        colF.setMinWidth(100);
        colF.setCellRenderer("button");
        columns.append(colF);

        AgColumnDef colG("col6", "Enabled");
        colG.setWidthMode(AgColumnDef::ColumnWidthMode::Fixed);
        colG.setWidth(90);
        colG.setCheckbox(true);
        colG.setEditable(true);
        columns.append(colG);

        columns[0].setHeaderFont(QFont("Arial", 12, QFont::Bold));
        columns[0].setHeaderForegroundColor(Qt::red);
        columns[0].setHeaderAlignment(Qt::AlignRight | Qt::AlignVCenter);

        model->setColumnDefs(columns);

        QVector<QVariantMap> rows;
        for (int r = 0; r < rowCount; ++r) {
            QVariantMap row;
            for (int c = 0; c < columnCount; ++c) {
                if (c == 3)
                    row.insert(QString("col%1").arg(c), comboOptions.at(r % comboOptions.size()));
                else if (c == 6)
                    row.insert(QString("col%1").arg(c), (r % 2 == 0));
                else
                    row.insert(QString("col%1").arg(c), QString("R%1C%2").arg(r).arg(c));
            }
            rows.append(row);
        }
        model->setRowData(rows);
        return model;
    };
    m_gridModel = makeModel();
    m_gridModelB = makeModel();

    m_gridWidget = new AgGridWidget(this);
    m_gridWidget->setModel(m_gridModel);
    m_gridWidget->setVisibleRowBuffer(5);
    m_gridWidget->registerCellRenderer("button", QSharedPointer<AgCellRenderer>(new ButtonRenderer()));
    m_gridWidget->registerHeaderRenderer("headerButton", QSharedPointer<AgHeaderRenderer>(new HeaderButtonRenderer()));
    m_gridWidget->registerCellEditor("combo", QSharedPointer<AgCellEditor>(new AgComboBoxCellEditor(comboOptions)));
    m_gridWidget->setCellEditValidator(QSharedPointer<AgCellEditValidator>(new DemoValidator()));

    // 动态可编辑示例：col2 只有在 col0 的值以 "R0" 开头时才可编辑
    m_gridWidget->setCellEditableCallback([](const AgGridModel *model, int row, int column) {
        if (column != 2 || !model)
            return true;
        return model->cellValue(row, "col0").toString().startsWith("R0");
    });

    // 启用内置行号列与复选框列
    m_gridWidget->setRowNumberColumnVisible(true);
    m_gridWidget->setCheckboxColumnVisible(true);
    connect(m_gridWidget, &AgGridWidget::rowCheckedChanged, this, [](int row, bool checked) {
        qDebug() << "Row checked changed: row=" << row << " checked=" << checked;
    });
    connect(m_gridWidget, &AgGridWidget::checkedStateChanged, this, []() {
        qDebug() << "Checkbox checked state changed";
    });

    connect(m_gridWidget, &AgGridWidget::cellValueChanged, this, [](int row, int column, const QVariant &value) {
        qDebug() << "Cell value changed: row=" << row << " column=" << column << " value=" << value;
    });

    connect(m_gridWidget, &AgGridWidget::cellCheckStateChanged, this, [](int row, int column, bool checked) {
        qDebug() << "Cell checkbox toggled: row=" << row << " column=" << column << " checked=" << checked;
    });

    connect(m_gridWidget, &AgGridWidget::cellRightClicked, this, [](int row, int column, const QPoint &globalPos) {
        qDebug() << "Cell right clicked: row=" << row << " column=" << column << " globalPos=" << globalPos;
    });
    connect(m_gridWidget, &AgGridWidget::headerRightClicked, this, [](int column, int level, const QPoint &globalPos) {
        qDebug() << "Header right clicked: column=" << column << " level=" << level << " globalPos=" << globalPos;
    });

    // 启用行拖拽排序
    m_gridWidget->setRowDragEnabled(true);
    connect(m_gridWidget, &AgGridWidget::rowsDragged, this, [](const QVector<int> &originalRows, int targetOriginalIndex) {
        qDebug() << "Rows dragged: count=" << originalRows.size() << " targetOriginalIndex=" << targetOriginalIndex;
    });

    // 左右两个相同数据的表格，通过 AgGridSyncGroup 同步滚动与列宽（数据对比场景）
    m_gridWidgetB = new AgGridWidget(this);
    m_gridWidgetB->setModel(m_gridModelB);
    m_gridWidgetB->setVisibleRowBuffer(5);
    m_gridWidgetB->registerCellRenderer("button", QSharedPointer<AgCellRenderer>(new ButtonRenderer()));
    m_gridWidgetB->registerHeaderRenderer("headerButton", QSharedPointer<AgHeaderRenderer>(new HeaderButtonRenderer()));
    m_gridWidgetB->registerCellEditor("combo", QSharedPointer<AgCellEditor>(new AgComboBoxCellEditor(comboOptions)));
    m_gridWidgetB->setCellEditValidator(QSharedPointer<AgCellEditValidator>(new DemoValidator()));
    m_gridWidgetB->setRowNumberColumnVisible(true);
    m_gridWidgetB->setCheckboxColumnVisible(true);

    QSplitter *splitter = new QSplitter(Qt::Horizontal, ui->centralwidget);
    splitter->addWidget(m_gridWidget);
    splitter->addWidget(m_gridWidgetB);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    ui->centralwidget->layout()->addWidget(splitter);

    m_syncGroup = new AgGridSyncGroup(this);
    m_syncGroup->addGrid(m_gridWidget);
    m_syncGroup->addGrid(m_gridWidgetB);
    m_syncGroup->syncFrom(m_gridWidget);


    // 滚动定位测试工具栏
    QToolBar *scrollBar = addToolBar(QStringLiteral(u"定位"));
    QLabel *rowLabel = new QLabel(QStringLiteral(u"行:"));
    QSpinBox *rowSpin = new QSpinBox(scrollBar);
    rowSpin->setRange(0, rowCount - 1);
    rowSpin->setValue(0);
    QLabel *colLabel = new QLabel(QStringLiteral(u"列:"));
    QSpinBox *colSpin = new QSpinBox(scrollBar);
    colSpin->setRange(0, columnCount - 1);
    colSpin->setValue(0);

    QPushButton *rowTopBtn = new QPushButton(QStringLiteral(u"行顶"), scrollBar);
    QPushButton *rowCenterBtn = new QPushButton(QStringLiteral(u"行中"), scrollBar);
    QPushButton *rowBottomBtn = new QPushButton(QStringLiteral(u"行底"), scrollBar);
    QPushButton *cellVisibleBtn = new QPushButton(QStringLiteral(u"单元格可见"), scrollBar);
    QPushButton *cellCenterBtn = new QPushButton(QStringLiteral(u"单元格居中"), scrollBar);
    QPushButton *selectRowBtn = new QPushButton(QStringLiteral(u"选中行"), scrollBar);
    QPushButton *selectCellBtn = new QPushButton(QStringLiteral(u"选中单元格"), scrollBar);
    QPushButton *clearSelectionBtn = new QPushButton(QStringLiteral(u"清除选中"), scrollBar);
    QPushButton *pinLeftBtn = new QPushButton(QStringLiteral(u"停靠左侧"), scrollBar);
    QPushButton *pinRightBtn = new QPushButton(QStringLiteral(u"停靠右侧"), scrollBar);
    QPushButton *unpinBtn = new QPushButton(QStringLiteral(u"取消停靠"), scrollBar);
    QPushButton *toggleRowNumberBtn = new QPushButton(QStringLiteral(u"行号列"), scrollBar);
    toggleRowNumberBtn->setCheckable(true);
    toggleRowNumberBtn->setChecked(m_gridWidget->isRowNumberColumnVisible());

    QPushButton *toggleCheckboxBtn = new QPushButton(QStringLiteral(u"复选框列"), scrollBar);
    toggleCheckboxBtn->setCheckable(true);
    toggleCheckboxBtn->setChecked(true);

    QPushButton *toggleSortingBtn = new QPushButton(QStringLiteral(u"列头排序"), scrollBar);
    toggleSortingBtn->setCheckable(true);
    toggleSortingBtn->setChecked(m_gridWidget->isSortingEnabled());

    QPushButton *toggleRowDragBtn = new QPushButton(QStringLiteral(u"行拖拽"), scrollBar);
    toggleRowDragBtn->setCheckable(true);
    toggleRowDragBtn->setChecked(m_gridWidget->isRowDragEnabled());

    QPushButton *logicalSortBtn = new QPushButton(QStringLiteral(u"逻辑排序"), scrollBar);
    QPushButton *clearSortBtn = new QPushButton(QStringLiteral(u"清除排序"), scrollBar);

    QPushButton *filterBtn = new QPushButton(QStringLiteral(u"过滤 R0"), scrollBar);
    QPushButton *clearFilterBtn = new QPushButton(QStringLiteral(u"清除过滤"), scrollBar);
    QPushButton *printCheckedBtn = new QPushButton(QStringLiteral(u"打印勾选"), scrollBar);

    scrollBar->addWidget(rowLabel);
    scrollBar->addWidget(rowSpin);
    scrollBar->addWidget(colLabel);
    scrollBar->addWidget(colSpin);
    scrollBar->addWidget(rowTopBtn);
    scrollBar->addWidget(rowCenterBtn);
    scrollBar->addWidget(rowBottomBtn);
    scrollBar->addWidget(cellVisibleBtn);
    scrollBar->addWidget(cellCenterBtn);
    scrollBar->addWidget(selectRowBtn);
    scrollBar->addWidget(selectCellBtn);
    scrollBar->addWidget(clearSelectionBtn);
    scrollBar->addWidget(pinLeftBtn);
    scrollBar->addWidget(pinRightBtn);
    scrollBar->addWidget(unpinBtn);
    scrollBar->addWidget(toggleRowNumberBtn);
    scrollBar->addWidget(toggleCheckboxBtn);
    scrollBar->addWidget(toggleSortingBtn);
    scrollBar->addWidget(toggleRowDragBtn);
    scrollBar->addWidget(logicalSortBtn);
    scrollBar->addWidget(clearSortBtn);
    scrollBar->addWidget(filterBtn);
    scrollBar->addWidget(clearFilterBtn);
    scrollBar->addWidget(printCheckedBtn);

    // 多表格同步开关：滚动与列宽在两个表格间互相同步
    QPushButton *toggleSyncBtn = new QPushButton(QStringLiteral(u"同步"), scrollBar);
    toggleSyncBtn->setCheckable(true);
    toggleSyncBtn->setChecked(true);
    scrollBar->addWidget(toggleSyncBtn);
    connect(toggleSyncBtn, &QPushButton::toggled, this, [this](bool checked) {
        if (m_syncGroup)
            m_syncGroup->setSyncEnabled(checked);
    });

    connect(rowTopBtn, &QPushButton::clicked, this, [this, rowSpin]() {
        m_gridWidget->scrollToRow(rowSpin->value(), AgGridWidget::ScrollHint::PositionTop);
    });
    connect(rowCenterBtn, &QPushButton::clicked, this, [this, rowSpin]() {
        m_gridWidget->scrollToRow(rowSpin->value(), AgGridWidget::ScrollHint::PositionCenter);
    });
    connect(rowBottomBtn, &QPushButton::clicked, this, [this, rowSpin]() {
        m_gridWidget->scrollToRow(rowSpin->value(), AgGridWidget::ScrollHint::PositionBottom);
    });
    connect(cellVisibleBtn, &QPushButton::clicked, this, [this, rowSpin, colSpin]() {
        m_gridWidget->scrollToCell(rowSpin->value(), colSpin->value(), AgGridWidget::ScrollHint::EnsureVisible);
    });
    connect(cellCenterBtn, &QPushButton::clicked, this, [this, rowSpin, colSpin]() {
        m_gridWidget->scrollToCell(rowSpin->value(), colSpin->value(), AgGridWidget::ScrollHint::PositionCenter);
    });
    connect(selectRowBtn, &QPushButton::clicked, this, [this, rowSpin]() {
        m_gridWidget->selectRow(rowSpin->value());
    });
    connect(selectCellBtn, &QPushButton::clicked, this, [this, rowSpin, colSpin]() {
        m_gridWidget->selectCell(rowSpin->value(), colSpin->value());
    });
    connect(clearSelectionBtn, &QPushButton::clicked, this, [this]() {
        m_gridWidget->clearSelection();
    });
    connect(pinLeftBtn, &QPushButton::clicked, this, [this, colSpin]() {
        m_gridWidget->pinColumn(colSpin->value(), AgColumnDef::PinMode::Left);
    });
    connect(pinRightBtn, &QPushButton::clicked, this, [this, colSpin]() {
        m_gridWidget->pinColumn(colSpin->value(), AgColumnDef::PinMode::Right);
    });
    connect(unpinBtn, &QPushButton::clicked, this, [this, colSpin]() {
        m_gridWidget->pinColumn(colSpin->value(), AgColumnDef::PinMode::None);
    });
    connect(toggleRowNumberBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_gridWidget->setRowNumberColumnVisible(checked);
    });
    connect(toggleCheckboxBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_gridWidget->setCheckboxColumnVisible(checked);
    });
    connect(toggleSortingBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_gridWidget->setSortingEnabled(checked);
    });
    connect(toggleRowDragBtn, &QPushButton::toggled, this, [this](bool checked) {
        m_gridWidget->setRowDragEnabled(checked);
    });
    connect(logicalSortBtn, &QPushButton::clicked, this, [this]() {
        // 逻辑排序示例：先按第 0 列升序，再按第 1 列降序
        QList<AgSortRule> rules;
        rules << AgSortRule{0, Qt::AscendingOrder};
        rules << AgSortRule{1, Qt::DescendingOrder};
        m_gridWidget->setSortRules(rules);
    });
    connect(clearSortBtn, &QPushButton::clicked, this, [this]() {
        m_gridWidget->clearSort();
    });
    connect(filterBtn, &QPushButton::clicked, this, [this]() {
        m_gridWidget->setFilterCallback([](const QVariantMap &row) {
            return row.value("col0").toString().startsWith("R0");
        });
    });
    connect(clearFilterBtn, &QPushButton::clicked, this, [this]() {
        m_gridWidget->clearFilter();
    });
    connect(printCheckedBtn, &QPushButton::clicked, this, [this]() {
        qDebug() << "Visible checked rows:" << m_gridWidget->checkedRows();
        qDebug() << "Original checked rows:" << m_gridWidget->checkedOriginalRows();
    });

    // 列隐藏/显示测试工具栏
    QToolBar *visibilityBar = addToolBar(QStringLiteral(u"列显隐"));
    QPushButton *hideColBtn = new QPushButton(QStringLiteral(u"隐藏列"), visibilityBar);
    QPushButton *showColBtn = new QPushButton(QStringLiteral(u"显示列"), visibilityBar);
    QPushButton *hideGroupBtn = new QPushButton(QStringLiteral(u"隐藏Multi分组"), visibilityBar);
    QPushButton *showGroupBtn = new QPushButton(QStringLiteral(u"显示Multi分组"), visibilityBar);
    QPushButton *hideLastBtn = new QPushButton(QStringLiteral(u"隐藏最右列"), visibilityBar);
    QPushButton *showLastBtn = new QPushButton(QStringLiteral(u"显示最右列"), visibilityBar);

    visibilityBar->addWidget(hideColBtn);
    visibilityBar->addWidget(showColBtn);
    visibilityBar->addWidget(hideGroupBtn);
    visibilityBar->addWidget(showGroupBtn);
    visibilityBar->addWidget(hideLastBtn);
    visibilityBar->addWidget(showLastBtn);

    connect(hideColBtn, &QPushButton::clicked, this, [this, colSpin]() {
        m_gridWidget->setColumnVisible(colSpin->value(), false);
    });
    connect(showColBtn, &QPushButton::clicked, this, [this, colSpin]() {
        m_gridWidget->setColumnVisible(colSpin->value(), true);
    });
    connect(hideGroupBtn, &QPushButton::clicked, this, [this]() {
        m_gridWidget->setColumnVisible(QStringLiteral("Multi"), false);
    });
    connect(showGroupBtn, &QPushButton::clicked, this, [this]() {
        m_gridWidget->setColumnVisible(QStringLiteral("Multi"), true);
    });
    connect(hideLastBtn, &QPushButton::clicked, this, [this]() {
        m_gridWidget->setColumnVisible(m_gridModel->columnCount() - 1, false);
    });
    connect(showLastBtn, &QPushButton::clicked, this, [this]() {
        m_gridWidget->setColumnVisible(m_gridModel->columnCount() - 1, true);
    });

    // 外观设置工具栏
    QToolBar *appearanceBar = addToolBar(QStringLiteral(u"外观"));

    QPushButton *sizeSmallBtn = new QPushButton(QStringLiteral(u"小"), appearanceBar);
    QPushButton *sizeMediumBtn = new QPushButton(QStringLiteral(u"中"), appearanceBar);
    QPushButton *sizeLargeBtn = new QPushButton(QStringLiteral(u"大"), appearanceBar);
    sizeSmallBtn->setCheckable(true);
    sizeMediumBtn->setCheckable(true);
    sizeLargeBtn->setCheckable(true);
    sizeSmallBtn->setChecked(true);

    QPushButton *themeToggleBtn = new QPushButton(QStringLiteral(u"切换主题"), appearanceBar);

    appearanceBar->addWidget(sizeSmallBtn);
    appearanceBar->addWidget(sizeMediumBtn);
    appearanceBar->addWidget(sizeLargeBtn);
    appearanceBar->addWidget(themeToggleBtn);

    auto updateSizeButtons = [sizeSmallBtn, sizeMediumBtn, sizeLargeBtn](AgGridWidget::SizeMode mode) {
        sizeSmallBtn->setChecked(mode == AgGridWidget::SizeMode::Small);
        sizeMediumBtn->setChecked(mode == AgGridWidget::SizeMode::Medium);
        sizeLargeBtn->setChecked(mode == AgGridWidget::SizeMode::Large);
    };

    connect(sizeSmallBtn, &QPushButton::clicked, this, [this, updateSizeButtons]() {
        m_gridWidget->setSizeMode(AgGridWidget::SizeMode::Small);
        updateSizeButtons(m_gridWidget->sizeMode());
    });
    connect(sizeMediumBtn, &QPushButton::clicked, this, [this, updateSizeButtons]() {
        m_gridWidget->setSizeMode(AgGridWidget::SizeMode::Medium);
        updateSizeButtons(m_gridWidget->sizeMode());
    });
    connect(sizeLargeBtn, &QPushButton::clicked, this, [this, updateSizeButtons]() {
        m_gridWidget->setSizeMode(AgGridWidget::SizeMode::Large);
        updateSizeButtons(m_gridWidget->sizeMode());
    });
    connect(themeToggleBtn, &QPushButton::clicked, this, []() {
        static bool dark = false;
        dark = !dark;
        AgGridStyle::setGlobalStyle(dark ? AgGridStyle::createDarkStyle()
                                         : AgGridStyle::createLightStyle());
    });

    // 动态更新演示：每秒修改一次 (row=5, col=1) 的值（两个表格同步修改，保持数据一致）
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        static int n = 0;
        const QString value = QString("Updated %1").arg(++n);
        m_gridModel->setCellValue(5, "col1", value);
        m_gridModelB->setCellValue(5, "col1", value);
    });
    timer->start(1000);

}

MainWindow::~MainWindow()
{
    delete ui;
}

