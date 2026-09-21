#ifndef AGSELECTDEMOWINDOW_H
#define AGSELECTDEMOWINDOW_H

#include "DemoWindowBase.h"

class AgSelect;
class QTextEdit;

/**
 * @brief AgSelect 下拉选择示例：单选 / 分组 / 禁用项 / 可编辑过滤 / 多选 chips /
 *        三种尺寸 / 亮暗主题 / 面板宽度模式 / 自定义箭头与事件回调日志。
 */
class AgSelectDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit AgSelectDemoWindow(QWidget *parent = nullptr);

private:
    void makeItemIcons();
    void log(const QString &text);

    AgSelect *m_basicSelect;        // 分组 + 禁用项（只读）
    AgSelect *m_editableSelect;     // 可编辑过滤
    AgSelect *m_multiSelect;        // 多选 chips
    AgSelect *m_smallSelect;
    AgSelect *m_mediumSelect;
    AgSelect *m_largeSelect;
    AgSelect *m_widthSelect;        // 固定宽度面板
    AgSelect *m_customArrowSelect;  // 自定义箭头 + 自定义委托（图标）
    QTextEdit *m_logView;
};

#endif // AGSELECTDEMOWINDOW_H
