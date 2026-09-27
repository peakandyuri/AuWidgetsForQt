#include "AuDefaultCellEditor.h"

#include <QLineEdit>

QWidget *AuDefaultCellEditor::createEditor(QWidget *parent) const
{
    QLineEdit *lineEdit = new QLineEdit(parent);
    lineEdit-> setFrame(false);
    return lineEdit;
}

void AuDefaultCellEditor::setEditorValue(QWidget *editor, const QVariant &value) const
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
    if (lineEdit)
        lineEdit->setText(value.toString());
}

QVariant AuDefaultCellEditor::editorValue(QWidget *editor) const
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
    if (lineEdit)
        return lineEdit->text();
    return QVariant();
}

void AuDefaultCellEditor::setInitialTypedText(QWidget *editor, const QString &text) const
{
    QLineEdit *lineEdit = qobject_cast<QLineEdit *>(editor);
    if (!lineEdit)
        return;
    lineEdit->setText(text);
    lineEdit->setCursorPosition(text.length());
}
