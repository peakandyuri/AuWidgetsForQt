// ============================================================================
// AuGridWidget 编辑实现：单元格编辑器生命周期（进入 / 提交校验 / 关闭）、
// 校验错误提示、编辑器几何、Tab 编辑导航查找、编辑器事件过滤。
// ============================================================================

#include "AuGridWidget.h"
#include "AuDefaultCellEditor.h"
#include "AuMultiLineCellEditor.h"
#include "AuGridWidget_p.h"

#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QLabel>

bool AuGridWidget::editCell(int row, int column)
{
    return editCell(row, column, QString());
}

bool AuGridWidget::editCell(int row, int column, const QString &initialText)
{

    if (!m_model || row < 0 || row >= m_model->rowCount()
        || column < 0 || column >= m_model->columnCount())
        return false;

    // 编辑合并单元格时始终编辑锚点数据
    const CellSpan span = spanAt(row, column);
    if (span.isValid()) {
        row = span.anchorRow;
        column = span.anchorColumn;
    }

    if (!isCellEditable(row, column))
        return false;

    // 内置复选框列通过点击/空格切换，不使用文本编辑器
    if (m_model->columnDefAt(column).isCheckbox())
        return false;

    // 如果当前有编辑器，先尝试结束当前编辑
    if (m_activeEditor && !tryFinishCurrentEdit())
        return false;

    if (m_activeEditor)
        closeEditor(false);

    QSharedPointer<AuCellEditor> editor = editorImplForColumn(column);

    QWidget *editorWidget = editor->createEditor(this);
    if (!editorWidget)
        return false;

    editor->setEditorValue(editorWidget, m_model->cellValue(row, column));
    if (!initialText.isEmpty())
        editor->setInitialTypedText(editorWidget, initialText);

    editorWidget->installEventFilter(this);
    editorWidget->setAutoFillBackground(true);
    editorWidget->setAttribute(Qt::WA_Hover, true); // 校验失败时悬停编辑框可重新唤起错误提示
    QPalette pal = editorWidget->palette();
    const QColor bgColor = m_style->color(AuGridStyle::CellBackgroundColor, m_model, row, column);
    pal.setColor(QPalette::Base, bgColor.isValid() ? bgColor : palette().base().color());
    editorWidget->setPalette(pal);

    m_activeEditor = editorWidget;
    m_activeEditorImpl = editor;
    m_editRow = row;
    m_editColumn = column;

    ensureCellVisible(row, column);
    updateEditorGeometry();
    editorWidget->setFocus();
    editorWidget->show();

    const bool selectionDidChange = (m_selectedRow != row || m_selectedColumn != column);
    m_selectedRow = row;
    m_selectedColumn = column;
    m_selectedOriginalRow = m_model->originalRowForVisibleRow(row);
    emit cellEditingStarted(row, column);
    if (selectionDidChange)
        emit selectionChanged();
    return true;
}

bool AuGridWidget::isEditing() const
{
    return m_activeEditor != nullptr;
}

void AuGridWidget::closeEditor(bool commit)
{
    if (!m_activeEditor)
        return;

    // 编辑结束即清除错误提示（编辑器红框 + 提示浮层）
    clearEditError();

    QWidget *editor = m_activeEditor;
    const int row = m_editRow;
    const int column = m_editColumn;
    QSharedPointer<AuCellEditor> editorImpl = m_activeEditorImpl;

    m_committing = true;
    m_activeEditor = nullptr;
    m_activeEditorImpl.reset();
    m_editRow = -1;
    m_editColumn = -1;

    editor->removeEventFilter(this);

    if (commit) {
        const QString field = m_model ? m_model->fieldAtColumn(column) : QString();
        if (!field.isEmpty() && editorImpl) {
            const QVariant value = editorImpl->editorValue(editor);
            if (m_model->setCellValue(row, field, value))
                emit cellValueChanged(row, column, value);
        }
    }

    emit cellEditingFinished(row, column);
    editor->deleteLater();
    // 编辑器子树仍持有焦点时（如 AuSelect / AuDatePicker 选择后主动提交），
    // 把焦点还给表格：否则编辑器销毁后焦点悬空，Tab / Enter / 方向键等键盘导航失效
    QWidget *focused = QApplication::focusWidget();
    if (focused && (focused == editor || editor->isAncestorOf(focused)))
        setFocus();
    m_committing = false;
    update();
}

void AuGridWidget::setCellEditValidator(QSharedPointer<AuCellEditValidator> validator)
{
    m_cellEditValidator = validator;
}

void AuGridWidget::setCellEditableCallback(const std::function<bool(const AuGridModel *, int, int)> &callback)
{
    m_cellEditableCallback = callback;
}

bool AuGridWidget::isCellEditable(int row, int column) const
{
    if (!m_model || row < 0 || row >= m_model->rowCount()
        || column < 0 || column >= m_model->columnCount())
        return false;

    if (!m_model->columnDefAt(column).isEditable())
        return false;

    if (m_cellEditableCallback)
        return m_cellEditableCallback(m_model, row, column);

    return true;
}

void AuGridWidget::toggleCheckboxCellValue(int row, int column)
{
    if (!m_model || row < 0 || row >= m_model->rowCount()
        || column < 0 || column >= m_model->columnCount())
        return;

    if (!m_model->columnDefAt(column).isCheckbox())
        return;
    if (!isCellEditable(row, column))
        return;

    const bool next = !m_model->cellValue(row, column).toBool();
    if (m_model->setCellValue(row, column, next))
        emit cellCheckStateChanged(row, column, next);
}

bool AuGridWidget::tryFinishCurrentEdit(bool focusOut)
{
    if (!m_activeEditor || !m_model)
        return true;

    clearEditError();

    const int row = m_editRow;
    const int column = m_editColumn;
    // 取值 / 校验必须用编辑时实际选用的实现（m_activeEditorImpl）：
    // editorImplForColumn 不含 WordWrap 回退等分支时，对多行编辑器做
    // qobject_cast 会得到空值，校验将永远针对空文本进行
    QSharedPointer<AuCellEditor> editorImpl = m_activeEditorImpl;
    if (!editorImpl)
        editorImpl = editorImplForColumn(column);

    const QVariant oldValue = m_model->cellValue(row, column);
    const QVariant newValue = editorImpl->editorValue(m_activeEditor);

    if (m_cellEditValidator) {
        QString errorMessage;
        const AuEditValidationResult result = m_cellEditValidator->validate(
            m_model, row, column, oldValue, newValue, errorMessage);

        switch (result) {
        case AuEditValidationResult::Accept:
            closeEditor(true);
            return true;
        case AuEditValidationResult::Reject:
            closeEditor(false);
            return true;
        case AuEditValidationResult::RejectKeepEditing:
            if (focusOut) {
                // 失焦时校验不通过直接取消
                closeEditor(false);
                return true;
            }
            showEditError(errorMessage);
            ensureCellVisible(row, column);
            updateEditorGeometry();
            if (m_activeEditor)
                m_activeEditor->setFocus();
            return false;
        }
    }

    closeEditor(true);
    return true;
}

void AuGridWidget::showEditError(const QString &message)
{
    if (!m_activeEditor)
        return;

    m_activeEditor->setStyleSheet("border: 2px solid red;");

    // 常驻错误提示浮层：紧贴编辑框正下方（间隙 2px），持续显示到校验通过
    //（替代 QToolTip：只弹一次、约 20px 悬浮间距、超时自动消失的观感都不符合需求）
    if (!m_editErrorTip) {
        m_editErrorTip = new QLabel(this);
        m_editErrorTip->setTextFormat(Qt::PlainText);
        m_editErrorTip->setStyleSheet(QStringLiteral(
            "QLabel { background-color: #fff1f0; color: #cf1322;"
            " border: 1px solid #ffa39e; border-radius: 4px; padding: 3px 8px; }"));
    }
    m_editErrorTip->setText(message);
    m_editErrorTip->adjustSize();
    updateEditErrorTipGeometry();
    m_editErrorTip->show();
    m_editErrorTip->raise();
}

void AuGridWidget::clearEditError()
{
    // 编辑器已被 closeEditor 清空时也要把残留的提示浮层收掉
    if (m_editErrorTip && m_editErrorTip->isVisible())
        m_editErrorTip->hide();

    if (!m_activeEditor)
        return;

    m_activeEditor->setStyleSheet(QString());
}

void AuGridWidget::refreshEditError()
{
    // 校验失败期间（提示浮层可见）按键时即时复查：
    // 通过则清除提示，不通过则按最新输入刷新提示内容与位置
    if (!m_editErrorTip || !m_editErrorTip->isVisible())
        return;
    if (!m_activeEditor || !m_model || !m_cellEditValidator) {
        clearEditError();
        return;
    }

    const int row = m_editRow;
    const int column = m_editColumn;
    // 与 tryFinishCurrentEdit 一致：用实际编辑器实现取值，避免 WordWrap
    // 列解析到单行实现后总是校验空文本
    const QSharedPointer<AuCellEditor> editorImpl = m_activeEditorImpl
            ? m_activeEditorImpl : editorImplForColumn(column);
    if (!editorImpl) {
        clearEditError();
        return;
    }

    const QVariant oldValue = m_model->cellValue(row, column);
    const QVariant newValue = editorImpl->editorValue(m_activeEditor);
    QString errorMessage;
    const AuEditValidationResult result = m_cellEditValidator->validate(
        m_model, row, column, oldValue, newValue, errorMessage);

    if (result == AuEditValidationResult::RejectKeepEditing) {
        if (!errorMessage.isEmpty())
            m_editErrorTip->setText(errorMessage);
        m_editErrorTip->adjustSize();
        updateEditErrorTipGeometry();
        m_editErrorTip->raise();
    } else {
        clearEditError();
    }
}

void AuGridWidget::updateEditErrorTipGeometry()
{
    if (!m_editErrorTip || !m_activeEditor)
        return;

    const QRect editorRect = m_activeEditor->geometry();
    const int gap = 2;
    const int scrollBarExtent = m_style ? m_style->integer(AuGridStyle::ScrollBarExtent, m_model) : 0;
    const int bottomLimit = height() - (m_hScrollBarVisible ? scrollBarExtent : 0);

    // 优先贴编辑框下方；编辑框贴近表格底部放不下时翻转到上方
    int y = editorRect.bottom() + 1 + gap;
    if (y + m_editErrorTip->height() > bottomLimit)
        y = editorRect.top() - gap - m_editErrorTip->height();
    const int x = qBound(0, editorRect.left(), qMax(0, width() - m_editErrorTip->width()));
    m_editErrorTip->setGeometry(x, y, m_editErrorTip->width(), m_editErrorTip->height());
}

QSharedPointer<AuCellEditor> AuGridWidget::editorImplForColumn(int column) const
{
    if (!m_model || column < 0 || column >= m_model->columnCount())
        return m_defaultCellEditor;

    // 与 editCell 的选择逻辑保持一致（含 WordWrap 多行回退），
    // 否则外部按列反查实现时会对多行编辑器解析到单行实现
    const AuColumnDef def = m_model->columnDefAt(column);
    const QString editorName = def.cellEditor();
    if (!editorName.isEmpty()) {
        auto it = m_cellEditors.find(editorName);
        if (it != m_cellEditors.end())
            return it.value();
    }
    if (def.textWrapMode() == AuColumnDef::TextWrapMode::WordWrap)
        return m_defaultMultiLineCellEditor;
    return m_defaultCellEditor;
}

void AuGridWidget::updateEditorGeometry()
{
    if (!m_activeEditor || !m_model)
        return;

    QRect rect = cellRect(m_editRow, m_editColumn);
    const int bodyTop = headerHeight();
    const int bodyBottom = height() - (m_hScrollBarVisible ? m_style->integer(AuGridStyle::ScrollBarExtent, m_model) : 0);
    const int viewportW = viewportWidthForColumns();

    const bool visible = !(rect.isEmpty() || rect.bottom() <= bodyTop || rect.top() >= bodyBottom
                           || rect.right() <= 0 || rect.left() >= viewportW);

    if (visible) {
        // 裁剪到可视区域
        if (rect.top() < bodyTop)
            rect.setTop(bodyTop);
        if (rect.bottom() > bodyBottom)
            rect.setBottom(bodyBottom);

        // body 单元格（含合并区域）不能覆盖到固定列/停靠列上
        if (columnRegion(m_editColumn) == ColumnRegion::Body) {
            if (rect.left() < stickyLeftBoundaryX())
                rect.setLeft(stickyLeftBoundaryX());
            if (rect.right() > stickyRightBoundaryX() - 1)
                rect.setRight(stickyRightBoundaryX() - 1);
        } else {
            if (rect.left() < 0)
                rect.setLeft(0);
            if (rect.right() > viewportW)
                rect.setRight(viewportW);
        }

        // 四边各留 1px 给单元格边框线：编辑框若完全覆盖单元格，会把与上一行 /
        // 下一行之间的分隔线一并盖住，视觉上与相邻行粘连（尤其无边框的文本编辑框）
        rect.adjust(1, 1, -1, -1);

        if (rect.width() > 0 && rect.height() > 0) {
            m_activeEditor->setGeometry(rect);
            if (!m_activeEditor->isVisible())
                m_activeEditor->show();
        } else if (m_activeEditor->isVisible()) {
            m_activeEditor->hide();
        }
    } else {
        if (m_activeEditor->isVisible())
            m_activeEditor->hide();
    }

    // 编辑器几何跟随（滚动 / 布局变化）时，错误提示浮层保持贴住编辑框
    if (m_editErrorTip && m_editErrorTip->isVisible()) {
        updateEditErrorTipGeometry();
        m_editErrorTip->raise();
    }
}

bool AuGridWidget::findNextEditableCell(int &row, int &column, int dColumn) const
{
    if (!m_model || m_model->rowCount() == 0 || m_model->columnCount() == 0 || qAbs(dColumn) != 1)
        return false;

    const int colCount = m_model->columnCount();
    const int rowCount = m_model->rowCount();

    int r = row;
    int c = column + dColumn;

    while (r >= 0 && r < rowCount) {
        while (c >= 0 && c < colCount) {
            if (!isColumnHidden(c) && !isSpanCoveredCell(r, c)
                && isCellFocusable(r, c)
                && isCellEditable(r, c) && !m_model->columnDefAt(c).isCheckbox()) {
                row = r;
                column = c;
                return true;
            }
            c += dColumn;
        }
        r += dColumn;
        c = (dColumn > 0) ? 0 : colCount - 1;
    }
    return false;
}

bool AuGridWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_activeEditor)
        return false;

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch (keyEvent->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            if (m_activeEditorImpl && m_activeEditorImpl->wantsReturnForNewLine())
                return false;
            if (tryFinishCurrentEdit())
                setFocus();
            return true;
        case Qt::Key_Escape:
            clearEditError();
            closeEditor(false);
            setFocus();
            return true;
        case Qt::Key_Tab:
        case Qt::Key_Backtab:
            keyPressEvent(keyEvent);
            return true;
        default:
            // 校验失败期间按键：即时复查，通过则清除提示，不通过则刷新内容位置
            refreshEditError();
            break;
        }
    } else if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverMove) {
        // 悬停编辑框时确保错误提示可见并贴住编辑框下方
        if (m_editErrorTip && m_editErrorTip->isVisible()) {
            updateEditErrorTipGeometry();
            m_editErrorTip->raise();
        }
    } else if (event->type() == QEvent::FocusOut) {
        if (m_committing)
            return false;
        // 焦点仍在编辑器自身或其子控件内部时不算失焦：复合编辑器（如 AuSelect /
        // AuDatePicker 展开面板时把焦点移入内部行编辑框）内部挪焦点会向编辑器
        // 本体发 FocusOut——若按失焦提交，面板刚弹出编辑器就被提交销毁，
        // 表现为“面板只剩文本框”
        QWidget *focus = QApplication::focusWidget();
        if (focus && (focus == m_activeEditor || m_activeEditor->isAncestorOf(focus)))
            return false;
        // 编辑器自有弹层（QComboBox 弹层 / AuSelect、AuDatePicker 面板）被点击
        // 或激活时，Linux/Wayland 会把焦点交给弹层窗口或短暂置空，不算失焦，
        // 见 auEditorPopupActive
        if (QApplication::activePopupWidget() || auEditorPopupActive(m_activeEditor))
            return false;
        tryFinishCurrentEdit(true);
        return false;
    }

    return false;
}

