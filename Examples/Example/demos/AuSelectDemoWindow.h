#ifndef AUSELECTDEMOWINDOW_H
#define AUSELECTDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuSelect;
class QTextEdit;

/**
 * @brief AuSelect 下拉选择示例：单选 / 分组 / 禁用项 / 可编辑过滤 / 多选 chips /
 *        三种尺寸 / 亮暗主题 / 面板宽度模式 / 自定义箭头与事件回调日志。
 */
class AuSelectDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit AuSelectDemoWindow(QWidget *parent = nullptr);

private:
    void makeItemIcons();
    void log(const QString &text);

    AuSelect *m_basicSelect;        // 分组 + 禁用项（只读）
    AuSelect *m_editableSelect;     // 可编辑过滤
    AuSelect *m_multiSelect;        // 多选 chips
    AuSelect *m_smallSelect;
    AuSelect *m_mediumSelect;
    AuSelect *m_largeSelect;
    AuSelect *m_widthSelect;        // 固定宽度面板
    AuSelect *m_customArrowSelect;  // 自定义箭头 + 自定义委托（图标）
    QTextEdit *m_logView;
};

#endif // AUSELECTDEMOWINDOW_H
