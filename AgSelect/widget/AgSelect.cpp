#include "AgSelect.h"

#include "AgPopupPanel.h"
#include "AgScrollBar.h"
#include "AgSelectItemDelegate.h"
#include "AgSelectListView.h"
#include "AgSelectModel.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMargins>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>

// ---------------------------------------------------------------------------
// 内置下拉箭头：随面板展开 / 收起旋转 180°
class AgSelectDefaultArrow : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation)

public:
    explicit AgSelectDefaultArrow(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_rotation(0.0)
    {
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_TransparentForMouseEvents); // 点击穿透到 AgSelect
    }

    qreal rotation() const { return m_rotation; }
    void setRotation(qreal rotation)
    {
        m_rotation = rotation;
        update();
    }

    void setArrowColor(const QColor &color)
    {
        if (m_color == color)
            return;
        m_color = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent * /*event*/) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPen pen(m_color.isValid() ? m_color : QColor(0, 0, 0, 115), 1.6);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        painter.translate(width() / 2.0, height() / 2.0);
        painter.rotate(m_rotation);
        // 较窄的 V 形（张角约 80°），避免开口过大显得松散
        const qreal w = qMin(width(), height()) * 0.42;
        QPainterPath path;
        path.moveTo(-w * 0.6, -w * 0.35);
        path.lineTo(0, w * 0.35);
        path.lineTo(w * 0.6, -w * 0.35);
        painter.drawPath(path);
    }

private:
    qreal m_rotation;
    QColor m_color;
};

// ---------------------------------------------------------------------------
// 清除图标绘制（与 AgDatePicker 一致）：实心圆圈 + ×

namespace {

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

AgSelect::AgSelect(QWidget *parent)
    : QWidget(parent)
    , m_lineEdit(new QLineEdit(this))
    , m_panel(nullptr)
    , m_listView(nullptr)
    , m_model(new AgSelectListModel(this))
    , m_proxy(new AgSelectFilterProxyModel(this))
    , m_delegate(nullptr)
    , m_ownStyle(nullptr)
    , m_styleFromSizeMode(false)
    , m_overrideSizeMode(AgSelectStyle::SizeMode::Medium)
    , m_arrowWidget(nullptr)
    , m_arrowAnimation(nullptr)
    , m_multiSelection(false)
    , m_editable(false)
    , m_clearButtonEnabled(false)
    , m_areaHovered(false)
    , m_lineHovered(false)
    , m_iconHovered(false)
    , m_currentIndex(-1)
    , m_activeGroupId(-1)
    , m_popupWidthMode(PopupMatchEditor)
    , m_fixedPopupWidth(0)
    , m_maxPopupHeight(0)
    , m_hoverChip(-1)
    , m_hoverRemove(false)
    , m_preferredHeight(0)
{
    setFocusPolicy(Qt::NoFocus); // 键盘交互由内部的行编辑框承载
    setMouseTracking(true);
    // 垂直 Fixed：高度严格跟随尺寸档位的 sizeHint，不被布局拉伸
    // （否则同层更高的新件会把小尺寸控件一并拉高，切换档位时高度不变）
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_lineEdit->setFrame(false);
    m_lineEdit->setReadOnly(true);
    m_lineEdit->setFocusPolicy(Qt::StrongFocus);
    m_lineEdit->setAttribute(Qt::WA_Hover, true);
    m_lineEdit->setStyleSheet(QStringLiteral("QLineEdit { background: transparent; border: none; }"));
    m_lineEdit->installEventFilter(this);

    connect(m_lineEdit, &QLineEdit::textEdited, this, [this](const QString &text) {
        emit textEdited(text);
        applyFilter(text);
        if (!isPopupVisible() && isEnabled() && count() > 0)
            showPopup();
    });

    m_arrowWidget = new AgSelectDefaultArrow(this);
    m_arrowAnimation = new QPropertyAnimation(m_arrowWidget, "rotation", this);
    m_arrowAnimation->setDuration(100);
    m_arrowAnimation->setEasingCurve(QEasingCurve::OutCubic);

    m_proxy->setSourceModel(m_model);

    // 跟随全局样式：未显式 setStyle / 未产生派生样式前注册观察者
    AgSelectStyle::registerGlobalObserver(this);

    relayoutEditor();
    applyStyleToWidgets();
}

AgSelect::~AgSelect()
{
    AgSelectStyle::unregisterGlobalObserver(this);
    // 面板仍处于弹出 / 收起动画中就销毁控件时，子对象按创建顺序先于面板销毁，
    // 面板基类析构阶段 dismiss() 发出的 closed 会回调 revertUnmatchedText() 访问
    // 已销毁的 m_lineEdit（切页 / 父窗口销毁时崩溃）。这里先断开面板回调并安全收起。
    if (m_panel) {
        QObject::disconnect(m_panel, nullptr, this, nullptr);
        m_panel->dismiss();
    }
    delete m_ownStyle;
}

// ---------------------------------------------------------------------------
// 条目管理

int AgSelect::addItem(const QString &text, const QVariant &userData, const QIcon &icon)
{
    const int row = m_model->appendItem(text, userData, icon, true, m_activeGroupId);
    if (m_currentIndex == -1 && count() == 1)
        setCurrentIndex(row); // 与 QComboBox 一致：首个条目自动成为当前项
    update();
    return row;
}

int AgSelect::addItems(const QStringList &texts)
{
    int last = -1;
    for (const QString &text : texts)
        last = addItem(text);
    return last;
}

void AgSelect::addGroup(const QString &title)
{
    if (title.isEmpty()) {
        m_activeGroupId = -1; // 结束当前分组
        return;
    }
    m_activeGroupId = m_model->appendGroupHeader(title);
}

void AgSelect::insertItem(int index, const QString &text, const QVariant &userData, const QIcon &icon)
{
    const int row = m_model->insertItem(index, text, userData, icon, true, m_activeGroupId);
    if (row <= m_currentIndex)
        ++m_currentIndex;
    update();
}

void AgSelect::removeItem(int index)
{
    if (index < 0 || index >= count())
        return;
    m_model->removeRecord(index);
    if (index == m_currentIndex) {
        m_currentIndex = -1;
        syncCurrentRowToModel();
        updateEditorText(QString());
        emit currentIndexChanged(-1);
        emit currentTextChanged(QString());
    } else if (index < m_currentIndex) {
        --m_currentIndex;
        emit currentIndexChanged(m_currentIndex);
    }
    relayoutEditor();
    update();
}

void AgSelect::clear()
{
    m_model->clear();
    m_activeGroupId = -1;
    m_currentIndex = -1;
    relayoutEditor();
    emit currentIndexChanged(-1);
    emit currentTextChanged(QString());
    update();
}

int AgSelect::count() const
{
    return m_model->count();
}

QString AgSelect::itemText(int index) const
{
    const AgSelectListModel::Record *record = m_model->recordAt(index);
    return (record && record->type == AgSelectListModel::Record::Item) ? record->text : QString();
}

QVariant AgSelect::itemData(int index) const
{
    const AgSelectListModel::Record *record = m_model->recordAt(index);
    return (record && record->type == AgSelectListModel::Record::Item) ? record->data : QVariant();
}

QIcon AgSelect::itemIcon(int index) const
{
    const AgSelectListModel::Record *record = m_model->recordAt(index);
    return (record && record->type == AgSelectListModel::Record::Item) ? record->icon : QIcon();
}

bool AgSelect::isItemEnabled(int index) const
{
    const AgSelectListModel::Record *record = m_model->recordAt(index);
    return record && record->type == AgSelectListModel::Record::Item && record->enabled;
}

void AgSelect::setItemEnabled(int index, bool enabled)
{
    m_model->setRecordEnabled(index, enabled);
    update();
}

void AgSelect::setItemIcon(int index, const QIcon &icon)
{
    m_model->setRecordIcon(index, icon);
    update();
}

int AgSelect::findText(const QString &text, Qt::MatchFlags flags) const
{
    const Qt::CaseSensitivity cs = flags.testFlag(Qt::MatchCaseSensitive)
                                       ? Qt::CaseSensitive
                                       : Qt::CaseInsensitive;
    const bool contains = flags.testFlag(Qt::MatchContains);
    for (int row = 0; row < m_model->count(); ++row) {
        const AgSelectListModel::Record *record = m_model->recordAt(row);
        if (!record || record->type != AgSelectListModel::Record::Item)
            continue;
        const bool hit = contains ? record->text.contains(text, cs)
                                  : record->text.compare(text, cs) == 0;
        if (hit)
            return row;
    }
    return -1;
}

int AgSelect::findData(const QVariant &data, Qt::MatchFlags flags) const
{
    Q_UNUSED(flags);
    for (int row = 0; row < m_model->count(); ++row) {
        const AgSelectListModel::Record *record = m_model->recordAt(row);
        if (record && record->type == AgSelectListModel::Record::Item && record->data == data)
            return row;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// 当前值与多选

int AgSelect::currentIndex() const
{
    return m_currentIndex;
}

QString AgSelect::currentText() const
{
    return m_lineEdit->text();
}

QVariant AgSelect::currentData() const
{
    return itemData(m_currentIndex);
}

bool AgSelect::hasSelection() const
{
    if (m_multiSelection)
        return !m_model->checkedRows().isEmpty();
    return m_currentIndex >= 0;
}

// ---------------------------------------------------------------------------
// 清除按钮

bool AgSelect::isClearButtonEnabled() const
{
    return m_clearButtonEnabled;
}

void AgSelect::setClearButtonEnabled(bool enabled)
{
    if (m_clearButtonEnabled == enabled)
        return;
    m_clearButtonEnabled = enabled;
    update();
}

void AgSelect::clearSelection()
{
    if (m_multiSelection) {
        if (m_model->checkedRows().isEmpty())
            return; // 无选择项：无动作也不发信号
        clearChecked();
    } else {
        if (m_currentIndex < 0)
            return;
        setCurrentIndex(-1);
    }
    emit cleared();
    update();
}

bool AgSelect::isMultiSelection() const
{
    return m_multiSelection;
}

QList<int> AgSelect::checkedIndexes() const
{
    const QSet<int> rows = m_model->checkedRows();
    QList<int> result;
    for (int row = 0; row < m_model->count(); ++row) {
        if (rows.contains(row))
            result.append(row);
    }
    return result;
}

QStringList AgSelect::checkedTexts() const
{
    QStringList texts;
    const QList<int> rows = checkedIndexes();
    for (int row : rows)
        texts.append(itemText(row));
    return texts;
}

QVariantList AgSelect::checkedData() const
{
    QVariantList data;
    const QList<int> rows = checkedIndexes();
    for (int row : rows)
        data.append(itemData(row));
    return data;
}

bool AgSelect::isEditable() const
{
    return m_editable;
}

QString AgSelect::placeholderText() const
{
    return m_lineEdit->placeholderText();
}

// ---------------------------------------------------------------------------
// 槽

void AgSelect::setCurrentIndex(int index)
{
    index = qBound(-1, index, count() - 1);
    const int previous = m_currentIndex;
    m_currentIndex = index;

    if (!m_multiSelection)
        syncCurrentRowToModel();
    updateEditorText(index >= 0 ? itemText(index) : QString());

    if (previous != index) {
        emit currentIndexChanged(index);
        emit currentTextChanged(currentText());
    }
    update();
}

void AgSelect::setCurrentText(const QString &text)
{
    const int index = findText(text, Qt::MatchExactly);
    if (index >= 0) {
        setCurrentIndex(index);
    } else if (m_editable && !m_multiSelection) {
        updateEditorText(text);
        emit currentTextChanged(currentText());
    }
}

void AgSelect::setMultiSelection(bool multi)
{
    if (multi == m_multiSelection)
        return;
    m_multiSelection = multi;
    if (m_delegate)
        m_delegate->setMultiSelection(multi);
    // 多选模式的过滤输入始终可用（与 ant-design 一致）
    m_lineEdit->setReadOnly(!m_editable && !m_multiSelection);

    if (!multi) {
        m_model->clearChecked();
        syncCurrentRowToModel();
    } else if (m_currentIndex >= 0) {
        m_model->setChecked(m_currentIndex, true); // 转多选时保留当前项为勾选
    }

    relayoutEditor();
    emit checkedChanged(checkedIndexes());
}

void AgSelect::setCheckedIndexes(const QList<int> &indexes)
{
    if (!m_multiSelection)
        return;
    QSet<int> rows;
    for (int index : indexes) {
        const AgSelectListModel::Record *record = m_model->recordAt(index);
        if (record && record->type == AgSelectListModel::Record::Item)
            rows.insert(index); // 组名行不可勾选
    }
    m_model->setCheckedRows(rows);
    relayoutEditor();
    emit checkedChanged(checkedIndexes());
    update();
}

void AgSelect::clearChecked()
{
    m_model->clearChecked();
    relayoutEditor();
    emit checkedChanged(checkedIndexes());
    update();
}

void AgSelect::setEditable(bool editable)
{
    m_editable = editable;
    // 多选模式的过滤输入始终可用（与 ant-design 一致）
    m_lineEdit->setReadOnly(!m_editable && !m_multiSelection);
    update();
}

void AgSelect::setPlaceholderText(const QString &text)
{
    m_lineEdit->setPlaceholderText(text);
}

AgSelectStyle *AgSelect::style() const
{
    return m_ownStyle ? m_ownStyle : AgSelectStyle::globalStyle();
}

void AgSelect::setStyle(AgSelectStyle *style)
{
    if (m_ownStyle == style)
        return;
    delete m_ownStyle;
    m_ownStyle = style;
    m_styleFromSizeMode = false; // 显式样式完全接管，不再跟随全局主题
    if (m_ownStyle) {
        AgSelectStyle::unregisterGlobalObserver(this);
    } else {
        AgSelectStyle::registerGlobalObserver(this);
    }
    if (m_delegate)
        m_delegate->setStyle(this->style());
    applyStyleToWidgets();
}

void AgSelect::setSizeMode(AgSelectStyle::SizeMode mode)
{
    if (!m_ownStyle) {
        // 首次切换尺寸档位：从全局样式派生一份私有样式，并保持跟随全局主题
        // （主题切换时重新继承配色，尺寸档位保持不变）
        m_ownStyle = new AgSelectStyle(*AgSelectStyle::globalStyle());
        m_styleFromSizeMode = true;
    }
    m_overrideSizeMode = mode;
    m_ownStyle->setSizeMode(mode);
    if (m_delegate)
        m_delegate->setStyle(m_ownStyle);
    applyStyleToWidgets();
}

AgSelectStyle::SizeMode AgSelect::sizeMode() const
{
    return style()->sizeMode();
}

void AgSelect::setItemDelegate(AgSelectItemDelegate *delegate)
{
    if (m_delegate == delegate)
        return;
    AgSelectItemDelegate *oldDelegate = m_delegate;
    m_delegate = delegate;
    if (m_delegate) {
        m_delegate->setParent(this);
        m_delegate->setStyle(style());
        m_delegate->setMultiSelection(m_multiSelection);
        if (m_listView)
            m_listView->setItemDelegate(m_delegate);
    }
    if (oldDelegate)
        oldDelegate->deleteLater();
}

AgSelectItemDelegate *AgSelect::itemDelegate() const
{
    return m_delegate;
}

void AgSelect::setPopupWidthMode(PopupWidthMode mode)
{
    m_popupWidthMode = mode;
}

AgSelect::PopupWidthMode AgSelect::popupWidthMode() const
{
    return m_popupWidthMode;
}

void AgSelect::setFixedPopupWidth(int width)
{
    m_fixedPopupWidth = qMax(0, width);
}

int AgSelect::fixedPopupWidth() const
{
    return m_fixedPopupWidth;
}

void AgSelect::setMaxPopupHeight(int height)
{
    m_maxPopupHeight = qMax(0, height);
}

int AgSelect::maxPopupHeight() const
{
    return m_maxPopupHeight;
}

bool AgSelect::isPopupVisible() const
{
    return m_panel && m_panel->isVisible();
}

QWidget *AgSelect::arrowWidget() const
{
    return m_arrowWidget;
}

void AgSelect::setArrowWidget(QWidget *widget)
{
    if (m_arrowWidget == widget)
        return;
    if (m_arrowWidget) {
        m_arrowAnimation->stop();
        m_arrowAnimation->setTargetObject(nullptr);
        m_arrowWidget->deleteLater();
    }
    if (widget) {
        m_arrowWidget = widget;
        widget->setParent(this);
        widget->show();
    } else {
        m_arrowWidget = new AgSelectDefaultArrow(this);
        m_arrowAnimation->setTargetObject(m_arrowWidget);
        m_arrowAnimation->setPropertyName("rotation");
    }
    relayoutEditor();
    applyStyleToWidgets();
}

void AgSelect::showPopup()
{
    if (!isEnabled() || count() == 0)
        return;
    ensurePopup();
    // 键盘导航（上下 / 回车）的前提：面板展开时焦点必须落在行编辑框，
    // 否则按键会落到当前焦点控件上
    m_lineEdit->setFocus(Qt::OtherFocusReason);
    if (m_panel->isVisible()) {
        m_panel->raise();
        return;
    }

    emit popupAboutToShow();

    syncCurrentRowToModel();
    m_delegate->setMultiSelection(m_multiSelection);
    applyFilter(QString());

    QSize content(width(), fullContentHeight());
    if (m_popupWidthMode == PopupFixed && m_fixedPopupWidth > 0)
        content.setWidth(qMax(width(), m_fixedPopupWidth));
    else if (m_popupWidthMode == PopupFitContent)
        content.setWidth(qMax(width(), fitContentWidth()));
    content.setHeight(qMin(content.height(), effectiveMaxPopupHeight()));

    m_panel->popupOver(this, content);
    scrollToCurrentRow();
    // 某些平台（如 offscreen）面板显示时窗口被激活会短暂夺焦，重新落回行编辑框，
    // 保证上下 / 回车始终驱动面板而不是其它焦点控件
    m_lineEdit->setFocus(Qt::OtherFocusReason);
    rotateArrow(true);
    update();
}

void AgSelect::hidePopup()
{
    if (m_panel && m_panel->isVisible())
        m_panel->closePopup();
}

void AgSelect::rotateArrow(bool open)
{
    // 自定义箭头部件的动效由用户借助面板回调自行实现
    if (!qobject_cast<AgSelectDefaultArrow *>(m_arrowWidget) || !m_arrowAnimation->targetObject())
        return;
    m_arrowAnimation->stop();
    m_arrowAnimation->setStartValue(m_arrowWidget->property("rotation"));
    m_arrowAnimation->setEndValue(open ? 180.0 : 0.0);
    m_arrowAnimation->start();
}

// ---------------------------------------------------------------------------
// 事件处理

void AgSelect::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    AgSelectStyle *s = style();

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

    // 右侧槽位：已有选择项且悬停且清除开关打开时，箭头切换为清除图标（参考 AgDatePicker）
    const bool clearActive = clearIconActive();
    if (m_arrowWidget && m_arrowWidget->isVisible() == clearActive)
        m_arrowWidget->setVisible(!clearActive);
    if (clearActive) {
        const QRect iconRect = arrowIconRect();
        if (iconRect.width() >= 6) {
            const QColor circle = m_iconHovered ? s->clearIconCircleHoverColor()
                                                : s->clearIconCircleColor();
            drawClearIcon(painter, iconRect,
                          enabled ? circle : s->editorDisabledForegroundColor(),
                          enabled ? s->clearIconCrossColor() : s->editorBackgroundColor());
        }
    }

    if (!m_multiSelection)
        return;

    // 多选 chips
    const QFontMetrics fm(m_lineEdit->font());
    const int chipRadius = qBound(2, s->chipHeight() / 5, 4);
    for (int i = 0; i < m_chipRects.size(); ++i) {
        const QRect chip = m_chipRects.at(i).second;
        const bool chipHovered = enabled && i == m_hoverChip;

        painter.setPen(Qt::NoPen);
        painter.setBrush(chipHovered ? s->chipHoverBackgroundColor() : s->chipBackgroundColor());
        painter.drawRoundedRect(chip, chipRadius, chipRadius);
        if (s->chipBorderColor().isValid()) {
            painter.setPen(QPen(s->chipBorderColor(), 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(chip).adjusted(0.5, 0.5, -0.5, -0.5),
                                    chipRadius, chipRadius);
            painter.setPen(Qt::NoPen);
        }

        // 文本（chip 高度即为 × 热区边长，见 relayoutEditor）
        const int removeSize = chip.height();
        const QRect textRect = chip.adjusted(6, 0, -(removeSize + 6), 0);
        painter.setPen(enabled ? s->chipForegroundColor() : s->editorDisabledForegroundColor());
        painter.setFont(m_lineEdit->font());
        const QString elided = fm.elidedText(itemText(m_chipRects.at(i).first),
                                             Qt::ElideRight, textRect.width());
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elided);

        // 右侧 ×（悬停高亮）
        const QRect &removeRect = m_chipRemoveRects.at(i);
        QColor iconColor = chipHovered && m_hoverRemove ? s->chipRemoveIconHoverColor()
                                                        : s->chipRemoveIconColor();
        if (!enabled)
            iconColor = s->editorDisabledForegroundColor();
        QPen pen(iconColor, 1.6, Qt::SolidLine, Qt::RoundCap);
        painter.setPen(pen);
        const qreal cross = removeRect.width() * 0.26;
        const QPointF center = QRectF(removeRect).center();
        painter.drawLine(center + QPointF(-cross, -cross), center + QPointF(cross, cross));
        painter.drawLine(center + QPointF(-cross, cross), center + QPointF(cross, -cross));
        painter.setFont(m_lineEdit->font());
    }
}

void AgSelect::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayoutEditor();
}

void AgSelect::enterEvent(QEvent * /*event*/)
{
    // 进入控件本体（行编辑框悬停由 eventFilter 的 HoverEnter/Leave 单独跟踪：
    // Qt 中光标移入子控件时父控件会收到 Leave，两侧状态需合并判断）
    if (!m_areaHovered) {
        m_areaHovered = true;
        update();
    }
}

void AgSelect::mousePressEvent(QMouseEvent *event)
{
    if (!isEnabled()) {
        event->ignore();
        return;
    }
    const QPoint pos = event->pos();

    // 清除图标激活时点击槽位即清除（不弹出面板），参考 AgDatePicker
    if (clearIconActive() && arrowSlotRect().contains(pos)) {
        clearSelection();
        return;
    }

    if (m_multiSelection) {
        const int chipIndex = chipAt(pos);
        if (chipIndex >= 0) {
            if (m_chipRemoveRects.at(chipIndex).contains(pos)) {
                // × 移除：面板保持当前状态（展开时连续移除）
                const int row = m_chipRects.at(chipIndex).first;
                m_model->setChecked(row, false);
                emit activated(row);
                emit checkedChanged(checkedIndexes());
                relayoutEditor();
            } else if (isPopupVisible()) {
                hidePopup(); // 展开时点击 chip 主体：收起
            } else {
                m_lineEdit->setFocus();
                showPopup();
            }
            return;
        }
    }

    if (isPopupVisible()) {
        // 展开时点击编辑区任意位置（文本框 / 空白区 / 箭头）：收起
        hidePopup();
        return;
    }

    if (arrowSlotRect().contains(pos) || !m_lineEdit->geometry().contains(pos)) {
        m_lineEdit->setFocus();
        showPopup();
    }
    // 落在行编辑框上的点击由 eventFilter 处理（聚焦并展开）
}

void AgSelect::mouseMoveEvent(QMouseEvent *event)
{
    // 清除图标悬停（切换实心圆底色加深）与手型光标
    const bool iconHovered = arrowSlotRect().contains(event->pos());
    if (iconHovered != m_iconHovered) {
        m_iconHovered = iconHovered;
        update();
    }
    updateIconCursor(event->pos());

    if (m_multiSelection) {
        const int chipIndex = chipAt(event->pos());
        const bool removeHover = chipIndex >= 0
                                     && m_chipRemoveRects.at(chipIndex).contains(event->pos());
        if (chipIndex != m_hoverChip || removeHover != m_hoverRemove) {
            m_hoverChip = chipIndex;
            m_hoverRemove = removeHover;
            update();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void AgSelect::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
}

void AgSelect::leaveEvent(QEvent * /*event*/)
{
    m_areaHovered = false;
    m_lineHovered = false;
    m_iconHovered = false;
    m_hoverChip = -1;
    m_hoverRemove = false;
    setCursor(Qt::ArrowCursor);
    update();
}

void AgSelect::keyPressEvent(QKeyEvent *event)
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

void AgSelect::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled() && isPopupVisible())
            hidePopup();
        applyStyleToWidgets(); // 刷新禁用配色
    } else if (event->type() == QEvent::FontChange) {
        applyStyleToWidgets(); // 用户自定义字体等字体变化同步到行编辑框与下拉面板
    }
    QWidget::changeEvent(event);
}

bool AgSelect::eventFilter(QObject *watched, QEvent *event)
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
                update();
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
            update(); // 焦点边框 / 淡色底随焦点获得立即刷新
            break;
        case QEvent::FocusOut:
            update(); // 失焦后焦点底色立即清除（无鼠标移动等其它重绘契机）
            revertUnmatchedText(); // 需求 1：失焦后清掉不匹配的输入
            break;
        case QEvent::KeyPress: {
            const QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            const int key = keyEvent->key();
            // 回车必须在过滤器里消费掉：QLineEdit 不处理回车并任其向上传播，
            // 会触发所在 QDialog 的 autoDefault 按钮（如演示页的主题切换按钮）
            if (key == Qt::Key_Enter || key == Qt::Key_Return) {
                if (isPopupVisible())
                    activateHighlight();
                else if (isEnabled() && count() > 0)
                    showPopup();
                return true;
            }
            const bool navigationKey = key == Qt::Key_Down || key == Qt::Key_Up
                                       || key == Qt::Key_PageUp || key == Qt::Key_PageDown
                                       || key == Qt::Key_Home || key == Qt::Key_End;
            if (!navigationKey)
                break;
            if (key == Qt::Key_Down && !isPopupVisible()) {
                if (isEnabled() && count() > 0)
                    showPopup();
                return true;
            }
            if (isPopupVisible()) {
                AgSelectStyle *s = style();
                const int page = qMax(1, (m_listView->viewport()->height()
                                          - 2 * s->itemSpacing())
                                             / qMax(1, s->itemHeight() + s->itemSpacing()) - 1);
                switch (key) {
                case Qt::Key_Down: moveHighlight(1); break;
                case Qt::Key_Up: moveHighlight(-1); break;
                case Qt::Key_PageDown: moveHighlight(page); break;
                case Qt::Key_PageUp: moveHighlight(-page); break;
                case Qt::Key_Home: moveHighlight(-m_proxy->rowCount()); break;
                case Qt::Key_End: moveHighlight(m_proxy->rowCount()); break;
                default: break;
                }
                return true;
            }
            return true; // 面板关闭时吞掉方向键
        }
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QSize AgSelect::sizeHint() const
{
    return QSize(120, m_preferredHeight);
}

QSize AgSelect::minimumSizeHint() const
{
    return QSize(60, m_preferredHeight);
}

// ---------------------------------------------------------------------------
// 私有实现

void AgSelect::onGlobalStyleChanged()
{
    if (m_styleFromSizeMode && m_ownStyle) {
        // 派生样式：跟随全局主题重新继承配色，尺寸档位保持不变
        // （注意：此时对 style() 的手动微调会被覆盖）
        *m_ownStyle = *AgSelectStyle::globalStyle();
        m_ownStyle->setSizeMode(m_overrideSizeMode);
        if (m_delegate)
            m_delegate->setStyle(m_ownStyle);
        applyStyleToWidgets();
        return;
    }
    if (!m_ownStyle)
        applyStyleToWidgets();
}

void AgSelect::ensurePopup()
{
    if (m_panel)
        return;

    m_panel = new AgPopupPanel(this);

    m_listView = new AgSelectListView();
    m_listView->setModel(m_proxy);
    m_listView->setFrameShape(QFrame::NoFrame);
    m_listView->setFocusPolicy(Qt::NoFocus);
    m_listView->setMouseTracking(true);
    m_listView->setSelectionMode(QAbstractItemView::NoSelection);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 不开 uniformItemSizes：组名行与选项行高不同，开启后列表用第一行的尺寸
    // 铺排全部行并估算内容高度，内容高度虚增 / 行高错乱，选项不多时也会出滚动条
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setVerticalScrollBar(new AgScrollBar(Qt::Vertical));

    m_delegate = new AgSelectItemDelegate(style(), this);
    m_delegate->setMultiSelection(m_multiSelection);
    m_listView->setItemDelegate(m_delegate);

    m_panel->setContentWidget(m_listView);

    connect(m_listView, &QListView::clicked, this, [this](const QModelIndex &index) {
        handleItemClicked(index);
    });
    connect(m_listView->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &) {
                setViewHighlightRow(current.isValid() ? m_proxy->mapToSource(current).row() : -1);
            });
    connect(m_panel, &AgPopupPanel::aboutToClose, this, [this]() {
        rotateArrow(false); // 箭头随收起转回（覆盖所有收起路径）
        emit popupAboutToHide();
    });
    connect(m_panel, &AgPopupPanel::closed, this, [this]() {
        emit popupHidden();
        revertUnmatchedText();
    });
    connect(m_panel, &AgPopupPanel::shown, this, [this]() {
        emit popupShown();
        scrollToCurrentRow(); // 展开动画结束后按最终视口再校正一次定位
    });

    applyStyleToWidgets();
}

void AgSelect::applyStyleToWidgets()
{
    AgSelectStyle *s = style();

    // 全局样式对象可能被整体替换，委托持有的指针必须同步刷新
    if (m_delegate)
        m_delegate->setStyle(s);

    // 字体：控件显式 setFont() 时优先于样式字体（WA_SetFont 语义），
    // 否则使用样式字体（随尺寸档位）；编辑框（含 chips 绘制）与下拉面板共用
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

    if (AgSelectDefaultArrow *arrow = qobject_cast<AgSelectDefaultArrow *>(m_arrowWidget))
        arrow->setArrowColor(isEnabled() ? s->arrowColor() : s->editorDisabledForegroundColor());

    if (m_panel) {
        m_panel->setBackgroundColor(s->panelBackgroundColor());
        m_panel->setBorderColor(s->panelBorderColor());
        m_panel->setCornerRadius(s->panelBorderRadius());
        m_panel->setShadowColor(s->panelShadowColor());
        m_panel->setShadowRadius(s->panelShadowRadius());
    }

    if (m_listView) {
        m_listView->setFont(effectiveFont);
        // 视口保持透明：透出面板自身背景色。不走调色板 Base —— 窗口级样式表
        // 激活时调色板会被 repolish 覆盖（暗色主题下出现白底白字）
        m_listView->viewport()->setAutoFillBackground(false);
        m_listView->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
        m_listView->setInnerMargins(QMargins(2, 2, 2, 2));
    }

    relayoutEditor();
    update();
}

void AgSelect::relayoutEditor()
{
    AgSelectStyle *s = style();
    const int border = s->editorBorderWidth();
    const int hPad = s->editorHorizontalPadding();
    const int slot = s->arrowSlotWidth();
    const QRect inner = innerRect();

    m_preferredHeight = editorHeight();
    m_chipRects.clear();
    m_chipRemoveRects.clear();

    if (!m_multiSelection) {
        m_lineEdit->setGeometry(inner.adjusted(hPad, 0, -slot, 0));
    } else {
        const int chipH = qMin(s->chipHeight(), editorHeight() - 2 * border - 4);
        const int gap = s->chipSpacing();
        const QFontMetrics fm(m_lineEdit->font());
        const int chipPadX = 6;
        const int removeSize = chipH;
        const int lineStartX = inner.left() + hPad;
        const int lineEndX = inner.right() - slot + 1;
        const int maxChipW = qMax(removeSize + 2 * chipPadX, lineEndX - lineStartX);
        const int vPad = qMax(2, (editorHeight() - 2 * border - chipH) / 2);

        int x = lineStartX;
        int y = inner.top() + vPad;
        const QList<int> checked = checkedIndexes();
        for (int row : checked) {
            const QString text = itemText(row);
            int chipW = fm.horizontalAdvance(text) + removeSize + 2 * chipPadX;
            chipW = qBound(removeSize + 2 * chipPadX, chipW, maxChipW);
            if (x > lineStartX && x + chipW > lineEndX) { // 换行
                x = lineStartX;
                y += chipH + gap;
            }
            m_chipRects.append(qMakePair(row, QRect(x, y, chipW, chipH)));
            m_chipRemoveRects.append(QRect(x + chipW - chipPadX - removeSize, y,
                                           removeSize, chipH));
            x += chipW + gap;
        }

        // 行编辑框跟在最后一个 chip 之后；剩余宽度不足时另起一行
        int inputX = qMin(x, lineEndX - 1);
        if (!checked.isEmpty() && x - gap > lineEndX - 24) {
            inputX = lineStartX;
            y += chipH + gap;
        }
        const int inputW = qMax(24, lineEndX - inputX);
        m_lineEdit->setGeometry(inputX, y, inputW, chipH);

        const int lines = (y - (inner.top() + vPad)) / (chipH + gap) + 1;
        m_preferredHeight = 2 * border + 2 * vPad + lines * chipH + (lines - 1) * gap;
    }

    // 箭头槽位（几何与清除图标一致，见 arrowIconRect）
    if (m_arrowWidget)
        m_arrowWidget->setGeometry(arrowIconRect());

    if (minimumHeight() != m_preferredHeight)
        setMinimumHeight(m_preferredHeight);
    updateGeometry();
    update();
}

int AgSelect::editorHeight() const
{
    return qMax(style()->editorHeight(), style()->editorBorderWidth() * 2 + 14);
}

int AgSelect::borderSpan() const
{
    return 2 * style()->editorBorderWidth();
}

QRect AgSelect::innerRect() const
{
    const int border = style()->editorBorderWidth();
    return rect().adjusted(border, border, -border, -border);
}

int AgSelect::arrowSlotWidth() const
{
    return style()->arrowSlotWidth();
}

QRect AgSelect::arrowSlotRect() const
{
    const int slot = arrowSlotWidth();
    return QRect(innerRect().right() - slot, 0, slot + borderSpan(), height());
}

QRect AgSelect::arrowIconRect() const
{
    AgSelectStyle *s = style();
    const int slot = s->arrowSlotWidth();
    const int w = qMin(slot - 4, 16);
    const int h = qMin(editorHeight() - 2 * s->editorBorderWidth() - 4, 16);
    const QRect inner = innerRect();
    return QRect(inner.right() - slot + (slot - w) / 2, inner.center().y() - h / 2, w, h);
}

bool AgSelect::clearIconActive() const
{
    // 悬停编辑框（本体或行编辑框）且已有选择项时，箭头切换为实心圆圈 ×
    return m_clearButtonEnabled && (m_areaHovered || m_lineHovered) && hasSelection();
}

void AgSelect::updateIconCursor(const QPoint &pos)
{
    setCursor(clearIconActive() && arrowSlotRect().contains(pos) ? Qt::PointingHandCursor
                                                                 : Qt::ArrowCursor);
}

void AgSelect::updateEditorText(const QString &text)
{
    if (m_lineEdit->text() != text)
        m_lineEdit->setText(text); // setText 不触发 textEdited，不会误启动过滤
}

void AgSelect::revertUnmatchedText()
{
    const QString text = m_lineEdit->text();
    if (m_multiSelection) {
        if (!text.isEmpty()) {
            m_lineEdit->clear();
            applyFilter(QString());
        }
        return;
    }
    if (!m_editable)
        return; // 只读模式文本始终与当前项一致

    const int matched = findText(text, Qt::MatchExactly);
    if (matched >= 0) {
        if (matched != m_currentIndex)
            setCurrentIndex(matched);
    } else {
        const QString fallback = m_currentIndex >= 0 ? itemText(m_currentIndex) : QString();
        if (text != fallback) {
            updateEditorText(fallback);
            emit currentTextChanged(fallback);
        }
    }
}

void AgSelect::applyFilter(const QString &text)
{
    m_proxy->setFilterText(text);
    if (m_delegate)
        m_delegate->setFilterText(text);

    if (!m_panel || !m_panel->isVisible())
        return;

    const int first = m_proxy->firstVisibleSourceRow();
    setViewHighlightRow(first);
    if (first >= 0) {
        const QModelIndex proxyIndex = m_proxy->mapFromSource(m_model->index(first, 0));
        if (proxyIndex.isValid())
            m_listView->scrollTo(proxyIndex, QAbstractItemView::EnsureVisible);
    }
}

void AgSelect::activateSourceRow(int sourceRow)
{
    const AgSelectListModel::Record *record = m_model->recordAt(sourceRow);
    if (!record || record->type != AgSelectListModel::Record::Item || !record->enabled)
        return;

    if (m_multiSelection) {
        const bool checked = !m_model->checkedRows().contains(sourceRow);
        m_model->setChecked(sourceRow, checked);
        emit activated(sourceRow);
        emit checkedChanged(checkedIndexes());
        relayoutEditor();
        if (m_currentIndex != sourceRow) {
            m_currentIndex = sourceRow; // 多选模式：当前项 = 最近激活项
            emit currentIndexChanged(sourceRow);
        }
    } else {
        setCurrentIndex(sourceRow);
        emit activated(sourceRow);
        hidePopup();
    }
}

void AgSelect::handleItemClicked(const QModelIndex &proxyIndex)
{
    if (!proxyIndex.isValid())
        return;
    const int sourceRow = m_proxy->mapToSource(proxyIndex).row();
    setViewHighlightRow(sourceRow);
    activateSourceRow(sourceRow);
}

void AgSelect::syncCurrentRowToModel()
{
    if (m_multiSelection)
        return;
    QSet<int> rows;
    if (m_currentIndex >= 0)
        rows.insert(m_currentIndex);
    m_model->setCheckedRows(rows);
}

void AgSelect::setViewHighlightRow(int sourceRow)
{
    m_model->setHighlightRow(sourceRow);
    if (sourceRow < 0 || !m_listView)
        return;
    const QModelIndex proxyIndex = m_proxy->mapFromSource(m_model->index(sourceRow, 0));
    if (proxyIndex.isValid() && m_listView->currentIndex() != proxyIndex)
        m_listView->setCurrentIndex(proxyIndex); // 触发 currentChanged -> 同值回写，无循环
}

int AgSelect::acceptableRowFrom(int startRow, int step) const
{
    const int count = m_proxy->rowCount();
    if (count == 0)
        return -1;
    int row = qBound(0, startRow, count - 1);
    const int direction = step >= 0 ? 1 : -1;
    while (row >= 0 && row < count) {
        const QModelIndex proxyIndex = m_proxy->index(row, 0);
        const int sourceRow = m_proxy->mapToSource(proxyIndex).row();
        const AgSelectListModel::Record *record = m_model->recordAt(sourceRow);
        if (record && record->type == AgSelectListModel::Record::Item && record->enabled)
            return row;
        row += direction;
    }
    return -1;
}

void AgSelect::moveHighlight(int step)
{
    if (!m_listView || m_proxy->rowCount() == 0)
        return;

    const QModelIndex current = m_listView->currentIndex();
    const int count = m_proxy->rowCount();
    const int start = current.isValid() ? current.row() : (step > 0 ? -1 : count);
    const int target = qBound(0, start + step, count - 1);

    int found = acceptableRowFrom(target, step);
    if (found < 0)
        found = acceptableRowFrom(qBound(0, start, count - 1), -step);
    if (found < 0)
        return;

    const QModelIndex proxyIndex = m_proxy->index(found, 0);
    setViewHighlightRow(m_proxy->mapToSource(proxyIndex).row());
    m_listView->scrollTo(proxyIndex, QAbstractItemView::EnsureVisible);
}

void AgSelect::activateHighlight()
{
    if (!m_listView)
        return;
    handleItemClicked(m_listView->currentIndex());
}

void AgSelect::scrollToCurrentRow()
{
    if (!m_listView)
        return;

    int sourceRow = -1;
    if (m_multiSelection) {
        const QList<int> checked = checkedIndexes();
        if (!checked.isEmpty())
            sourceRow = checked.first(); // 需求 14：定位到第一个选中项
    } else {
        sourceRow = m_currentIndex;
    }

    if (sourceRow < 0) {
        m_listView->scrollToTop();
        return;
    }
    const QModelIndex proxyIndex = m_proxy->mapFromSource(m_model->index(sourceRow, 0));
    if (proxyIndex.isValid()) {
        setViewHighlightRow(sourceRow);
        m_listView->scrollTo(proxyIndex, QAbstractItemView::EnsureVisible);
    }
}

int AgSelect::fitContentWidth() const
{
    AgSelectStyle *s = style();
    const QFontMetrics fm(m_listView ? m_listView->font() : font());
    int maxText = 0;
    bool hasIcon = false;
    for (int row = 0; row < m_model->count(); ++row) {
        const AgSelectListModel::Record *record = m_model->recordAt(row);
        if (!record)
            continue;
        int width = fm.horizontalAdvance(record->text);
        if (record->type == AgSelectListModel::Record::Item && record->groupId >= 0)
            width += s->itemIndentWidth();
        maxText = qMax(maxText, width);
        hasIcon |= !record->icon.isNull();
    }
    return maxText + 2 * s->itemHorizontalPadding() + 4 + 12 + (hasIcon ? 24 : 0);
}

int AgSelect::fullContentHeight() const
{
    AgSelectStyle *s = style();
    const int rows = m_model->count();
    if (rows == 0)
        return 8;
    int height = 4; // 列表内容区上下内边距
    for (int row = 0; row < rows; ++row) {
        const AgSelectListModel::Record *record = m_model->recordAt(row);
        const bool group = record && record->type == AgSelectListModel::Record::GroupHeader;
        height += group ? s->groupHeaderHeight() : s->itemHeight();
        height += s->itemSpacing(); // 行高包含间距（见委托 sizeHint）
    }
    return height;
}

int AgSelect::effectiveMaxPopupHeight() const
{
    return m_maxPopupHeight > 0 ? m_maxPopupHeight : style()->panelMaxHeight();
}

int AgSelect::chipAt(const QPoint &pos) const
{
    for (int i = 0; i < m_chipRects.size(); ++i) {
        if (m_chipRects.at(i).second.contains(pos))
            return i;
    }
    return -1;
}

#include "AgSelect.moc"
