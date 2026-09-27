#ifndef AUDATEPICKERDEMOWINDOW_H
#define AUDATEPICKERDEMOWINDOW_H

#include "DemoWindowBase.h"

class AuDatePicker;
class QLabel;

/// AuDatePicker 日期选择功能演示：格式 / 范围 / 粒度 / footer / 尺寸 / 主题。
class AuDatePickerDemoWindow : public DemoWindowBase
{
    Q_OBJECT

public:
    explicit AuDatePickerDemoWindow(QWidget *parent = nullptr);

private slots:
    void toggleTheme(bool dark);

private:
    QWidget *makeFooterCustomWidget();

    AuDatePicker *m_basicPicker;
    AuDatePicker *m_formatPicker;
    AuDatePicker *m_rangePicker;
    AuDatePicker *m_dateRangePicker;
    AuDatePicker *m_monthPicker;
    AuDatePicker *m_yearPicker;
    AuDatePicker *m_smallPicker;
    AuDatePicker *m_mediumPicker;
    AuDatePicker *m_largePicker;
    AuDatePicker *m_heightPicker;
    AuDatePicker *m_footerPicker;
    AuDatePicker *m_noFooterPicker;
    AuDatePicker *m_customFooterPicker;
    QLabel *m_valueLabel;
    bool m_darkTheme;
};

#endif // AUDATEPICKERDEMOWINDOW_H
