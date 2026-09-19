#ifndef AGDATEPICKER_H
#define AGDATEPICKER_H

#include "AgDatePicker_global.h"

#include <QWidget>

/**
 * @brief 日期选择控件（目标：替代 QCalendarWidget）。
 *
 * 当前为工程骨架版本：仅包含保证动态库可编译、可链接的最小占位实现，
 * 组件 API 尚未实现。后续版本将在此类上补齐（参考 AG Grid 交互风格）：
 *  - 内嵌月历面板与弹层（popup）两种形态；
 *  - 范围选择、快捷项（今天 / 本周 / 本月等）；
 *  - 日期格式与本地化（QLocale）、禁用日期区间；
 *  - 自定义日期单元格渲染与样式系统。
 */
class AGDATEPICKER_EXPORT AgDatePicker : public QWidget
{
    Q_OBJECT

public:
    explicit AgDatePicker(QWidget *parent = nullptr);
    ~AgDatePicker();
};

#endif // AGDATEPICKER_H
