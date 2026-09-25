#include "AgDatePicker.h"

#include "AgPopupPanel.h"
#include "AgDatePickerPanel.h"
#include "AgDateRangePanel.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMargins>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>

namespace {
/// 范围模式编辑框文本的分隔符与默认占位段。
const QString kRangeSeparator = QStringLiteral(u" ~ ");
const QString kRangeStartPlaceholder = QStringLiteral(u"开始日期");
const QString kRangeEndPlaceholder = QStringLiteral(u"结束日期");
} // namespace

// ---------------------------------------------------------------------------
// 图标绘制（日历 / 清除），随编辑框尺寸与样式配色缩放

namespace {

/// 日历图标：圆角外框 + 顶部装订线圈 + 表头分隔线 + 两个小圆点。
void drawCalendarIcon(QPainter &painter, const QRect &rect, const QColor &color)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    const qreal bodyLeft = rect.left() + rect.width() * 0.08;
    const qreal bodyW = rect.width() * 0.84;
    const qreal bodyTop = rect.top() + rect.height() * 0.16;
    const qreal bodyH = rect.height() * 0.78;
    const QRectF body(bodyLeft, bodyTop, bodyW, bodyH);
    const qreal radius = qMax(1.0, bodyW * 0.12);

    // 装订线圈（先画，压在边框下方）
    QPen ringPen(color, bodyW * 0.09, Qt::SolidLine, Qt::RoundCap);
    painter.setPen(ringPen);
    const qreal ringTop = bodyTop - bodyH * 0.14;
    const qreal ringBottom = bodyTop + bodyH * 0.18;
    painter.drawLine(QPointF(bodyLeft + bodyW * 0.26, ringTop),
                     QPointF(bodyLeft + bodyW * 0.26, ringBottom));
    painter.drawLine(QPointF(bodyLeft + bodyW * 0.74, ringTop),
                     QPointF(bodyLeft + bodyW * 0.74, ringBottom));

    // 外框
    QPen pen(color, 1.4);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(body, radius, radius);

    // 表头分隔线
    const qreal headerY = bodyTop + bodyH * 0.3;
    painter.drawLine(QPointF(bodyLeft + 1.0, headerY),
                     QPointF(bodyLeft + bodyW - 1.0, headerY));

    // 两个小圆点
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    const qreal dotR = bodyW * 0.06;
    painter.drawEllipse(QPointF(bodyLeft + bodyW * 0.36, bodyTop + bodyH * 0.62), dotR, dotR);
    painter.drawEllipse(QPointF(bodyLeft + bodyW * 0.64, bodyTop + bodyH * 0.62), dotR, dotR);

    painter.restore();
}

/// 清除图标：实心圆圈 + ×。
void drawClearIcon(QPainter &painter, const QRect &rect, const QColor &circleColor,
                   const QColor &crossColor)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);

    const QPointF center = QRectF(rect).center();
    const qreal radius = qMin(rect.width(), rect.height()) / 2.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(circleColor);
    painter.drawEllipse(center, radius, radius);

    QPen pen(crossColor, qMax(1.4, radius * 0.28), Qt::SolidLine, Qt::RoundCap);
    painter.setPen(pen);
    const qreal arm = radius * 0.5;
    painter.drawLine(center + QPointF(-arm, -arm), center + QPointF(arm, arm));
    painter.drawLine(center + QPointF(-arm, arm), center + QPointF(arm, -arm));

    painter.restore();
}

} // namespace

// ---------------------------------------------------------------------------

AgDatePicker::AgDatePicker(QWidget *parent)
    : QWidget(parent)
    , m_lineEdit(new QLineEdit(this))
    , m_panel(nullptr)
    , m_content(nullptr)
    , m_ownStyle(nullptr)
    , m_styleFromSizeMode(false)
    , m_overrideSizeMode(AgDatePickerStyle::SizeMode::Medium)
    , m_formatCustom(false)
    , m_selectionMode(SelectDay)
    , m_editorWidthMode(WidthFitContent)
    , m_picking(false)
    , m_rangePlaceholderAuto(false)
    , m_rangeStartPlaceholder(kRangeStartPlaceholder)
    , m_rangeEndPlaceholder(kRangeEndPlaceholder)
    , m_clearButtonEnabled(true)
    , m_footerVisible(true)
    , m_footerWidget(nullptr)
    , m_areaHovered(false)
    , m_lineHovered(false)
    , m_iconHovered(false)
    , m_preferredHeight(0)
{
    setFocusPolicy(Qt::NoFocus); // 键盘交互由内部的行编辑框承载
    setMouseTracking(true);
    // 固定尺寸：不随父布局拉伸；默认宽高来自 sizeHint（宽度按显示格式计算），
    // 用户可用 setFixedWidth() / setMinimumWidth() / setEditorHeight() 等调整
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_displayFormat = defaultFormatForMode(m_selectionMode);

    m_lineEdit->setFrame(false);
    m_lineEdit->setReadOnly(true);
    m_lineEdit->setFocusPolicy(Qt::StrongFocus);
    m_lineEdit->setAttribute(Qt::WA_Hover, true);
    m_lineEdit->setStyleSheet(QStringLiteral("QLineEdit { background: transparent; border: none; }"));
    m_lineEdit->installEventFilter(this);

    // 范围模式下划线位置动画（开始日期段 <-> 结束日期段）
    m_underlineAnim = new QVariantAnimation(this);
    m_underlineAnim->setDuration(160);
    m_underlineAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_underlineAnim, &QVariantAnimation::valueChanged, this,
            [this](const QVariant &value) {
                m_underlineRect = value.toRectF();
                update();
            });

    // 跟随全局样式：未显式 setStyle / 未产生派生样式前注册观察者
    AgDatePickerStyle::registerGlobalObserver(this);

    relayoutEditor();
    applyStyleToWidgets();
}

AgDatePicker::~AgDatePicker()
{
    AgDatePickerStyle::unregisterGlobalObserver(this);
    // 面板仍处于弹出 / 收起动画中就销毁控件时，面板基类析构阶段 dismiss() 发出的
    // closed 会在半析构状态下回调本控件的槽（切页 / 父窗口销毁时存在隐患）。
    // 这里先断开面板回调并安全收起。
    if (m_panel) {
        QObject::disconnect(m_panel, nullptr, this, nullptr);
        m_panel->dismiss();
    }
    delete m_ownStyle;
}

// ---------------------------------------------------------------------------
// 当前值

QDate AgDatePicker::date() const
{
    return m_date;
}

bool AgDatePicker::hasSelection() const
{
    if (m_selectionMode == SelectDateRange)
        return m_date.isValid() || m_endDate.isValid();
    return m_date.isValid();
}

QDate AgDatePicker::startDate() const
{
    return m_date;
}

QDate AgDatePicker::endDate() const
{
    return m_endDate;
}

void AgDatePicker::setDate(const QDate &date)
{
    if (m_selectionMode == SelectDateRange) {
        setStartDate(date); // 范围模式下 setDate 只设置开始日期
        return;
    }
    const QDate normalized = date.isValid() ? date : QDate();
    if (normalized == m_date)
        return;
    m_date = normalized;
    updateEditorText();
    syncPanelToValue(); // 自定义 footer 等程序设值：展开中的面板同步高亮新日期
    emit dateChanged(m_date);
    update();
}

void AgDatePicker::setSelectedRange(const QDate &start, const QDate &end)
{
    QDate s = start.isValid() ? start : QDate();
    QDate e = end.isValid() ? end : QDate();
    if (!s.isValid())
        e = QDate(); // 开始为空则整体视为空
    if (s.isValid() && e.isValid() && e < s)
        qSwap(s, e);
    const bool changed = (s != m_date) || (e != m_endDate);
    m_date = s;
    m_endDate = e;
    updateEditorText();
    syncPanelToValue(); // 程序设值：展开中的面板同步范围与定位
    if (changed) {
        emit dateChanged(m_date);
        emit dateRangeChanged(m_date, m_endDate);
    }
    update();
}

void AgDatePicker::setStartDate(const QDate &start)
{
    // 结束日期早于新开始日期时清空结束日期
    setSelectedRange(start, (start.isValid() && m_endDate.isValid() && m_endDate < start)
                                ? QDate() : m_endDate);
}

void AgDatePicker::setEndDate(const QDate &end)
{
    if (!m_date.isValid()) {
        // 无开始日期时视为整体设置（仅有结束日期则存为开始）
        setSelectedRange(end, QDate());
        return;
    }
    setSelectedRange(m_date, end);
}

void AgDatePicker::clear()
{
    const bool had = m_selectionMode == SelectDateRange
                         ? (m_date.isValid() || m_endDate.isValid())
                         : m_date.isValid();
    if (m_selectionMode == SelectDateRange)
        setSelectedRange(QDate(), QDate());
    else
        setDate(QDate());
    if (had) {
        emit cleared();
        update();
    }
}

// ---------------------------------------------------------------------------
// 显示格式

QString AgDatePicker::displayFormat() const
{
    return m_displayFormat;
}

void AgDatePicker::setDisplayFormat(const QString &format)
{
    if (format.isEmpty() || format == m_displayFormat)
        return;
    m_displayFormat = format;
    m_formatCustom = true;
    updateEditorText();
    updateGeometry(); // 固定宽度按格式计算，格式变化后同步
    update();
}

QString AgDatePicker::defaultFormatForMode(SelectionMode mode)
{
    switch (mode) {
    case SelectMonth: return QStringLiteral("yyyy-MM");
    case SelectYear: return QStringLiteral("yyyy");
    default: return QStringLiteral("yyyy-MM-dd"); // SelectDay / SelectDateRange
    }
}

// ---------------------------------------------------------------------------
// 选择粒度

AgDatePicker::SelectionMode AgDatePicker::selectionMode() const
{
    return m_selectionMode;
}

void AgDatePicker::setSelectionMode(SelectionMode mode)
{
    if (m_selectionMode == mode)
        return;
    // 离开范围模式时清除自动设置的范围占位文本
    if (m_selectionMode == SelectDateRange && m_rangePlaceholderAuto) {
        m_lineEdit->setPlaceholderText(QString());
        m_rangePlaceholderAuto = false;
    }
    m_selectionMode = mode;
    m_picking = false;
    m_pickStart = QDate();
    m_hoverDate = QDate();
    m_underlineAnim->stop();
    m_underlineRect = QRectF();
    if (!m_formatCustom)
        m_displayFormat = defaultFormatForMode(mode); // 默认格式随粒度切换
    if (mode == SelectDateRange && m_lineEdit->placeholderText().isEmpty()) {
        m_lineEdit->setPlaceholderText(m_rangeStartPlaceholder + kRangeSeparator
                                       + m_rangeEndPlaceholder);
        m_rangePlaceholderAuto = true;
    }
    updateEditorText();
    updateGeometry(); // 默认格式 / 范围文本变化会改变固定宽度
    if (m_dayContent && mode != SelectDateRange)
        m_dayContent->setSelectionMode(mode);
    update();
}

// ---------------------------------------------------------------------------
// 可选范围

QDate AgDatePicker::minimumDate() const
{
    return m_minimumDate;
}

QDate AgDatePicker::maximumDate() const
{
    return m_maximumDate;
}

bool AgDatePicker::isDateSelectable(const QDate &date) const
{
    if (!date.isValid())
        return false;
    if (m_minimumDate.isValid() && date < m_minimumDate)
        return false;
    if (m_maximumDate.isValid() && date > m_maximumDate)
        return false;
    return true;
}

void AgDatePicker::setMinimumDate(const QDate &min)
{
    setFixedRange(min, m_maximumDate);
}

void AgDatePicker::setMaximumDate(const QDate &max)
{
    setFixedRange(m_minimumDate, max);
}

void AgDatePicker::setDateRange(const QDate &min, const QDate &max)
{
    setFixedRange(min, max);
}

void AgDatePicker::setFixedRange(const QDate &min, const QDate &max)
{
    m_minimumDate = min.isValid() ? min : QDate();
    m_maximumDate = max.isValid() ? max : QDate();
    if (m_minimumDate.isValid() && m_maximumDate.isValid() && m_minimumDate > m_maximumDate)
        qSwap(m_minimumDate, m_maximumDate);
    if (m_dayContent) {
        m_dayContent->setDateRange(m_minimumDate, m_maximumDate);
        m_dayContent->update();
    }
    if (m_rangeContent) {
        m_rangeContent->setDateRange(m_minimumDate, m_maximumDate);
        m_rangeContent->update();
    }
}

// ---------------------------------------------------------------------------
// 编辑框

QString AgDatePicker::placeholderText() const
{
    return m_lineEdit->placeholderText();
}

void AgDatePicker::setPlaceholderText(const QString &text)
{
    m_lineEdit->setPlaceholderText(text);
}

void AgDatePicker::setRangePlaceholderText(const QString &start, const QString &end)
{
    m_rangeStartPlaceholder = start.isEmpty() ? kRangeStartPlaceholder : start;
    m_rangeEndPlaceholder = end.isEmpty() ? kRangeEndPlaceholder : end;
    // 编辑框整体为空的占位文本随之前同步（仅覆盖自动设置的占位，用户显式设置的不动）
    if (m_selectionMode == SelectDateRange && m_rangePlaceholderAuto) {
        m_lineEdit->setPlaceholderText(m_rangeStartPlaceholder + kRangeSeparator
                                       + m_rangeEndPlaceholder);
    }
    updateGeometry(); // 固定宽度按占位文本计算
    update();
}

QString AgDatePicker::rangeStartPlaceholder() const
{
    return m_rangeStartPlaceholder;
}

QString AgDatePicker::rangeEndPlaceholder() const
{
    return m_rangeEndPlaceholder;
}

int AgDatePicker::editorHeight() const
{
    return effectiveEditorHeight();
}

void AgDatePicker::setEditorHeight(int height)
{
    ensureOwnStyle(); // 从全局样式派生一份私有样式（保持跟随全局主题）
    m_ownStyle->setEditorHeight(qMax(16, height));
    applyStyleToWidgets();
}

// ---------------------------------------------------------------------------
// 宽度模式

AgDatePicker::EditorWidthMode AgDatePicker::editorWidthMode() const
{
    return m_editorWidthMode;
}

void AgDatePicker::setEditorWidthMode(EditorWidthMode mode)
{
    if (m_editorWidthMode == mode)
        return;
    m_editorWidthMode = mode;
    // WidthFitContent：固定尺寸（默认，宽度按显示格式自适应）；
    // WidthStretch：水平跟随布局拉伸，最小宽度取格式自适应宽度保证文本不截断。
    // 两种模式下都可用 setFixedWidth 手动固定；弹层面板宽度始终固定不变
    setSizePolicy(mode == WidthStretch ? QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed)
                                       : QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed));
    updateGeometry();
    update();
}

// ---------------------------------------------------------------------------
// 清除按钮

bool AgDatePicker::isClearButtonEnabled() const
{
    return m_clearButtonEnabled;
}

void AgDatePicker::setClearButtonEnabled(bool enabled)
{
    if (m_clearButtonEnabled == enabled)
        return;
    m_clearButtonEnabled = enabled;
    update();
}

// ---------------------------------------------------------------------------
// footer

bool AgDatePicker::isFooterVisible() const
{
    return m_footerVisible;
}

void AgDatePicker::setFooterVisible(bool visible)
{
    if (m_footerVisible == visible)
        return;
    m_footerVisible = visible;
    pushFooterConfig();
    update();
}

void AgDatePicker::setFooterLink(FooterSlot slot, const QString &text,
                                 const std::function<void()> &callback)
{
    if (slot < 0 || slot > 2)
        return;
    m_footerLinks[slot].text = text;
    m_footerLinks[slot].callback = callback;
    m_footerLinks[slot].custom = true;
    pushFooterConfig();
}

void AgDatePicker::resetFooterLink(FooterSlot slot)
{
    if (slot < 0 || slot > 2)
        return;
    m_footerLinks[slot] = FooterLinkData();
    pushFooterConfig();
}

QWidget *AgDatePicker::footerWidget() const
{
    return m_footerWidget;
}

void AgDatePicker::setFooterWidget(QWidget *widget)
{
    if (m_footerWidget == widget)
        return;
    if (m_footerWidget)
        m_footerWidget->deleteLater();
    m_footerWidget = widget;
    if (m_footerWidget) {
        m_footerWidget->setParent(this);
        m_footerWidget->hide(); // 弹出时由面板接管布局
    }
    pushFooterConfig();
}

void AgDatePicker::pushFooterConfig()
{
    if (m_dayContent) {
        m_dayContent->setFooterVisible(m_footerVisible);
        for (int slot = 0; slot < 3; ++slot)
            m_dayContent->setFooterLink(slot, m_footerLinks[slot].text,
                                        m_footerLinks[slot].callback, m_footerLinks[slot].custom);
        m_dayContent->setFooterWidget(m_footerWidget);
    }
    if (m_rangeContent) {
        // 范围面板（双日历）：同样支持三槽位自定义链接；无内置快捷项、
        // 不支持整体部件替换（footerWidget 仅单日历生效），
        // 未配置任何链接时不出空 footer 条
        m_rangeContent->setFooterVisible(m_footerVisible);
        for (int slot = 0; slot < 3; ++slot)
            m_rangeContent->setFooterLink(slot, m_footerLinks[slot].text,
                                          m_footerLinks[slot].callback, m_footerLinks[slot].custom);
    }
}

// ---------------------------------------------------------------------------
// 样式 / 尺寸

AgDatePickerStyle *AgDatePicker::style() const
{
    return m_ownStyle ? m_ownStyle : AgDatePickerStyle::globalStyle();
}

void AgDatePicker::setStyle(AgDatePickerStyle *style)
{
    if (m_ownStyle == style)
        return;
    delete m_ownStyle;
    m_ownStyle = style;
    m_styleFromSizeMode = false; // 显式样式完全接管，不再跟随全局主题
    if (m_ownStyle) {
        AgDatePickerStyle::unregisterGlobalObserver(this);
    } else {
        AgDatePickerStyle::registerGlobalObserver(this);
    }
    applyStyleToWidgets();
}

AgDatePickerStyle::SizeMode AgDatePicker::sizeMode() const
{
    return style()->sizeMode();
}

void AgDatePicker::setSizeMode(AgDatePickerStyle::SizeMode mode)
{
    ensureOwnStyle();
    m_overrideSizeMode = mode;
    m_ownStyle->setSizeMode(mode);
    applyStyleToWidgets();
}

void AgDatePicker::ensureOwnStyle()
{
    if (m_ownStyle)
        return;
    // 首次调整：从全局样式派生一份私有样式，并保持跟随全局主题
    // （主题切换时重新继承配色，尺寸档位 / 高度微调保持不变）
    m_ownStyle = new AgDatePickerStyle(*AgDatePickerStyle::globalStyle());
    m_styleFromSizeMode = true;
}

void AgDatePicker::onGlobalStyleChanged()
{
    if (m_styleFromSizeMode && m_ownStyle) {
        // 派生样式：跟随全局主题重新继承配色，尺寸档位与手动微调保持不变
        // （注意：此时对 style() 的颜色微调会被覆盖）
        const int editorHeight = m_ownStyle->editorHeight();
        *m_ownStyle = *AgDatePickerStyle::globalStyle();
        m_ownStyle->setSizeMode(m_overrideSizeMode);
        m_ownStyle->setEditorHeight(editorHeight);
        applyStyleToWidgets();
        return;
    }
    if (!m_ownStyle)
        applyStyleToWidgets();
}

// ---------------------------------------------------------------------------
// 面板

bool AgDatePicker::isPopupVisible() const
{
    return m_panel && m_panel->isVisible();
}

void AgDatePicker::showPopup()
{
    if (!isEnabled())
        return;
    ensurePopup();
    ensurePopupContent();
    // 键盘交互（Esc 关闭等）的前提：面板展开时焦点落在行编辑框
    m_lineEdit->setFocus(Qt::OtherFocusReason);
    if (m_panel->isVisible()) {
        m_panel->raise();
        return;
    }

    emit popupAboutToShow();

    m_picking = false;
    m_pickStart = QDate();
    m_hoverDate = QDate();

    if (m_selectionMode == SelectDateRange) {
        m_rangeContent->setStyle(style());
        m_rangeContent->setDateRange(m_minimumDate, m_maximumDate);
        m_rangeContent->setSelectedRange(m_date, m_endDate);
        pushFooterConfig();
        m_rangeContent->resetView();
        m_panel->popupOver(this, m_rangeContent->sizeHint());
        // 展开期间提示当前选择的是开始日期（无动画直接就位）
        animateUnderlineTo(false);
    } else {
        m_dayContent->setStyle(style());
        m_dayContent->setSelectionMode(m_selectionMode);
        m_dayContent->setSelectedDate(m_date);
        m_dayContent->setDateRange(m_minimumDate, m_maximumDate);
        m_dayContent->resetView();
        pushFooterConfig();
        m_panel->popupOver(this, m_dayContent->sizeHint());
    }
    m_lineEdit->setFocus(Qt::OtherFocusReason);
    update();
}

void AgDatePicker::hidePopup()
{
    if (m_panel && m_panel->isVisible())
        m_panel->closePopup();
}

void AgDatePicker::ensurePopup()
{
    if (m_panel)
        return;

    m_panel = new AgPopupPanel(this);
    // 面板宽度固定（按内容 sizeHint），不随编辑框宽度拉伸；
    // Esc 不直接收起：年 / 月选择页先返回上一级视图，已在初始页才收起
    m_panel->setMatchOwnerWidth(false);
    m_panel->setEscapeCloses(false);

    connect(m_panel, &AgPopupPanel::escapePressed, this, [this]() {
        handleEscape();
    });
    connect(m_panel, &AgPopupPanel::aboutToClose, this, [this]() {
        // 范围选择未完成即收起（Esc / 点击外部）：撤销本次选择，恢复已提交范围
        if (m_selectionMode == SelectDateRange && m_picking) {
            m_picking = false;
            m_pickStart = QDate();
            m_hoverDate = QDate();
            if (m_rangeContent)
                m_rangeContent->endPicking();
            updateEditorText();
        }
        // 清掉下划线并立即重绘：收起动画期间与结束后编辑框都不能残留旧下划线
        //（此时控件没有其它重绘契机，漏掉 update() 会留下残影）
        m_underlineAnim->stop();
        if (m_underlineRect.isValid()) {
            m_underlineRect = QRectF();
            update();
        }
        emit popupAboutToHide();
    });
    connect(m_panel, &AgPopupPanel::closed, this, [this]() {
        update(); // 保险：面板完全隐藏后再刷一遍，清除收起过程中的任何绘制残留
        emit popupHidden();
    });
    connect(m_panel, &AgPopupPanel::shown, this, [this]() {
        emit popupShown();
    });

    applyStyleToWidgets();
}

void AgDatePicker::ensurePopupContent()
{
    const bool range = (m_selectionMode == SelectDateRange);
    if (range) {
        if (m_rangeContent && m_content == m_rangeContent.data())
            return;
        m_rangeContent = new AgDateRangePanel();
        connect(m_rangeContent, &AgDateRangePanel::dateSelected, this,
                &AgDatePicker::handlePanelDateSelected);
        connect(m_rangeContent, &AgDateRangePanel::dateHovered, this,
                [this](const QDate &date) {
                    // 仅响应有效日期：掠过格子间隙 / 离开面板时保持当前预览，
                    // 避免范围带随鼠标移动闪烁
                    if (!m_picking || !date.isValid())
                        return;
                    m_hoverDate = date;
                    updateRangePreview();
                });
        m_panel->setContentWidget(m_rangeContent); // 旧内容（单日历）由面板销毁
        m_content = m_rangeContent.data();
    } else {
        if (m_dayContent && m_content == m_dayContent.data())
            return;
        m_dayContent = new AgDatePickerPanel();
        connect(m_dayContent, &AgDatePickerPanel::dateSelected, this,
                &AgDatePicker::handlePanelDateSelected);
        m_panel->setContentWidget(m_dayContent);
        m_content = m_dayContent.data();
    }
    applyStyleToWidgets();
}

void AgDatePicker::handlePanelDateSelected(const QDate &date)
{
    if (m_selectionMode == SelectDateRange) {
        if (!m_picking) {
            // 第一次选择：起点（终点待选，起点前后日期均可选）
            m_picking = true;
            m_pickStart = date;
            m_hoverDate = QDate();
            m_rangeContent->beginPicking(date);
            m_rangeContent->setPreviewEnd(date);
            updateEditorText();          // "开始日期 ~ "
            animateUnderlineTo(true);    // 下划线滑到结束日期下方
        } else {
            // 第二次选择：终点；选了起点之前的日期时自动交换（第一次选择转为结束）
            QDate start = m_pickStart;
            QDate end = date;
            if (end < start)
                qSwap(start, end);
            m_picking = false;
            m_pickStart = QDate();
            m_hoverDate = QDate();
            commitRange(start, end);
            hidePopup();
        }
        return;
    }
    setDate(date);
    emit activated(date);
    hidePopup();
}

void AgDatePicker::commitRange(const QDate &start, const QDate &end)
{
    const bool changed = (start != m_date) || (end != m_endDate);
    m_date = start;
    m_endDate = end;
    updateEditorText();
    if (changed) {
        emit dateChanged(m_date);
        emit dateRangeChanged(m_date, m_endDate);
    }
    emit activated(m_date);
    emit rangeActivated(m_date, m_endDate);
}

void AgDatePicker::handleEscape()
{
    if (m_selectionMode == SelectDateRange) {
        hidePopup(); // 双日历无上级视图，直接收起（未完成的选点在收起回调中撤销）
        return;
    }
    if (!m_dayContent || !m_dayContent->goBack())
        hidePopup(); // 已在初始页：收起面板
}

// ---------------------------------------------------------------------------
// 范围模式交互

void AgDatePicker::updateRangePreview()
{
    if (!m_rangeContent)
        return;
    // 预览终点：悬停日期优先，否则键盘焦点日期
    const QDate preview = m_hoverDate.isValid() ? m_hoverDate
                                                : m_rangeContent->focusDate();
    m_rangeContent->setPreviewEnd(preview);
}

qreal AgDatePicker::rangeEditorTextX() const
{
    // 文本起点：借输入法光标几何取得（QLineEdit::cursorRect 为 protected），
    // 临时把光标移到 0 位再还原
    const int savedPos = m_lineEdit->cursorPosition();
    m_lineEdit->setCursorPosition(0);
    const QRect cursor = m_lineEdit->inputMethodQuery(Qt::ImCursorRectangle).toRect();
    m_lineEdit->setCursorPosition(savedPos);
    if (!cursor.isValid())
        return -1;
    return m_lineEdit->geometry().left() + cursor.left();
}

QRectF AgDatePicker::rangeUnderlineRect(bool endSegment) const
{
    const qreal x0 = rangeEditorTextX();
    if (x0 < 0)
        return QRectF();
    const QFontMetrics fm(m_lineEdit->font());
    const QDate start = m_picking ? m_pickStart : m_date;
    const QDate end = m_picking ? QDate() : m_endDate;
    // 未选侧用占位文本宽度估算
    const QString startText = start.isValid() ? start.toString(m_displayFormat)
                                              : m_rangeStartPlaceholder;
    const QString endText = end.isValid() ? end.toString(m_displayFormat)
                                          : m_rangeEndPlaceholder;
    const QString fallback = QDate(8888, 12, 31).toString(m_displayFormat);
    const int wStart = fm.horizontalAdvance(startText.isEmpty() ? fallback : startText);
    const int wSep = fm.horizontalAdvance(kRangeSeparator);
    const int wEnd = fm.horizontalAdvance(endText.isEmpty() ? fallback : endText);
    const QRectF inner = QRectF(innerRect()).adjusted(2.0, 0, -2.0, 0);
    const qreal h = 2.5;
    const qreal y = inner.bottom() - h - 1.0;
    QRectF segment = endSegment ? QRectF(x0 + wStart + wSep, y, wEnd, h)
                                : QRectF(x0, y, wStart, h);
    segment = segment.intersected(QRectF(inner.left(), y, inner.width(), h));
    return segment.isValid() ? segment : QRectF();
}

void AgDatePicker::animateUnderlineTo(bool endSegment)
{
    const QRectF target = rangeUnderlineRect(endSegment);
    if (!target.isValid()) {
        m_underlineAnim->stop();
        m_underlineRect = QRectF();
        update();
        return;
    }
    // 面板刚展开（或位置尚未初始化）时直接就位，选起点后才播放移动动画
    if (!m_underlineRect.isValid() || !isPopupVisible()) {
        m_underlineAnim->stop();
        m_underlineRect = target;
        update();
        return;
    }
    if (m_underlineRect == target)
        return;
    m_underlineAnim->stop();
    m_underlineAnim->setStartValue(m_underlineRect);
    m_underlineAnim->setEndValue(target);
    m_underlineAnim->start();
}

void AgDatePicker::navigatePopup(int key)
{
    if (m_selectionMode == SelectDateRange) {
        if (m_rangeContent && m_rangeContent->navigateKey(key) && m_picking) {
            m_hoverDate = QDate(); // 键盘接管：预览终点改随焦点日期
            updateRangePreview();
        }
    } else if (m_dayContent) {
        m_dayContent->navigateKey(key);
    }
}

void AgDatePicker::activatePopupFocus()
{
    if (m_selectionMode == SelectDateRange) {
        if (m_rangeContent)
            m_rangeContent->activateFocus();
    } else if (m_dayContent) {
        m_dayContent->activateFocus();
    }
}

// ---------------------------------------------------------------------------
// 事件处理

void AgDatePicker::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    AgDatePickerStyle *s = style();

    const bool enabled = isEnabled();
    const bool active = isPopupVisible() || m_lineEdit->hasFocus();
    const bool focusVisible = enabled && m_lineEdit->hasFocus(); // Tab / 点击获得的键盘焦点
    const bool hovered = m_areaHovered || m_lineHovered;
    const int border = s->editorBorderWidth();
    const qreal radius = s->editorBorderRadius();

    const QColor background = enabled ? s->editorBackgroundColor()
                                      : s->editorDisabledBackgroundColor();
    QColor borderColor;
    if (!enabled)
        borderColor = s->editorDisabledBorderColor();
    else if (focusVisible)
        borderColor = s->editorFocusBorderColor().isValid()
                          ? s->editorFocusBorderColor() : s->editorActiveBorderColor();
    else if (active)
        borderColor = s->editorActiveBorderColor();
    else if (hovered)
        borderColor = s->editorHoverBorderColor();
    else
        borderColor = s->editorBorderColor();

    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(rect(), radius, radius);

    // 键盘焦点淡色底（内部整体着色，Tab 切换时一眼可见）
    if (focusVisible && s->editorFocusBackgroundColor().isValid()) {
        painter.setBrush(s->editorFocusBackgroundColor());
        const qreal inset = border;
        painter.drawRoundedRect(QRectF(rect()).adjusted(inset, inset, -inset, -inset),
                                qMax<qreal>(0.0, radius - border),
                                qMax<qreal>(0.0, radius - border));
    }

    if (border > 0 && borderColor.isValid()) {
        QPen pen(borderColor, qreal(border));
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        const qreal inset = border / 2.0;
        painter.drawRoundedRect(QRectF(rect()).adjusted(inset, inset, -inset, -inset),
                                radius, radius);
    }

    // 右侧图标：日历 <-> 清除（悬停且已选值时）
    const QRect slot = iconSlotRect();
    const int iconSize = qMin(slot.width(), qMin(slot.height(), effectiveEditorHeight())) - 8;
    if (iconSize >= 6) {
        const QRect iconRect(slot.center().x() - iconSize / 2, slot.center().y() - iconSize / 2,
                             iconSize, iconSize);
        if (clearIconActive()) {
            const QColor circle = m_iconHovered ? s->clearIconCircleHoverColor()
                                                : s->clearIconCircleColor();
            drawClearIcon(painter, iconRect, enabled ? circle : s->editorDisabledForegroundColor(),
                          enabled ? s->clearIconCrossColor() : s->editorBackgroundColor());
        } else {
            drawCalendarIcon(painter, iconRect,
                             enabled ? s->calendarIconColor() : s->editorDisabledForegroundColor());
        }
    }

    // 范围模式：起始已选而结束为空时，在分隔符后灰显结束占位文本（提示待选终点）
    if (m_selectionMode == SelectDateRange && m_lineEdit->text().contains(kRangeSeparator)) {
        const QDate start = m_picking ? m_pickStart : m_date;
        const QDate end = m_picking ? QDate() : m_endDate;
        if (start.isValid() && !end.isValid()) {
            const qreal x0 = rangeEditorTextX();
            if (x0 >= 0) {
                const QFontMetrics fm(m_lineEdit->font());
                const int wStart = fm.horizontalAdvance(start.toString(m_displayFormat));
                const int wSep = fm.horizontalAdvance(kRangeSeparator);
                const int ghostX = qRound(x0) + wStart + wSep;
                const int right = iconSlotRect().left() - 4;
                if (right > ghostX) {
                    painter.setFont(m_lineEdit->font());
                    painter.setPen(s->editorPlaceholderColor());
                    painter.drawText(QRect(ghostX, m_lineEdit->geometry().top(),
                                           right - ghostX, m_lineEdit->geometry().height()),
                                     Qt::AlignVCenter | Qt::AlignLeft, m_rangeEndPlaceholder);
                }
            }
        }
    }

    // 范围模式：面板展开期间在当前选择侧（开始 / 结束日期）文本下方绘制主色调
    // 下划线，提示本次点击落到哪一侧（位置由 m_underlineAnim 驱动）
    if (m_selectionMode == SelectDateRange && isPopupVisible() && m_underlineRect.isValid()
        && s->editorActiveBorderColor().isValid()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(s->editorActiveBorderColor());
        const QRectF r = m_underlineRect;
        painter.drawRoundedRect(r, r.height() / 2.0, r.height() / 2.0);
    }
}

void AgDatePicker::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayoutEditor();
}

void AgDatePicker::enterEvent(QEvent * /*event*/)
{
    // 进入控件本体（行编辑框悬停由 eventFilter 的 HoverEnter/Leave 单独跟踪：
    // Qt 中光标移入子控件时父控件会收到 Leave，两侧状态需合并判断）
    if (!m_areaHovered) {
        m_areaHovered = true;
        update();
    }
}

void AgDatePicker::mousePressEvent(QMouseEvent *event)
{
    if (!isEnabled()) {
        event->ignore();
        return;
    }
    const QPoint pos = event->pos();

    if (iconSlotRect().contains(pos)) {
        if (clearIconActive()) {
            clear(); // 清除后不弹出面板
            return;
        }
        if (isPopupVisible()) {
            hidePopup();
            return;
        }
        m_lineEdit->setFocus();
        showPopup();
        return;
    }

    if (isPopupVisible()) {
        hidePopup(); // 展开时点击编辑区：收起
        return;
    }

    m_lineEdit->setFocus();
    showPopup();
}

void AgDatePicker::mouseMoveEvent(QMouseEvent *event)
{
    const bool iconHovered = iconSlotRect().contains(event->pos());
    if (iconHovered != m_iconHovered) {
        m_iconHovered = iconHovered;
        update();
    }
    updateIconCursor(event->pos());
    QWidget::mouseMoveEvent(event);
}

void AgDatePicker::leaveEvent(QEvent * /*event*/)
{
    m_areaHovered = false;
    m_iconHovered = false;
    setCursor(Qt::ArrowCursor);
    update();
}
void AgDatePicker::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
    case Qt::Key_Space:
    case Qt::Key_Down:
        if (!isPopupVisible()) {
            showPopup();
            return;
        }
        break;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void AgDatePicker::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled() && isPopupVisible())
            hidePopup();
        applyStyleToWidgets(); // 刷新禁用配色
    } else if (event->type() == QEvent::FontChange) {
        applyStyleToWidgets(); // 用户自定义字体同步到行编辑框与面板
    }
    QWidget::changeEvent(event);
}

bool AgDatePicker::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_lineEdit) {
        switch (event->type()) {
        case QEvent::MouseButtonPress:
            if (!isEnabled())
                break;
            if (isPopupVisible()) {
                hidePopup(); // 展开时点击文本框区域：收起
                break;
            }
            m_lineEdit->setFocus();
            showPopup();
            break;
        case QEvent::HoverEnter:
            if (!m_lineHovered) {
                m_lineHovered = true;
                update(); // 悬停切换清除图标
            }
            break;
        case QEvent::HoverLeave:
            if (m_lineHovered) {
                m_lineHovered = false;
                update();
            }
            break;
        case QEvent::FocusAboutToChange:
        case QEvent::FocusIn:
        case QEvent::FocusOut:
            // 焦点边框 / 淡色底随焦点状态立即刷新（失焦时没有鼠标移动等
            // 其它重绘契机，漏掉 update() 会留下焦点底色残影）
            update();
            break;
        case QEvent::KeyPress: {
            const QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            const int key = keyEvent->key();
            if (isPopupVisible()) {
                // 面板展开：方向键移动焦点日期、回车选中（Esc 由弹层接管逐级返回）
                switch (key) {
                case Qt::Key_Left:
                case Qt::Key_Right:
                case Qt::Key_Up:
                case Qt::Key_Down:
                case Qt::Key_PageUp:
                case Qt::Key_PageDown:
                    navigatePopup(key);
                    return true;
                case Qt::Key_Enter:
                case Qt::Key_Return:
                    activatePopupFocus();
                    return true;
                default:
                    break;
                }
                break;
            }
            // 回车必须在过滤器里消费掉：QLineEdit 不处理回车并任其向上传播，
            // 会触发所在 QDialog 的 autoDefault 按钮
            if (key == Qt::Key_Enter || key == Qt::Key_Return
                || key == Qt::Key_Space || key == Qt::Key_Down) {
                showPopup();
                return true;
            }
            break;
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QSize AgDatePicker::sizeHint() const
{
    // 固定尺寸：宽度按显示格式最长文本计算（不随内容与布局变化），高度随尺寸档位
    return QSize(fixedEditorWidth(), m_preferredHeight);
}

QSize AgDatePicker::minimumSizeHint() const
{
    // WidthStretch 模式下最小宽度取格式自适应宽度，布局再窄也不截断日期文本
    if (m_editorWidthMode == WidthStretch)
        return QSize(fixedEditorWidth(), m_preferredHeight);
    return QSize(60, m_preferredHeight);
}

// ---------------------------------------------------------------------------
// 私有实现

void AgDatePicker::applyStyleToWidgets()
{
    AgDatePickerStyle *s = style();

    // 字体：控件显式 setFont() 时优先于样式字体（WA_SetFont 语义）
    const QFont effectiveFont = testAttribute(Qt::WA_SetFont) ? font() : s->font();
    m_lineEdit->setFont(effectiveFont);

    // 文字颜色写入实例样式表（优先级高于任何祖先样式表的级联），禁用态单独着色
    m_lineEdit->setStyleSheet(QStringLiteral(
                                  "QLineEdit { background: transparent; border: none; color: %1; } "
                                  "QLineEdit:disabled { color: %2; }")
                                  .arg(s->editorForegroundColor().name(QColor::HexArgb),
                                       s->editorDisabledForegroundColor().name(QColor::HexArgb)));

    QPalette linePalette = m_lineEdit->palette();
    linePalette.setBrush(QPalette::Text, isEnabled() ? s->editorForegroundColor()
                                                     : s->editorDisabledForegroundColor());
    linePalette.setBrush(QPalette::PlaceholderText, s->editorPlaceholderColor());
    m_lineEdit->setPalette(linePalette);

    if (m_panel) {
        m_panel->setBackgroundColor(s->panelBackgroundColor());
        m_panel->setBorderColor(s->panelBorderColor());
        m_panel->setCornerRadius(s->panelBorderRadius());
        m_panel->setShadowColor(s->panelShadowColor());
        m_panel->setShadowRadius(s->panelShadowRadius());
    }
    if (m_dayContent)
        m_dayContent->setStyle(s);
    if (m_rangeContent)
        m_rangeContent->setStyle(s);

    relayoutEditor();
    update();
}

void AgDatePicker::relayoutEditor()
{
    AgDatePickerStyle *s = style();
    const int hPad = s->editorHorizontalPadding();
    const int slot = s->iconSlotWidth();
    const QRect inner = innerRect();

    m_preferredHeight = effectiveEditorHeight();
    m_lineEdit->setGeometry(inner.adjusted(hPad, 0, -slot, 0));

    if (minimumHeight() != m_preferredHeight)
        setMinimumHeight(m_preferredHeight);
    updateGeometry();
    update();
}

int AgDatePicker::effectiveEditorHeight() const
{
    return qMax(style()->editorHeight(), style()->editorBorderWidth() * 2 + 14);
}

int AgDatePicker::fixedEditorWidth() const
{
    // 以格式能呈现的最长文本（8888-12-31）估算宽度，保证日期完整显示不截断
    AgDatePickerStyle *s = style();
    const QFontMetrics fm(m_lineEdit->font());
    QString text = QDate(8888, 12, 31).toString(m_displayFormat);
    if (m_selectionMode == SelectDateRange) {
        // "开始 ~ 结束" 双侧最长文本；占位文本更长时按占位算
        const QString full = text + kRangeSeparator + text;
        const QString ph = m_rangeStartPlaceholder + kRangeSeparator + m_rangeEndPlaceholder;
        text = fm.horizontalAdvance(ph) > fm.horizontalAdvance(full) ? ph : full;
    }
    const int textWidth = fm.horizontalAdvance(text);
    const int width = 2 * s->editorBorderWidth() + 2 * s->editorHorizontalPadding()
                      + textWidth + s->iconSlotWidth() + 6;
    return qMax(80, width);
}

QRect AgDatePicker::innerRect() const
{
    const int border = style()->editorBorderWidth();
    return rect().adjusted(border, border, -border, -border);
}

QRect AgDatePicker::iconSlotRect() const
{
    const QRect inner = innerRect();
    const int slot = style()->iconSlotWidth();
    return QRect(inner.right() - slot + 1, inner.top(), slot, inner.height());
}

bool AgDatePicker::clearIconActive() const
{
    // 悬停编辑框（本体或行编辑框）且已有选中值时，日历图标切换为实心圆圈 ×
    return m_clearButtonEnabled && (m_areaHovered || m_lineHovered) && m_date.isValid();
}

void AgDatePicker::updateIconCursor(const QPoint &pos)
{
    setCursor(iconSlotRect().contains(pos) ? Qt::PointingHandCursor : Qt::ArrowCursor);
}

void AgDatePicker::syncPanelToValue()
{
    // 值被程序改变（自定义 footer 回调、外部逻辑）且面板展开时，面板同步：
    // 高亮文本框中的日期并翻到所在月
    if (!isPopupVisible())
        return;
    if (m_selectionMode == SelectDateRange) {
        if (m_rangeContent) {
            if (m_picking) {
                // 程序设值结束选点过程：撤销待选状态并清除下划线
                m_picking = false;
                m_pickStart = QDate();
                m_hoverDate = QDate();
                m_underlineAnim->stop();
                m_underlineRect = QRectF();
            }
            m_rangeContent->setSelectedRange(m_date, m_endDate);
            m_rangeContent->resetView();
        }
    } else if (m_dayContent) {
        m_dayContent->setSelectedDate(m_date);
        m_dayContent->resetView();
    }
}

void AgDatePicker::updateEditorText()
{
    QString text;
    if (m_selectionMode == SelectDateRange) {
        // 范围模式："开始日期 ~ 结束日期"；选终点过程中结束侧留空
        const QDate start = m_picking ? m_pickStart : m_date;
        const QDate end = m_picking ? QDate() : m_endDate;
        if (start.isValid()) {
            text = start.toString(m_displayFormat) + kRangeSeparator;
            if (end.isValid())
                text += end.toString(m_displayFormat);
        }
    } else {
        text = m_date.isValid() ? m_date.toString(m_displayFormat) : QString();
    }
    if (m_lineEdit->text() != text)
        m_lineEdit->setText(text);
}
