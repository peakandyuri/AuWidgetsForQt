#include "GridPages.h"

#include "PageHelpers.h"

#include "AuCellEditValidator.h"
#include "AuCellButton.h"
#include "AuButtonCellRenderer.h"
#include "AuCellRenderer.h"
#include "AuColumnDef.h"
#include "AuComboBoxCellEditor.h"
#include "AuDatePicker.h"
#include "AuGridModel.h"
#include "AuGridStyle.h"
#include "AuGridSyncGroup.h"
#include "AuGridWidget.h"
#include "AuSelect.h"

#include <QCheckBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSharedPointer>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace {

/// 校验器演示用：数量列（叶子列 1）禁止负数，其余放行。
class CountValidator : public AuCellEditValidator
{
public:
    AuEditValidationResult validate(const AuGridModel * /*model*/, int /*row*/, int column,
                                    const QVariant & /*oldValue*/, const QVariant &newValue,
                                    QString &errorMessage) const override
    {
        if (column == 1 && newValue.toInt() < 0) {
            errorMessage = QStringLiteral(u"数量不能为负数");
            return AuEditValidationResult::RejectKeepEditing;
        }
        return AuEditValidationResult::Accept;
    }
};

/// 演示用：操作列按钮渲染器（画一个按钮，悬停变色，整格为手型热区）。
class ButtonRenderer : public AuCellRenderer
{
public:
    void paint(QPainter *painter, const QRect &rect, const QVariant &value,
               const AuColumnDef & /*columnDef*/, int /*row*/, int /*column*/,
               const AuGridModel * /*model*/, const AuGridStyle * /*style*/,
               const QColor & /*textColor*/) const override
    {
        paintButton(painter, rect, value, AuCellRendererContext());
    }

    void paint(QPainter *painter, const QRect &rect, const QVariant &value,
               const AuColumnDef & /*columnDef*/, int /*row*/, int /*column*/,
               const AuGridModel * /*model*/, const AuGridStyle * /*style*/,
               const QColor & /*textColor*/, const AuCellRendererContext &ctx) const override
    {
        paintButton(painter, rect, value, ctx);
    }

    bool wantsHoverCursor(int /*row*/, int /*column*/, const QPoint & /*cellPos*/) const override
    {
        return true;   // 演示从简：整格视为按钮热区
    }

private:
    static void paintButton(QPainter *painter, const QRect &rect, const QVariant &value,
                            const AuCellRendererContext &ctx)
    {
        const QString text = value.toString();
        const QFontMetrics fm(painter->font());
        const int w = qMin(rect.width() - 8, fm.horizontalAdvance(text) + 24);
        const int h = qMin(rect.height() - 8, 24);
        // rect 为控件绝对坐标：按钮居中需叠加 rect.topLeft()
        const QRect btn(rect.x() + (rect.width() - w) / 2,
                        rect.y() + (rect.height() - h) / 2, w, h);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QColor(0x40, 0x96, 0xff));
        painter->setBrush(ctx.hovered ? QColor(0x40, 0x96, 0xff) : QColor(0xe6, 0xf4, 0xff));
        painter->drawRoundedRect(btn, 4, 4);
        painter->setPen(ctx.hovered ? Qt::white : QColor(0x16, 0x77, 0xff));
        painter->drawText(btn, Qt::AlignCenter, text);
        painter->restore();
    }
};

/// 演示用：徽标渲染器（bool 值画成彩色圆角徽标）。
class BadgeRenderer : public AuCellRenderer
{
public:
    void paint(QPainter *painter, const QRect &rect, const QVariant &value,
               const AuColumnDef & /*columnDef*/, int /*row*/, int /*column*/,
               const AuGridModel * /*model*/, const AuGridStyle * /*style*/,
               const QColor & /*textColor*/) const override
    {
        const bool on = value.toBool();
        const QString text = on ? QStringLiteral(u"启用中") : QStringLiteral(u"已停用");
        const QColor bg = on ? QColor(0x43, 0xa0, 0x47) : QColor(0, 0, 0, 38);
        const QColor fg = on ? QColor(Qt::white) : QColor(0, 0, 0, 140);

        const QFontMetrics fm(painter->font());
        const int w = qMin(rect.width() - 8, fm.horizontalAdvance(text) + 18);
        const int h = qMin(rect.height() - 8, 20);
        // rect 为控件绝对坐标：徽标居中需叠加 rect.topLeft()
        const QRect badge(rect.x() + (rect.width() - w) / 2,
                          rect.y() + (rect.height() - h) / 2, w, h);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(bg);
        painter->drawRoundedRect(badge, h / 2.0, h / 2.0);
        painter->setPen(fg);
        painter->drawText(badge, Qt::AlignCenter, text);
        painter->restore();
    }
};

/// 演示用：QSpinBox 编辑器（范围 0~500，越界自动钳制）。
class SpinBoxEditor : public AuCellEditor
{
public:
    QWidget *createEditor(QWidget *parent) const override
    {
        QSpinBox *spin = new QSpinBox(parent);
        spin->setFrame(false);
        spin->setRange(0, 500);
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        return spin;
    }

    void setEditorValue(QWidget *editor, const QVariant &value) const override
    {
        if (QSpinBox *spin = qobject_cast<QSpinBox *>(editor))
            spin->setValue(value.toInt());
    }

    QVariant editorValue(QWidget *editor) const override
    {
        if (QSpinBox *spin = qobject_cast<QSpinBox *>(editor))
            return spin->value();
        return QVariant();
    }

    void setInitialTypedText(QWidget *editor, const QString &text) const override
    {
        if (QSpinBox *spin = qobject_cast<QSpinBox *>(editor))
            spin->setValue(text.toInt());
    }
};

/// 演示用：表头按钮渲染器（标题左侧 + 右侧“清零”按钮；按钮上点击不触发排序）。
class HeaderActionButton : public AuHeaderRenderer
{
public:
    explicit HeaderActionButton(const std::function<void(int column)> &onClick)
        : m_onClick(onClick)
    {}

    void paint(QPainter *painter, const QRect &rect, const AuColumnDef &columnDef,
               int /*column*/, int /*level*/, const AuGridModel * /*model*/,
               const AuGridStyle * /*style*/, const QColor &textColor,
               const AuHeaderRendererContext &context) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        // 标题文字（左侧，右侧给按钮留位）
        QRect textRect = rect.adjusted(8, 0, -(38 + 12), 0);
        painter->setPen(textColor);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          columnDef.headerName());

        // 排序指示（按钮左侧小三角，排序时可见）
        const QRect btn = buttonRect(rect);
        if (context.sorted) {
            const qreal cx = btn.left() - 9.0;
            const qreal cy = rect.center().y();
            QPolygonF tri;
            if (context.sortOrder == Qt::AscendingOrder)
                tri << QPointF(cx, cy + 3.5) << QPointF(cx - 4.5, cy - 3.5)
                    << QPointF(cx + 4.5, cy - 3.5);
            else
                tri << QPointF(cx, cy - 3.5) << QPointF(cx - 4.5, cy + 3.5)
                    << QPointF(cx + 4.5, cy + 3.5);
            painter->setPen(Qt::NoPen);
            painter->setBrush(textColor);
            painter->drawPolygon(tri);
        }

        // 按钮（右侧）
        const bool over = context.hovered
                          && buttonRect(QRect(0, 0, rect.width(), rect.height()))
                                 .contains(context.headerPos);
        painter->setPen(over ? QColor(0x40, 0x96, 0xff) : QColor(0xa0, 0xa8, 0xb8));
        painter->setBrush(over ? QColor(0xba, 0xe0, 0xff) : QColor(0, 0, 0, 14));
        painter->drawRoundedRect(btn, 4, 4);
        painter->setPen(over ? QColor(0x16, 0x77, 0xff) : QColor(90, 90, 90));
        painter->drawText(btn, Qt::AlignCenter, QStringLiteral(u"清零"));
        painter->restore();
    }

    bool wantsHoverCursor(int, int, const QRect &rect, const QPoint &headerPos) const override
    {
        return hitTest(0, 0, rect, headerPos);
    }

    bool hitTest(int, int, const QRect &rect, const QPoint &headerPos) const override
    {
        return buttonRect(rect).contains(headerPos);
    }

    void headerMouseClick(int column, int, const QRect &rect, const QPoint &headerPos) override
    {
        if (hitTest(column, 0, rect, headerPos) && m_onClick)
            m_onClick(column);
    }

private:
    static QRect buttonRect(const QRect &nodeRect)
    {
        return QRect(nodeRect.right() - 44, nodeRect.top() + (nodeRect.height() - 18) / 2,
                     38, 18);
    }

    std::function<void(int)> m_onClick;
};

// ---------------------------------------------------------------------------
// AuSelect / AuDatePicker 单元格编辑器（完全外部集成，AuGrid 零改动）
//
// 原理：AuGridWidget 的编辑器是插件（AuCellEditor 接口 + registerCellEditor），
// 任意 QWidget 都能包装进去。包装类在 createEditor(parent) 里创建 AuSelect /
// AuDatePicker（parent 即宿主 AuGridWidget），选择完成后通过公开 API
// closeEditor(true) 主动提交——全程不需要改动 AuGrid 源码。
//
// 交互闭环：编辑器打开即自动展开面板（Enter/双击编辑 → 面板直接就位 →
// Enter 或点击确认 → 提交并把焦点还给表格 → Tab / 方向键继续导航）；
// 编辑框高度按宿主行高定制，完全贴合单元格。

/// 演示用：AuSelect 单元格编辑器（选择即提交，提交值 = 当前文本）。
class AuSelectCellEditor : public QObject, public AuCellEditor
{
public:
    explicit AuSelectCellEditor(const QStringList &items)
        : m_items(items)
    {}

    QWidget *createEditor(QWidget *parent) const override
    {
        AuSelect *select = new AuSelect(parent);
        select->addItems(m_items);
        select->setFocusPolicy(Qt::StrongFocus);   // 让 editCell 的 setFocus 生效

        // 编辑框高度贴合单元格：按宿主默认行高定制（独立样式实例，随当前全局主题）
        if (AuGridWidget *grid = qobject_cast<AuGridWidget *>(parent)) {
            AuSelectStyle *style = new AuSelectStyle(*AuSelectStyle::globalStyle());
            style->setEditorHeight(grid->rowHeight());
            select->setStyle(style);
        }

        // 打开编辑器即展开面板（延迟到 editCell 流程结束后），焦点有明确归属
        QTimer::singleShot(0, select, [select]() {
            if (select->isEnabled())
                select->showPopup();
        });

        // 选择完成（点击 / 键盘回车激活）→ 提交并关闭编辑器。
        // 提交后由宿主把焦点还给表格（closeEditor 内部处理），Tab / 方向键可继续
        // 注：该路径走 closeEditor 公开 API，直接提交、不经过校验器
        QObject::connect(select, &AuSelect::activated, select, [select]() {
            if (AuGridWidget *grid = qobject_cast<AuGridWidget *>(select->parent()))
                grid->closeEditor(true);
        });
        return select;
    }

    void setEditorValue(QWidget *editor, const QVariant &value) const override
    {
        if (AuSelect *select = qobject_cast<AuSelect *>(editor))
            select->setCurrentIndex(select->findText(value.toString(), Qt::MatchExactly));
    }

    QVariant editorValue(QWidget *editor) const override
    {
        if (AuSelect *select = qobject_cast<AuSelect *>(editor))
            return select->currentText();
        return QVariant();
    }

private:
    QStringList m_items;
};

/// 演示用：AuDatePicker 单元格编辑器（选中即提交，提交值 = QDate；清除提交空值）。
class AuDatePickerCellEditor : public QObject, public AuCellEditor
{
public:
    QWidget *createEditor(QWidget *parent) const override
    {
        AuDatePicker *picker = new AuDatePicker(parent);
        picker->setFocusPolicy(Qt::StrongFocus);

        // 编辑框高度贴合单元格：按宿主默认行高定制（内部派生独立样式）
        if (AuGridWidget *grid = qobject_cast<AuGridWidget *>(parent))
            picker->setEditorHeight(grid->rowHeight());

        // 打开编辑器即展开日历面板
        QTimer::singleShot(0, picker, [picker]() {
            if (picker->isEnabled())
                picker->showPopup();
        });

        QObject::connect(picker, &AuDatePicker::activated, picker, [picker]() {
            if (AuGridWidget *grid = qobject_cast<AuGridWidget *>(picker->parent()))
                grid->closeEditor(true);
        });
        // 圆圈 × 清除 → 提交无效 QDate（单元格清空）
        QObject::connect(picker, &AuDatePicker::cleared, picker, [picker]() {
            if (AuGridWidget *grid = qobject_cast<AuGridWidget *>(picker->parent()))
                grid->closeEditor(true);
        });
        return picker;
    }

    void setEditorValue(QWidget *editor, const QVariant &value) const override
    {
        if (AuDatePicker *picker = qobject_cast<AuDatePicker *>(editor))
            picker->setDate(value.toDate());
    }

    QVariant editorValue(QWidget *editor) const override
    {
        if (AuDatePicker *picker = qobject_cast<AuDatePicker *>(editor))
            return QVariant(picker->date());
        return QVariant();
    }
};

/// 便捷组合：创建网格 + 平铺数据模型（模型以网格为父，随网格销毁）。
AuGridWidget *newFlatGrid(int rowCount, AuGridModel **modelOut = nullptr)
{
    AuGridWidget *grid = new AuGridWidget;
    AuGridModel *model = PageHelpers::makeFlatModel(rowCount, grid);
    grid->setModel(model);
    if (modelOut)
        *modelOut = model;
    return grid;
}

} // namespace

QVector<FeaturePage> makeGridPages()
{
    QVector<FeaturePage> pages;

    // ============ 1. 最小用法 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"最小用法");
        page.description = QStringLiteral(
            u"三步上手：定义列（AuColumnDef）→ 载入行数据（QVariantMap 列表，键 = 列 field）→ "
            u"绑定视图（setModel）。虚拟化渲染开箱即用。");
        page.createDemo = []() {
            return PageHelpers::composeDemo(nullptr, newFlatGrid(30));
        };
        page.code = R"snip(
// 1. 列定义（值类型，可拷贝）——与演示一致：名称 / 数量 / 类别 / 启用 四列
QVector<AuColumnDef> columns;
AuColumnDef name("name", "名称");
name.setWidth(140);
columns.append(name);
AuColumnDef count("count", "数量");
count.setWidth(100);
columns.append(count);
AuColumnDef category("category", "类别");
category.setWidth(110);
columns.append(category);
AuColumnDef enabled("enabled", "启用");
enabled.setWidth(80);
columns.append(enabled);

// 2. 行数据：QVariantMap 列表，键 = 列的 field；演示共 30 行
const QStringList categories = {"水果", "蔬菜", "谷物", "坚果"};
QVector<QVariantMap> rows;
for (int r = 0; r < 30; ++r) {
    QVariantMap row;
    row.insert("name", QString("R%1").arg(r));
    row.insert("count", r * 10);
    row.insert("category", categories.at(r % categories.size()));
    row.insert("enabled", r % 3 != 0);
    rows.append(row);
}

// 3. 模型 + 视图
AuGridModel *model = new AuGridModel(this);
model->setColumnDefs(columns);
model->setRowData(rows);

AuGridWidget *grid = new AuGridWidget(this);
grid->setModel(model);   // 控件不获得模型所有权
)snip";
        pages.append(page);
    }

    // ============ 2. 行号列 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"行号列");
        page.description = QStringLiteral(
            u"内置行号列显示“可见行号 + 1”，排序 / 过滤 / 拖拽重排后跟随可见顺序"
            u"重新编号；显隐带与列显隐一致的动画。");
        page.createDemo = []() {
            AuGridWidget *grid = newFlatGrid(30);
            grid->setRowNumberColumnVisible(true);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *showBtn = new QPushButton(QStringLiteral(u"显示行号列"));
            QPushButton *hideBtn = new QPushButton(QStringLiteral(u"隐藏行号列"));
            controls->addWidget(showBtn);
            controls->addWidget(hideBtn);
            controls->addStretch(1);
            QObject::connect(showBtn, &QPushButton::clicked, grid, [grid]() {
                grid->setRowNumberColumnVisible(true);
            });
            QObject::connect(hideBtn, &QPushButton::clicked, grid, [grid]() {
                grid->setRowNumberColumnVisible(false);
            });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：名称 / 数量 / 类别 / 启用 四列 × 30 行
AuGridWidget *grid = new AuGridWidget(this);
grid->setModel(model);

grid->setRowNumberColumnVisible(true);   // 显示内置行号列（演示按钮：显示 / 隐藏，带动画）
grid->isRowNumberColumnVisible();        // 查询状态

// 行号列显示“可见行号 + 1”（排序 / 过滤 / 拖拽重排后的当前顺序），
// 排序 / 过滤后行号随之重新编号。需要跨排序过滤引用某一行时，
// 使用 checkedOriginalRows / selectedOriginalRows 等原始行号 API。
)snip";
        pages.append(page);
    }

    // ============ 3. 复选框列 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"复选框列");
        page.description = QStringLiteral(
            u"内置复选框列：表头复选框支持全选 / 半选，行头点击切换勾选；"
            u"勾选集合按“原始行号”记录，跨排序过滤稳定。");
        page.createDemo = []() {
            AuGridModel *model = nullptr;
            AuGridWidget *grid = newFlatGrid(30, &model);
            grid->setCheckboxColumnVisible(true);

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(grid, &AuGridWidget::rowCheckedChanged, grid,
                             [log](int row, bool checked) {
                                 PageHelpers::appendLog(
                                     log, QString("rowCheckedChanged: row=%1 checked=%2")
                                              .arg(row)
                                              .arg(checked));
                             });
            QObject::connect(grid, &AuGridWidget::checkedStateChanged, grid, [grid, log]() {
                PageHelpers::appendLog(log,
                                       QString("checkedStateChanged: 可见=%1 原始=%2")
                                           .arg(grid->checkedRows().size())
                                           .arg(grid->checkedOriginalRows().size()));
            });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *selectAllBtn = new QPushButton(QStringLiteral(u"全选"));
            QPushButton *clearBtn = new QPushButton(QStringLiteral(u"清空勾选"));
            QPushButton *printBtn = new QPushButton(QStringLiteral(u"打印勾选（原始行号）"));
            controls->addWidget(selectAllBtn);
            controls->addWidget(clearBtn);
            controls->addWidget(printBtn);
            controls->addStretch(1);
            QObject::connect(selectAllBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->selectAllRows(); });
            QObject::connect(clearBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->clearAllCheckedRows(); });
            QObject::connect(printBtn, &QPushButton::clicked, grid, [grid, log]() {
                QStringList parts;
                const QList<int> rows = grid->checkedOriginalRows().values();
                for (int v : rows)
                    parts.append(QString::number(v));
                PageHelpers::appendLog(
                    log, QString("checkedOriginalRows=[%1]").arg(parts.join(',')));
            });
            return PageHelpers::composeDemo(controls, grid, log);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：名称 / 数量 / 类别 / 启用 四列 × 30 行
grid->setCheckboxColumnVisible(true);   // 内置复选框列，表头支持全选 / 半选

// 行勾选变化（row 为可见行号）
connect(grid, &AuGridWidget::rowCheckedChanged,
        this, [](int row, bool checked) { /* ... */ });

// 勾选集合整体变化（全选 / 清空 / 拖拽移动等）
connect(grid, &AuGridWidget::checkedStateChanged, this, []() { /* ... */ });

// 读取勾选集合
QSet<int> visible = grid->checkedRows();           // 可见行号
QSet<int> original = grid->checkedOriginalRows();  // 原始行号（持久化推荐）

grid->selectAllRows();        // 全选（等效点击表头复选框）
grid->clearAllCheckedRows();  // 清空勾选
)snip";
        pages.append(page);
    }

    // ============ 4. 行选中模式 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"行选中模式");
        page.description = QStringLiteral(
            u"单选（默认）：点击选中一行；多选：点击 / Ctrl+点击切换选中、可累积多行；"
            u"不可选中：鼠标键盘与 API 均不生效。");
        page.createDemo = []() {
            AuGridWidget *grid = newFlatGrid(30);

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(grid, &AuGridWidget::selectionChanged, grid, [grid, log]() {
                QStringList parts;
                const QList<int> rows = grid->selectedOriginalRows().values();
                for (int v : rows)
                    parts.append(QString::number(v));
                PageHelpers::appendLog(
                    log, QString("selectionChanged: selectedOriginalRows=[%1]")
                             .arg(parts.join(',')));
            });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *singleBtn = new QPushButton(QStringLiteral(u"单选"));
            QPushButton *multiBtn = new QPushButton(QStringLiteral(u"多选"));
            QPushButton *noneBtn = new QPushButton(QStringLiteral(u"不可选中"));
            QPushButton *apiBtn = new QPushButton(QStringLiteral(u"API 选中 2,4,6"));
            for (QPushButton *btn : {singleBtn, multiBtn, noneBtn})
                btn->setCheckable(true);
            singleBtn->setChecked(true);
            controls->addWidget(singleBtn);
            controls->addWidget(multiBtn);
            controls->addWidget(noneBtn);
            controls->addWidget(apiBtn);
            controls->addStretch(1);
            QObject::connect(singleBtn, &QPushButton::clicked, grid, [grid, singleBtn, multiBtn, noneBtn]() {
                grid->setSelectionMode(AuGridWidget::SelectionMode::Single);
                singleBtn->setChecked(true);
                multiBtn->setChecked(false);
                noneBtn->setChecked(false);
            });
            QObject::connect(multiBtn, &QPushButton::clicked, grid, [grid, singleBtn, multiBtn, noneBtn]() {
                grid->setSelectionMode(AuGridWidget::SelectionMode::Multiple);
                multiBtn->setChecked(true);
                singleBtn->setChecked(false);
                noneBtn->setChecked(false);
            });
            QObject::connect(noneBtn, &QPushButton::clicked, grid, [grid, singleBtn, multiBtn, noneBtn]() {
                grid->setSelectionMode(AuGridWidget::SelectionMode::None);
                noneBtn->setChecked(true);
                singleBtn->setChecked(false);
                multiBtn->setChecked(false);
            });
            QObject::connect(apiBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->selectRows(QSet<int>{2, 4, 6}); });
            return PageHelpers::composeDemo(controls, grid, log);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：名称 / 数量 / 类别 / 启用 四列 × 30 行

// 选中模式切换（对应演示按钮：单选 / 多选 / 不可选中）
grid->setSelectionMode(AuGridWidget::SelectionMode::Multiple);
// Single：点击选中一行，Ctrl+点击已选行可取消
// Multiple：点击 / Ctrl+点击均切换该行，可累积多行
// None：鼠标 / 键盘 / 选中 API 均不改变选中

// 选中变化（演示日志打印的是原始行号）
connect(grid, &AuGridWidget::selectionChanged, this, [grid]() {
    QSet<int> rows = grid->selectedRows();              // 可见行号
    QSet<int> original = grid->selectedOriginalRows();  // 原始行号（持久化推荐）
});

// 程序化选中（对应演示按钮“API 选中 2,4,6”）
grid->selectRows(QSet<int>{2, 4, 6});   // 多选模式：整体替换选中集合
)snip";
        pages.append(page);
    }

    // ============ 5. 单元格焦点控制 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"单元格焦点控制");
        page.description = QStringLiteral(
            u"列定义 setFocusable(false) 让该列单元格不成为键盘 / 选中锚点：方向键与 Tab "
            u"导航直接跳过，点击不把焦点框落到该格（行选中、cellClicked、按钮交互照常）。"
            u"适合放操作按钮的自定义列；逐格动态裁决用 setCellFocusableCallback。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"名称"));
            name.setWidth(120);
            columns.append(name);
            columns.append(AuColumnDef("count", QStringLiteral(u"数量")));
            columns.append(AuColumnDef("category", QStringLiteral(u"类别")));
            // 操作列：按钮渲染器 + 不可聚焦（点击按钮时焦点框保持原位）
            AuColumnDef op("op", QStringLiteral(u"操作"));
            op.setWidth(110);
            op.setCellRenderer(QStringLiteral("demoButton"));
            op.setFocusable(false);
            columns.append(op);
            model->setColumnDefs(columns);
            model->setRowData(PageHelpers::makeFlatRows(20));

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);
            grid->registerCellRenderer(
                QStringLiteral("demoButton"),
                QSharedPointer<AuCellRenderer>(new ButtonRenderer));

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(grid, &AuGridWidget::cellClicked, grid,
                             [grid, log](int row, int column) {
                                 const QString field = grid->model()->fieldAtColumn(column);
                                 PageHelpers::appendLog(
                                     log, QString("cellClicked: r=%1 c=%2(%3) 可聚焦=%4")
                                              .arg(row)
                                              .arg(column)
                                              .arg(field)
                                              .arg(grid->isCellFocusable(row, column)
                                                       ? QStringLiteral(u"是")
                                                       : QStringLiteral(u"否")));
                             });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *toggleBtn = new QPushButton(QStringLiteral(u"切换操作列可聚焦"));
            controls->addWidget(toggleBtn);
            controls->addStretch(1);
            QObject::connect(toggleBtn, &QPushButton::clicked, grid, [grid, toggleBtn]() {
                const int op = grid->model()->columnIndexForField(QStringLiteral("op"));
                const bool focusable = !grid->model()->columnDefAt(op).isFocusable();
                AuColumnDef def = grid->model()->columnDefAt(op);
                def.setFocusable(focusable);
                // 列定义是值类型：改完整树回写使变更生效
                QVector<AuColumnDef> columns = grid->model()->topLevelColumnDefs();
                columns[op] = def;
                grid->model()->setColumnDefs(columns);
                toggleBtn->setText(focusable ? QStringLiteral(u"切换操作列不可聚焦")
                                             : QStringLiteral(u"切换操作列可聚焦"));
            });
            return PageHelpers::composeDemo(controls, grid, log);
        };
        page.code = R"snip(
// 自定义操作列：按钮渲染器（paint 里画按钮，wantsHoverCursor 返回 true）
class ButtonRenderer : public AuCellRenderer { /* 见本页完整示例 */ };

grid->registerCellRenderer("button",
    QSharedPointer<AuCellRenderer>(new ButtonRenderer));

AuColumnDef op("op", "操作");
op.setCellRenderer("button");
op.setFocusable(false);   // 关键：该列单元格不成为键盘 / 选中锚点
columns.append(op);

// 效果：
//  - 方向键左右导航直接跳过操作列；Tab 编辑导航同样跳过；
//  - 点击操作列：行选中与 cellClicked 照常触发（按钮逻辑挂在这里），
//    但焦点框保持原位不移动；
//  - 切换回 setFocusable(true) 后恢复默认行为。

// 也可逐格动态裁决（在列 focusable=true 的基础上进一步收紧）：
grid->setCellFocusableCallback(
    [](const AuGridModel *model, int row, int column) -> bool {
        return model->cellValue(row, column).toString() != "禁用";   // 示例
    });

grid->isCellFocusable(0, 3);   // 查询某格是否可聚焦
)snip";
        pages.append(page);
    }

    // ============ 6. 行拖拽重排 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"行拖拽重排");
        page.description = QStringLiteral(
            u"开启后显示拖拽手柄列，按住手柄上下拖动整行重排；拖动完成发出 rowsDragged"
            u"（原始行号 + 目标位置），方便同步业务数据。");
        page.createDemo = []() {
            AuGridWidget *grid = newFlatGrid(30);
            grid->setRowDragEnabled(true);
            grid->setDragPreviewTextCallback([](const AuGridModel *, const QVector<int> &rows) {
                return QStringLiteral(u"正在移动 %1 行").arg(rows.size());
            });

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(grid, &AuGridWidget::rowsDragged, grid,
                             [grid, log](const QVector<int> &originalRows, int targetOriginalIndex) {
                                 QStringList parts;
                                 for (int v : originalRows)
                                     parts.append(QString::number(v));
                                 PageHelpers::appendLog(
                                     log, QString("rowsDragged: rows=[%1] target=%2")
                                              .arg(parts.join(','))
                                              .arg(targetOriginalIndex));
                             });
            return PageHelpers::composeDemo(nullptr, grid, log);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：名称 / 数量 / 类别 / 启用 四列 × 30 行

grid->setRowDragEnabled(true);   // 显示拖拽手柄列，按住手柄拖动整行重排

// 拖动完成：originalRows 为被拖动的原始行号，targetOriginalIndex 为插入位置
//（演示日志打印 rowsDragged 两个参数）
connect(grid, &AuGridWidget::rowsDragged,
        this, [](const QVector<int> &originalRows, int targetOriginalIndex) {
    // 模型内部顺序已移动；在此同步业务数据的顺序
});

// 拖动时的浮动提示文字（演示返回“正在移动 N 行”，默认“共 N 行”）
grid->setDragPreviewTextCallback(
    [](const AuGridModel *, const QVector<int> &rows) {
        return QString("正在移动 %1 行").arg(rows.size());
    });
)snip";
        pages.append(page);
    }

    // ============ 6. 表头点击排序 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"表头点击排序");
        page.description = QStringLiteral(
            u"列定义 sortable=true 的列显示排序箭头，配合总开关 setSortingEnabled(true)；"
            u"点击表头在升序 / 降序间切换。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"名称"));
            name.setSortable(true);
            columns.append(name);
            AuColumnDef count("count", QStringLiteral(u"数量"));
            count.setSortable(true);
            columns.append(count);
            AuColumnDef category("category", QStringLiteral(u"类别（不可排序）"));
            columns.append(category);
            model->setColumnDefs(columns);
            model->setRowData(PageHelpers::makeFlatRows(30));

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);
            grid->setSortingEnabled(true);

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(grid, &AuGridWidget::sortChanged, grid,
                             [log](int column, Qt::SortOrder order) {
                                 PageHelpers::appendLog(
                                     log, QString("sortChanged: column=%1 order=%2")
                                              .arg(column)
                                              .arg(order == Qt::AscendingOrder ? "Asc" : "Desc"));
                             });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *enableBtn = new QPushButton(QStringLiteral(u"开启排序总开关"));
            QPushButton *disableBtn = new QPushButton(QStringLiteral(u"关闭"));
            QPushButton *clearBtn = new QPushButton(QStringLiteral(u"清除排序"));
            controls->addWidget(enableBtn);
            controls->addWidget(disableBtn);
            controls->addWidget(clearBtn);
            controls->addStretch(1);
            QObject::connect(enableBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setSortingEnabled(true); });
            QObject::connect(disableBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setSortingEnabled(false); });
            QObject::connect(clearBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->clearSort(); });
            return PageHelpers::composeDemo(controls, grid, log);
        };
        page.code = R"snip(
// 示例数据：名称 / 数量 / 类别（不可排序）三列 × 30 行
AuColumnDef name("name", "名称");
name.setSortable(true);                 // 显示排序箭头
AuColumnDef count("count", "数量");
count.setSortable(true);
AuColumnDef category("category", "类别（不可排序）");   // 未设 sortable，无箭头

// 总开关（默认关闭；仅对 sortable 列生效，对应演示按钮）
grid->setSortingEnabled(true);
grid->setSortingEnabled(false);   // 关闭

// 排序变化（点击表头触发，演示日志打印 column 与方向）
connect(grid, &AuGridWidget::sortChanged,
        this, [](int column, Qt::SortOrder order) { /* ... */ });

grid->clearSort();   // 清除排序，恢复原始顺序
)snip";
        pages.append(page);
    }

    // ============ 7. 多规则排序 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"多规则排序");
        page.description = QStringLiteral(
            u"setSortRules 设置多条规则，按列表顺序依次比较：第一列相等时才比较第二列；"
            u"setSortComparator 设置整行自定义比较函数（优先于规则），可自由组合多列逻辑。");
        page.createDemo = []() {
            AuGridModel *model = nullptr;
            AuGridWidget *grid = newFlatGrid(40, &model);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *applyBtn = new QPushButton(QStringLiteral(u"类别升序 + 数量降序"));
            QPushButton *funcBtn = new QPushButton(QStringLiteral(u"自定义比较函数"));
            QPushButton *clearBtn = new QPushButton(QStringLiteral(u"清除排序"));
            controls->addWidget(applyBtn);
            controls->addWidget(funcBtn);
            controls->addWidget(clearBtn);
            controls->addStretch(1);
            QObject::connect(applyBtn, &QPushButton::clicked, grid, [grid]() {
                grid->setSortRules({AuSortRule(2, Qt::AscendingOrder),
                                    AuSortRule(1, Qt::DescendingOrder)});
            });
            // 自定义比较函数：规则表达不了的多列组合逻辑。
            // 示例：偶数量的行排前面，同为偶 / 奇再按数量升序——
            // 无法用“某列 + 升降序”规则描述，只能在函数里自由判断
            QObject::connect(funcBtn, &QPushButton::clicked, grid, [grid]() {
                grid->setSortComparator([](const QVariantMap &a, const QVariantMap &b) -> int {
                    const int ca = a.value("count").toInt();
                    const int cb = b.value("count").toInt();
                    const int pa = (ca % 2 == 0) ? 0 : 1;   // 偶数在前
                    const int pb = (cb % 2 == 0) ? 0 : 1;
                    if (pa != pb)
                        return pa - pb;
                    return ca - cb;                          // 组内按数量升序
                });
            });
            QObject::connect(clearBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->clearSort(); });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致（四列 × 40 行）

// 多规则排序：按顺序依次比较（对应演示按钮“类别升序 + 数量降序”）
// 先按“类别”升序（叶子列 2），类别相同再按“数量”降序（叶子列 1）
grid->setSortRules({
    AuSortRule(2, Qt::AscendingOrder),
    AuSortRule(1, Qt::DescendingOrder)
});

// 整行自定义比较函数（对应演示按钮“自定义比较函数”）：
// 返回 <0 表示 a 排前，>0 表示 b 排前，0 保持原顺序（稳定排序）。
// 适合规则表达不了的组合逻辑（先看 A 列取值再比 B 列等）；
// 生效期间优先于 setSortRules 规则
grid->setSortComparator([](const QVariantMap &a, const QVariantMap &b) -> int {
    return a.value("count").toInt() - b.value("count").toInt();
});

grid->sortRules();          // 读取当前规则
grid->hasSortComparator();  // 是否设置了自定义比较函数
grid->clearSort();          // 清除全部规则与比较函数，恢复原始顺序
)snip";
        pages.append(page);
    }

    // ============ 8. 数据过滤 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"数据过滤");
        page.description = QStringLiteral(
            u"setFilterCallback 对每行（QVariantMap）裁决保留与否；清除过滤恢复全部行。"
            u"过滤 / 取消过滤带行位移动画。");
        page.createDemo = []() {
            AuGridModel *model = nullptr;
            AuGridWidget *grid = newFlatGrid(50, &model);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *filterBtn = new QPushButton(QStringLiteral(u"只留数量 ≥ 250"));
            QPushButton *clearBtn = new QPushButton(QStringLiteral(u"清除过滤"));
            controls->addWidget(filterBtn);
            controls->addWidget(clearBtn);
            controls->addStretch(1);
            QObject::connect(filterBtn, &QPushButton::clicked, grid, [grid]() {
                grid->setFilterCallback([](const QVariantMap &row) {
                    return row.value("count").toInt() >= 250;
                });
            });
            QObject::connect(clearBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->clearFilter(); });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：四列 × 50 行（数量列取值 r*10）

// 过滤回调：对每行返回 true 表示保留（对应演示按钮“只留数量 ≥ 250”）
grid->setFilterCallback([](const QVariantMap &row) {
    return row.value("count").toInt() >= 250;
});

grid->isFiltered();    // 是否设置了过滤且生效中
grid->clearFilter();   // 清除过滤，恢复全部行

// 过滤后的行号体系：界面 API 使用“可见行号”，
// 勾选 / 选中可改用原始行号 API（checkedOriginalRows 等）跨过滤稳定。
)snip";
        pages.append(page);
    }

    // ============ 9. 单元格编辑 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"单元格编辑");
        page.description = QStringLiteral(
            u"列定义 editable=true 后，双击 / 回车 / 直接输入字符进入编辑；提交后模型已更新并发出信号。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"名称（只读）"));
            columns.append(name);
            AuColumnDef count("count", QStringLiteral(u"数量（可编辑）"));
            count.setEditable(true);
            columns.append(count);
            AuColumnDef enabled("enabled", QStringLiteral(u"启用（复选框）"));
            enabled.setCheckbox(true);
            enabled.setEditable(true);
            columns.append(enabled);
            model->setColumnDefs(columns);
            model->setRowData(PageHelpers::makeFlatRows(20));

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(grid, &AuGridWidget::cellEditingStarted, grid,
                             [log](int row, int column) {
                                 PageHelpers::appendLog(
                                     log, QString("cellEditingStarted: r=%1 c=%2").arg(row).arg(column));
                             });
            QObject::connect(grid, &AuGridWidget::cellValueChanged, grid,
                             [log](int row, int column, const QVariant &value) {
                                 PageHelpers::appendLog(
                                     log, QString("cellValueChanged: r=%1 c=%2 value=%3")
                                              .arg(row)
                                              .arg(column)
                                              .arg(value.toString()));
                             });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *editBtn = new QPushButton(QStringLiteral(u"编辑 (2,1)"));
            QPushButton *commitBtn = new QPushButton(QStringLiteral(u"提交关闭"));
            QPushButton *cancelBtn = new QPushButton(QStringLiteral(u"放弃关闭"));
            controls->addWidget(editBtn);
            controls->addWidget(commitBtn);
            controls->addWidget(cancelBtn);
            controls->addStretch(1);
            // 提交 / 放弃按钮必须设为 NoFocus：普通按钮被点击时先夺走编辑器焦点，
            // 失焦路径会按“提交”结束编辑，随后按钮里的 closeEditor(false) 已无编辑器可关，
            // 表现为“放弃也提交了”。NoFocus 让点击不转移焦点，编辑器保持打开等按钮裁决。
            for (QPushButton *btn : {commitBtn, cancelBtn})
                btn->setFocusPolicy(Qt::NoFocus);
            QObject::connect(editBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->editCell(2, 1); });
            QObject::connect(commitBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->closeEditor(true); });
            QObject::connect(cancelBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->closeEditor(false); });
            return PageHelpers::composeDemo(controls, grid, log);
        };
        page.code = R"snip(
// 示例数据：名称（只读）/ 数量（可编辑）/ 启用（复选框）三列 × 20 行
AuColumnDef name("name", "名称（只读）");      // 未设 editable，双击无效
AuColumnDef count("count", "数量（可编辑）");
count.setEditable(true);                       // 双击 / 回车 / 输入字符进入编辑
AuColumnDef enabled("enabled", "启用（复选框）");
enabled.setCheckbox(true);
enabled.setEditable(true);                     // 复选框列：允许点击 / 空格切换

// 提交成功（通过校验器）后模型已更新（演示日志打印新值）
connect(grid, &AuGridWidget::cellValueChanged,
        this, [](int row, int column, const QVariant &value) { /* ... */ });

// 程序化开始编辑（对应演示按钮“编辑 (2,1)”）
grid->editCell(2, 1);
grid->editCell(2, 1, "100");   // 可注入初始文本

// 对应演示按钮：提交关闭 / 放弃关闭
grid->isEditing();        // 是否有编辑器打开
grid->closeEditor(true);  // 提交并关闭；false = 放弃
// 注意：外置“放弃关闭”按钮要设 setFocusPolicy(Qt::NoFocus)，
// 否则点击按钮先夺走编辑器焦点，失焦路径按“提交”结束编辑
)snip";
        pages.append(page);
    }

    // ============ 10. 编辑校验器 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"编辑校验器");
        page.description = QStringLiteral(
            u"setCellEditValidator 对全部编辑提交生效：Accept 接受 / Reject 拒绝关闭 / "
            u"RejectKeepEditing 拒绝并保持编辑器打开（红字提示 errorMessage）。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"名称"));
            columns.append(name);
            AuColumnDef count("count", QStringLiteral(u"数量（不允许负数）"));
            count.setEditable(true);
            columns.append(count);
            model->setColumnDefs(columns);
            model->setRowData(PageHelpers::makeFlatRows(20));

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);
            grid->setCellEditValidator(QSharedPointer<AuCellEditValidator>(new CountValidator));

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *editBtn = new QPushButton(QStringLiteral(u"编辑 (2,1) 试试输入负数"));
            controls->addWidget(editBtn);
            controls->addStretch(1);
            QObject::connect(editBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->editCell(2, 1); });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 自定义校验器：对全部编辑提交生效
// 示例数据：名称 / 数量（不允许负数，可编辑）两列 × 20 行
class CountValidator : public AuCellEditValidator
{
public:
    AuEditValidationResult validate(const AuGridModel *model, int row, int column,
                                    const QVariant &oldValue, const QVariant &newValue,
                                    QString &errorMessage) const override
    {
        if (column == 1 && newValue.toInt() < 0) {   // “数量”列不允许负数
            errorMessage = "数量不能为负数";
            return AuEditValidationResult::RejectKeepEditing;  // 保持编辑器打开并红字提示
        }
        return AuEditValidationResult::Accept;
    }
};

grid->setCellEditValidator(
    QSharedPointer<AuCellEditValidator>(new CountValidator));

grid->editCell(2, 1);   // 打开编辑器，试试输入负数（对应演示按钮）
)snip";
        pages.append(page);
    }

    // ============ 10a. 内置按钮渲染器 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"内置按钮渲染器");
        page.description = QStringLiteral(
            u"AuButtonCellRenderer 内置 \"button\" / \"link\" 注册名：一格多个按钮，"
            u"支持默认 / 悬停 / 按下三态样式（未设置自动派生）与语义预设。"
            u"按钮来源三选一：按行回调 / 固定列表 / 单元格值解析；"
            u"热区按下 + 释放确认点击，统一经 cellButtonClicked 信号发出。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"名称"));
            name.setWidth(90);
            columns.append(name);
            AuColumnDef status("status", QStringLiteral(u"状态"));
            status.setWidth(90);
            columns.append(status);
            AuColumnDef op("op", QStringLiteral(u"操作(按行回调)"));
            op.setWidth(190);
            op.setCellRenderer(QStringLiteral("demoOp"));
            op.setFocusable(false);   // 操作列建议关闭焦点锚点
            columns.append(op);
            AuColumnDef cfg("cfg", QStringLiteral(u"配置(数据驱动)"));
            cfg.setWidth(210);
            cfg.setCellRenderer(QStringLiteral("button"));
            cfg.setFocusable(false);
            columns.append(cfg);
            model->setColumnDefs(columns);

            // 行数据：status 驱动“操作”列按钮；cfg 为 QVariantList 按钮定义（数据驱动）
            QVector<QVariantMap> rows;
            for (int r = 0; r < 10; ++r) {
                QVariantMap row;
                row.insert(QStringLiteral("name"), QString("R%1").arg(r));
                row.insert(QStringLiteral("status"), r % 2 == 0
                            ? QStringLiteral(u"待审核") : QStringLiteral(u"已通过"));
                QVariantList buttons;
                QVariantMap detail;
                detail.insert(QStringLiteral("text"), QStringLiteral(u"详情"));
                detail.insert(QStringLiteral("type"), QStringLiteral("link"));
                buttons.append(detail);
                QVariantMap sync;
                sync.insert(QStringLiteral("text"), QStringLiteral(u"同步"));
                buttons.append(sync);
                if (r == 3) {   // 演示禁用态
                    QVariantMap off;
                    off.insert(QStringLiteral("text"), QStringLiteral(u"禁用"));
                    off.insert(QStringLiteral("enabled"), false);
                    buttons.append(off);
                }
                row.insert(QStringLiteral("cfg"), buttons);
                rows.append(row);
            }
            model->setRowData(rows);

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);

            // “操作”列：按行回调，状态不同出不同按钮（改状态即换按钮组）
            AuButtonCellRenderer *opRenderer = new AuButtonCellRenderer;
            opRenderer->setButtonProvider(
                [](int row, int column, const AuGridModel *m) -> QVector<AuCellButton> {
                    Q_UNUSED(column)
                    const QString status =
                        m->rowData().value(row).value(QStringLiteral("status")).toString();
                    if (status == QStringLiteral(u"待审核"))
                        return QVector<AuCellButton>{ AuCellButton::primary(QStringLiteral(u"通过")),
                                                      AuCellButton::warning(QStringLiteral(u"驳回")),
                                                      AuCellButton::danger(QStringLiteral(u"删除")) };
                    return QVector<AuCellButton>{ AuCellButton::plain(QStringLiteral(u"查看")),
                                                  AuCellButton::danger(QStringLiteral(u"删除")) };
                });
            grid->registerCellRenderer(QStringLiteral("demoOp"),
                                       QSharedPointer<AuCellRenderer>(opRenderer));

            QPlainTextEdit *log = PageHelpers::makeLog();
            // 所有按钮点击统一走表格信号：id 识别按钮，userData 随按钮带回
            QObject::connect(grid, &AuGridWidget::cellButtonClicked, grid,
                             [grid, log](int row, int column, const QVariantMap &button) {
                                 Q_UNUSED(column)
                                 const QString id = button.value(QStringLiteral("id")).toString();
                                 PageHelpers::appendLog(
                                     log, QStringLiteral(u"cellButtonClicked: [%1] 行%2")
                                              .arg(id)
                                              .arg(row));
                                 if (id == QStringLiteral(u"删除")) {
                                     grid->model()->removeRows(
                                         {grid->model()->originalRowForVisibleRow(row)});
                                 } else if (id == QStringLiteral(u"通过")) {
                                     grid->model()->setCellValue(row, QStringLiteral("status"),
                                                                 QStringLiteral(u"已通过"));
                                 } else if (id == QStringLiteral(u"驳回")) {
                                     grid->model()->setCellValue(row, QStringLiteral("status"),
                                                                 QStringLiteral(u"待审核"));
                                 }
                             });
            return PageHelpers::composeDemo(nullptr, grid, log);
        };
        page.code = R"snip(
// 一格多按钮渲染器（AuGridWidget 已内置注册名 "button" / "link"，
// 零配置直接 setCellRenderer 使用；需要配置按钮来源时创建实例注册）
AuButtonCellRenderer *opRenderer = new AuButtonCellRenderer;

// 按钮来源一：按行回调 —— 状态不同出不同按钮（改状态即换按钮组）
opRenderer->setButtonProvider([](int row, int column, const AuGridModel *model) {
    const QString status = model->rowData().value(row).value("status").toString();
    if (status == "待审核")
        return QVector<AuCellButton>{ AuCellButton::primary("通过"),
                                      AuCellButton::warning("驳回"),
                                      AuCellButton::danger("删除") };
    return QVector<AuCellButton>{ AuCellButton::plain("查看") };
});
grid->registerCellRenderer("op",
    QSharedPointer<AuCellRenderer>(opRenderer));

// 按钮来源二：固定按钮集 —— opRenderer->setButtons({AuCellButton::primary("编辑")});
// 按钮来源三：数据驱动 —— 单元格值直接给按钮定义（QVariantList / map / 字符串）
row.insert("cfg", QVariantList{
    QVariantMap{{"text", "详情"}, {"type", "link"}},
    QVariantMap{{"text", "禁用"}, {"enabled", false}} });

AuColumnDef op("op", "操作");
op.setCellRenderer("op");
op.setFocusable(false);          // 操作列建议关闭焦点锚点
columns.append(op);

// 样式：hover / pressed 未设置时由 normal 自动派生（提亮 / 加深）；
// 也可逐状态覆盖：button.hover.textColor = QColor(...); button.radius = 12;

// 点击统一走表格信号（热区按下 + 释放确认，移出即取消），无需逐按钮绑定回调
connect(grid, &AuGridWidget::cellButtonClicked,
        grid, [grid](int row, int column, const QVariantMap &button) {
    const QString id = button.value("id").toString();   // id 缺省取 text
    if (id == "删除")
        grid->model()->removeRows({ grid->model()->originalRowForVisibleRow(row) });
});
)snip";
        pages.append(page);
    }

    // ============ 10b. 自定义单元格渲染器 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"自定义单元格渲染器");
        page.description = QStringLiteral(
            u"registerCellRenderer + 列定义 setCellRenderer 替换单元格绘制："
            u"“启用”列画成彩色徽标，“操作”列画成按钮（悬停变色、手型光标，点击删除该行）。"
            u"渲染器只管绘制，交互通过 cellClicked / 鼠标回调接入。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"名称"));
            name.setWidth(130);
            columns.append(name);
            columns.append(AuColumnDef("count", QStringLiteral(u"数量")));
            AuColumnDef enabled("enabled", QStringLiteral(u"启用"));
            enabled.setWidth(110);
            enabled.setCellRenderer(QStringLiteral("demoBadge"));
            columns.append(enabled);
            AuColumnDef op("op", QStringLiteral(u"操作"));
            op.setWidth(110);
            op.setCellRenderer(QStringLiteral("demoButton"));
            op.setFocusable(false);   // 按钮列不参与键盘焦点（见“单元格焦点控制”页）
            columns.append(op);
            model->setColumnDefs(columns);
            model->setRowData(PageHelpers::makeFlatRows(15));

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);
            grid->registerCellRenderer(QStringLiteral("demoBadge"),
                                       QSharedPointer<AuCellRenderer>(new BadgeRenderer));
            grid->registerCellRenderer(QStringLiteral("demoButton"),
                                       QSharedPointer<AuCellRenderer>(new ButtonRenderer));

            QPlainTextEdit *log = PageHelpers::makeLog();
            // 操作列按钮：cellClicked 里按列 field 分发，删除该行（带消失动画）
            QObject::connect(grid, &AuGridWidget::cellClicked, grid,
                             [grid, log](int row, int column) {
                                 if (grid->model()->fieldAtColumn(column)
                                     != QStringLiteral("op"))
                                     return;
                                 const int original =
                                     grid->model()->originalRowForVisibleRow(row);
                                 PageHelpers::appendLog(
                                     log, QString("点击删除按钮: r=%1 (原始行 %2)")
                                              .arg(row)
                                              .arg(original));
                                 grid->model()->removeRows({original});
                                 PageHelpers::appendLog(
                                     log, QString("已删除，剩余 %1 行")
                                              .arg(grid->model()->rowCount()));
                             });
            return PageHelpers::composeDemo(nullptr, grid, log);
        };
        page.code = R"snip(
// 徽标渲染器：按值画彩色圆角徽标（paint 里画圆角矩形 + 文字，完整实现见演示源码）
class BadgeRenderer : public AuCellRenderer { /* ... */ };

// 按钮渲染器：画按钮 + wantsHoverCursor 返回 true 显示手型光标
//（完整实现见“单元格焦点控制”页）
class ButtonRenderer : public AuCellRenderer { /* ... */ };

grid->registerCellRenderer("badge",
    QSharedPointer<AuCellRenderer>(new BadgeRenderer));
grid->registerCellRenderer("button",
    QSharedPointer<AuCellRenderer>(new ButtonRenderer));

AuColumnDef enabled("enabled", "启用");
enabled.setCellRenderer("badge");    // “启用”列画成徽标
columns.append(enabled);

AuColumnDef op("op", "操作");
op.setCellRenderer("button");        // “操作”列画成按钮
op.setFocusable(false);              // 按钮列不参与键盘焦点（可选）
columns.append(op);

// 渲染器只负责绘制；交互在控件的 cellClicked 里按列 field 分发
//（本页演示：点击按钮删除该行，带行消失动画）
connect(grid, &AuGridWidget::cellClicked, this, [grid](int row, int column) {
    if (grid->model()->fieldAtColumn(column) == "op")
        grid->model()->removeRows({ grid->model()->originalRowForVisibleRow(row) });
});
)snip";
        pages.append(page);
    }

    // ============ 10b2. 自定义单元格编辑器 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"自定义单元格编辑器");
        page.description = QStringLiteral(
            u"AuCellEditor 把任意 QWidget 包装成编辑器，注册后按列引用。"
            u"演示自定义 QSpinBox 编辑器（数量列 0~500，越界自动钳制）与内置 "
            u"AuComboBoxCellEditor 下拉编辑器（类别列）。双击 / 回车 / 直接输入进入编辑。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"名称"));
            name.setWidth(120);
            columns.append(name);
            AuColumnDef count("count", QStringLiteral(u"数量(0~500)"));
            count.setEditable(true);
            count.setCellEditor(QStringLiteral("demoSpin"));
            columns.append(count);
            AuColumnDef category("category", QStringLiteral(u"类别(下拉)"));
            category.setWidth(140);
            category.setEditable(true);
            category.setCellEditor(QStringLiteral("demoCombo"));
            columns.append(category);
            model->setColumnDefs(columns);
            model->setRowData(PageHelpers::makeFlatRows(15));

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);
            grid->registerCellEditor(QStringLiteral("demoSpin"),
                                     QSharedPointer<AuCellEditor>(new SpinBoxEditor));
            grid->registerCellEditor(QStringLiteral("demoCombo"),
                                     QSharedPointer<AuCellEditor>(new AuComboBoxCellEditor(
                                         {QStringLiteral(u"水果"), QStringLiteral(u"蔬菜"),
                                          QStringLiteral(u"谷物"), QStringLiteral(u"坚果")})));

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(grid, &AuGridWidget::cellValueChanged, grid,
                             [log](int row, int column, const QVariant &value) {
                                 PageHelpers::appendLog(
                                     log, QString("cellValueChanged: r=%1 c=%2 value=%3")
                                              .arg(row)
                                              .arg(column)
                                              .arg(value.toString()));
                             });
            return PageHelpers::composeDemo(nullptr, grid, log);
        };
        page.code = R"snip(
// 自定义编辑器：把 QSpinBox 包装成 AuCellEditor（三个纯虚接口）
class SpinBoxEditor : public AuCellEditor
{
public:
    QWidget *createEditor(QWidget *parent) const override {
        QSpinBox *spin = new QSpinBox(parent);
        spin->setFrame(false);
        spin->setRange(0, 500);
        return spin;
    }
    void setEditorValue(QWidget *editor, const QVariant &value) const override {
        qobject_cast<QSpinBox *>(editor)->setValue(value.toInt());
    }
    QVariant editorValue(QWidget *editor) const override {
        return qobject_cast<QSpinBox *>(editor)->value();
    }
    // 直接输入字符触发编辑时注入初值（可选）
    void setInitialTypedText(QWidget *editor, const QString &text) const override {
        qobject_cast<QSpinBox *>(editor)->setValue(text.toInt());
    }
};

// 注册 + 按列引用（双击 / 回车 / 直接输入进入编辑）
grid->registerCellEditor("spin",
    QSharedPointer<AuCellEditor>(new SpinBoxEditor));
count.setCellEditor("spin");
count.setEditable(true);

// 内置下拉编辑器（可编辑 QComboBox）
grid->registerCellEditor("combo",
    QSharedPointer<AuCellEditor>(new AuComboBoxCellEditor({"水果", "蔬菜", "谷物", "坚果"})));
category.setCellEditor("combo");
category.setEditable(true);

// 提交后模型已更新并发出信号
connect(grid, &AuGridWidget::cellValueChanged,
        this, [](int row, int column, const QVariant &value) { /* ... */ });
)snip";
        pages.append(page);
    }

    // ============ 10b3. 集成 AuSelect / AuDatePicker 编辑器 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"集成 AuSelect / AuDatePicker");
        page.description = QStringLiteral(
            u"把 AuSelect / AuDatePicker 集成进单元格编辑——完全通过外部代码完成，"
            u"AuGrid 零改动：AuCellEditor 插件接口包装控件 + registerCellEditor 注册，"
            u"选择 / 选日期后通过公开 API closeEditor(true) 主动提交。"
            u"交互闭环：编辑器打开即自动展开面板，Enter / 点击确认后提交并把焦点还给"
            u"表格，Tab / 方向键可继续导航；编辑框高度按行高定制、完全贴合单元格。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"名称"));
            name.setWidth(110);
            columns.append(name);
            AuColumnDef category("category", QStringLiteral(u"类别(AuSelect)"));
            category.setWidth(150);
            category.setEditable(true);
            category.setCellEditor(QStringLiteral("auSelect"));
            columns.append(category);
            AuColumnDef date("date", QStringLiteral(u"日期(AuDatePicker)"));
            date.setWidth(150);
            date.setEditable(true);
            date.setCellEditor(QStringLiteral("auDatePicker"));
            columns.append(date);
            model->setColumnDefs(columns);

            QVector<QVariantMap> rows;
            const QStringList categories = {QStringLiteral(u"水果"), QStringLiteral(u"蔬菜"),
                                            QStringLiteral(u"谷物"), QStringLiteral(u"坚果")};
            for (int r = 0; r < 12; ++r) {
                QVariantMap row;
                row.insert("name", QString("R%1").arg(r));
                row.insert("category", categories.at(r % categories.size()));
                row.insert("date", QDate(2026, 9, r + 1));
                rows.append(row);
            }
            model->setRowData(rows);

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);
            grid->registerCellEditor(QStringLiteral("auSelect"),
                                     QSharedPointer<AuCellEditor>(new AuSelectCellEditor(
                                         {QStringLiteral(u"苹果 Apple"),
                                          QStringLiteral(u"香蕉 Banana"),
                                          QStringLiteral(u"樱桃 Cherry"),
                                          QStringLiteral(u"葡萄 Grape"),
                                          QStringLiteral(u"芒果 Mango"),
                                          QStringLiteral(u"橙子 Orange")})));
            grid->registerCellEditor(QStringLiteral("auDatePicker"),
                                     QSharedPointer<AuCellEditor>(new AuDatePickerCellEditor));

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(grid, &AuGridWidget::cellValueChanged, grid,
                             [grid, log](int row, int column, const QVariant &value) {
                                 const QString field = grid->model()->fieldAtColumn(column);
                                 PageHelpers::appendLog(
                                     log, QString("cellValueChanged: r=%1 %2=%3")
                                              .arg(row)
                                              .arg(field)
                                              .arg(value.toString()));
                             });
            return PageHelpers::composeDemo(nullptr, grid, log);
        };
        page.code = R"snip(
// —— 完全外部集成，AuGrid 零改动 ——
// 1) 包装类：QObject + AuCellEditor 多继承（用静态 QObject::connect，无需 moc）
class AuSelectCellEditor : public QObject, public AuCellEditor
{
public:
    explicit AuSelectCellEditor(const QStringList &items) : m_items(items) {}

    QWidget *createEditor(QWidget *parent) const override {
        AuSelect *select = new AuSelect(parent);      // parent 即宿主 AuGridWidget
        select->addItems(m_items);
        select->setFocusPolicy(Qt::StrongFocus);      // 让 editCell 的 setFocus 生效

        // 编辑框高度贴合单元格：按宿主行高定制（独立样式实例）
        if (AuGridWidget *grid = qobject_cast<AuGridWidget *>(parent)) {
            AuSelectStyle *style = new AuSelectStyle(*AuSelectStyle::globalStyle());
            style->setEditorHeight(grid->rowHeight());
            select->setStyle(style);
        }

        // 打开编辑器即展开面板（延迟一拍），Enter 直接确认选择
        QTimer::singleShot(0, select, [select]() {
            if (select->isEnabled())
                select->showPopup();
        });

        // 选择完成 → 公开 API closeEditor(true) 提交；
        // 提交后宿主自动把焦点还给表格，Tab / 方向键可继续导航
        QObject::connect(select, &AuSelect::activated, select, [select]() {
            if (AuGridWidget *grid = qobject_cast<AuGridWidget *>(select->parent()))
                grid->closeEditor(true);
        });
        return select;
    }
    void setEditorValue(QWidget *editor, const QVariant &value) const override {
        AuSelect *select = static_cast<AuSelect *>(editor);
        select->setCurrentIndex(select->findText(value.toString(), Qt::MatchExactly));
    }
    QVariant editorValue(QWidget *editor) const override {
        return static_cast<AuSelect *>(editor)->currentText();
    }
private:
    QStringList m_items;
};

// AuDatePicker 同理：setEditorHeight(行高)、打开即展开日历、
// activated 提交选中值（QDate）、cleared 提交空值
class AuDatePickerCellEditor : public QObject, public AuCellEditor { /* 同上 */ };

// 2) 注册并按列引用——与内置编辑器完全同一套用法
grid->registerCellEditor("auSelect",
    QSharedPointer<AuCellEditor>(new AuSelectCellEditor({"苹果 Apple", "香蕉 Banana"})));
category.setCellEditor("auSelect");
category.setEditable(true);

grid->registerCellEditor("auDatePicker",
    QSharedPointer<AuCellEditor>(new AuDatePickerCellEditor));
date.setCellEditor("auDatePicker");
date.setEditable(true);

// 键盘闭环：Enter / 双击编辑（面板自动展开）→ Enter 确认 → 提交回表格
// → Tab 跳下一可编辑格 / 方向键移动 → Enter 再编辑……
// Esc 取消、点击其它单元格提交；弹层面板跟随单元格全局定位，无需额外处理。
)snip";
        pages.append(page);
    }

    // ============ 10d. 自定义表头渲染器 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"自定义表头渲染器");
        page.description = QStringLiteral(
            u"registerHeaderRenderer + 列定义 setHeaderRenderer 替换列头绘制："
            u"数量列表头右侧内嵌“清零”按钮——按钮上点击不触发排序（hitTest 拦截），"
            u"点击后整列数量清零；表头其余区域仍可点击排序 / 拖动调宽。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"名称"));
            name.setWidth(140);
            columns.append(name);
            AuColumnDef count("count", QStringLiteral(u"数量"));
            count.setSortable(true);
            count.setHeaderRenderer(QStringLiteral("demoHeaderButton"));
            columns.append(count);
            model->setColumnDefs(columns);
            model->setRowData(PageHelpers::makeFlatRows(15));

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);
            grid->setSortingEnabled(true);
            grid->registerHeaderRenderer(
                QStringLiteral("demoHeaderButton"),
                QSharedPointer<AuHeaderRenderer>(new HeaderActionButton(
                    [grid](int column) {
                        // 整列数量清零（可见行坐标逐格写值）
                        for (int r = 0; r < grid->model()->rowCount(); ++r)
                            grid->model()->setCellValue(r, column, 0);
                    })));

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(grid, &AuGridWidget::sortChanged, grid,
                             [log](int column, Qt::SortOrder order) {
                                 PageHelpers::appendLog(
                                     log, QString("sortChanged: column=%1 order=%2")
                                              .arg(column)
                                              .arg(order == Qt::AscendingOrder ? "Asc" : "Desc"));
                             });
            return PageHelpers::composeDemo(nullptr, grid, log);
        };
        page.code = R"snip(
// 表头渲染器：接管列头绘制；hitTest 命中的区域点击不触发排序，
// 在 headerMouseClick 里回调（完整实现见演示源码）
class HeaderButtonRenderer : public AuHeaderRenderer
{
public:
    void paint(QPainter *painter, const QRect &rect, const AuColumnDef &columnDef,
               int column, int level, const AuGridModel *model,
               const AuGridStyle *style, const QColor &textColor,
               const AuHeaderRendererContext &context) const override {
        // 画标题文字 + 右侧按钮（context.hovered / headerPos 做悬停高亮）
        // context.sorted / sortOrder 可自行绘制排序指示
    }
    bool hitTest(int, int, const QRect &rect, const QPoint &headerPos) const override {
        return buttonRect(rect).contains(headerPos);   // 按钮热区
    }
    void headerMouseClick(int column, int level, const QRect &rect,
                          const QPoint &headerPos) override {
        if (hitTest(column, level, rect, headerPos))
            emit clicked(column);   // 转成业务信号
    }
};

grid->registerHeaderRenderer("headerButton",
    QSharedPointer<AuHeaderRenderer>(new HeaderButtonRenderer));
count.setHeaderRenderer("headerButton");
count.setSortable(true);   // 按钮外的表头区域仍可点击排序

// 悬停手型光标：wantsHoverCursor 返回 true（按钮热区上）
// 另有 headerMouseEnter / Leave / Move 可做更细的交互状态
)snip";
        pages.append(page);
    }

    // ============ 11. 合并单元格 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"合并单元格");
        page.description = QStringLiteral(
            u"setSpan(row, column, rowSpan, columnSpan)，语义参考 QTableView::setSpan："
            u"点击 / 悬停 / 编辑都落在锚点单元格上，合并不跨越列停靠区域边界。");
        page.createDemo = []() {
            AuGridModel *model = nullptr;
            AuGridWidget *grid = newFlatGrid(10, &model);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *span1Btn = new QPushButton(QStringLiteral(u"合并 (0,0) 2行"));
            QPushButton *span2Btn = new QPushButton(QStringLiteral(u"合并 (2,0) 3行x2列"));
            QPushButton *clearOneBtn = new QPushButton(QStringLiteral(u"清除 (0,0)"));
            QPushButton *clearAllBtn = new QPushButton(QStringLiteral(u"清除全部"));
            controls->addWidget(span1Btn);
            controls->addWidget(span2Btn);
            controls->addWidget(clearOneBtn);
            controls->addWidget(clearAllBtn);
            controls->addStretch(1);
            QObject::connect(span1Btn, &QPushButton::clicked, grid,
                             [grid]() { grid->setSpan(0, 0, 2, 1); });
            QObject::connect(span2Btn, &QPushButton::clicked, grid,
                             [grid]() { grid->setSpan(2, 0, 3, 2); });
            QObject::connect(clearOneBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->clearSpan(0, 0); });
            QObject::connect(clearAllBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->clearSpans(); });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：四列 × 10 行

grid->setSpan(0, 0, 2, 1);   // (行, 列, 行跨度, 列跨度)，左上角为锚点
grid->setSpan(2, 0, 3, 2);   // 合并 3 行 x 2 列；新合并替换重叠旧合并

grid->clearSpan(0, 0);   // 清除某处所在的合并
grid->clearSpans();      // 清除全部合并

grid->rowSpan(2, 0);     // 查询：所在合并的行跨度（不在合并内 = 1）
grid->hasSpans();        // 是否存在任何合并区域

// 注意：行索引为当前可见行（排序 / 过滤后不自动跟随数据），
// 跨度超出模型边界自动截断。
)snip";
        pages.append(page);
    }

    // ============ 12. 列宽模式 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"列宽模式");
        page.description = QStringLiteral(
            u"三种宽度模式：Fixed 固定像素 / AutoFit 按内容自适应（只增不减）/ "
            u"Flex 按权重分享剩余空间；也支持运行时 setColumnWidth 与整体自适应。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef fixed("name", QStringLiteral(u"Fixed 100"));
            fixed.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);
            fixed.setWidth(100);
            columns.append(fixed);
            AuColumnDef fit("count", QStringLiteral(u"AutoFit"));
            fit.setWidthMode(AuColumnDef::ColumnWidthMode::AutoFit);
            columns.append(fit);
            AuColumnDef flex1("category", QStringLiteral(u"Flex 权重1"));
            flex1.setWidthMode(AuColumnDef::ColumnWidthMode::Flex);
            flex1.setFlexWeight(1);
            columns.append(flex1);
            AuColumnDef flex2("enabled", QStringLiteral(u"Flex 权重2"));
            flex2.setWidthMode(AuColumnDef::ColumnWidthMode::Flex);
            flex2.setFlexWeight(2);
            columns.append(flex2);
            model->setColumnDefs(columns);
            model->setRowData(PageHelpers::makeFlatRows(30));

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *autoBtn = new QPushButton(QStringLiteral(u"autoSizeAllColumns"));
            QPushButton *widthBtn = new QPushButton(QStringLiteral(u"名称列宽 → 180"));
            controls->addWidget(autoBtn);
            controls->addWidget(widthBtn);
            controls->addStretch(1);
            QObject::connect(autoBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->autoSizeAllColumns(); });
            QObject::connect(widthBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setColumnWidth("name", 180); });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 示例数据：四列，表头与宽度模式与演示一致（数据 30 行，取自 makeFlatRows）
AuColumnDef fixed("name", "Fixed 100");
fixed.setWidthMode(AuColumnDef::ColumnWidthMode::Fixed);  // 固定像素
fixed.setWidth(100);

AuColumnDef fit("count", "AutoFit");
fit.setWidthMode(AuColumnDef::ColumnWidthMode::AutoFit);  // 按表头 + 可视内容测量
                                                          //（只增不减，受 minWidth/maxWidth 约束）
AuColumnDef flex1("category", "Flex 权重1");
flex1.setWidthMode(AuColumnDef::ColumnWidthMode::Flex);   // 弹性填充剩余空间
flex1.setFlexWeight(1);                                   // 权重越大分得越多
AuColumnDef flex2("enabled", "Flex 权重2");
flex2.setWidthMode(AuColumnDef::ColumnWidthMode::Flex);
flex2.setFlexWeight(2);

// 运行时调整（对应演示按钮）
grid->autoSizeAllColumns();         // 全部列按内容自适应
grid->setColumnWidth("name", 180);  // 按 field 设宽（分组列作用于全部叶子）
)snip";
        pages.append(page);
    }

    // ============ 13. 多级表头 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"多级表头");
        page.description = QStringLiteral(
            u"分组列通过 addChild 携带子列，列定义展开为列树 → 多级表头；叶子列才承载数据。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            AuColumnDef group1(QStringLiteral(u"基本信息"));
            group1.addChild(AuColumnDef("name", QStringLiteral(u"名称")));
            group1.addChild(AuColumnDef("count", QStringLiteral(u"数量")));
            AuColumnDef group2(QStringLiteral(u"其他"));
            group2.addChild(AuColumnDef("category", QStringLiteral(u"类别")));
            group2.addChild(AuColumnDef("enabled", QStringLiteral(u"启用")));
            model->setColumnDefs({group1, group2});
            model->setRowData(PageHelpers::makeFlatRows(30));

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);
            return PageHelpers::composeDemo(nullptr, grid);
        };
        page.code = R"snip(
// 分组列携带子列 → 多级表头（叶子数据与“最小用法”一致：四列 × 30 行）
AuColumnDef group1("basic", "基本信息");
group1.addChild(AuColumnDef("name", "名称"));
group1.addChild(AuColumnDef("count", "数量"));

AuColumnDef group2("extra", "其他");
group2.addChild(AuColumnDef("category", "类别"));
group2.addChild(AuColumnDef("enabled", "启用"));

model->setColumnDefs({group1, group2});   // 两级表头

// 分组列可嵌套出更深层级；
// field 定位的 API（setColumnWidth / setColumnVisible / pinColumn）
// 指向分组列时作用于其全部叶子列。
)snip";
        pages.append(page);
    }

    // ============ 14. 列停靠 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"列停靠");
        page.description = QStringLiteral(
            u"停靠列在水平滚动时保持可见：运行时 pinColumn（column 为叶子列索引，或按 field "
            u"定位；指向分组列时整组停靠）。演示有 12 列（撑出水平滚动条），滚动中间列即可"
            u"观察两侧停靠效果。");
        page.createDemo = []() {
            // 12 列 × 30 行：列多到撑出水平滚动条，才能滚动观察停靠效果
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"名称"));
            name.setWidth(110);
            columns.append(name);
            columns.append(AuColumnDef("count", QStringLiteral(u"数量")));
            columns.append(AuColumnDef("category", QStringLiteral(u"类别")));
            AuColumnDef enabled("enabled", QStringLiteral(u"启用"));
            enabled.setCheckbox(true);
            columns.append(enabled);
            for (int c = 1; c <= 8; ++c)
                columns.append(AuColumnDef(QString("col%1").arg(c),
                                           QStringLiteral(u"字段%1").arg(c)));
            model->setColumnDefs(columns);

            const QStringList categories = {QStringLiteral(u"水果"), QStringLiteral(u"蔬菜"),
                                            QStringLiteral(u"谷物"), QStringLiteral(u"坚果")};
            QVector<QVariantMap> rows;
            rows.reserve(30);
            for (int r = 0; r < 30; ++r) {
                QVariantMap row;
                row.insert("name", QString("R%1").arg(r));
                row.insert("count", r * 10);
                row.insert("category", categories.at(r % categories.size()));
                row.insert("enabled", r % 3 != 0);
                for (int c = 1; c <= 8; ++c)
                    row.insert(QString("col%1").arg(c), QString("V%1-%2").arg(r).arg(c));
                rows.append(row);
            }
            model->setRowData(rows);

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *pinLeftBtn = new QPushButton(QStringLiteral(u"名称列 → 左停靠"));
            QPushButton *pinLeft2Btn = new QPushButton(QStringLiteral(u"数量列 → 左停靠"));
            QPushButton *pinRightBtn = new QPushButton(QStringLiteral(u"字段8列 → 右停靠"));
            QPushButton *unpinBtn = new QPushButton(QStringLiteral(u"全部取消"));
            controls->addWidget(pinLeftBtn);
            controls->addWidget(pinLeft2Btn);
            controls->addWidget(pinRightBtn);
            controls->addWidget(unpinBtn);
            controls->addStretch(1);
            QObject::connect(pinLeftBtn, &QPushButton::clicked, grid, [grid]() {
                grid->pinColumn("name", AuColumnDef::PinMode::Left);
            });
            QObject::connect(pinLeft2Btn, &QPushButton::clicked, grid, [grid]() {
                grid->pinColumn("count", AuColumnDef::PinMode::Left);
            });
            QObject::connect(pinRightBtn, &QPushButton::clicked, grid, [grid]() {
                grid->pinColumn("col8", AuColumnDef::PinMode::Right);
            });
            QObject::connect(unpinBtn, &QPushButton::clicked, grid, [grid]() {
                grid->pinColumn("name", AuColumnDef::PinMode::None);
                grid->pinColumn("count", AuColumnDef::PinMode::None);
                grid->pinColumn("col8", AuColumnDef::PinMode::None);
            });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 演示数据：名称 / 数量 / 类别 / 启用 + 字段1~字段8 共 12 列 × 30 行，
// 列足够多撑出水平滚动条，滚动中间列即可观察停靠效果

// 运行时停靠（对应演示按钮）
grid->pinColumn("name", AuColumnDef::PinMode::Left);       // 名称列 → 左停靠
grid->pinColumn("count", AuColumnDef::PinMode::Left);      // 数量列 → 左停靠
grid->pinColumn("col8", AuColumnDef::PinMode::Right);      // 字段8列 → 右停靠
grid->pinColumn("name", AuColumnDef::PinMode::None);       // 取消停靠

grid->columnPinMode(0);   // 查询停靠状态

// 也可在列定义中指定：name.setPinMode(AuColumnDef::PinMode::Left)
// 合并单元格不会跨越停靠区域边界；
// 行号 / 复选框 / 拖拽手柄等辅助列始终固定在左侧。
)snip";
        pages.append(page);
    }

    // ============ 15. 列显示隐藏 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"列显示隐藏");
        page.description = QStringLiteral(
            u"setColumnVisible 显隐列（带位移动画）；隐藏列的叶子索引不变；field 指向分组列时整组显隐。");
        page.createDemo = []() {
            AuGridModel *model = nullptr;
            AuGridWidget *grid = newFlatGrid(30, &model);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *hideBtn = new QPushButton(QStringLiteral(u"隐藏“数量”列"));
            QPushButton *showBtn = new QPushButton(QStringLiteral(u"显示“数量”列"));
            controls->addWidget(hideBtn);
            controls->addWidget(showBtn);
            controls->addStretch(1);
            QObject::connect(hideBtn, &QPushButton::clicked, grid, [grid]() {
                grid->setColumnVisible("count", false);
            });
            QObject::connect(showBtn, &QPushButton::clicked, grid, [grid]() {
                grid->setColumnVisible("count", true);
            });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：四列 × 30 行

// 隐藏 / 显示“数量”列（对应演示按钮，带位移动画）
grid->setColumnVisible("count", false);
grid->setColumnVisible("count", true);

grid->isColumnVisible("count");   // 查询可见性

// 说明：
//  - 隐藏列宽度按 0 参与布局，叶子列索引不变；
//  - field 可指向分组列（整组显隐）；
//  - 列定义里的 setVisible(false) 为初始不可见。
)snip";
        pages.append(page);
    }

    // ============ 16. 树形表格 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"树形表格");
        page.description = QStringLiteral(
            u"模型 setTreeData 载入层级数据后自动进入树形模式：首列缩进 + 展开箭头，"
            u"支持双击 / 键盘切换与批量展开折叠。");
        page.createDemo = []() {
            auto makeTree = []() {
                QVector<AuTreeRow> roots;
                for (int i = 0; i < 4; ++i) {
                    AuTreeRow root;
                    root.data.insert("name", QStringLiteral(u"分组 %1").arg(i + 1));
                    root.data.insert("count", i * 100);
                    for (int j = 0; j < 3; ++j) {
                        AuTreeRow child;
                        child.data.insert("name", QStringLiteral(u"　子项 %1-%2").arg(i + 1).arg(j + 1));
                        child.data.insert("count", i * 100 + j);
                        AuTreeRow leaf;
                        leaf.data.insert("name", QStringLiteral(u"　　明细 %1-%2-a").arg(i + 1).arg(j + 1));
                        leaf.data.insert("count", j);
                        child.children.append(leaf);
                        root.children.append(child);
                    }
                    roots.append(root);
                }
                return roots;
            };

            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            columns.append(AuColumnDef("name", QStringLiteral(u"名称")));
            columns.append(AuColumnDef("count", QStringLiteral(u"数量")));
            model->setColumnDefs(columns);
            model->setTreeData(makeTree());

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *expandBtn = new QPushButton(QStringLiteral(u"全部展开"));
            QPushButton *collapseBtn = new QPushButton(QStringLiteral(u"全部折叠"));
            QPushButton *depthBtn = new QPushButton(QStringLiteral(u"展开到 1 层"));
            QPushButton *reloadBtn = new QPushButton(QStringLiteral(u"重载树数据"));
            controls->addWidget(expandBtn);
            controls->addWidget(collapseBtn);
            controls->addWidget(depthBtn);
            controls->addWidget(reloadBtn);
            controls->addStretch(1);
            QObject::connect(expandBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->expandAll(); });
            QObject::connect(collapseBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->collapseAll(); });
            QObject::connect(depthBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->expandToDepth(1); });
            QObject::connect(reloadBtn, &QPushButton::clicked, grid, [grid, makeTree]() {
                grid->model()->setTreeData(makeTree());
            });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 列定义：名称 / 数量（叶子数据放在 AuTreeRow::data，键 = 列 field）
model->setColumnDefs({AuColumnDef("name", "名称"),
                      AuColumnDef("count", "数量")});

// 层级数据：AuTreeRow 携带 data 与 children
// 演示数据：4 个分组 × 3 个子项 × 1 条明细（三级）
AuTreeRow root;
root.data.insert("name", "分组 1");
root.data.insert("count", 100);
root.expanded = true;             // 初始展开态（可选，默认折叠）

AuTreeRow child;
child.data.insert("name", "子项 1-1");
child.data.insert("count", 100);
AuTreeRow leaf;                   // children 为空 = 叶子行
leaf.data.insert("name", "明细 1-1-a");
leaf.data.insert("count", 0);
child.children.append(leaf);
root.children.append(child);

model->setTreeData({root});       // 树形模式自动生效

// 展开 / 折叠（对应演示按钮；视图 API 使用可见行号，模型 API 使用原始行号）
grid->expandAll();
grid->collapseAll();
grid->expandToDepth(1);           // 展开深度 ≤ 1 的行
grid->model()->setTreeData(makeTree());   // 重载树数据
grid->rowDepth(0);                // 层级（根 = 0）
)snip";
        pages.append(page);
    }

    // ============ 17. 树形勾选级联 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"树形勾选级联");
        page.description = QStringLiteral(
            u"树形模式 + 复选框列 → 父子级联三态：勾选父行自动勾选全部子行，"
            u"子行部分勾选时父行半选；折叠不可见的子行同样参与级联。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            columns.append(AuColumnDef("name", QStringLiteral(u"名称")));
            columns.append(AuColumnDef("count", QStringLiteral(u"数量")));
            model->setColumnDefs(columns);

            QVector<AuTreeRow> roots;
            for (int i = 0; i < 3; ++i) {
                AuTreeRow root;
                root.data.insert("name", QStringLiteral(u"分组 %1").arg(i + 1));
                root.data.insert("count", i * 100);
                for (int j = 0; j < 3; ++j) {
                    AuTreeRow child;
                    child.data.insert("name", QStringLiteral(u"　子项 %1-%2").arg(i + 1).arg(j + 1));
                    child.data.insert("count", i * 100 + j);
                    root.children.append(child);
                }
                roots.append(root);
            }
            model->setTreeData(roots);

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);
            grid->setCheckboxColumnVisible(true);
            grid->expandAll();

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(grid, &AuGridWidget::rowCheckedChanged, grid,
                             [grid, log](int row, bool checked) {
                                 PageHelpers::appendLog(
                                     log, QString("rowCheckedChanged: row=%1 checked=%2 半选=%3")
                                              .arg(row)
                                              .arg(checked)
                                              .arg(grid->isRowPartiallyChecked(row)));
                             });

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *checkFirstBtn = new QPushButton(QStringLiteral(u"勾选第 1 行（看级联）"));
            QPushButton *selectAllBtn = new QPushButton(QStringLiteral(u"全选"));
            QPushButton *clearBtn = new QPushButton(QStringLiteral(u"清空"));
            controls->addWidget(checkFirstBtn);
            controls->addWidget(selectAllBtn);
            controls->addWidget(clearBtn);
            controls->addStretch(1);
            QObject::connect(checkFirstBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setRowChecked(0, true); });
            QObject::connect(selectAllBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->selectAllRows(); });
            QObject::connect(clearBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->clearAllCheckedRows(); });
            return PageHelpers::composeDemo(controls, grid, log);
        };
        page.code = R"snip(
// 示例数据：名称 / 数量 两列，3 个分组 × 3 个子项（树形）
model->setColumnDefs({AuColumnDef("name", "名称"),
                      AuColumnDef("count", "数量")});
model->setTreeData(roots);

grid->setCheckboxColumnVisible(true); // 复选框列 → 父子级联三态
grid->expandAll();                    // 全部展开，便于观察级联

// 勾选父行自动勾选全部子行；子行部分勾选时父行半选
//（对应演示按钮“勾选第 1 行（看级联）”；日志打印每行的半选状态）
grid->setRowChecked(0, true);
grid->isRowChecked(0);                // 勾选状态
grid->isRowPartiallyChecked(0);       // 半选状态

grid->selectAllRows();         // 全选（与表头全选等效）
grid->clearAllCheckedRows();   // 清空
grid->checkedOriginalRows();   // 勾选集合（原始行号）
)snip";
        pages.append(page);
    }

    // ============ 18. 加载遮罩 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"加载遮罩");
        page.description = QStringLiteral(
            u"setLoading 用于异步取数场景：半透明蒙层 + 旋转圆环 + 文案，淡入淡出；"
            u"显示期间交互被屏蔽、打开中的编辑器自动关闭。");
        page.createDemo = []() {
            AuGridModel *model = nullptr;
            AuGridWidget *grid = newFlatGrid(50, &model);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *loadBtn = new QPushButton(QStringLiteral(u"模拟请求（遮罩 2 秒）"));
            QPushButton *stopBtn = new QPushButton(QStringLiteral(u"立即关闭"));
            controls->addWidget(loadBtn);
            controls->addWidget(stopBtn);
            controls->addStretch(1);
            QObject::connect(loadBtn, &QPushButton::clicked, grid, [grid]() {
                grid->setLoading(true);
                grid->setLoadingText(QStringLiteral(u"正在加载数据…"));
                QTimer::singleShot(2000, grid, [grid]() { grid->setLoading(false); });
            });
            QObject::connect(stopBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setLoading(false); });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：四列 × 50 行

// 发起请求前显示遮罩，数据返回后关闭（对应演示按钮“模拟请求（遮罩 2 秒）”）
grid->setLoading(true);
grid->setLoadingText("正在加载数据…");   // 空串只显示圆环

QTimer::singleShot(2000, grid, [grid]() {
    grid->setLoading(false);   // 淡出，恢复交互（演示按钮“立即关闭”同理）
});

grid->isLoading();   // 遮罩状态（淡出过渡中已返回 false）

// 显示期间：鼠标键盘交互被屏蔽、滚动条隐藏、
// 打开中的编辑器自动关闭（不提交）。
)snip";
        pages.append(page);
    }

    // ============ 19. 数据变化动画 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"数据变化动画");
        page.description = QStringLiteral(
            u"两类动画：排序 / 过滤的行位移动画（FLIP），重设数据的切换动画"
            u"（旧内容上移出场、新内容移入）；均可调时长或整体关闭。");
        page.createDemo = []() {
            AuGridModel *model = nullptr;
            AuGridWidget *grid = newFlatGrid(30, &model);
            grid->setSortingEnabled(true);
            // 让“数量”列可排序，方便触发布移动画
            {
                QVector<AuColumnDef> columns = model->topLevelColumnDefs();
                if (!columns.isEmpty()) {
                    columns[1].setSortable(true);
                    model->setColumnDefs(columns);
                }
            }

            QHBoxLayout *controls = new QHBoxLayout;
            QCheckBox *animToggle = new QCheckBox(QStringLiteral(u"动画"));
            animToggle->setChecked(true);
            QPushButton *resetBtn = new QPushButton(QStringLiteral(u"重设数据（切换动画）"));
            QPushButton *sortBtn = new QPushButton(QStringLiteral(u"按数量排序（位移动画）"));
            controls->addWidget(animToggle);
            controls->addWidget(resetBtn);
            controls->addWidget(sortBtn);
            controls->addStretch(1);
            QObject::connect(animToggle, &QCheckBox::toggled, grid, [grid](bool on) {
                grid->setDataAnimationEnabled(on);
            });
            QObject::connect(resetBtn, &QPushButton::clicked, grid, [grid]() {
                QVector<QVariantMap> rows;
                for (int r = 0; r < 30; ++r) {
                    QVariantMap row;
                    row.insert("name", QString("NEW%1").arg(r));
                    row.insert("count", (r * 37) % 300);
                    row.insert("category", r % 2 ? "A" : "B");
                    row.insert("enabled", r % 2 == 0);
                    rows.append(row);
                }
                grid->model()->setRowData(rows);
            });
            QObject::connect(sortBtn, &QPushButton::clicked, grid, [grid]() {
                grid->setSortRules({AuSortRule(1, grid->sortOrder() == Qt::AscendingOrder
                                                        ? Qt::DescendingOrder
                                                        : Qt::AscendingOrder)});
            });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：四列 × 30 行

// 动画总开关（对应演示中的“动画”复选框，默认开启）
grid->setDataAnimationEnabled(true);
grid->setSortAnimationDuration(100);        // 排序 / 过滤位移动画时长（ms，可调）
grid->setDataResetAnimationDuration(200);   // 重设数据切换动画时长（ms，可调）
// 设为 0 关闭对应动画

// 重设数据 → 切换动画：旧内容上移出场、新内容自下方移入
//（对应演示按钮“重设数据”，30 行 NEW%1 数据）
model->setRowData(newRows);

// 排序规则 → 行位移动画：行从旧布局位置移向新位置
//（对应演示按钮“按数量排序”，按数量列叶子索引 1 升 / 降序切换）
grid->setSortRules({AuSortRule(1, Qt::AscendingOrder)});
)snip";
        pages.append(page);
    }

    // ============ 20. 自动换行行高 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"自动换行行高");
        page.description = QStringLiteral(
            u"TextWrapMode::WordWrap 使单元格按列宽自动换行，行高随内容与列宽自适应；"
            u"调整列宽后行高自动重排。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"名称"));
            name.setWidth(90);
            columns.append(name);
            AuColumnDef desc("desc", QStringLiteral(u"描述（自动换行）"));
            desc.setWidth(260);
            desc.setTextWrapMode(AuColumnDef::TextWrapMode::WordWrap);
            columns.append(desc);
            model->setColumnDefs(columns);

            QVector<QVariantMap> rows;
            for (int r = 0; r < 20; ++r) {
                QVariantMap row;
                row.insert("name", QString("R%1").arg(r));
                row.insert("desc", QStringLiteral(
                                       u"这是一段较长的描述文本，用于演示自动换行："
                                       u"当列宽不足以单行容纳时按词换行，行高随之增加。"));
                rows.append(row);
            }
            model->setRowData(rows);

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *wideBtn = new QPushButton(QStringLiteral(u"描述列宽 → 420"));
            QPushButton *narrowBtn = new QPushButton(QStringLiteral(u"描述列宽 → 180"));
            controls->addWidget(wideBtn);
            controls->addWidget(narrowBtn);
            controls->addStretch(1);
            QObject::connect(wideBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setColumnWidth("desc", 420); });
            QObject::connect(narrowBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setColumnWidth("desc", 180); });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 示例数据：名称（90px）/ 描述（自动换行，260px）两列 × 20 行长描述文本
AuColumnDef name("name", "名称");
name.setWidth(90);

AuColumnDef desc("desc", "描述（自动换行）");
desc.setWidth(260);
desc.setTextWrapMode(AuColumnDef::TextWrapMode::WordWrap);  // 自动换行
// 行高按内容与列宽自适应；调整列宽后自动重排
//（对应演示按钮：描述列宽 → 420 / 180）
grid->setColumnWidth("desc", 420);

// 其它模式：
// NoWrap          单行，超出裁剪（默认）
// NoWrapTruncate  单行，超出显示省略号
// NoWrapOverflow  单行，允许溢出到相邻单元格
)snip";
        pages.append(page);
    }

    // ============ 21. 网格线 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"网格线");
        page.description = QStringLiteral(
            u"setVerticalGridLinesVisible 控制数据单元格左右（列方向）边框线；"
            u"隐藏后仅保留行方向边框，列未铺满宽度时自动在内容右边缘补一条竖线"
            u"（始终描住当前最右侧列的右边界），辅助列与表头不受影响。");
        page.createDemo = []() {
            AuGridModel *model = nullptr;
            AuGridWidget *grid = newFlatGrid(20, &model);

            QHBoxLayout *controls = new QHBoxLayout;
            QPushButton *showBtn = new QPushButton(QStringLiteral(u"显示纵向网格线"));
            QPushButton *hideBtn = new QPushButton(QStringLiteral(u"隐藏纵向网格线"));
            controls->addWidget(showBtn);
            controls->addWidget(hideBtn);
            controls->addStretch(1);
            QObject::connect(showBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setVerticalGridLinesVisible(true); });
            QObject::connect(hideBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setVerticalGridLinesVisible(false); });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：四列 × 20 行

grid->setVerticalGridLinesVisible(false);  // 隐藏列方向边框（对应演示按钮）
grid->setVerticalGridLinesVisible(true);   // 恢复（默认显示）

grid->isVerticalGridLinesVisible();        // 查询状态

// 隐藏后 body 区数据列（含合并单元格）只保留行方向边框；
// 行号 / 复选框 / 拖拽手柄等辅助列与表头边框不受影响。
)snip";
        pages.append(page);
    }

    // ============ 22. 单元格 Tooltip ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"单元格 Tooltip");
        page.description = QStringLiteral(
            u"列 tooltipEnabled=true 时，内容超出列宽被裁剪则悬停约 0.7 秒弹出提示；"
            u"可通过回调自定义内容（支持富文本）。");
        page.createDemo = []() {
            AuGridModel *model = new AuGridModel(nullptr);
            QVector<AuColumnDef> columns;
            AuColumnDef name("name", QStringLiteral(u"长文本列"));
            name.setWidth(150);
            name.setTooltipEnabled(true);
            columns.append(name);
            AuColumnDef count("count", QStringLiteral(u"数量"));
            columns.append(count);
            model->setColumnDefs(columns);

            QVector<QVariantMap> rows;
            for (int r = 0; r < 20; ++r) {
                QVariantMap row;
                row.insert("name", QStringLiteral(u"第 %1 行：这段文本明显超出 150px 列宽，悬停试试").arg(r));
                row.insert("count", r);
                rows.append(row);
            }
            model->setRowData(rows);

            AuGridWidget *grid = new AuGridWidget;
            grid->setModel(model);
            model->setParent(grid);
            grid->setCellTooltipTextCallback(
                [](const AuGridModel *, int row, int column, const QString &text) {
                    return QStringLiteral(u"<b>R%1C%2</b><br/>%3")
                        .arg(row)
                        .arg(column)
                        .arg(text.toHtmlEscaped());
                });
            return PageHelpers::composeDemo(nullptr, grid);
        };
        page.code = R"snip(
// 示例数据：长文本列（150px，tooltipEnabled）+ 数量列 × 20 行超宽文本
AuColumnDef name("name", "长文本列");
name.setWidth(150);
name.setTooltipEnabled(true);   // 内容被裁剪时悬停约 0.7 秒弹出
AuColumnDef count("count", "数量");

// 自定义 tooltip 内容（与演示一致：富文本，行 / 列 / 原文）
grid->setCellTooltipTextCallback(
    [](const AuGridModel *, int row, int column, const QString &text) {
        return QString("<b>R%1C%2</b><br/>%3")
            .arg(row)
            .arg(column)
            .arg(text.toHtmlEscaped());
    });

// 说明：
//  - 仅 NoWrap / NoWrapTruncate 列生效（WordWrap 已完整展示、Overflow 有意溢出）；
//  - 未设置回调时显示单元格文本本身；
//  - 截断判定不受回调影响。
)snip";
        pages.append(page);
    }

    // ============ 23. 右键菜单 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"右键菜单");
        page.description = QStringLiteral(
            u"cellRightClicked / headerRightClicked 给出全局坐标，可直接用于 QMenu::exec 弹出菜单。");
        page.createDemo = []() {
            AuGridModel *model = nullptr;
            AuGridWidget *grid = newFlatGrid(30, &model);

            QPlainTextEdit *log = PageHelpers::makeLog();
            QObject::connect(grid, &AuGridWidget::cellRightClicked, grid,
                             [grid, log](int row, int column, const QPoint &globalPos) {
                                 QMenu menu;
                                 QString field = grid->model()->fieldAtColumn(column);
                                 menu.addAction(QStringLiteral(u"查看 %1（r=%2 c=%3）").arg(field).arg(row).arg(column));
                                 menu.addAction(QStringLiteral(u"删除该行"));
                                 QAction *act = menu.exec(globalPos);
                                 if (act)
                                     PageHelpers::appendLog(log, QString("菜单点击: %1").arg(act->text()));
                             });
            QObject::connect(grid, &AuGridWidget::headerRightClicked, grid,
                             [log](int column, int level, const QPoint &globalPos) {
                                 QMenu menu;
                                 menu.addAction(QStringLiteral(u"表头操作 c=%1 level=%2").arg(column).arg(level));
                                 menu.exec(globalPos);
                             });
            return PageHelpers::composeDemo(nullptr, grid, log);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：四列 × 30 行

// 单元格右键：globalPos 为全局坐标，可直接用于 QMenu::exec
//（与演示一致：菜单项显示列 field 名 + 行列号）
connect(grid, &AuGridWidget::cellRightClicked,
        this, [grid](int row, int column, const QPoint &globalPos) {
    QMenu menu;
    QString field = grid->model()->fieldAtColumn(column);
    menu.addAction(QString("查看 %1（r=%2 c=%3）").arg(field).arg(row).arg(column));
    menu.addAction("删除该行");
    QAction *act = menu.exec(globalPos);
    if (act) { /* 处理菜单动作 */ }
});

// 表头右键：column 为该节点首个叶子列，level 为 1 起始层级
//（与演示一致：菜单项显示列与层级）
connect(grid, &AuGridWidget::headerRightClicked,
        this, [](int column, int level, const QPoint &globalPos) {
    QMenu menu;
    menu.addAction(QString("表头操作 c=%1 level=%2").arg(column).arg(level));
    menu.exec(globalPos);
});
)snip";
        pages.append(page);
    }

    // ============ 24. 滚动定位与大数据量 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"滚动定位");
        page.description = QStringLiteral(
            u"十万行数据直接载入（虚拟化渲染只绘制可视区）；scrollToRow / scrollToCell "
            u"程序化定位，可选顶端 / 居中 / 底端对齐。");
        page.createDemo = []() {
            constexpr int kRows = 100000;
            AuGridModel *model = nullptr;
            AuGridWidget *grid = newFlatGrid(kRows, &model);

            QHBoxLayout *controls = new QHBoxLayout;
            QSpinBox *spin = new QSpinBox;
            spin->setRange(0, kRows - 1);
            spin->setValue(99999);
            controls->addWidget(new QLabel(QStringLiteral(u"目标行")));
            controls->addWidget(spin);
            QPushButton *visibleBtn = new QPushButton(QStringLiteral(u"EnsureVisible"));
            QPushButton *topBtn = new QPushButton(QStringLiteral(u"顶端对齐"));
            QPushButton *centerBtn = new QPushButton(QStringLiteral(u"居中"));
            QPushButton *bottomBtn = new QPushButton(QStringLiteral(u"底端对齐"));
            controls->addWidget(visibleBtn);
            controls->addWidget(topBtn);
            controls->addWidget(centerBtn);
            controls->addWidget(bottomBtn);
            controls->addStretch(1);
            using Hint = AuGridWidget::ScrollHint;
            QObject::connect(visibleBtn, &QPushButton::clicked, grid, [grid, spin]() {
                grid->scrollToRow(spin->value(), Hint::EnsureVisible);
            });
            QObject::connect(topBtn, &QPushButton::clicked, grid, [grid, spin]() {
                grid->scrollToRow(spin->value(), Hint::PositionTop);
            });
            QObject::connect(centerBtn, &QPushButton::clicked, grid, [grid, spin]() {
                grid->scrollToRow(spin->value(), Hint::PositionCenter);
            });
            QObject::connect(bottomBtn, &QPushButton::clicked, grid, [grid, spin]() {
                grid->scrollToRow(spin->value(), Hint::PositionBottom);
            });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 演示数据：标准四列 × 100000 行（构造见“最小用法”）
// 虚拟化渲染只绘制可视区与缓冲区，直接载入即可
model->setRowData(rows);

// 垂直定位到某行（演示有目标行输入框 + 四个按钮，默认 99999）
grid->scrollToRow(99999, AuGridWidget::ScrollHint::EnsureVisible);  // 滚到刚好可见（默认）
grid->scrollToRow(99999, AuGridWidget::ScrollHint::PositionTop);    // 顶端对齐
grid->scrollToRow(99999, AuGridWidget::ScrollHint::PositionCenter); // 居中
grid->scrollToRow(99999, AuGridWidget::ScrollHint::PositionBottom); // 底端对齐
)snip";
        pages.append(page);
    }

    // ============ 25. 多表格同步 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"多表格同步");
        page.description = QStringLiteral(
            u"AuGridSyncGroup 让多个表格的滚动偏移与列宽互相跟随，适合左右对比场景；"
            u"两个表格可共用同一个模型。");
        page.createDemo = []() {
            QWidget *body = new QWidget;
            QHBoxLayout *h = new QHBoxLayout(body);
            h->setContentsMargins(0, 0, 0, 0);
            h->setSpacing(6);

            AuGridWidget *gridA = new AuGridWidget;
            AuGridModel *model = PageHelpers::makeFlatModel(100, gridA);
            gridA->setModel(model);
            AuGridWidget *gridB = new AuGridWidget;
            gridB->setModel(model);   // 共用模型
            h->addWidget(gridA);
            h->addWidget(gridB);

            AuGridSyncGroup *sync = new AuGridSyncGroup(body);
            sync->addGrid(gridA);
            sync->addGrid(gridB);
            sync->setFeatures(AuGridSyncGroup::SyncAll);
            sync->syncFrom(gridA);
            return PageHelpers::composeDemo(nullptr, body);
        };
        page.code = R"snip(
// 两个表格并排显示，共用同一个模型（标准四列 × 100 行，构造见“最小用法”）
AuGridWidget *gridA = new AuGridWidget(this);
AuGridWidget *gridB = new AuGridWidget(this);
gridA->setModel(model);
gridB->setModel(model);   // 共用模型

AuGridSyncGroup *sync = new AuGridSyncGroup(this);

sync->addGrid(gridA);   // 左表
sync->addGrid(gridB);   // 右表

sync->setFeatures(AuGridSyncGroup::SyncAll);
// 也可只开一项：SyncScroll 滚动偏移 / SyncColumnWidths 列宽

sync->syncFrom(gridA);  // 初始对齐（一次性把 source 状态推给其他成员）

// 成员用 QPointer 持有，控件销毁时自动失效；
// setSyncEnabled(false) 可临时暂停同步。
)snip";
        pages.append(page);
    }

    // ============ 26. 尺寸与主题 ============
    {
        FeaturePage page;
        page.title = QStringLiteral(u"尺寸与主题");
        page.description = QStringLiteral(
            u"setSizeMode 切换小 / 中 / 大三档密度（内部克隆样式）；"
            u"setStyle 设置独立亮 / 暗主题（控件接管所有权），传 nullptr 恢复跟随全局。");
        page.createDemo = []() {
            AuGridModel *model = nullptr;
            AuGridWidget *grid = newFlatGrid(20, &model);

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
            using SizeMode = AuGridStyle::SizeMode;
            QObject::connect(smallBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setSizeMode(SizeMode::Small); });
            QObject::connect(mediumBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setSizeMode(SizeMode::Medium); });
            QObject::connect(largeBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setSizeMode(SizeMode::Large); });
            QObject::connect(darkBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setStyle(AuGridStyle::createDarkStyle()); });
            QObject::connect(lightBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setStyle(AuGridStyle::createLightStyle()); });
            QObject::connect(globalBtn, &QPushButton::clicked, grid,
                             [grid]() { grid->setStyle(nullptr); });
            return PageHelpers::composeDemo(controls, grid);
        };
        page.code = R"snip(
// 演示数据与“最小用法”一致：四列 × 20 行

// 密度档位：小 / 中 / 大（对应演示按钮，内部克隆样式并调整尺寸预设）
grid->setSizeMode(AuGridStyle::SizeMode::Large);
grid->sizeMode();

// 独立主题：亮 / 暗工厂（控件接管样式所有权并负责销毁）
grid->setStyle(AuGridStyle::createDarkStyle());
grid->setStyle(AuGridStyle::createLightStyle());

grid->setStyle(nullptr);   // 恢复跟随全局 AuGridStyle::globalStyle()

// 全局设置一次，所有未显式 setStyle 的控件自动生效（本程序 main.cpp 即如此）：
AuGridStyle::setGlobalStyle(AuGridStyle::createLightStyle());
)snip";
        pages.append(page);
    }

    return pages;
}
