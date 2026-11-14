#include "AuMessageBox.h"
#include "AuMessageBoxStyle.h"
#include "AuPushButton.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QtMath>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

// 全局文案覆盖表（setDefaultButtonText 写入；键 = StandardButton 值）
static QHash<int, QString> s_globalButtonTexts;

// 标准按钮的展示顺序：主按钮靠左、取消类靠右（固定一套，不随平台主题切换）。
// Ok|Cancel → [确定][取消]，Yes|No|Cancel → [是][否][取消]，与 Windows 习惯一致。
static const AuMessageBox::StandardButton kDisplayOrder[] = {
    AuMessageBox::Yes, AuMessageBox::No, AuMessageBox::Ok,
    AuMessageBox::Cancel, AuMessageBox::Close
};

// ---- 内置图标（矢量绘制：彩色圆形徽章 + 白色符号，无资源文件依赖） ----

static void drawBadgeDot(QPainter &painter, qreal cx, qreal cy, qreal stroke)
{
    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 240));
    const qreal r = stroke * 0.62;
    painter.drawEllipse(QPointF(cx, cy), r, r);
    painter.restore();
}

static QPixmap drawIconBadge(AuMessageBox::Icon icon, int size, const QColor &color, qreal dpr)
{
    QPixmap pm(qRound(size * dpr), qRound(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(QRectF(0.5, 0.5, size - 1.0, size - 1.0));

    const qreal stroke = size * 0.10;
    QPen pen(QColor(255, 255, 255, 240), stroke);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const qreal cx = size / 2.0;

    switch (icon) {
    case AuMessageBox::Information:     // i：上点 + 竖条
        drawBadgeDot(p, cx, size * 0.30, stroke);
        p.drawLine(QPointF(cx, size * 0.47), QPointF(cx, size * 0.71));
        break;
    case AuMessageBox::Warning:         // !：竖条 + 下点
        p.drawLine(QPointF(cx, size * 0.26), QPointF(cx, size * 0.60));
        drawBadgeDot(p, cx, size * 0.74, stroke);
        break;
    case AuMessageBox::Critical:        // ✕：交叉双线
        p.drawLine(QPointF(size * 0.33, size * 0.33), QPointF(size * 0.67, size * 0.67));
        p.drawLine(QPointF(size * 0.67, size * 0.33), QPointF(size * 0.33, size * 0.67));
        break;
    case AuMessageBox::Question: {      // ?：上钩弧 + 内收尾 + 下点
        const qreal R = size * 0.165;
        const qreal cy = size * 0.36;
        QRectF arc(cx - R, cy - R, R * 2.0, R * 2.0);
        QPainterPath path;
        path.arcMoveTo(arc, 160);
        path.arcTo(arc, 160, -240);     // 160° 经顶→右→-80°（右下钩尾）
        path.quadTo(QPointF(cx, cy + R * 1.6), QPointF(cx, cy + R * 1.3));
        p.drawPath(path);
        drawBadgeDot(p, cx, size * 0.74, stroke);
        break;
    }
    default:
        break;
    }
    return pm;
}

// QLabel 文字颜色（palette 驱动，不走 QSS）
static void setLabelColor(QLabel *label, const QColor &color)
{
    QPalette pal = label->palette();
    pal.setColor(QPalette::WindowText, color);
    pal.setColor(QPalette::Text, color);
    label->setPalette(pal);
}

// ==================== 构造 / 析构 ====================

AuMessageBox::AuMessageBox(QWidget *parent)
    : AuWindow(parent, Qt::Dialog)
    , m_content(nullptr)
    , m_contentLayout(nullptr)
    , m_textRowLayout(nullptr)
    , m_iconLabel(nullptr)
    , m_messageLabel(nullptr)
    , m_informativeLabel(nullptr)
    , m_customContent(nullptr)
    , m_buttonRow(nullptr)
    , m_icon(NoIcon)
    , m_standardButtonFlags(NoButton)
    , m_defaultButton(nullptr)
    , m_defaultStandard(NoButton)
    , m_clickedButton(nullptr)
    , m_clickedStandard(NoButton)
    , m_style(nullptr)
    , m_sizeFixed(false)
{
    setButtonVisible(AuTitleBar::ButtonMinimize, false);
    setButtonVisible(AuTitleBar::ButtonMaximize, false);
    // 提示窗不参与贴靠 / 磁吸（手动拖动通道的扩展行为）
    setEdgeSnapEnabled(false);
    setWindowSnapEnabled(false);

    buildContent();
    applyMessageStyle();
    AuMessageBoxStyle::registerGlobalObserver(this);
}

AuMessageBox::~AuMessageBox()
{
    AuMessageBoxStyle::unregisterGlobalObserver(this);
    delete m_style;     // 实例样式（全局样式指针不会进入 m_style，见 setStyle）
    m_style = nullptr;
}

void AuMessageBox::buildContent()
{
    m_content = new QWidget(this);
    m_contentLayout = new QVBoxLayout(m_content);
    m_contentLayout->setContentsMargins(24, 24, 24, 24);
    m_contentLayout->setSpacing(16);

    // 文本块：图标 + （主文本 / 补充文本）
    m_textRowLayout = new QHBoxLayout;
    m_textRowLayout->setContentsMargins(0, 0, 0, 0);
    m_textRowLayout->setSpacing(14);
    m_iconLabel = new QLabel(m_content);
    m_iconLabel->setVisible(false);
    m_textRowLayout->addWidget(m_iconLabel, 0, Qt::AlignTop);

    QVBoxLayout *textCol = new QVBoxLayout;
    textCol->setContentsMargins(0, 0, 0, 0);
    textCol->setSpacing(6);
    m_messageLabel = new QLabel(m_content);
    m_messageLabel->setWordWrap(true);
    m_messageLabel->setTextFormat(Qt::PlainText);
    m_messageLabel->setVisible(false);
    textCol->addWidget(m_messageLabel);
    m_informativeLabel = new QLabel(m_content);
    m_informativeLabel->setWordWrap(true);
    m_informativeLabel->setTextFormat(Qt::PlainText);
    m_informativeLabel->setVisible(false);
    textCol->addWidget(m_informativeLabel);
    m_textRowLayout->addLayout(textCol, 1);
    m_contentLayout->addLayout(m_textRowLayout);

    // 按钮行：右对齐（stretch 占首）
    m_buttonRow = new QHBoxLayout;
    m_buttonRow->setContentsMargins(0, 0, 0, 0);
    m_buttonRow->addStretch(1);
    m_contentLayout->addLayout(m_buttonRow);

    setCentralWidget(m_content);
}

// ==================== 静态便捷函数 ====================

AuMessageBox::StandardButton AuMessageBox::information(QWidget *parent, const QString &title,
                                                       const QString &text, StandardButtons buttons,
                                                       StandardButton defaultButton)
{
    return runMessageBox(parent, title, text, Information, buttons, defaultButton);
}

AuMessageBox::StandardButton AuMessageBox::warning(QWidget *parent, const QString &title,
                                                   const QString &text, StandardButtons buttons,
                                                   StandardButton defaultButton)
{
    return runMessageBox(parent, title, text, Warning, buttons, defaultButton);
}

AuMessageBox::StandardButton AuMessageBox::critical(QWidget *parent, const QString &title,
                                                    const QString &text, StandardButtons buttons,
                                                    StandardButton defaultButton)
{
    return runMessageBox(parent, title, text, Critical, buttons, defaultButton);
}

AuMessageBox::StandardButton AuMessageBox::question(QWidget *parent, const QString &title,
                                                    const QString &text, StandardButtons buttons,
                                                    StandardButton defaultButton)
{
    return runMessageBox(parent, title, text, Question, buttons, defaultButton);
}

AuMessageBox::StandardButton AuMessageBox::runMessageBox(QWidget *parent, const QString &title,
                                                         const QString &text, Icon icon,
                                                         StandardButtons buttons,
                                                         StandardButton defaultButton)
{
    // 堆分配 + QPointer 守卫：exec 期间父窗口可能被销毁并连带销毁本框
    // （析构会退出模态循环），此时不能再访问成员，返回 NoButton
    QPointer<AuMessageBox> box = new AuMessageBox(parent);
    box->setWindowTitle(title);
    box->setIcon(icon);
    box->setText(text);
    box->setStandardButtons(buttons);
    AuPushButton *def = defaultButton != NoButton ? box->button(defaultButton) : nullptr;
    if (!def)
        def = box->firstStandardButton();
    if (def)
        box->setDefaultButton(def);
    box->exec();
    StandardButton ret = NoButton;
    if (box) {
        ret = box->m_clickedStandard != NoButton ? box->m_clickedStandard
                                                 : box->escapeStandardButton();
        delete box;
    }
    return ret;
}

// ==================== 标准按钮文案（改中文零配置） ====================

QString AuMessageBox::builtinButtonText(StandardButton button)
{
    switch (button) {
    case Ok:     return QStringLiteral("确定");
    case Yes:    return QStringLiteral("是");
    case No:     return QStringLiteral("否");
    case Cancel: return QStringLiteral("取消");
    case Close:  return QStringLiteral("关闭");
    default:
        break;
    }
    return QString();
}

QString AuMessageBox::standardButtonText(StandardButton button)
{
    const QString overrideText = s_globalButtonTexts.value(int(button));
    if (!overrideText.isEmpty())
        return overrideText;
    return builtinButtonText(button);
}

void AuMessageBox::setDefaultButtonText(StandardButton button, const QString &text)
{
    if (text.isEmpty())
        s_globalButtonTexts.remove(int(button));
    else
        s_globalButtonTexts.insert(int(button), text);
    // 刷新已存在的消息框（实例级覆盖优先，未覆盖者跟随全局表）
    const QVector<AuMessageBox *> observers = AuMessageBoxStyle::s_observers;
    for (AuMessageBox *box : observers) {
        if (AuMessageBoxStyle::s_observers.contains(box))
            box->refreshStandardButtonText(button);
    }
}

void AuMessageBox::setButtonText(StandardButton button, const QString &text)
{
    if (text.isEmpty())
        m_buttonTextOverrides.remove(int(button));
    else
        m_buttonTextOverrides.insert(int(button), text);
    refreshStandardButtonText(button);
}

QString AuMessageBox::effectiveButtonText(StandardButton button) const
{
    const QString overrideText = m_buttonTextOverrides.value(int(button));
    if (!overrideText.isEmpty())
        return overrideText;
    return standardButtonText(button);
}

void AuMessageBox::refreshStandardButtonText(StandardButton button)
{
    AuPushButton *btn = m_standardButtons.value(int(button));
    if (btn)
        btn->setText(effectiveButtonText(button));
}

// ==================== 文本 / 图标 ====================

void AuMessageBox::setText(const QString &text)
{
    m_messageLabel->setText(text);
    m_messageLabel->setVisible(!text.isEmpty());
    if (m_sizeFixed && isVisible())
        fixContentSize();
}

QString AuMessageBox::text() const
{
    return m_messageLabel->text();
}

void AuMessageBox::setInformativeText(const QString &text)
{
    m_informativeLabel->setText(text);
    m_informativeLabel->setVisible(!text.isEmpty());
    if (m_sizeFixed && isVisible())
        fixContentSize();
}

QString AuMessageBox::informativeText() const
{
    return m_informativeLabel->text();
}

void AuMessageBox::setIcon(Icon icon)
{
    if (m_icon == icon && m_iconPixmap.isNull())
        return;
    m_icon = icon;
    m_iconPixmap = QPixmap();
    refreshIcon();
}

AuMessageBox::Icon AuMessageBox::icon() const
{
    return m_icon;
}

void AuMessageBox::setIconPixmap(const QPixmap &pixmap)
{
    m_iconPixmap = pixmap;
    refreshIcon();
}

QPixmap AuMessageBox::iconPixmap() const
{
    return m_iconPixmap;
}

void AuMessageBox::refreshIcon()
{
    if (m_iconPixmap.isNull() && m_icon == NoIcon) {
        m_iconLabel->setVisible(false);
        return;
    }
    if (!m_iconPixmap.isNull()) {
        // 自定义贴图：按贴图实际尺寸定标签大小（不受样式 iconSize 裁剪）
        m_iconLabel->setPixmap(m_iconPixmap);
        const qreal dpr = m_iconPixmap.devicePixelRatio() > 0
                              ? m_iconPixmap.devicePixelRatio() : 1.0;
        const QSizeF logical = QSizeF(m_iconPixmap.size()) / dpr;
        m_iconLabel->setFixedSize(qCeil(logical.width()), qCeil(logical.height()));
    } else {
        const AuMessageBoxStyle *st = style();
        QColor color;
        switch (m_icon) {
        case Information: color = st->iconInformationColor(); break;
        case Question:    color = st->iconQuestionColor(); break;
        case Warning:     color = st->iconWarningColor(); break;
        case Critical:    color = st->iconCriticalColor(); break;
        default:          color = st->iconInformationColor(); break;
        }
        m_iconLabel->setFixedSize(st->iconSize(), st->iconSize());
        m_iconLabel->setPixmap(drawIconBadge(m_icon, st->iconSize(), color,
                                             m_iconLabel->devicePixelRatioF()));
    }
    m_iconLabel->setVisible(true);
    if (m_sizeFixed && isVisible())
        fixContentSize();
}

// ==================== 按钮 ====================

void AuMessageBox::setStandardButtons(StandardButtons buttons)
{
    if (m_standardButtonFlags == buttons)
        return;
    m_standardButtonFlags = buttons;
    rebuildButtonRow();
}

AuMessageBox::StandardButtons AuMessageBox::standardButtons() const
{
    return m_standardButtonFlags;
}

AuPushButton *AuMessageBox::button(StandardButton which) const
{
    return m_standardButtons.value(int(which));
}

AuPushButton *AuMessageBox::addButton(const QString &text, ButtonRole role)
{
    AuPushButton *btn = new AuPushButton(text, m_content);
    connect(btn, &QPushButton::clicked, this, [this, btn, role]() {
        m_clickedButton = btn;
        m_clickedStandard = NoButton;
        emit buttonClicked(NoButton);
        done(role == AcceptRole ? int(Accepted) : int(Rejected));
    });
    m_customButtons.append(qMakePair(btn, role));
    m_buttonRow->addWidget(btn);
    btn->setVisible(true);  // 显式恢复显示（box 可见状态下动态添加同样适用）
    applyButtonVisual(btn);
    if (m_sizeFixed && isVisible())
        fixContentSize();
    return btn;
}

void AuMessageBox::rebuildButtonRow()
{
    const StandardButton prevDefault = m_defaultStandard;

    // 清空按钮行（保留自定义按钮对象，重建后重新排入）
    QLayoutItem *item;
    while ((item = m_buttonRow->takeAt(0)) != nullptr) {
        if (item->widget())
            item->widget()->setVisible(false);
        delete item;
    }
    for (AuPushButton *btn : m_standardButtons)
        btn->deleteLater();
    m_standardButtons.clear();
    m_clickedButton = nullptr;
    m_defaultButton = nullptr;
    m_buttonRow->addStretch(1);

    for (StandardButton which : kDisplayOrder) {
        if (!m_standardButtonFlags.testFlag(which))
            continue;
        AuPushButton *btn = new AuPushButton(effectiveButtonText(which), m_content);
        connect(btn, &QPushButton::clicked, this, [this, which, btn]() {
            m_clickedButton = btn;
            m_clickedStandard = which;
            emit buttonClicked(which);
            done(int(which));
        });
        m_standardButtons.insert(int(which), btn);
        m_buttonRow->addWidget(btn);
        // 显式恢复显示：被 hide 过的控件重新加入布局不会自动显示（Qt 实测语义）
        btn->setVisible(true);
    }
    for (const auto &entry : m_customButtons) {
        m_buttonRow->addWidget(entry.first);
        entry.first->setVisible(true);
    }

    if (prevDefault != NoButton)
        setDefaultButton(prevDefault);
    for (AuPushButton *btn : m_standardButtons)
        applyButtonVisual(btn);
    for (const auto &entry : m_customButtons)
        applyButtonVisual(entry.first);

    if (m_sizeFixed && isVisible())
        fixContentSize();
}

void AuMessageBox::setDefaultButton(StandardButton which)
{
    AuPushButton *btn = m_standardButtons.value(int(which));
    if (btn)
        setDefaultButton(btn);
}

void AuMessageBox::setDefaultButton(AuPushButton *button)
{
    if (m_defaultButton == button)
        return;
    if (m_defaultButton) {
        m_defaultButton->setDefault(false);
        m_defaultButton->setHighlight(false);
        applyButtonVisual(m_defaultButton);
    }
    m_defaultButton = button;
    m_defaultStandard = NoButton;
    if (button) {
        for (auto it = m_standardButtons.constBegin(); it != m_standardButtons.constEnd(); ++it) {
            if (it.value() == button) {
                m_defaultStandard = StandardButton(it.key());
                break;
            }
        }
        button->setDefault(true);
        button->setHighlight(true);
        applyButtonVisual(button);
        if (isVisible())
            button->setFocus();
    }
}

AuPushButton *AuMessageBox::defaultButton() const
{
    return m_defaultButton;
}

AuPushButton *AuMessageBox::clickedButton() const
{
    return m_clickedButton;
}

AuPushButton *AuMessageBox::firstStandardButton() const
{
    for (StandardButton which : kDisplayOrder) {
        if (m_standardButtonFlags.testFlag(which))
            return m_standardButtons.value(int(which));
    }
    return nullptr;
}

AuMessageBox::StandardButton AuMessageBox::escapeStandardButton() const
{
    if (m_standardButtonFlags.testFlag(Cancel))
        return Cancel;
    if (m_standardButtonFlags.testFlag(Close))
        return Close;
    if (m_standardButtonFlags.testFlag(No))
        return No;
    if (m_standardButtonFlags.testFlag(Ok))
        return Ok;
    return NoButton;
}

// ==================== 自定义内容 ====================

void AuMessageBox::setCustomContent(QWidget *widget)
{
    if (m_customContent == widget)
        return;
    if (m_customContent) {
        m_contentLayout->removeWidget(m_customContent);
        m_customContent->deleteLater();
        m_customContent = nullptr;
    }
    if (widget) {
        widget->setParent(m_content);
        m_contentLayout->insertWidget(1, widget);    // 文本行之后、按钮行之前
        m_customContent = widget;
    }
    if (m_sizeFixed && isVisible())
        fixContentSize();
}

QWidget *AuMessageBox::customContent() const
{
    return m_customContent;
}

// ==================== 样式 ====================

void AuMessageBox::setStyle(AuMessageBoxStyle *style)
{
    // 全局样式指针归 setGlobalStyle 管理（仅在设置新全局样式时释放旧的）：
    // setStyle 传入它时直接跳过——不接管所有权、不改变当前样式状态；
    // 恢复跟随全局请显式传 nullptr
    if (style && style == AuMessageBoxStyle::globalStyle())
        return;
    if (m_style == style)
        return;
    delete m_style;
    m_style = style;
    applyMessageStyle();
    if (m_sizeFixed && isVisible())
        fixContentSize();
}

AuMessageBoxStyle *AuMessageBox::style() const
{
    return m_style ? m_style : AuMessageBoxStyle::globalStyle();
}

AuWindowStyle *AuMessageBox::windowStyle() const
{
    return AuWindow::style();
}

void AuMessageBox::onGlobalMessageStyleChanged()
{
    if (m_style)
        return; // 独立样式不受全局影响
    applyMessageStyle();
    if (m_sizeFixed && isVisible())
        fixContentSize();
}

void AuMessageBox::applyMessageStyle()
{
    const AuMessageBoxStyle *st = style();

    // 文本
    m_messageLabel->setFont(st->messageFont());
    setLabelColor(m_messageLabel, st->messageColor());
    m_informativeLabel->setFont(st->informativeFont());
    setLabelColor(m_informativeLabel, st->informativeColor());
    m_messageLabel->setMaximumWidth(st->maxContentWidth());
    m_informativeLabel->setMaximumWidth(st->maxContentWidth());

    // 布局
    const int m = st->contentMargin();
    m_contentLayout->setContentsMargins(m, m, m, m);
    m_contentLayout->setSpacing(st->contentSpacing());
    m_textRowLayout->setSpacing(st->iconSpacing());
    m_buttonRow->setSpacing(st->buttonSpacing());

    // 图标（尺寸在 refreshIcon 内按内置/自定义贴图分别决定）
    refreshIcon();

    // 按钮
    for (AuPushButton *btn : m_standardButtons)
        applyButtonVisual(btn);
    for (const auto &entry : m_customButtons)
        applyButtonVisual(entry.first);
}

void AuMessageBox::applyButtonVisual(AuPushButton *button)
{
    const AuMessageBoxStyle *st = style();
    button->setFont(st->buttonFont());
    button->setMinimumWidth(st->buttonMinWidth());
    button->setFixedHeight(st->buttonHeight());
    button->setRadius(st->buttonRadius());
    button->setContentPadding(st->buttonPadding());
    if (button->isHighlight()) {
        button->setBackground(st->defaultButtonBackground());
        button->setHoverBackground(st->defaultButtonHoverBackground());
        button->setPressBackground(st->defaultButtonPressBackground());
        button->setTextColor(st->defaultButtonTextColor());
        QColor disabled = st->defaultButtonTextColor();
        disabled.setAlpha(int(disabled.alpha() * 0.45));
        button->setDisabledTextColor(disabled);
    } else {
        button->setBackground(st->buttonBackground());
        button->setHoverBackground(st->buttonHoverBackground());
        button->setPressBackground(st->buttonPressBackground());
        button->setBorderColor(st->buttonBorderColor());
        button->setTextColor(st->buttonTextColor());
        button->setDisabledTextColor(st->buttonDisabledTextColor());
    }
}

// ==================== 尺寸 / 显示 ====================

QSize AuMessageBox::sizeHint() const
{
    const AuMessageBoxStyle *st = style();
    const QSize contentHint = m_content ? m_content->sizeHint() : QSize(0, 0);
    int w = qMin(contentHint.width(), st->maxContentWidth());
    w = qMax(w, st->minContentWidth());
    if (m_content)
        w = qMax(w, m_content->minimumSizeHint().width());
    int h = -1;
    if (m_contentLayout && m_contentLayout->hasHeightForWidth())
        h = m_contentLayout->heightForWidth(w);
    if (h < 0)
        h = contentHint.height();
    const int titleBarHeight = titleBar() ? qMax(titleBar()->sizeHint().height(), 0) : 0;
    return QSize(w, h + titleBarHeight);
}

QSize AuMessageBox::minimumSizeHint() const
{
    return sizeHint();
}

void AuMessageBox::fixContentSize()
{
    m_sizeFixed = true;
    setFixedSize(sizeHint());
}

void AuMessageBox::setVisible(bool visible)
{
    if (visible && !isVisible()) {
        m_clickedButton = nullptr;
        m_clickedStandard = NoButton;
        // 用户未手动约束尺寸时按内容定尺寸；消息框不支持用户拖拽缩放
        const bool userSized = minimumSize() != QSize(0, 0)
                               || maximumSize() != QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        if (!m_sizeFixed && !userSized)
            fixContentSize();
    }
    AuWindow::setVisible(visible);
}

void AuMessageBox::showEvent(QShowEvent *event)
{
    AuWindow::showEvent(event);
    // 初始焦点：默认按钮优先，无默认则首个标准按钮（回车可直接触发）
    if (m_defaultButton) {
        m_defaultButton->setFocus();
    } else {
        AuPushButton *first = firstStandardButton();
        if (first)
            first->setFocus();
    }
}
