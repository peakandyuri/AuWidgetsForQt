#include "DatePickerPages.h"

#include "PageHelpers.h"

#include "AuDatePicker.h"
#include "AuDatePickerStyle.h"

#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

/// 创建默认尺寸的日期选择控件。
AuDatePicker *newPicker()
{
    AuDatePicker *picker = new AuDatePicker;
    return picker;
}

/// 演示页布局：顶部控制条 + 控件本体（居左）+ 可选日志。
QWidget *pickerPanel(AuDatePicker *picker, QHBoxLayout *controls = nullptr, QWidget *log = nullptr)
{
    QWidget *body = new QWidget;
    QVBoxLayout *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    v->addWidget(picker);
    v->addStretch(1);
    return PageHelpers::composeDemo(controls, body, log);
}

} // namespace

QVector<FeaturePage> makeDatePickerPages()
{
    QVector<FeaturePage> pages;

    // ============ 1. 基础选日 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"基础选日");
        page.description = QStringLiteral(
            u"点击编辑框弹出日历面板；点击日期选择，head 区域翻年翻月；"
            u"程序侧用 setDate / clear 读写。");
        page.createDemo = []() {
            AuDatePicker *picker = newPicker();

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(picker, &AuDatePicker::dateChanged, picker,
                             [log](const QDate &date) {
                                 PageHelpers::appendLog(log,
                                                        QString("dateChanged: %1").arg(date.toString("yyyy-MM-dd")));
                             });
            QObject::connect(picker, &AuDatePicker::activated, picker, [log](const QDate &date) {
                PageHelpers::appendLog(log,
                                       QString("activated: %1").arg(date.toString("yyyy-MM-dd")));
            });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *todayBtn = new QPushButton(QStringLiteral(u"setDate(今天)"));
            QPushButton *openBtn = new QPushButton(QStringLiteral(u"showPopup()"));
            QPushButton *clearBtn = new QPushButton(QStringLiteral(u"clear()"));
            controls->addWidget(todayBtn);
            controls->addWidget(openBtn);
            controls->addWidget(clearBtn);
            controls->addStretch(1);
            QObject::connect(todayBtn, &QPushButton::clicked, picker,
                             [picker]() { picker->setDate(QDate::currentDate()); });
            QObject::connect(openBtn, &QPushButton::clicked, picker,
                             [picker]() { picker->showPopup(); });
            QObject::connect(clearBtn, &QPushButton::clicked, picker,
                             [picker]() { picker->clear(); });
            return pickerPanel(picker, controls, log);
        };
        page.code = R"snip(
AuDatePicker *picker = new AuDatePicker(this);

// 读写当前值
picker->setDate(QDate::currentDate());   // 对应演示按钮 setDate(今天)
picker->setDate(QDate());                // 无效 QDate = 清空
QDate date = picker->date();
bool has = picker->hasSelection();
picker->clear();                         // 对应演示按钮

// 变化回调（程序设置或用户选择都会发出，演示日志打印）
connect(picker, &AuDatePicker::dateChanged,
        this, [](const QDate &date) { /* ... */ });

// 用户在面板中选择日期（含 footer 快捷选择）
connect(picker, &AuDatePicker::activated,
        this, [](const QDate &date) { /* ... */ });

// 程序化开合（对应演示按钮 showPopup）
picker->showPopup();
picker->hidePopup();
)snip";
        pages.append(page);
    }

    // ============ 2. 显示格式 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"显示格式");
        page.description = QStringLiteral(
            u"setDisplayFormat 自定义编辑框文本格式（QDate::toString 表达式）；"
            u"未显式设置时随选择粒度切换默认格式。");
        page.createDemo = []() {
            AuDatePicker *picker = newPicker();
            picker->setDate(QDate::currentDate());

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *isoBtn = new QPushButton(QStringLiteral(u"yyyy-MM-dd"));
            QPushButton *slashBtn = new QPushButton(QStringLiteral(u"yyyy/MM/dd"));
            QPushButton *cnBtn = new QPushButton(QStringLiteral(u"yyyy年MM月dd日"));
            controls->addWidget(isoBtn);
            controls->addWidget(slashBtn);
            controls->addWidget(cnBtn);
            controls->addStretch(1);
            QObject::connect(isoBtn, &QPushButton::clicked, picker, [picker]() {
                picker->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
            });
            QObject::connect(slashBtn, &QPushButton::clicked, picker, [picker]() {
                picker->setDisplayFormat(QStringLiteral("yyyy/MM/dd"));
            });
            QObject::connect(cnBtn, &QPushButton::clicked, picker, [picker]() {
                picker->setDisplayFormat(QStringLiteral(u"yyyy年MM月dd日"));
            });
            return pickerPanel(picker, controls);
        };
        page.code = R"snip(
AuDatePicker *picker = new AuDatePicker(this);
picker->setDate(QDate::currentDate());   // 与演示一致：先设一个日期便于观察

// 三种格式（对应演示按钮）
picker->setDisplayFormat("yyyy-MM-dd");
picker->setDisplayFormat("yyyy/MM/dd");
picker->setDisplayFormat("yyyy年MM月dd日");

QString fmt = picker->displayFormat();

// 默认格式随选择粒度切换：
// 选日 yyyy-MM-dd / 仅选月 yyyy-MM / 仅选年 yyyy；
// 显式设置过格式后，切换粒度不再重置。
)snip";
        pages.append(page);
    }

    // ============ 3. 仅选月 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"仅选月");
        page.description = QStringLiteral(
            u"setSelectionMode(SelectMonth)：面板直接呈现月份九宫格，编辑框显示 yyyy-MM。");
        page.createDemo = []() {
            AuDatePicker *picker = newPicker();
            picker->setSelectionMode(AuDatePicker::SelectMonth);

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(picker, &AuDatePicker::dateChanged, picker, [log](const QDate &date) {
                PageHelpers::appendLog(log, QString("dateChanged: %1").arg(date.toString("yyyy-MM")));
            });
            return pickerPanel(picker, nullptr, log);
        };
        page.code = R"snip(
AuDatePicker *picker = new AuDatePicker(this);

picker->setSelectionMode(AuDatePicker::SelectMonth);   // 仅选月
// 面板直接呈现月份九宫格；编辑框默认显示 yyyy-MM

picker->setDate(QDate(2026, 9, 1));
QDate date = picker->date();   // 选中月的任意代表日期，显示按 yyyy-MM

connect(picker, &AuDatePicker::dateChanged,
        this, [](const QDate &date) { /* ... */ });
)snip";
        pages.append(page);
    }

    // ============ 4. 仅选年 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"仅选年");
        page.description = QStringLiteral(
            u"setSelectionMode(SelectYear)：面板直接呈现年份九宫格（每页 12 年，"
            u"滚动 / 箭头翻页），编辑框显示 yyyy。");
        page.createDemo = []() {
            AuDatePicker *picker = newPicker();
            picker->setSelectionMode(AuDatePicker::SelectYear);

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(picker, &AuDatePicker::dateChanged, picker, [log](const QDate &date) {
                PageHelpers::appendLog(log, QString("dateChanged: %1").arg(date.toString("yyyy")));
            });
            return pickerPanel(picker, nullptr, log);
        };
        page.code = R"snip(
AuDatePicker *picker = new AuDatePicker(this);

picker->setSelectionMode(AuDatePicker::SelectYear);   // 仅选年
// 面板直接呈现年份九宫格（4 列 x 3 行 / 页），
// 鼠标滚动或 head 右侧箭头翻页（带滚动动画）

picker->setDate(QDate(2026, 1, 1));
QDate date = picker->date();   // 显示按 yyyy
)snip";
        pages.append(page);
    }

    // ============ 5. 日期范围 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"日期范围");
        page.description = QStringLiteral(
            u"setSelectionMode(SelectDateRange)：双日历面板，第一次点击选起点（起止可交换），"
            u"第二次点击选终点；悬停实时预览范围带，选中终点后面板收起。");
        page.createDemo = []() {
            AuDatePicker *picker = newPicker();
            picker->setSelectionMode(AuDatePicker::SelectDateRange);

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(picker, &AuDatePicker::dateRangeChanged, picker,
                             [log](const QDate &start, const QDate &end) {
                                 PageHelpers::appendLog(
                                     log, QString("dateRangeChanged: [%1 ~ %2]")
                                              .arg(start.toString("yyyy-MM-dd"))
                                              .arg(end.toString("yyyy-MM-dd")));
                             });
            QObject::connect(picker, &AuDatePicker::rangeActivated, picker,
                             [log](const QDate &start, const QDate &end) {
                                 PageHelpers::appendLog(
                                     log, QString("rangeActivated: [%1 ~ %2]")
                                              .arg(start.toString("yyyy-MM-dd"))
                                              .arg(end.toString("yyyy-MM-dd")));
                             });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *setBtn = new QPushButton(QStringLiteral(u"setSelectedRange(近 7 天)"));
            QPushButton *clearBtn = new QPushButton(QStringLiteral(u"clear()"));
            controls->addWidget(setBtn);
            controls->addWidget(clearBtn);
            controls->addStretch(1);
            QObject::connect(setBtn, &QPushButton::clicked, picker, [picker]() {
                picker->setSelectedRange(QDate::currentDate().addDays(-6), QDate::currentDate());
            });
            QObject::connect(clearBtn, &QPushButton::clicked, picker, [picker]() { picker->clear(); });
            return pickerPanel(picker, controls, log);
        };
        page.code = R"snip(
AuDatePicker *picker = new AuDatePicker(this);

picker->setSelectionMode(AuDatePicker::SelectDateRange);   // 日期范围（双日历）

// 程序设置（自动按时间先后排序；对应演示按钮 setSelectedRange 近 7 天）
picker->setSelectedRange(QDate::currentDate().addDays(-6), QDate::currentDate());
QDate start = picker->startDate();
QDate end = picker->endDate();
picker->clear();   // 清空（对应演示按钮）

// 程序设置 / 用户选择完成都发出（演示日志打印）
connect(picker, &AuDatePicker::dateRangeChanged,
        this, [](const QDate &start, const QDate &end) { /* ... */ });

// 用户确定范围（第二次点击 / 回车）
connect(picker, &AuDatePicker::rangeActivated,
        this, [](const QDate &start, const QDate &end) { /* ... */ });

// 交互：第一次点击选起点（起止可自动交换），悬停实时预览范围带，
// 选中终点后面板收起；两侧占位文本见“占位文本”页。
)snip";
        pages.append(page);
    }

    // ============ 6. 可选范围限制 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"可选范围限制");
        page.description = QStringLiteral(
            u"setMinimumDate / setMaximumDate（或 setDateRange 一次设置）限制可选范围："
            u"范围外日期灰显、无悬停、不可点击，年月粒度同样遵守。");
        page.createDemo = []() {
            AuDatePicker *picker = newPicker();
            QDate today = QDate::currentDate();
            picker->setDateRange(today.addDays(-7), today.addDays(7));

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *limitBtn = new QPushButton(QStringLiteral(u"限制：今天 ± 7 天"));
            QPushButton *unlimitBtn = new QPushButton(QStringLiteral(u"取消限制"));
            controls->addWidget(limitBtn);
            controls->addWidget(unlimitBtn);
            controls->addStretch(1);
            QObject::connect(limitBtn, &QPushButton::clicked, picker, [today, picker]() {
                picker->setDateRange(today.addDays(-7), today.addDays(7));
            });
            QObject::connect(unlimitBtn, &QPushButton::clicked, picker, [picker]() {
                picker->setMinimumDate(QDate());
                picker->setMaximumDate(QDate());
            });
            return pickerPanel(picker, controls);
        };
        page.code = R"snip(
AuDatePicker *picker = new AuDatePicker(this);

// 一次设置上下限（对应演示按钮“限制：今天 ± 7 天”）
picker->setDateRange(QDate::currentDate().addDays(-7), QDate::currentDate().addDays(7));

// 或分别设置（传无效 QDate 取消，对应演示按钮“取消限制”）
picker->setMinimumDate(minDate);
picker->setMaximumDate(maxDate);
picker->setMinimumDate(QDate());
picker->setMaximumDate(QDate());

// 范围外日期灰显、无悬停、不可点击；年 / 月 / 范围粒度同样遵守。
)snip";
        pages.append(page);
    }

    // ============ 7. 占位文本 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"占位文本");
        page.description = QStringLiteral(
            u"setPlaceholderText 在未选日期时灰显提示；范围模式另有 setRangePlaceholderText "
            u"配置两侧占位（演示按钮可切换到范围模式查看）。");
        page.createDemo = []() {
            AuDatePicker *picker = newPicker();
            picker->setPlaceholderText(QStringLiteral(u"请选择日期…"));

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *clearBtn = new QPushButton(QStringLiteral(u"clear()"));
            QPushButton *rangeBtn = new QPushButton(QStringLiteral(u"范围模式 + 自定义占位"));
            controls->addWidget(clearBtn);
            controls->addWidget(rangeBtn);
            controls->addStretch(1);
            QObject::connect(clearBtn, &QPushButton::clicked, picker, [picker]() { picker->clear(); });
            QObject::connect(rangeBtn, &QPushButton::clicked, picker, [picker]() {
                picker->setSelectionMode(AuDatePicker::SelectDateRange);
                picker->setRangePlaceholderText(QStringLiteral(u"出发日期"), QStringLiteral(u"返程日期"));
                picker->clear();
            });
            return pickerPanel(picker, controls);
        };
        page.code = R"snip(
AuDatePicker *picker = new AuDatePicker(this);

// 普通模式：未选日期时灰显提示文字
picker->setPlaceholderText("请选择日期…");

// 范围模式：编辑框整体为空时灰显 “start ~ end” 两侧占位
//（对应演示按钮“范围模式 + 自定义占位”）
picker->setSelectionMode(AuDatePicker::SelectDateRange);
picker->setRangePlaceholderText("出发日期", "返程日期");
picker->rangeStartPlaceholder();   // 读取开始占位
picker->rangeEndPlaceholder();     // 读取结束占位

picker->clear();   // 清空后占位文本重新显示（对应演示按钮）
)snip";
        pages.append(page);
    }

    // ============ 8. 清除按钮 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"清除按钮");
        page.description = QStringLiteral(
            u"setClearButtonEnabled(true) 后，已选日期时悬停编辑框，日历图标切换为圆圈 ×，"
            u"点击清除并发出 cleared()。");
        page.createDemo = []() {
            AuDatePicker *picker = newPicker();
            picker->setClearButtonEnabled(true);
            picker->setDate(QDate::currentDate());

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(picker, &AuDatePicker::cleared, picker, [log]() {
                PageHelpers::appendLog(log, QStringLiteral("cleared"));
            });
            return pickerPanel(picker, nullptr, log);
        };
        page.code = R"snip(
AuDatePicker *picker = new AuDatePicker(this);

picker->setClearButtonEnabled(true);      // 开启清除按钮
picker->setDate(QDate::currentDate());    // 先有选中日期，悬停编辑框才出现清除图标

// 已选日期时悬停编辑框 → 日历图标变为实心圆圈 × → 点击清除
//（演示日志打印 cleared）
connect(picker, &AuDatePicker::cleared, this, []() { /* ... */ });

picker->clear();   // API 清除同样发出 cleared()
)snip";
        pages.append(page);
    }

    // ============ 9. Footer 定制 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"Footer 定制");
        page.description = QStringLiteral(
            u"面板底部左下角默认显示内置快捷项（无时间 “今天”；含时间部分为 “此刻”，"
            u"点击直接选中并提交收起；选月 / 选年为 “当月 / 今年”）；setFooterLink 可配置"
            u"左 / 中 / 右三个超链接位（自定义左槽位会覆盖内置快捷项），"
            u"setFooterVisible(false) 隐藏 footer。范围模式（双日历）同样支持三槽位"
            u"自定义链接；回调里调用 hidePopup() 可控制面板收起"
            u"（见范围面板的“完成”链接）。");
        page.createDemo = []() {
            AuDatePicker *picker = newPicker();
            picker->setFooterLink(AuDatePicker::FooterLeft, QStringLiteral(u"昨天"), [picker]() {
                picker->setDate(QDate::currentDate().addDays(-1));
            });
            picker->setFooterLink(AuDatePicker::FooterRight, QStringLiteral(u"下周一"), [picker]() {
                QDate d = QDate::currentDate();
                picker->setDate(d.addDays(8 - d.dayOfWeek()));
            });

            // 范围模式：footer 自定义链接同样可用，回调可控制面板收起
            AuDatePicker *rangePicker = newPicker();
            rangePicker->setSelectionMode(AuDatePicker::SelectDateRange);
            rangePicker->setFooterLink(AuDatePicker::FooterLeft, QStringLiteral(u"近 7 天"), [rangePicker]() {
                rangePicker->setSelectedRange(QDate::currentDate().addDays(-6), QDate::currentDate());
            });
            rangePicker->setFooterLink(AuDatePicker::FooterRight, QStringLiteral(u"完成（收起面板）"), [rangePicker]() {
                rangePicker->hidePopup();   // footer 回调里直接控制面板收起
            });

            QWidget *body = new QWidget;
            QVBoxLayout *v = new QVBoxLayout(body);
            v->setContentsMargins(0, 0, 0, 0);
            v->setSpacing(12);
            v->addWidget(picker);
            v->addWidget(rangePicker);
            v->addStretch(1);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *hideBtn = new QPushButton(QStringLiteral(u"隐藏 footer"));
            QPushButton *showBtn = new QPushButton(QStringLiteral(u"显示 footer"));
            QPushButton *resetBtn = new QPushButton(QStringLiteral(u"恢复左槽位内置快捷项"));
            controls->addWidget(hideBtn);
            controls->addWidget(showBtn);
            controls->addWidget(resetBtn);
            controls->addStretch(1);
            QObject::connect(hideBtn, &QPushButton::clicked, body, [body]() {
                // 同时作用于页面上的两个示例控件
                for (AuDatePicker *p : body->findChildren<AuDatePicker *>())
                    p->setFooterVisible(false);
            });
            QObject::connect(showBtn, &QPushButton::clicked, body, [body]() {
                for (AuDatePicker *p : body->findChildren<AuDatePicker *>())
                    p->setFooterVisible(true);
            });
            QObject::connect(resetBtn, &QPushButton::clicked, body, [body]() {
                for (AuDatePicker *p : body->findChildren<AuDatePicker *>())
                    p->resetFooterLink(AuDatePicker::FooterLeft);
            });
            return PageHelpers::composeDemo(controls, body);
        };
        page.code = R"snip(
// 配置 footer 快捷链接（左 / 中 / 右三槽位；与演示一致的“昨天”/“下周一”）
picker->setFooterLink(AuDatePicker::FooterLeft, "昨天", [picker]() {
    picker->setDate(QDate::currentDate().addDays(-1));
});
picker->setFooterLink(AuDatePicker::FooterRight, "下周一", [picker]() {
    QDate d = QDate::currentDate();
    picker->setDate(d.addDays(8 - d.dayOfWeek()));
});

// 范围模式（双日历面板）同样支持三槽位自定义链接（无内置快捷项）
rangePicker->setSelectionMode(AuDatePicker::SelectDateRange);
rangePicker->setFooterLink(AuDatePicker::FooterLeft, "近 7 天", [rangePicker]() {
    rangePicker->setSelectedRange(QDate::currentDate().addDays(-6), QDate::currentDate());
});
rangePicker->setFooterLink(AuDatePicker::FooterRight, "完成（收起面板）", [rangePicker]() {
    rangePicker->hidePopup();   // footer 回调里调用 hidePopup() 控制面板收起
});

picker->resetFooterLink(AuDatePicker::FooterLeft);    // 恢复左槽位内置快捷项（演示按钮）
picker->setFooterVisible(false);                      // 隐藏 footer（演示按钮）
picker->setFooterVisible(true);                       // 显示 footer（演示按钮）
)snip";
        pages.append(page);
    }

    // ============ 10. 尺寸与主题 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"尺寸与主题");
        page.description = QStringLiteral(
            u"setSizeMode 切换小 / 中 / 大三档尺寸预设；setEditorHeight 微调编辑框高度；"
            u"setEditorWidthMode 切换宽度模式（按格式自适应 / 跟随布局拉伸，面板宽度始终固定）；"
            u"setStyle 设置独立亮 / 暗主题（控件接管所有权），传 nullptr 恢复跟随全局。");
        page.createDemo = []() {
            AuDatePicker *picker = newPicker();

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *smallBtn = new QPushButton(QStringLiteral(u"小"));
            QPushButton *mediumBtn = new QPushButton(QStringLiteral(u"中"));
            QPushButton *largeBtn = new QPushButton(QStringLiteral(u"大"));
            QPushButton *fitBtn = new QPushButton(QStringLiteral(u"宽度自适应（默认）"));
            QPushButton *stretchBtn = new QPushButton(QStringLiteral(u"宽度拉伸"));
            controls->addWidget(smallBtn);
            controls->addWidget(mediumBtn);
            controls->addWidget(largeBtn);
            controls->addWidget(fitBtn);
            controls->addWidget(stretchBtn);
            controls->addStretch(1);
            QPushButton *darkBtn = new QPushButton(QStringLiteral(u"暗色主题"));
            QPushButton *lightBtn = new QPushButton(QStringLiteral(u"亮色主题"));
            QPushButton *globalBtn = new QPushButton(QStringLiteral(u"跟随全局"));
            controls->addWidget(darkBtn);
            controls->addWidget(lightBtn);
            controls->addWidget(globalBtn);
            controls->addStretch(1);
            using SizeMode = AuDatePickerStyle::SizeMode;
            QObject::connect(smallBtn, &QPushButton::clicked, picker,
                             [picker]() { picker->setSizeMode(SizeMode::Small); });
            QObject::connect(mediumBtn, &QPushButton::clicked, picker,
                             [picker]() { picker->setSizeMode(SizeMode::Medium); });
            QObject::connect(largeBtn, &QPushButton::clicked, picker,
                             [picker]() { picker->setSizeMode(SizeMode::Large); });
            // 宽度拉伸：setFixedWidth(240) 取消手动固定，水平跟随布局拉伸
            QObject::connect(fitBtn, &QPushButton::clicked, picker, [picker]() {
                picker->setFixedWidth(QWIDGETSIZE_MAX);
                picker->setEditorWidthMode(AuDatePicker::WidthFitContent);
            });
            QObject::connect(stretchBtn, &QPushButton::clicked, picker, [picker]() {
                picker->setFixedWidth(QWIDGETSIZE_MAX);
                picker->setEditorWidthMode(AuDatePicker::WidthStretch);
            });
            QObject::connect(darkBtn, &QPushButton::clicked, picker,
                             [picker]() { picker->setStyle(AuDatePickerStyle::createDarkStyle()); });
            QObject::connect(lightBtn, &QPushButton::clicked, picker,
                             [picker]() { picker->setStyle(AuDatePickerStyle::createLightStyle()); });
            QObject::connect(globalBtn, &QPushButton::clicked, picker,
                             [picker]() { picker->setStyle(nullptr); });
            return pickerPanel(picker, controls);
        };
        page.code = R"snip(
// 尺寸档位：小 / 中 / 大（对应演示按钮，样式实例的尺寸类字段随之重置）
picker->setSizeMode(AuDatePickerStyle::SizeMode::Large);
picker->setEditorHeight(36);   // 可选：逐控件微调编辑框高度

// 宽度模式（对应演示按钮）：
// WidthFitContent 默认按显示格式自适应；WidthStretch 跟随布局水平拉伸
//（最小宽度 = 格式自适应宽度，文本不截断；弹层面板宽度始终固定）
picker->setEditorWidthMode(AuDatePicker::WidthStretch);
picker->setFixedWidth(QWIDGETSIZE_MAX);   // 拉伸前取消手动固定宽度
picker->setFixedWidth(260);               // 也可随时手动固定宽度

// 独立主题（对应演示按钮，所有权转移给控件）
picker->setStyle(AuDatePickerStyle::createDarkStyle());
picker->setStyle(AuDatePickerStyle::createLightStyle());

picker->setStyle(nullptr);   // 恢复跟随全局样式（对应演示按钮）

// 全局设置一次，所有未显式 setStyle 的控件自动生效（本程序 main.cpp 即如此）：
AuDatePickerStyle::setGlobalStyle(AuDatePickerStyle::createLightStyle());
)snip";
        pages.append(page);
    }

    // ============ 11. 日期时间选择 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"日期时间选择");
        page.description = QStringLiteral(
            u"displayFormat 含时间 token 时启用时间选择：日历右侧出现时 / 分 / 秒列"
            u"（仅 HH:mm 显示时、分；HH:mm:ss 显示全部），点击日期 / 时间仅更新待选值，"
            u"右下角确定按钮或回车提交，左下角快捷项变为“此刻”（点击直接选中今天 + 当前时刻并提交收起；当前时刻在时间窗口外时自动取最近的合法边界值），Esc / 点击外部取消。format 仅含时间（HH:mm）"
            u"为纯时间模式。setTimeRange 限制每日时刻窗口（如大于 12:30：0~11 时不可选，"
            u"12 时的 0~29 分不可选），窗口外的预设时间无法直接提交。");
        page.createDemo = []() {
            AuDatePicker *picker = newPicker();
            picker->setDateTime(QDateTime(QDate::currentDate(), QTime(14, 30)));

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(picker, &AuDatePicker::dateTimeChanged, picker,
                             [log](const QDateTime &dateTime) {
                                 PageHelpers::appendLog(
                                     log, QString("dateTimeChanged: %1")
                                              .arg(dateTime.toString("yyyy-MM-dd HH:mm:ss")));
                             });
            QObject::connect(picker, &AuDatePicker::dateTimeActivated, picker,
                             [log](const QDateTime &dateTime) {
                                 PageHelpers::appendLog(
                                     log, QString("dateTimeActivated: %1")
                                              .arg(dateTime.toString("yyyy-MM-dd HH:mm:ss")));
                             });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *hmBtn = new QPushButton(QStringLiteral(u"yyyy-MM-dd HH:mm"));
            QPushButton *hmsBtn = new QPushButton(QStringLiteral(u"yyyy-MM-dd HH:mm:ss"));
            QPushButton *timeOnlyBtn = new QPushButton(QStringLiteral(u"HH:mm（纯时间）"));
            QPushButton *rangeBtn = new QPushButton(QStringLiteral(u"限 12:30 ~ 18:00"));
            QPushButton *noRangeBtn = new QPushButton(QStringLiteral(u"取消时刻限制"));
            controls->addWidget(hmBtn);
            controls->addWidget(hmsBtn);
            controls->addWidget(timeOnlyBtn);
            controls->addWidget(rangeBtn);
            controls->addWidget(noRangeBtn);
            controls->addStretch(1);
            QObject::connect(hmBtn, &QPushButton::clicked, picker, [picker]() {
                picker->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
            });
            QObject::connect(hmsBtn, &QPushButton::clicked, picker, [picker]() {
                picker->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            });
            QObject::connect(timeOnlyBtn, &QPushButton::clicked, picker, [picker]() {
                picker->setDisplayFormat(QStringLiteral("HH:mm"));
            });
            // 时刻窗口：大于 12:30 才可选（0~11 时禁用，12 时的 0~29 分禁用）
            QObject::connect(rangeBtn, &QPushButton::clicked, picker, [picker, log]() {
                picker->setTimeRange(QTime(12, 30), QTime(18, 0));
                PageHelpers::appendLog(log, QStringLiteral(u"setTimeRange(12:30, 18:00)"));
            });
            QObject::connect(noRangeBtn, &QPushButton::clicked, picker, [picker, log]() {
                picker->setTimeRange(QTime(), QTime());
                PageHelpers::appendLog(log, QStringLiteral(u"setTimeRange(取消)"));
            });
            return pickerPanel(picker, controls, log);
        };
        page.code = R"snip(
AuDatePicker *picker = new AuDatePicker(this);

// format 含时间 token 即启用时间选择（决定时 / 分 / 秒列与确认交互）
picker->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));   // 时 + 分
picker->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm:ss")); // 时 + 分 + 秒
picker->setDisplayFormat(QStringLiteral("HH:mm"));              // 纯时间模式（无日历）

// 读写组合值（时间部分未设置时 QDateTime 无效）
picker->setDateTime(QDateTime(QDate::currentDate(), QTime(14, 30)));
picker->setTime(QTime(9, 0));          // 仅改时间部分
QDateTime value = picker->dateTime();
QTime time = picker->time();

// 每日时刻窗口（大于 12:30：0~11 时不可选，12 时的 0~29 分不可选）
picker->setTimeRange(QTime(12, 30), QTime(18, 0));
picker->setMinimumTime(QTime(12, 30));
picker->setMaximumTime(QTime(18, 0));
bool ok = picker->isTimeValid(QTime(13, 0));

// 交互：点日期 / 时间仅更新待选值（编辑框实时预览），
// footer 确定按钮或回车提交，Esc / 点击外部取消；
// 窗口外的预设时间打开面板后确定按钮置灰，调整为合法值才能提交
connect(picker, &AuDatePicker::dateTimeChanged,
        this, [](const QDateTime &dateTime) { /* ... */ });
connect(picker, &AuDatePicker::dateTimeActivated,
        this, [](const QDateTime &dateTime) { /* ... */ });
)snip";
        pages.append(page);
    }

    return pages;
}
