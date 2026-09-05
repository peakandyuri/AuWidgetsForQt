#include "AgDefaultCellEditor.h"

#include <QLineEdit>

QWidget *AgDefaultCellEditor::createEditor(QWidget *parent) const
{
    QLineEdit *lineEdit = new QLineEdit(parent);
    lineEdit-> setFrame(false);
    return lineEdit;
}

void AgDefaultCellEditor::setEditorValue(QWidget *editor, const QVariant &value) const
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
    if (lineEdit)
        lineEdit->setText(value.toString());
}

QVariant AgDefaultCellEditor::editorValue(QWidget *editor) const
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
    if (lineEdit)
        return lineEdit->text();
    return QVariant();
}

void AgDefaultCellEditor::setInitialTypedText(QWidget *editor, const QString &text) const
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
    if (!lineEdit)
        return;
    lineEdit->setText(text);
    lineEdit->setCursorPosition(text.length());
}
