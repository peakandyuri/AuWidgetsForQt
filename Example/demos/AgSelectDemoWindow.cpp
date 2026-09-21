#include "AgSelectDemoWindow.h"

#include "AgSelect.h"
#include "AgSelectItemDelegate.h"
#include "AgSelectStyle.h"

#include <QColor>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

/// 生成一个带色块的圆形图标，用于演示下拉项的图标渲染。
QIcon makeColorDotIcon(const QColor &color)
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(4, 4, 24, 24);
    return QIcon(pixmap);
}

} // namespace

AgSelectDemoWindow::AgSelectDemoWindow(QWidget *parent)
    : DemoWindowBase(QStringLiteral(u"AgSelect 下拉选择"),
                     QStringLiteral(u"替代 QComboBox：单选 / 分组 / 禁用项 / 可编辑过滤 / 多选 chips / "
                                    u"三种尺寸 / 亮暗主题 / 面板宽度模式 / 自定义箭头与事件回调。"),
                     parent)
{
    QGridLayout *grid = new QGridLayout;
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(12);

    auto addRow = [&grid, this](int row, const QString &label, QWidget *widget) {
        grid->addWidget(new QLabel(label, this), row, 0, Qt::AlignRight | Qt::AlignVCenter);
        grid->addWidget(widget, row, 1);
    };

    // ---- 1. 基础单选：分组 + 禁用项 + 自定义字体 ----
    m_basicSelect = new AgSelect(this);
    m_basicSelect->addGroup(QStringLiteral(u"水果"));
    m_basicSelect->addItem(QStringLiteral(u"苹果"));
    m_basicSelect->addItem(QStringLiteral(u"梨"));
    m_basicSelect->addItem(QStringLiteral(u"荔枝（缺货）"));
    m_basicSelect->setItemEnabled(m_basicSelect->count() - 1, false);
    m_basicSelect->addGroup(QStringLiteral(u"蔬菜"));
    m_basicSelect->addItem(QStringLiteral(u"白菜"));
    m_basicSelect->addItem(QStringLiteral(u"萝卜"));
    m_basicSelect->addItem(QStringLiteral(u"香菜（缺货）"));
    m_basicSelect->setItemEnabled(m_basicSelect->count() - 1, false);
    m_basicSelect->setCurrentIndex(1);
    QFont yaHei(QStringLiteral("Microsoft YaHei"));
    yaHei.setPointSize(9);
    m_basicSelect->setFont(yaHei); // 自定义字体：控件级 setFont 优先于样式字体
    addRow(0, QStringLiteral(u"单选（分组 / 禁用项 / 微软雅黑）"), m_basicSelect);

    // ---- 2. 可编辑 + 过滤 ----
    m_editableSelect = new AgSelect(this);
    m_editableSelect->addItems(QStringList()
                               << QStringLiteral(u"北京") << QStringLiteral(u"上海") << QStringLiteral(u"广州")
                               << QStringLiteral(u"深圳") << QStringLiteral(u"杭州") << QStringLiteral(u"成都")
                               << QStringLiteral(u"武汉") << QStringLiteral(u"西安") << QStringLiteral(u"南京"));
    m_editableSelect->setEditable(true);
    m_editableSelect->setPlaceholderText(QStringLiteral(u"输入城市名过滤…"));
    addRow(1, QStringLiteral(u"可编辑（输入过滤）"), m_editableSelect);

    // ---- 3. 多选 chips ----
    m_multiSelect = new AgSelect(this);
    m_multiSelect->addGroup(QStringLiteral(u"前端"));
    m_multiSelect->addItem(QStringLiteral(u"Qt Widgets"));
    m_multiSelect->addItem(QStringLiteral(u"QML"));
    m_multiSelect->addGroup(QStringLiteral(u"后端"));
    m_multiSelect->addItem(QStringLiteral(u"C++"));
    m_multiSelect->addItem(QStringLiteral(u"Python"));
    m_multiSelect->addItem(QStringLiteral(u"Go"));
    m_multiSelect->addItem(QStringLiteral(u"Rust"));
    m_multiSelect->setMultiSelection(true);
    m_multiSelect->setCheckedIndexes({1, 3});
    m_multiSelect->setPlaceholderText(QStringLiteral(u"多选技术栈…"));
    addRow(2, QStringLiteral(u"多选（chips / × 移除）"), m_multiSelect);

    // ---- 4. 三种尺寸 ----
    m_smallSelect = new AgSelect(this);
    m_smallSelect->addItems(QStringList() << QStringLiteral(u"小号 Small") << QStringLiteral(u"选项 A") << QStringLiteral(u"选项 B"));
    m_smallSelect->setSizeMode(AgSelectStyle::SizeMode::Small);
    m_mediumSelect = new AgSelect(this);
    m_mediumSelect->addItems(QStringList() << QStringLiteral(u"中号 Medium") << QStringLiteral(u"选项 A") << QStringLiteral(u"选项 B"));
    m_mediumSelect->setSizeMode(AgSelectStyle::SizeMode::Medium);
    m_largeSelect = new AgSelect(this);
    m_largeSelect->addItems(QStringList() << QStringLiteral(u"大号 Large") << QStringLiteral(u"选项 A") << QStringLiteral(u"选项 B"));
    m_largeSelect->setSizeMode(AgSelectStyle::SizeMode::Large);

    QHBoxLayout *sizeRow = new QHBoxLayout;
    sizeRow->setSpacing(12);
    sizeRow->addWidget(m_smallSelect, 1);
    sizeRow->addWidget(m_mediumSelect, 1);
    sizeRow->addWidget(m_largeSelect, 1);
    grid->addWidget(new QLabel(QStringLiteral(u"三种尺寸"), this), 3, 0, Qt::AlignRight | Qt::AlignVCenter);
    grid->addLayout(sizeRow, 3, 1);

    // ---- 5. 面板宽度模式 ----
    m_widthSelect = new AgSelect(this);
    m_widthSelect->addItems(QStringList()
                            << QStringLiteral(u"固定宽度 320px 的下拉面板（超出编辑框宽度）")
                            << QStringLiteral(u"选项 B") << QStringLiteral(u"选项 C"));
    m_widthSelect->setPopupWidthMode(AgSelect::PopupFixed);
    m_widthSelect->setFixedPopupWidth(320);
    addRow(4, QStringLiteral(u"固定宽度面板"), m_widthSelect);

    // ---- 6. 自定义箭头 + 图标条目 ----
    m_customArrowSelect = new AgSelect(this);
    m_customArrowSelect->addItem(QStringLiteral(u"红色"), QVariant(), makeColorDotIcon(QColor(0xf5, 0x22, 0x2d)));
    m_customArrowSelect->addItem(QStringLiteral(u"绿色"), QVariant(), makeColorDotIcon(QColor(0x00, 0xb5, 0x78)));
    m_customArrowSelect->addItem(QStringLiteral(u"蓝色"), QVariant(), makeColorDotIcon(QColor(0x16, 0x77, 0xff)));
    m_customArrowSelect->addItem(QStringLiteral(u"橙色"), QVariant(), makeColorDotIcon(QColor(0xfa, 0x8c, 0x16)));
    QLabel *arrowLabel = new QLabel(QStringLiteral(u"▼"), this);
    arrowLabel->setAlignment(Qt::AlignCenter);
    arrowLabel->setFixedWidth(20);
    m_customArrowSelect->setArrowWidget(arrowLabel);
    addRow(5, QStringLiteral(u"自定义箭头 + 图标"), m_customArrowSelect);

    // ---- 控制行：主题切换 / 禁用 ----
    QHBoxLayout *controls = new QHBoxLayout;
    QPushButton *themeButton = new QPushButton(QStringLiteral(u"切换 亮 / 暗主题"), this);
    QPushButton *disableButton = new QPushButton(QStringLiteral(u"禁用第一个下拉框"), this);
    disableButton->setCheckable(true);
    controls->addWidget(themeButton);
    controls->addWidget(disableButton);
    controls->addStretch(1);
    grid->addLayout(controls, 6, 1);

    m_contentLayout->addLayout(grid);

    // ---- 事件日志 ----
    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setFixedHeight(120);
    m_contentLayout->addWidget(new QLabel(QStringLiteral(u"事件回调日志："), this));
    m_contentLayout->addWidget(m_logView);

    auto hookSignals = [this](AgSelect *select, const QString &name) {
        connect(select, &AgSelect::activated, this, [this, name](int index) {
            log(QStringLiteral(u"[%1] activated(%2)").arg(name).arg(index));
        });
        connect(select, &AgSelect::currentIndexChanged, this, [this, name](int index) {
            log(QStringLiteral(u"[%1] currentIndexChanged(%2)").arg(name).arg(index));
        });
        connect(select, &AgSelect::textEdited, this, [this, name](const QString &text) {
            log(QStringLiteral(u"[%1] textEdited(\"%2\")").arg(name, text));
        });
        connect(select, &AgSelect::checkedChanged, this, [this, name](const QList<int> &indexes) {
            QStringList list;
            for (int index : indexes)
                list.append(QString::number(index));
            log(QStringLiteral(u"[%1] checkedChanged([%2])").arg(name, list.join(QStringLiteral(", "))));
        });
        connect(select, &AgSelect::popupAboutToShow, this, [this, name]() {
            log(QStringLiteral(u"[%1] popupAboutToShow").arg(name));
        });
        connect(select, &AgSelect::popupShown, this, [this, name]() {
            log(QStringLiteral(u"[%1] popupShown").arg(name));
        });
        connect(select, &AgSelect::popupAboutToHide, this, [this, name]() {
            log(QStringLiteral(u"[%1] popupAboutToHide").arg(name));
        });
        connect(select, &AgSelect::popupHidden, this, [this, name]() {
            log(QStringLiteral(u"[%1] popupHidden").arg(name));
        });
    };
    hookSignals(m_basicSelect, QStringLiteral(u"基础"));
    hookSignals(m_editableSelect, QStringLiteral(u"可编辑"));
    hookSignals(m_multiSelect, QStringLiteral(u"多选"));
    hookSignals(m_customArrowSelect, QStringLiteral(u"图标"));

    connect(themeButton, &QPushButton::clicked, this, [this]() {
        const QColor windowColor = palette().color(QPalette::Window);
        const bool switchToDark = windowColor.lightness() >= 128;
        if (switchToDark) {
            AgSelectStyle::setGlobalStyle(AgSelectStyle::createDarkStyle());
            setStyleSheet(QStringLiteral("AgSelectDemoWindow { background: #1f1f1f; color: #e0e0e0; }"
                                         "QLabel { color: #e0e0e0; }"));
        } else {
            AgSelectStyle::setGlobalStyle(AgSelectStyle::createLightStyle());
            setStyleSheet(QString());
        }
        log(QStringLiteral(u"全局样式切换为%1").arg(switchToDark ? QStringLiteral(u"暗色") : QStringLiteral(u"亮色")));
    });
    connect(disableButton, &QPushButton::toggled, this, [this](bool checked) {
        m_basicSelect->setEnabled(!checked);
        log(QStringLiteral(u"基础下拉框%1").arg(checked ? QStringLiteral(u"已禁用") : QStringLiteral(u"已启用")));
    });

    addNote(QStringLiteral(u"· 单选：点击展开，支持分组标题与灰色禁用项；选择后面板收起，Esc 也可关闭。\n"
                           u"· 可编辑：输入即过滤并高亮命中文字；与选项不匹配的内容在失焦 / 面板隐藏后被清空。\n"
                           u"· 多选：点击切换勾选（右侧对勾），编辑区以 chip 呈现，chip 上的 × 可移除；"
                           u"展开时点击编辑区收起面板，× 移除不受影响。\n"
                           u"· 主题切换影响所有未单独 setStyle() 的下拉框（含已切尺寸档位的派生样式）。\n"
                           u"· 面板默认与编辑框同宽，下方空间不足自动翻转到上方；展开 / 收起有 100ms 动画。"));
}

void AgSelectDemoWindow::log(const QString &text)
{
    m_logView->append(text);
    if (m_logView->document()->blockCount() > 200)
        m_logView->clear();
}
