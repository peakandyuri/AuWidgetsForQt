#include "AuMultiLineCellEditor.h"

#include <QPlainTextEdit>

QWidget *AuMultiLineCellEditor::createEditor(QWidget *parent) const
{
    QPlainTextEdit *editor = new QPlainTextEdit(parent);
    editor->setFrameStyle(QFrame::NoFrame);
    editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return editor;
}

void AuMultiLineCellEditor::setEditorValue(QWidget *editor, const QVariant &value) const
{
    if (auto *e = qobject_cast<QPlainTextEdit *>(editor))
        e->setPlainText(value.toString());
}

QVariant AuMultiLineCellEditor::editorValue(QWidget *editor) const
{
    if (auto *e = qobject_cast<QPlainTextEdit *>(editor))
        return e->toPlainText();
    return QVariant();
}

void AuMultiLineCellEditor::setInitialTypedText(QWidget *editor, const QString &text) const
{
    if (auto *e = qobject_cast<QPlainTextEdit *>(editor)) {
        e->setPlainText(text);
        e->moveCursor(QTextCursor::End);
    }
}

bool AuMultiLineCellEditor::wantsReturnForNewLine() const
{
    return true;
}
