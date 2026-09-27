#ifndef AUCELLEDITOR_H
#define AUCELLEDITOR_H

#include "AuGrid_global.h"

#include <QVariant>
#include <QWidget>

/**
 * @brief 单元格编辑器抽象基类：把任意 QWidget 包装成表格编辑器。
 *
 * 通过 AuGridWidget::registerCellEditor(name, editor) 注册，
 * 再由 AuColumnDef::setCellEditor(name) 按列引用；未指定的列使用
 * AuDefaultCellEditor（单行输入）。
 *
 * 编辑生命周期：
 *  1. 双击 / Enter / F2 / 直接输入可打印字符触发编辑；
 *  2. createEditor() 创建编辑控件（parent 为 AuGridWidget）；
 *  3. setEditorValue() 写入模型当前值（直接输入时随后调 setInitialTypedText()）；
 *  4. 编辑期间滚动 / 布局变化时编辑器几何自动跟随；
 *  5. 提交时 editorValue() 取值 → 校验器裁决（AuCellEditValidator）→ 写入模型。
 *     Escape 取消编辑；Tab 提交并跳到下一个可编辑格。
 *
 * 内置实现：AuDefaultCellEditor（单行）、AuMultiLineCellEditor（多行）、
 * AuComboBoxCellEditor（下拉）。
 */
class AUGRID_EXPORT AuCellEditor
{
public:
    virtual ~AuCellEditor();

    // 创建编辑器 QWidget，parent 为 AuGridWidget
    /// 创建编辑器控件（纯虚）。控件生命周期由表格管理。
    virtual QWidget *createEditor(QWidget *parent) const = 0;

    // 将模型值设置到编辑器
    /// 把模型当前值写入编辑控件（纯虚）。
    virtual void setEditorValue(QWidget *editor, const QVariant &value) const = 0;

    // 从编辑器读取值
    /// 提交时从编辑控件读取最终值（纯虚）。
    virtual QVariant editorValue(QWidget *editor) const = 0;

    // 按键直接开始编辑时，把初始字符注入编辑器（默认无操作）
    /// 键盘直接输入字符触发编辑时，把该字符注入编辑器（如填入行编辑开头）。
    virtual void setInitialTypedText(QWidget *editor, const QString &text) const;

    // 返回 true 表示编辑器需要把 Return 键当作换行，而不是提交编辑
    /// 返回 true 时 Return 键作为换行而不是提交（多行编辑器使用）。
    virtual bool wantsReturnForNewLine() const;
};

#endif // AUCELLEDITOR_H
