#ifndef AGCELLEDITOR_H
#define AGCELLEDITOR_H

#include "AgGrid_global.h"

#include <QVariant>
#include <QWidget>

class AGGRID_EXPORT AgCellEditor
{
public:
    virtual ~AgCellEditor();

    // 创建编辑器 QWidget，parent 为 AgGridWidget
    virtual QWidget *createEditor(QWidget *parent) const = 0;

    // 将模型值设置到编辑器
    virtual void setEditorValue(QWidget *editor, const QVariant &value) const = 0;

    // 从编辑器读取值
    virtual QVariant editorValue(QWidget *editor) const = 0;

    // 按键直接开始编辑时，把初始字符注入编辑器（默认无操作）
    virtual void setInitialTypedText(QWidget *editor, const QString &text) const;

    // 返回 true 表示编辑器需要把 Return 键当作换行，而不是提交编辑
    virtual bool wantsReturnForNewLine() const;
};

#endif // AGCELLEDITOR_H
