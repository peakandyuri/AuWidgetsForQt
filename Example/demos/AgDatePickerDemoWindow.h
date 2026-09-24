#ifndef AGDATEPICKERDEMOWINDOW_H
#define AGDATEPICKERDEMOWINDOW_H

#include "DemoWindowBase.h"

class AgDatePicker;
class QLabel;

/// AgDatePicker 日期选择功能演示：格式 / 范围 / 粒度 / footer / 尺寸 / 主题。
class AgDatePickerDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit AgDatePickerDemoWindow(QWidget *parent = nullptr);

private slots:
    void toggleTheme(bool dark);

private:
    QWidget *makeFooterCustomWidget();

    AgDatePicker *m_basicPicker;
    AgDatePicker *m_formatPicker;
    AgDatePicker *m_rangePicker;
    AgDatePicker *m_dateRangePicker;
    AgDatePicker *m_monthPicker;
    AgDatePicker *m_yearPicker;
    AgDatePicker *m_smallPicker;
    AgDatePicker *m_mediumPicker;
    AgDatePicker *m_largePicker;
    AgDatePicker *m_heightPicker;
    AgDatePicker *m_footerPicker;
    AgDatePicker *m_noFooterPicker;
    AgDatePicker *m_customFooterPicker;
    QLabel *m_valueLabel;
    bool m_darkTheme;
};

#endif // AGDATEPICKERDEMOWINDOW_H
