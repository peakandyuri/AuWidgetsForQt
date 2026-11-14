#include "MainWindow.h"

#include "aujson.hpp"
#include "aujson_qt.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <QJsonDocument>

#include <algorithm>

// ---------------------------------------------------------------- 辅助

namespace {

/// 计时器读数转毫秒（ns 精度，解析单次通常不足 1ms）。
inline double nsToMs(qint64 ns)
{
    return static_cast<double>(ns) / 1e6;
}

/// 防抖间隔（ms）：输入停顿这么久之后才触发自动解析。
const int AUTO_PARSE_DEBOUNCE_MS = 300;

} // namespace

// ---------------------------------------------------------------- 构造 / UI

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_input(nullptr)
    , m_tree(nullptr)
    , m_timeLabel(nullptr)
    , m_statLabel(nullptr)
    , m_errLabel(nullptr)
    , m_log(nullptr)
    , m_sampleCombo(nullptr)
    , m_runsSpin(nullptr)
    , m_autoCheck(nullptr)
    , m_cmpCheck(nullptr)
    , m_autoTimer(nullptr)
{
    setWindowTitle(QStringLiteral(u"AuJson 速度测试"));
    resize(1100, 720);

    const QFont monoFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);

    auto *central = new QWidget(this);
    auto *outer = new QVBoxLayout(central);
    outer->setSpacing(8);

    auto *desc = new QLabel(
        QStringLiteral(u"左侧录入 JSON 文本，输入停顿后自动解析为右侧树结构，并显示本次解析耗时；"
                       u"底部可对同一份数据连续解析多次进行测速（可选与 QJsonDocument 对比）。"),
        central);
    desc->setWordWrap(true);
    outer->addWidget(desc);

    // ---- 上半区：输入 | 解析树 ----
    auto *splitter = new QSplitter(Qt::Horizontal, central);

    auto *inputBox = new QGroupBox(QStringLiteral(u"JSON 输入"), splitter);
    auto *inputLayout = new QVBoxLayout(inputBox);

    m_input = new QPlainTextEdit(inputBox);
    m_input->setFont(monoFont);
    m_input->setPlaceholderText(QStringLiteral(u"在此录入 JSON…（Ctrl+Enter 立即解析）"));
    m_input->installEventFilter(this);
    inputLayout->addWidget(m_input, 1);

    auto *inputBar = new QHBoxLayout;
    m_sampleCombo = new QComboBox(inputBox);
    m_sampleCombo->addItem(QStringLiteral(u"示例 1：配置对象（小）"));
    m_sampleCombo->addItem(QStringLiteral(u"示例 2：大数组 × 1000"));
    m_sampleCombo->addItem(QStringLiteral(u"示例 3：深嵌套 × 50"));
    auto *loadBtn = new QPushButton(QStringLiteral(u"载入示例"), inputBox);
    auto *clearBtn = new QPushButton(QStringLiteral(u"清空"), inputBox);
    auto *parseBtn = new QPushButton(QStringLiteral(u"解析"), inputBox);
    parseBtn->setDefault(true);
    inputBar->addWidget(new QLabel(QStringLiteral(u"示例:"), inputBox));
    inputBar->addWidget(m_sampleCombo, 1);
    inputBar->addWidget(loadBtn);
    inputBar->addWidget(clearBtn);
    inputBar->addWidget(parseBtn);
    inputLayout->addLayout(inputBar);

    m_autoCheck = new QCheckBox(QStringLiteral(u"输入后自动解析（延迟 %1ms）")
                                    .arg(AUTO_PARSE_DEBOUNCE_MS), inputBox);
    m_autoCheck->setChecked(true);
    inputLayout->addWidget(m_autoCheck);

    splitter->addWidget(inputBox);

    auto *treeBox = new QGroupBox(QStringLiteral(u"解析结果树"), splitter);
    auto *treeLayout = new QVBoxLayout(treeBox);

    m_tree = new QTreeWidget(treeBox);
    m_tree->setHeaderLabels({ QStringLiteral(u"名称"), QStringLiteral(u"类型"), QStringLiteral(u"值") });
    m_tree->setColumnWidth(0, 220);
    m_tree->setColumnWidth(1, 90);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tree->setAlternatingRowColors(true);
    m_tree->setExpandsOnDoubleClick(true);
    treeLayout->addWidget(m_tree, 1);

    splitter->addWidget(treeBox);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    outer->addWidget(splitter, 1);

    // ---- 统计行 ----
    auto *statBar = new QHBoxLayout;
    m_timeLabel = new QLabel(QStringLiteral(u"解析耗时：—"), central);
    QFont bigFont = m_timeLabel->font();
    bigFont.setBold(true);
    bigFont.setPointSize(bigFont.pointSize() + 2);
    m_timeLabel->setFont(bigFont);
    m_statLabel = new QLabel(QStringLiteral(u"字节 — · 节点 — · 最大深度 —"), central);
    m_errLabel = new QLabel(central);
    m_errLabel->setStyleSheet(QStringLiteral(u"color: #c00000;"));
    m_errLabel->setWordWrap(true);
    statBar->addWidget(m_timeLabel);
    statBar->addSpacing(16);
    statBar->addWidget(m_statLabel);
    statBar->addStretch(1);
    outer->addLayout(statBar);
    outer->addWidget(m_errLabel);

    // ---- 底部：速度测试 ----
    auto *benchBox = new QGroupBox(QStringLiteral(u"速度测试（同一份数据连续解析）"), central);
    auto *benchLayout = new QVBoxLayout(benchBox);

    auto *benchBar = new QHBoxLayout;
    benchBar->addWidget(new QLabel(QStringLiteral(u"重复次数:"), benchBox));
    m_runsSpin = new QSpinBox(benchBox);
    m_runsSpin->setRange(1, 100000);
    m_runsSpin->setValue(100);
    benchBar->addWidget(m_runsSpin);
    m_cmpCheck = new QCheckBox(QStringLiteral(u"与 QJsonDocument 对比"), benchBox);
    benchBar->addWidget(m_cmpCheck);
    benchBar->addStretch(1);
    auto *benchBtn = new QPushButton(QStringLiteral(u"开始测速"), benchBox);
    benchBar->addWidget(benchBtn);
    benchLayout->addLayout(benchBar);

    m_log = new QPlainTextEdit(benchBox);
    m_log->setFont(monoFont);
    m_log->setReadOnly(true);
    m_log->setMaximumHeight(140);
    m_log->setPlaceholderText(QStringLiteral(u"测速结果输出…"));
    benchLayout->addWidget(m_log);

    outer->addWidget(benchBox);

    setCentralWidget(central);

    // ---- 信号 ----
    m_autoTimer = new QTimer(this);
    m_autoTimer->setSingleShot(true);
    m_autoTimer->setInterval(AUTO_PARSE_DEBOUNCE_MS);

    connect(m_input, &QPlainTextEdit::textChanged, this, &MainWindow::onTextChanged);
    connect(m_autoTimer, &QTimer::timeout, this, &MainWindow::onParse);
    connect(parseBtn, &QPushButton::clicked, this, &MainWindow::onParse);
    connect(loadBtn, &QPushButton::clicked, this, [this] { onLoadSample(m_sampleCombo->currentIndex()); });
    connect(m_sampleCombo,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLoadSample);
    connect(clearBtn, &QPushButton::clicked, m_input, &QPlainTextEdit::clear);
    connect(benchBtn, &QPushButton::clicked, this, &MainWindow::onBench);

    // 初始载入示例 1，稍后自动解析，开箱即见效果
    onLoadSample(0);
}

// ---------------------------------------------------------------- 解析与显示

void MainWindow::onTextChanged()
{
    if (m_autoCheck->isChecked()) m_autoTimer->start();
}

void MainWindow::onLoadSample(int index)
{
    m_input->setPlainText(sampleText(index));
}

void MainWindow::onParse()
{
    m_autoTimer->stop();
    m_errLabel->clear();

    const QByteArray data = m_input->toPlainText().toUtf8();

    if (data.trimmed().isEmpty()) {
        m_tree->clear();
        m_timeLabel->setText(QStringLiteral(u"解析耗时：—"));
        m_statLabel->setText(QStringLiteral(u"字节 0 · 节点 0 · 最大深度 0"));
        m_lastData.clear();
        return;
    }

    aujson::Value root;
    QString err;
    QElapsedTimer t;
    t.start();
    const bool ok = aujson::qt::parse(data, root, &err);
    const double ms = nsToMs(t.nsecsElapsed());

    if (!ok) {
        m_tree->clear();
        m_timeLabel->setText(QStringLiteral(u"解析耗时：失败"));
        m_statLabel->setText(QStringLiteral(u"字节 %1 · 节点 — · 最大深度 —").arg(data.size()));
        m_errLabel->setText(QStringLiteral(u"解析失败：%1").arg(err));
        m_lastData.clear();
        return;
    }

    m_lastData = data;

    m_tree->clear();
    m_tree->setUpdatesEnabled(false);
    const int nodes = buildTree(root, nullptr, QStringLiteral(u"<root>"));
    m_tree->setUpdatesEnabled(true);
    m_tree->expandToDepth(2);

    m_timeLabel->setText(QStringLiteral(u"解析耗时：%1 ms").arg(ms, 0, 'f', 3));
    m_statLabel->setText(QStringLiteral(u"字节 %1 · 节点 %2 · 最大深度 %3")
                             .arg(data.size())
                             .arg(nodes)
                             .arg(maxDepth(root, 1)));
}

int MainWindow::buildTree(const aujson::Value &v, QTreeWidgetItem *parent, const QString &name)
{
    // 根调用 parent 为空：以 m_tree 构造，QTreeWidgetItem(QTreeWidget*) 会将其登记为顶层项；
    // 否则根 item 成孤儿节点，整棵树不显示
    auto *item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_tree);
    item->setText(0, name);
    item->setText(1, QString::fromLatin1(v.typeName()));
    item->setText(2, valueSummary(v));
    if (v.isString())
        item->setToolTip(2, QString::fromStdString(v.asString()));
    if (v.isArray() || v.isObject()) {
        QFont f = item->font(0);
        f.setBold(true);
        item->setFont(0, f);
    }

    int count = 1;
    if (v.isArray()) {
        for (size_t i = 0; i < v.size(); ++i)
            count += buildTree(v[i], item, QString::number(i));
    } else if (v.isObject()) {
        for (auto it = v.begin(); it != v.end(); ++it) {
            const aujson::Value::ConstItem ci = *it;
            count += buildTree(*ci.value, item,
                               ci.key ? QString::fromStdString(*ci.key) : QString());
        }
    }
    return count;
}

int MainWindow::maxDepth(const aujson::Value &v, int cur) const
{
    int best = cur;
    if (v.isArray()) {
        for (size_t i = 0; i < v.size(); ++i)
            best = std::max(best, maxDepth(v[i], cur + 1));
    } else if (v.isObject()) {
        for (auto it = v.begin(); it != v.end(); ++it)
            best = std::max(best, maxDepth(*(*it).value, cur + 1));
    }
    return best;
}

QString MainWindow::valueSummary(const aujson::Value &v) const
{
    switch (v.type()) {
    case aujson::Type::Null:   return QStringLiteral(u"null");
    case aujson::Type::Bool:   return v.asBool() ? QStringLiteral(u"true") : QStringLiteral(u"false");
    case aujson::Type::Int:    return QString::number(v.asInt());
    case aujson::Type::UInt:   return QString::number(v.asUInt());
    case aujson::Type::Double: return QString::number(v.asDouble(), 'g', 12);
    case aujson::Type::String: {
        const QString s = QString::fromStdString(v.asString());
        return s.size() > 120 ? s.left(120) + QStringLiteral(u"…") : s;
    }
    case aujson::Type::Array:  return QStringLiteral(u"[ %1 项 ]").arg(v.size());
    case aujson::Type::Object: return QStringLiteral(u"{ %1 项 }").arg(v.size());
    }
    return QString();
}

// ---------------------------------------------------------------- 测速

void MainWindow::onBench()
{
    const QByteArray data = m_lastData.isEmpty() ? m_input->toPlainText().toUtf8() : m_lastData;
    if (data.trimmed().isEmpty()) {
        m_log->appendPlainText(QStringLiteral(u"请先录入或解析 JSON。"));
        return;
    }

    const int runs = m_runsSpin->value();
    QElapsedTimer t;

    m_log->appendPlainText(QStringLiteral(u"===== 测速开始（%1 次，输入 %2 字节）=====")
                               .arg(runs)
                               .arg(data.size()));

    // ---- AuJson DOM 解析 ----
    double best = 1e300, worst = 0.0, total = 0.0;
    size_t sink = 0;
    aujson::Value root;
    QString err;
    for (int i = 0; i < runs; ++i) {
        t.restart();
        const bool ok = aujson::qt::parse(data, root, &err);
        const double ms = nsToMs(t.nsecsElapsed());
        if (!ok) {
            m_log->appendPlainText(QStringLiteral(u"AuJson 解析失败：%1").arg(err));
            return;
        }
        best = std::min(best, ms);
        worst = std::max(worst, ms);
        total += ms;
        sink += root.size();   // 轻量校验和，防止整段被优化掉
    }
    m_log->appendPlainText(QStringLiteral(u"AuJson DOM 解析 ×%1：最快 %2 ms · 平均 %3 ms · 最慢 %4 ms · 合计 %5 ms · 校验和 %6")
                               .arg(runs)
                               .arg(best, 0, 'f', 4)
                               .arg(total / runs, 0, 'f', 4)
                               .arg(worst, 0, 'f', 4)
                               .arg(total, 0, 'f', 2)
                               .arg(static_cast<qlonglong>(sink)));

    // ---- 对比 QJsonDocument（同一份数据）----
    if (m_cmpCheck->isChecked()) {
        double qBest = 1e300, qWorst = 0.0, qTotal = 0.0;
        size_t qSink = 0;
        for (int i = 0; i < runs; ++i) {
            t.restart();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            const double ms = nsToMs(t.nsecsElapsed());
            qBest = std::min(qBest, ms);
            qWorst = std::max(qWorst, ms);
            qTotal += ms;
            qSink += doc.isArray() ? 1u : 0u;
        }
        m_log->appendPlainText(QStringLiteral(u"QJsonDocument ×%1：最快 %2 ms · 平均 %3 ms · 最慢 %4 ms · 校验和 %5")
                                   .arg(runs)
                                   .arg(qBest, 0, 'f', 4)
                                   .arg(qTotal / runs, 0, 'f', 4)
                                   .arg(qWorst, 0, 'f', 4)
                                   .arg(static_cast<qlonglong>(qSink)));
        const double speedup = (total / runs) > 0.0 ? (qTotal / runs) / (total / runs) : 0.0;
        m_log->appendPlainText(
            QStringLiteral(u"对比（平均）：AuJson 是 QJsonDocument 的 %1 倍")
                .arg(speedup, 0, 'f', 2));
    }

    m_log->appendPlainText(QStringLiteral(u"===== 测速结束 ====="));
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

// ---------------------------------------------------------------- 示例文本

QString MainWindow::sampleText(int index) const
{
    aujson::WriteOptions pretty;
    pretty.pretty = true;

    if (index == 1) {
        // 大数组：1000 个对象，紧凑输出（约几十 KB）
        aujson::Value arr = aujson::Value::makeArray();
        for (int i = 0; i < 1000; ++i) {
            aujson::Value o = aujson::Value::makeObject();
            o["id"] = i;
            o["name"] = "item" + std::to_string(i);
            o["price"] = i * 1.5;
            o["active"] = (i % 2) == 0;
            o["tags"].push_back("a");
            o["tags"].push_back("b");
            arr.push_back(std::move(o));
        }
        return QString::fromStdString(aujson::stringify(arr));
    }

    if (index == 2) {
        // 深嵌套：50 层对象链
        aujson::Value root = aujson::Value::makeObject();
        aujson::Value *cur = &root;
        for (int i = 0; i < 50; ++i) {
            (*cur)["child"] = aujson::Value::makeObject();
            cur = &((*cur)["child"]);
        }
        (*cur)["leaf"] = 42;
        return QString::fromStdString(aujson::stringify(root, pretty));
    }

    // 默认：配置对象（混合类型，含中文）
    aujson::Value o = aujson::Value::makeObject();
    o["window"]["title"] = "AuWidgets JsonDemo";
    o["window"]["size"].push_back(1280);
    o["window"]["size"].push_back(720);
    o["window"]["fullscreen"] = false;
    o["theme"]["name"] = "dark";
    o["theme"]["accent"] = "#4A90D9";
    o["theme"]["fonts"].push_back("Microsoft YaHei");
    o["theme"]["fonts"].push_back("Consolas");
    o["recentFiles"].push_back("C:/docs/a.pdf");
    o["recentFiles"].push_back("D:/docs/b.pdf");
    o["recentFiles"].push_back("E:/docs/中文.pdf");
    o["flags"]["aaa"] = true;
    o["flags"]["bbb"] = 42;
    o["flags"]["ccc"] = 3.14159;
    return QString::fromStdString(aujson::stringify(o, pretty));
}

// ---------------------------------------------------------------- 事件过滤

bool MainWindow::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == m_input && ev->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(ev);
        if (ke->key() == Qt::Key_Return && (ke->modifiers() & Qt::ControlModifier)) {
            onParse();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}
