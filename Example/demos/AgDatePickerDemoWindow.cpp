#include "AgDatePickerDemoWindow.h"

#include "AgDatePicker.h"
#include "AgDatePickerStyle.h"

#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

AgDatePickerDemoWindow::AgDatePickerDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"AgDatePicker 日期选择"),
                     QStringLiteral(u"替代 QCalendarWidget 弹出形态：yyyy-MM-dd 格式化显示（可自定义）/ "
                                    u"年月日三粒度 / 日期范围双日历选择（范围带预览 + 下划线动画）/ "
                                    u"方向键与回车键盘操作 / 可选范围 / 悬停清除 / 年月九宫格选择页（滚动动画）/ "
                                    u"footer 快捷项与自定义 / 三种尺寸 / 亮暗主题。"),
                     parent)
    , m_darkTheme(false)
{
    QGridLayout *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(12);

    // 控件为固定尺寸（不随布局拉伸）：列内左对齐、垂直居中
    auto addRow = [&grid, this](int row, const QString &label, QWidget *widget) {
        grid->addWidget(new QLabel(label, this), row, 0, Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(widget, row, 1, Qt::AlignLeft | Qt::AlignVCenter);
    };

    // ---- 1. 基础选日 ----
    m_basicPicker = new AgDatePicker(this);
    m_basicPicker->setPlaceholderText(QStringLiteral(u"选择日期…"));
    m_basicPicker->setDate(QDate::currentDate());
    addRow(0, QStringLiteral(u"基础（yyyy-MM-dd / 悬停 × 清除）"), m_basicPicker);

    // ---- 2. 自定义显示格式 ----
    m_formatPicker = new AgDatePicker(this);
    m_formatPicker->setDisplayFormat(QStringLiteral(u"yyyy年MM月dd日"));
    m_formatPicker->setDate(QDate::currentDate());
    addRow(1, QStringLiteral(u"自定义格式（yyyy年MM月dd日）"), m_formatPicker);

    // ---- 3. 可选范围 ----
    m_rangePicker = new AgDatePicker(this);
    m_rangePicker->setDateRange(QDate::currentDate().addDays(-7),
                                QDate::currentDate().addDays(7));
    addRow(2, QStringLiteral(u"范围限制（当前日期前后 7 天）"), m_rangePicker);

    // ---- 3.5 日期范围选择（双日历） ----
    m_dateRangePicker = new AgDatePicker(this);
    m_dateRangePicker->setSelectionMode(AgDatePicker::SelectDateRange);
    QLabel *rangeValueLabel = new QLabel(this);
    auto syncRangeValue = [rangeValueLabel](const QDate &start, const QDate &end) {
        rangeValueLabel->setText(
            start.isValid()
                ? QStringLiteral(u"当前范围：%1 ~ %2")
                      .arg(start.toString(QStringLiteral("yyyy-MM-dd")),
                           end.isValid() ? end.toString(QStringLiteral("yyyy-MM-dd"))
                                         : QStringLiteral(u"（未选终点）"))
                : QStringLiteral(u"当前范围：（未选择）"));
    };
    syncRangeValue(m_dateRangePicker->startDate(), m_dateRangePicker->endDate());
    connect(m_dateRangePicker, &AgDatePicker::dateRangeChanged, syncRangeValue);
    QHBoxLayout *rangeRow = new QHBoxLayout;
    rangeRow->setSpacing(12);
    rangeRow->addWidget(m_dateRangePicker, 0, Qt::AlignLeft | Qt::AlignVCenter);
    rangeRow->addWidget(rangeValueLabel, 1);
    grid->addWidget(new QLabel(QStringLiteral(u"日期范围（双日历 / 下划线提示）"), this), 3, 0,
                    Qt::AlignRight | Qt::AlignVCenter);
    grid->addLayout(rangeRow, 3, 1);

    // ---- 4. 仅选月 / 仅选年 ----
    m_monthPicker = new AgDatePicker(this);
    m_monthPicker->setSelectionMode(AgDatePicker::SelectMonth);
    m_monthPicker->setDate(QDate::currentDate());
    m_yearPicker = new AgDatePicker(this);
    m_yearPicker->setSelectionMode(AgDatePicker::SelectYear);
    m_yearPicker->setDate(QDate::currentDate());
    QHBoxLayout *modeRow = new QHBoxLayout;
    modeRow->setSpacing(12);
    modeRow->addWidget(m_monthPicker, 1);
    modeRow->addWidget(m_yearPicker, 1);
    grid->addWidget(new QLabel(QStringLiteral(u"选到月 / 选到年"), this), 4, 0,
                    Qt::AlignRight | Qt::AlignVCenter);
    grid->addLayout(modeRow, 4, 1);

    // ---- 5. 三种尺寸 + 自定义高度 ----
    m_smallPicker = new AgDatePicker(this);
    m_smallPicker->setSizeMode(AgDatePickerStyle::SizeMode::Small);
    m_mediumPicker = new AgDatePicker(this);
    m_mediumPicker->setSizeMode(AgDatePickerStyle::SizeMode::Medium);
    m_largePicker = new AgDatePicker(this);
    m_largePicker->setSizeMode(AgDatePickerStyle::SizeMode::Large);
    m_heightPicker = new AgDatePicker(this);
    m_heightPicker->setEditorHeight(42); // 覆盖档位预设的高度
    QHBoxLayout *sizeRow = new QHBoxLayout;
    sizeRow->setSpacing(12);
    sizeRow->addWidget(m_smallPicker, 1);
    sizeRow->addWidget(m_mediumPicker, 1);
    sizeRow->addWidget(m_largePicker, 1);
    sizeRow->addWidget(m_heightPicker, 1);
    grid->addWidget(new QLabel(QStringLiteral(u"小 / 中 / 大 / 高度 42px"), this), 5, 0,
                    Qt::AlignRight | Qt::AlignVCenter);
    grid->addLayout(sizeRow, 5, 1);

    // ---- 6. footer 定制 ----
    m_footerPicker = new AgDatePicker(this);
    m_footerPicker->setFooterLink(AgDatePicker::FooterLeft, QStringLiteral(u"昨天"), [this]() {
        if (m_footerPicker)
            m_footerPicker->setDate(QDate::currentDate().addDays(-1));
    });
    m_footerPicker->setFooterLink(AgDatePicker::FooterRight, QStringLiteral(u"一周后"), [this]() {
        if (m_footerPicker)
            m_footerPicker->setDate(QDate::currentDate().addDays(7));
    });
    addRow(6, QStringLiteral(u"footer 左右快捷链接（中为“今天”）"), m_footerPicker);

    m_noFooterPicker = new AgDatePicker(this);
    m_noFooterPicker->setFooterVisible(false);
    addRow(7, QStringLiteral(u"隐藏 footer"), m_noFooterPicker);

    m_customFooterPicker = new AgDatePicker(this);
    m_customFooterPicker->setFooterWidget(makeFooterCustomWidget());
    addRow(8, QStringLiteral(u"完全自定义 footer"), m_customFooterPicker);

    // ---- 7. 主题切换 + 当前值回显 ----
    QPushButton *themeButton = new QPushButton(QStringLiteral(u"切换亮 / 暗主题"), this);
    connect(themeButton, &QPushButton::clicked, this, [this]() { toggleTheme(!m_darkTheme); });
    m_valueLabel = new QLabel(this);
    auto syncValue = [this](const QDate &date) {
        m_valueLabel->setText(date.isValid()
                                  ? QStringLiteral(u"基础选择器当前值：%1")
                                        .arg(date.toString(QStringLiteral("yyyy-MM-dd")))
                                  : QStringLiteral(u"基础选择器当前值：（已清空）"));
    };
    syncValue(m_basicPicker->date());
    connect(m_basicPicker, &AgDatePicker::dateChanged, this, syncValue);
    QHBoxLayout *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(12);
    bottomRow->addWidget(themeButton);
    bottomRow->addWidget(m_valueLabel, 1);
    grid->addWidget(new QLabel(QStringLiteral(u"主题 / 回调"), this), 9, 0,
                    Qt::AlignRight | Qt::AlignVCenter);
    grid->addLayout(bottomRow, 9, 1);

    m_contentLayout->addLayout(grid);
    m_contentLayout->addStretch(1);

    addNote(QStringLiteral(u"· 控件为固定尺寸（宽度按显示格式计算），可用 setFixedWidth / setEditorHeight 调整；"
                           u"弹层面板宽度固定（约 230px），不随编辑框宽度变化；\n"
                           u"· 键盘：Tab 在表单控件间切换（焦点控件显示蓝色边框与淡色底）；面板展开时上下左右方向键"
                           u"移动焦点日期（跨月自动翻页），PageUp / PageDown 切换月份，回车选中焦点日期；"
                           u"年 / 月九宫格页同样支持方向键与回车；\n"
                           u"· 日期范围模式：面板为左右双日历（右侧晚一个月，翻月联动）；第一次点击选开始日期"
                           u"（编辑框中开始日期下方出现主色调下划线），起点前后日期均可选，选了之前的日期自动交换；"
                           u"起点与悬停 / 焦点日期之间淡蓝色范围带实时预览；选开始日期后下划线动画滑到结束日期下方，"
                           u"选中结束日期后面板收起；\n"
                           u"· 面板 head：左 “<<” 上一年 / “<” 上一月，中间 “xxxx年xx月” 年与月分段可点击"
                           u"（悬停变蓝），右 “>” 下一月 / “>>” 下一年；\n"
                           u"· 点击年份进入年份选择页：4x3 格子 / 页，鼠标滚动或右侧上下箭头整屏切换（带滚动动画、"
                           u"无滚动条），head 左侧显示当前页区间（如 2020 ~ 2031）；点击年份后直接回到日历页；\n"
                           u"· 点击月份进入月份选择页：4x3 格子一次显示 12 个月，右侧上下箭头 / 滚轮切换年份；\n"
                           u"· 日历中前后月补齐日期灰显但可点选；范围外日期无悬停、不可选；\n"
                           u"· 已选日期时悬停编辑框，日历图标变为实心圆圈 ×，点击清除；\n"
                           u"· Esc 在年 / 月选择页返回上一级视图，在日历页（初始页）收起面板。"));
}

QWidget *AgDatePickerDemoWindow::makeFooterCustomWidget()
{
    // 完全自定义 footer：一个整体替换内置 footer 的部件（此例为三个快捷按钮）
    QWidget *footer = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(footer);
    layout->setContentsMargins(12, 4, 12, 4);
    const QVector<QPair<QString, int>> presets = {
        { QStringLiteral(u"近 1 天"), -1 },
        { QStringLiteral(u"近 1 周"), -7 },
        { QStringLiteral(u"近 1 月"), -30 },
    };
    for (const auto &preset : presets) {
        QPushButton *button = new QPushButton(preset.first, footer);
        button->setFlat(true);
        button->setCursor(Qt::PointingHandCursor);
        connect(button, &QPushButton::clicked, this, [this, days = preset.second]() {
            if (m_customFooterPicker)
                m_customFooterPicker->setDate(QDate::currentDate().addDays(days));
        });
        layout->addWidget(button);
    }
    layout->addStretch(1);
    return footer;
}

void AgDatePickerDemoWindow::toggleTheme(bool dark)
{
    m_darkTheme = dark;
    AgDatePickerStyle::setGlobalStyle(dark ? AgDatePickerStyle::createDarkStyle()
                                           : AgDatePickerStyle::createLightStyle());
}
