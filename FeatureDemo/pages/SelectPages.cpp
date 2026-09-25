#include "SelectPages.h"

#include "PageHelpers.h"

#include "AgSelect.h"
#include "AgSelectStyle.h"

#include <QColor>
#include <QHBoxLayout>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

/// 通用示例条目。
QStringList fruitItems()
{
    return {
        QStringLiteral(u"苹果 Apple"),
        QStringLiteral(u"香蕉 Banana"),
        QStringLiteral(u"樱桃 Cherry"),
        QStringLiteral(u"葡萄 Grape"),
        QStringLiteral(u"芒果 Mango"),
        QStringLiteral(u"橙子 Orange"),
    };
}

/// 创建带示例条目的下拉框。
AgSelect *newSelect()
{
    AgSelect *select = new AgSelect;
    select->addItems(fruitItems());
    select->setFixedWidth(240);
    return select;
}

/// 演示页布局：顶部控制条 + 控件本体（居左）+ 可选日志。
QWidget *selectPanel(AgSelect *select, QHBoxLayout *controls = nullptr, QWidget *log = nullptr)
{
    QWidget *body = new QWidget;
    QVBoxLayout *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    v->addWidget(select);
    v->addStretch(1);
    return PageHelpers::composeDemo(controls, body, log);
}

} // namespace

QVector<FeaturePage> makeSelectPages()
{
    QVector<FeaturePage> pages;

    // ============ 1. 基础单选 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"基础单选");
        page.description = QStringLiteral(
            u"addItem / addItems 添加条目，setCurrentIndex / setCurrentText 设置当前值；"
            u"点击弹出面板点选，或用键盘上下键 + 回车选择。");
        page.createDemo = []() {
            AgSelect *select = newSelect();

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(select, &AgSelect::currentIndexChanged, select,
                             [log](int index) {
                                 PageHelpers::appendLog(
                                     log, QString("currentIndexChanged: index=%1").arg(index));
                             });
            QObject::connect(select, &AgSelect::currentTextChanged, select,
                             [log](const QString &text) {
                                 PageHelpers::appendLog(
                                     log, QString("currentTextChanged: %1").arg(text));
                             });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *setBtn = new QPushButton(QStringLiteral(u"setCurrentIndex(2)"));
            QPushButton *findBtn = new QPushButton(QStringLiteral(u"findText 定位“芒果”"));
            controls->addWidget(setBtn);
            controls->addWidget(findBtn);
            controls->addStretch(1);
            QObject::connect(setBtn, &QPushButton::clicked, select,
                             [select]() { select->setCurrentIndex(2); });
            QObject::connect(findBtn, &QPushButton::clicked, select, [select]() {
                int index = select->findText(QStringLiteral(u"芒果"), Qt::MatchContains);
                select->setCurrentIndex(index);
            });
            return selectPanel(select, controls, log);
        };
        page.code = R"snip(
AgSelect *select = new AgSelect(this);

// 条目：与演示一致的 6 项（addItem 单条追加，addItem 第二参数可带用户数据）
select->addItems({"苹果 Apple", "香蕉 Banana", "樱桃 Cherry",
                  "葡萄 Grape", "芒果 Mango", "橙子 Orange"});

// 读写当前值
select->setCurrentIndex(2);          // 对应演示按钮
int index = select->currentIndex();
QString text = select->currentText();
QVariant data = select->currentData();
bool has = select->hasSelection();

// 查找（MatchExactly / MatchContains / MatchCaseSensitive）
// 对应演示按钮：findText 定位“芒果”
int found = select->findText("芒果", Qt::MatchContains);
select->setCurrentIndex(found);

// 变化回调（演示日志打印）
connect(select, &AgSelect::currentIndexChanged,
        this, [](int index) { /* ... */ });
connect(select, &AgSelect::currentTextChanged,
        this, [](const QString &text) { /* ... */ });
)snip";
        pages.append(page);
    }

    // ============ 2. 多选 chips ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"多选 Chips");
        page.description = QStringLiteral(
            u"setMultiSelection(true) 进入多选：点击选项切换勾选，编辑框以 chip 标签呈现，"
            u"chip 右侧 × 可移除，支持换行。");
        page.createDemo = []() {
            AgSelect *select = newSelect();
            select->setMultiSelection(true);
            select->setFixedWidth(360);

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(select, &AgSelect::checkedChanged, select,
                             [select, log](const QList<int> &indexes) {
                                 Q_UNUSED(indexes);
                                 PageHelpers::appendLog(
                                     log, QString("checkedChanged: [%1]")
                                              .arg(select->checkedTexts().join(", ")));
                             });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *setBtn = new QPushButton(QStringLiteral(u"勾选 0,2,4"));
            QPushButton *clearBtn = new QPushButton(QStringLiteral(u"清空勾选"));
            QPushButton *printBtn = new QPushButton(QStringLiteral(u"打印勾选"));
            controls->addWidget(setBtn);
            controls->addWidget(clearBtn);
            controls->addWidget(printBtn);
            controls->addStretch(1);
            QObject::connect(setBtn, &QPushButton::clicked, select,
                             [select]() { select->setCheckedIndexes({0, 2, 4}); });
            QObject::connect(clearBtn, &QPushButton::clicked, select,
                             [select]() { select->clearChecked(); });
            QObject::connect(printBtn, &QPushButton::clicked, select, [select, log]() {
                PageHelpers::appendLog(log, QString("checkedIndexes=[%1] texts=[%2]")
                                                 .arg(select->checkedIndexes().size())
                                                 .arg(select->checkedTexts().join(", ")));
            });
            return selectPanel(select, controls, log);
        };
        page.code = R"snip(
AgSelect *select = new AgSelect(this);
select->addItems({"苹果 Apple", "香蕉 Banana", "樱桃 Cherry",
                  "葡萄 Grape", "芒果 Mango", "橙子 Orange"});

select->setMultiSelection(true);   // 多选：chips 呈现，点击选项切换勾选

// 读写勾选集合（对应演示按钮“勾选 0,2,4”）
select->setCheckedIndexes({0, 2, 4});
QList<int> indexes = select->checkedIndexes();      // 按条目顺序
QStringList texts = select->checkedTexts();
select->clearChecked();                             // 清空勾选

// 勾选集合变化（演示日志打印勾选文本）
connect(select, &AgSelect::checkedChanged,
        this, [](const QList<int> &indexes) { /* ... */ });

// 编辑框 chip 右侧 × 可移除单个选项（面板保持当前状态）。
)snip";
        pages.append(page);
    }

    // ============ 3. 分组 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"分组");
        page.description = QStringLiteral(
            u"addGroup 追加分组标题行，其后的 addItem 归入该组；组名行不可选中、无悬停，"
            u"组内条目缩进显示。传空串结束当前分组。");
        page.createDemo = []() {
            AgSelect *select = newSelect();
            select->clear();
            select->addGroup(QStringLiteral(u"水果"));
            select->addItem(QStringLiteral(u"苹果 Apple"));
            select->addItem(QStringLiteral(u"香蕉 Banana"));
            select->addGroup(QStringLiteral(u"蔬菜"));
            select->addItem(QStringLiteral(u"白菜"));
            select->addItem(QStringLiteral(u"萝卜"));
            select->addGroup(QString());
            select->addItem(QStringLiteral(u"分组外条目"));

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *openBtn = new QPushButton(QStringLiteral(u"showPopup()"));
            controls->addWidget(openBtn);
            controls->addStretch(1);
            QObject::connect(openBtn, &QPushButton::clicked, select,
                             [select]() { select->showPopup(); });
            return selectPanel(select, controls);
        };
        page.code = R"snip(
AgSelect *select = new AgSelect(this);

select->addGroup("水果");            // 分组标题行（不可选中）
select->addItem("苹果 Apple");       // 其后的条目归入该组
select->addItem("香蕉 Banana");

select->addGroup("蔬菜");
select->addItem("白菜");
select->addItem("萝卜");

select->addGroup("");                // 空串结束分组，后续条目不缩进
select->addItem("分组外条目");
)snip";
        pages.append(page);
    }

    // ============ 4. 禁用项 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"禁用项");
        page.description = QStringLiteral(
            u"setItemEnabled 置灰个别条目：灰显不可选、不参与键盘导航。");
        page.createDemo = []() {
            AgSelect *select = newSelect();
            select->setItemEnabled(1, false);
            select->setItemEnabled(3, false);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *toggleBtn = new QPushButton(QStringLiteral(u"切换“香蕉”可用性"));
            controls->addWidget(toggleBtn);
            controls->addStretch(1);
            QObject::connect(toggleBtn, &QPushButton::clicked, select, [select]() {
                // 条目文本是“香蕉 Banana”，必须用 MatchContains 才能命中
                int index = select->findText(QStringLiteral(u"香蕉"), Qt::MatchContains);
                if (index < 0)
                    return;
                select->setItemEnabled(index, !select->isItemEnabled(index));
            });
            return selectPanel(select, controls);
        };
        page.code = R"snip(
AgSelect *select = new AgSelect(this);
select->addItems({"苹果 Apple", "香蕉 Banana", "樱桃 Cherry",
                  "葡萄 Grape", "芒果 Mango", "橙子 Orange"});

// 与演示一致：初始置灰“香蕉”(1)、“葡萄”(3)
select->setItemEnabled(1, false);   // 置灰不可选（不参与键盘导航）
select->setItemEnabled(3, false);

select->setItemEnabled(1, true);    // 恢复（演示按钮可反复切换“香蕉”）

select->isItemEnabled(1);   // 查询可用性

// 整个控件禁用用 setEnabled(false)。
)snip";
        pages.append(page);
    }

    // ============ 5. 可编辑过滤 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"可编辑过滤");
        page.description = QStringLiteral(
            u"setEditable(true) 后可直接输入，实时过滤下拉项并高亮命中文字；选择后回填，"
            u"输入与任何选项都不匹配时失焦还原。");
        page.createDemo = []() {
            AgSelect *select = newSelect();
            select->setEditable(true);
            select->setFixedWidth(300);

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(select, &AgSelect::textEdited, select, [log](const QString &text) {
                PageHelpers::appendLog(log, QString("textEdited: %1").arg(text));
            });
            return selectPanel(select, nullptr, log);
        };
        page.code = R"snip(
AgSelect *select = new AgSelect(this);
select->addItems({"苹果 Apple", "香蕉 Banana", "樱桃 Cherry",
                  "葡萄 Grape", "芒果 Mango", "橙子 Orange"});

select->setEditable(true);   // 允许输入，实时过滤下拉项

// 输入回调（演示日志打印）
connect(select, &AgSelect::textEdited,
        this, [](const QString &text) { /* ... */ });

// 行为约定：
//  - 输入实时过滤并高亮命中文字，选择后回填文本；
//  - 输入内容与任何选项都不匹配时，失焦 / 面板隐藏后
//    还原为当前选中项文本（无选中则清空）。
)snip";
        pages.append(page);
    }

    // ============ 6. 占位文本 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"占位文本");
        page.description = QStringLiteral(
            u"setPlaceholderText 在未选中任何项时灰显提示文字；选择后自动隐藏，清除后恢复。");
        page.createDemo = []() {
            AgSelect *select = newSelect();
            select->setPlaceholderText(QStringLiteral(u"请选择一种水果…"));

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *clearBtn = new QPushButton(QStringLiteral(u"clearSelection()"));
            controls->addWidget(clearBtn);
            controls->addStretch(1);
            QObject::connect(clearBtn, &QPushButton::clicked, select,
                             [select]() { select->clearSelection(); });
            return selectPanel(select, controls);
        };
        page.code = R"snip(
AgSelect *select = new AgSelect(this);
select->addItems({"苹果 Apple", "香蕉 Banana", "樱桃 Cherry",
                  "葡萄 Grape", "芒果 Mango", "橙子 Orange"});

select->setPlaceholderText("请选择一种水果…");
QString ph = select->placeholderText();

select->clearSelection();   // 清除选择后占位文本重新显示（对应演示按钮）
)snip";
        pages.append(page);
    }

    // ============ 7. 清除按钮 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"清除按钮");
        page.description = QStringLiteral(
            u"setClearButtonEnabled(true) 后，已有选择时悬停编辑框，箭头切换为圆圈 ×，"
            u"点击清除选择（单选还原为未选中，多选清空全部勾选）。");
        page.createDemo = []() {
            AgSelect *select = newSelect();
            select->setClearButtonEnabled(true);
            select->setCurrentIndex(1);

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(select, &AgSelect::cleared, select, [log]() {
                PageHelpers::appendLog(log, QStringLiteral("cleared"));
            });
            return selectPanel(select, nullptr, log);
        };
        page.code = R"snip(
AgSelect *select = new AgSelect(this);
select->addItems({"苹果 Apple", "香蕉 Banana", "樱桃 Cherry",
                  "葡萄 Grape", "芒果 Mango", "橙子 Orange"});
select->setClearButtonEnabled(true);   // 开启清除按钮
select->setCurrentIndex(1);            // 先有选中项，悬停编辑框才会出现清除图标

// 已有选择项时悬停编辑框 → 箭头变为实心圆圈 × → 点击清除（演示日志打印 cleared）
connect(select, &AgSelect::cleared, this, []() { /* ... */ });

select->clearSelection();   // API 清除同样发出 cleared()
)snip";
        pages.append(page);
    }

    // ============ 8. 图标项 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"图标项");
        page.description = QStringLiteral(
            u"addItem / setItemIcon 可为条目配置图标，显示在弹层条目左侧（当前项文本不带图标）。");
        page.createDemo = []() {
            auto colorIcon = [](const QColor &color) {
                QPixmap pix(16, 16);
                pix.fill(Qt::transparent);
                QPainter p(&pix);
                p.setRenderHint(QPainter::Antialiasing);
                p.setBrush(color);
                p.setPen(Qt::NoPen);
                p.drawEllipse(1, 1, 14, 14);
                return QIcon(pix);
            };

            AgSelect *select = new AgSelect;
            select->addItem(QStringLiteral(u"红色"), "red",
                            colorIcon(QColor(0xd9, 0x4a, 0x3e)));
            select->addItem(QStringLiteral(u"绿色"), "green",
                            colorIcon(QColor(0x43, 0xa0, 0x47)));
            select->addItem(QStringLiteral(u"蓝色"), "blue",
                            colorIcon(QColor(0x34, 0x74, 0xdb)));
            select->setFixedWidth(240);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *openBtn = new QPushButton(QStringLiteral(u"showPopup()"));
            controls->addWidget(openBtn);
            controls->addStretch(1);
            QObject::connect(openBtn, &QPushButton::clicked, select,
                             [select]() { select->showPopup(); });
            return selectPanel(select, controls);
        };
        page.code = R"snip(
// 与演示一致的三个颜色圆点图标（QPainter 画 16x16 实心圆）
auto colorIcon = [](const QColor &color) {
    QPixmap pix(16, 16);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(color);
    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, 14, 14);
    return QIcon(pix);
};

AgSelect *select = new AgSelect(this);

// 文本 + 用户数据 + 图标（与演示一致：红 / 绿 / 蓝）
select->addItem("红色", "red",   colorIcon(QColor(0xd9, 0x4a, 0x3e)));
select->addItem("绿色", "green", colorIcon(QColor(0x43, 0xa0, 0x47)));
select->addItem("蓝色", "blue",  colorIcon(QColor(0x34, 0x74, 0xdb)));

// 运行时替换 / 读取图标
select->setItemIcon(0, newIcon);
QIcon icon = select->itemIcon(0);
)snip";
        pages.append(page);
    }

    // ============ 9. 面板宽度与高度 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"面板宽度高度");
        page.description = QStringLiteral(
            u"面板默认与编辑框同宽：可切换按内容自适应（PopupFitContent）或固定宽度"
            u"（PopupFixed）；maxPopupHeight 控制内容区最大高度，超出出滚动条。");
        page.createDemo = []() {
            AgSelect *select = newSelect();
            for (int i = 0; i < 20; ++i)
                select->addItem(QStringLiteral(u"额外选项 %1").arg(i + 1));
            select->setPopupWidthMode(AgSelect::PopupFitContent);
            select->setMaxPopupHeight(220);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *fitBtn = new QPushButton(QStringLiteral(u"按内容自适应"));
            QPushButton *matchBtn = new QPushButton(QStringLiteral(u"同宽（默认）"));
            QPushButton *fixedBtn = new QPushButton(QStringLiteral(u"固定 320px"));
            QPushButton *openBtn = new QPushButton(QStringLiteral(u"showPopup()"));
            controls->addWidget(fitBtn);
            controls->addWidget(matchBtn);
            controls->addWidget(fixedBtn);
            controls->addWidget(openBtn);
            controls->addStretch(1);
            QObject::connect(fitBtn, &QPushButton::clicked, select, [select]() {
                select->setPopupWidthMode(AgSelect::PopupFitContent);
                select->showPopup();
            });
            QObject::connect(matchBtn, &QPushButton::clicked, select, [select]() {
                select->setPopupWidthMode(AgSelect::PopupMatchEditor);
                select->showPopup();
            });
            QObject::connect(fixedBtn, &QPushButton::clicked, select, [select]() {
                select->setPopupWidthMode(AgSelect::PopupFixed);
                select->setFixedPopupWidth(320);
                select->showPopup();
            });
            QObject::connect(openBtn, &QPushButton::clicked, select,
                             [select]() { select->showPopup(); });
            return selectPanel(select, controls);
        };
        page.code = R"snip(
AgSelect *select = new AgSelect(this);
// 条目：6 个水果条目（见“基础单选”页）+ 20 个“额外选项”，撑出滚动
for (int i = 0; i < 20; ++i)
    select->addItem(QString("额外选项 %1").arg(i + 1));
select->setMaxPopupHeight(220);   // 面板内容区最大高度（超出出滚动条）

// 面板宽度模式（对应演示按钮）
select->setPopupWidthMode(AgSelect::PopupMatchEditor);  // 与编辑框同宽（默认）
select->setPopupWidthMode(AgSelect::PopupFixed);        // 固定宽度
select->setFixedPopupWidth(320);                        // （不小于编辑框宽度）
select->setPopupWidthMode(AgSelect::PopupFitContent);   // 按内容自适应（演示默认）

// 程序化开合
select->showPopup();
select->hidePopup();
)snip";
        pages.append(page);
    }

    // ============ 10. 尺寸与主题 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"尺寸与主题");
        page.description = QStringLiteral(
            u"setSizeMode 切换小 / 中 / 大三档尺寸预设；setStyle 设置独立亮 / 暗主题"
            u"（控件接管所有权），传 nullptr 恢复跟随全局样式。");
        page.createDemo = []() {
            AgSelect *select = newSelect();

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *smallBtn = new QPushButton(QStringLiteral(u"小"));
            QPushButton *mediumBtn = new QPushButton(QStringLiteral(u"中"));
            QPushButton *largeBtn = new QPushButton(QStringLiteral(u"大"));
            QPushButton *darkBtn = new QPushButton(QStringLiteral(u"暗色主题"));
            QPushButton *lightBtn = new QPushButton(QStringLiteral(u"亮色主题"));
            QPushButton *globalBtn = new QPushButton(QStringLiteral(u"跟随全局"));
            controls->addWidget(smallBtn);
            controls->addWidget(mediumBtn);
            controls->addWidget(largeBtn);
            controls->addStretch(1);
            controls->addWidget(darkBtn);
            controls->addWidget(lightBtn);
            controls->addWidget(globalBtn);
            controls->addStretch(1);
            using SizeMode = AgSelectStyle::SizeMode;
            QObject::connect(smallBtn, &QPushButton::clicked, select,
                             [select]() { select->setSizeMode(SizeMode::Small); });
            QObject::connect(mediumBtn, &QPushButton::clicked, select,
                             [select]() { select->setSizeMode(SizeMode::Medium); });
            QObject::connect(largeBtn, &QPushButton::clicked, select,
                             [select]() { select->setSizeMode(SizeMode::Large); });
            QObject::connect(darkBtn, &QPushButton::clicked, select,
                             [select]() { select->setStyle(AgSelectStyle::createDarkStyle()); });
            QObject::connect(lightBtn, &QPushButton::clicked, select,
                             [select]() { select->setStyle(AgSelectStyle::createLightStyle()); });
            QObject::connect(globalBtn, &QPushButton::clicked, select,
                             [select]() { select->setStyle(nullptr); });
            return selectPanel(select, controls);
        };
        page.code = R"snip(
// 演示为 6 项水果条目的下拉框

// 尺寸档位：小 / 中 / 大（对应演示按钮，样式实例的尺寸类字段随之重置）
select->setSizeMode(AgSelectStyle::SizeMode::Large);

// 独立主题（所有权转移给控件）
select->setStyle(AgSelectStyle::createDarkStyle());
select->setStyle(AgSelectStyle::createLightStyle());

select->setStyle(nullptr);   // 恢复跟随全局样式

// 全局设置一次，所有未显式 setStyle 的控件自动生效（本程序 main.cpp 即如此）：
AgSelectStyle::setGlobalStyle(AgSelectStyle::createLightStyle());
)snip";
        pages.append(page);
    }

    // ============ 11. 事件信号 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"事件信号");
        page.description = QStringLiteral(
            u"常用回调一览（演示日志依次打印）：activated 用户激活选项、cleared 清除、"
            u"popup 系列面板开合动画回调；多选 checkedChanged 见“多选 Chips”页。");
        page.createDemo = []() {
            AgSelect *select = newSelect();
            select->setClearButtonEnabled(true);

            QPlainTextEdit *log = PageHelpers::makeLog(160);
            QObject::connect(select, &AgSelect::activated, select, [log](int index) {
                PageHelpers::appendLog(log, QString("activated: index=%1").arg(index));
            });
            QObject::connect(select, &AgSelect::cleared, select, [log]() {
                PageHelpers::appendLog(log, QStringLiteral("cleared"));
            });
            QObject::connect(select, &AgSelect::popupAboutToShow, select, [log]() {
                PageHelpers::appendLog(log, QStringLiteral("popupAboutToShow"));
            });
            QObject::connect(select, &AgSelect::popupShown, select, [log]() {
                PageHelpers::appendLog(log, QStringLiteral("popupShown"));
            });
            QObject::connect(select, &AgSelect::popupAboutToHide, select, [log]() {
                PageHelpers::appendLog(log, QStringLiteral("popupAboutToHide"));
            });
            QObject::connect(select, &AgSelect::popupHidden, select, [log]() {
                PageHelpers::appendLog(log, QStringLiteral("popupHidden"));
            });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *openBtn = new QPushButton(QStringLiteral(u"showPopup()"));
            controls->addWidget(openBtn);
            controls->addStretch(1);
            QObject::connect(openBtn, &QPushButton::clicked, select,
                             [select]() { select->showPopup(); });
            return selectPanel(select, controls, log);
        };
        page.code = R"snip(
// 演示：6 项水果条目 + 清除按钮开启，日志依次打印下列回调

// 用户激活选项（单选点选 / 键盘回车）
connect(select, &AgSelect::activated, this, [](int index) { /* ... */ });

// 用户通过清除按钮 / clearSelection() 清空
connect(select, &AgSelect::cleared, this, []() { /* ... */ });

// 面板开合回调（About = 动画开始前，完成 = 动画结束后）
connect(select, &AgSelect::popupAboutToShow, this, []() { /* ... */ });
connect(select, &AgSelect::popupShown, this, []() { /* ... */ });
connect(select, &AgSelect::popupAboutToHide, this, []() { /* ... */ });
connect(select, &AgSelect::popupHidden, this, []() { /* ... */ });

select->isPopupVisible();   // 面板是否展开
// 注：多选勾选集合变化见“多选 Chips”页（checkedChanged）。
)snip";
        pages.append(page);
    }

    return pages;
}
