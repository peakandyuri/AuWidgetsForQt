#ifndef AUGRIDWIDGET_P_H
#define AUGRIDWIDGET_P_H

//
// AuGridWidget 内部共享实现细节：仅供模块内各 AuGridWidget*.cpp 编译单元
// 使用，不属于公共 API，不随库对外安装。
//

#include <QApplication>
#include <QWidget>

/// 判断 widget 是否位于编辑器子树内。与 QWidget::isAncestorOf 不同：这里跨越
/// 顶层窗口边界（编辑器弹出的下拉面板 / QComboBox 弹层等都是挂在编辑器下的
/// 子顶级窗口，其 parentWidget() 链仍指向编辑器）。
inline bool auInEditorSubtree(const QWidget *editor, const QWidget *widget)
{
    while (widget) {
        if (widget == editor)
            return true;
        widget = widget->parentWidget();
    }
    return false;
}

/// 编辑器自有弹层（QComboBox 弹层 / AuSelect、AuDatePicker 的面板）是否处于打开状态。
/// Linux/Wayland 上点击这类弹层窗口会把激活与键盘焦点交给弹层（其内部通常没有
/// 可聚焦控件，焦点表现为空），且激活面板的同时宿主窗口失活——这些都不是真正的
/// "焦点离开编辑器"，不能据此提交销毁编辑器（Windows 由 MA_NOACTIVATE 天然规避）。
inline bool auEditorPopupActive(const QWidget *editor)
{
    const QWidget *popup = QApplication::activePopupWidget();
    if (popup && auInEditorSubtree(editor, popup))
        return true;
    const QWidget *active = QApplication::activeWindow();
    return active && active != editor->window() && auInEditorSubtree(editor, active);
}

#endif // AUGRIDWIDGET_P_H
