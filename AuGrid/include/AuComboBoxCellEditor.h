#ifndef AUCOMBOBOXCELLEDITOR_H
#define AUCOMBOBOXCELLEDITOR_H

#include "AuGrid_global.h"
#include "AuCellEditor.h"

#include <QStringList>

/**
 * @brief 下拉选择编辑器：内部使用可编辑 QComboBox。
 *
 * 用法：
 * @code
 *   grid->registerCellEditor("combo",
 *       QSharedPointer<AuCellEditor>(new AuComboBoxCellEditor({"Alpha", "Beta"})));
 *   columnDef.setCellEditor("combo");
 *   columnDef.setEditable(true);
 * @endcode
 */
class AUGRID_EXPORT AuComboBoxCellEditor : public AuCellEditor
{
public:
    /// 构造并指定下拉选项（默认为空）。
    explicit AuComboBoxCellEditor(const QStringList &items = QStringList());

    /// 重新设置下拉选项。
    void setItems(const QStringList &items);
    /// 当前下拉选项。
    QStringList items() const;

    QWidget *createEditor(QWidget *parent) const override;
    void setEditorValue(QWidget *editor, const QVariant &value) const override;
    QVariant editorValue(QWidget *editor) const override;
    void setInitialTypedText(QWidget *editor, const QString &text) const override;

private:
    QStringList m_items;   ///< 下拉选项
};

#endif // AUCOMBOBOXCELLEDITOR_H
