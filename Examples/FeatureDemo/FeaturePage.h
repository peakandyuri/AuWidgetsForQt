#ifndef FEATUREPAGE_H
#define FEATUREPAGE_H

#include <QString>
#include <QVector>
#include <functional>

class QWidget;

/**
 * @brief 一个"细化功能演示页"的描述：一个功能 = 一个 Tab = 一个独立演示 + 一段代码。
 *
 *  - title：TabBar 上的功能名（如"行号列"）；
 *  - description：该功能的简短说明，显示在演示区上方；
 *  - createDemo：功能演示部件工厂（只演示这一个功能，部件归调用者所有）；
 *  - code：右侧代码示例（PlainText 显示，只包含该功能的关键实现代码）。
 */
struct FeaturePage
{
    QString title;
    QString description;
    std::function<QWidget *()> createDemo;
    QString code;
};

#endif // FEATUREPAGE_H
