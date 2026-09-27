#ifndef PAGEHELPERS_H
#define PAGEHELPERS_H

#include <QVector>
#include <QVariantMap>

class AuGridModel;
class QHBoxLayout;
class QPlainTextEdit;
class QWidget;

/**
 * @brief 各功能演示页共享的小工具：示例数据、信号日志、代码字体。
 */
namespace PageHelpers {

/// 生成 rowCount 行 "R{行}C{列}" 风格扁平数据（名称 / 数量 / 类别 / 启用）。
QVector<QVariantMap> makeFlatRows(int rowCount);

/// 创建"名称 / 数量 / 类别 / 启用"四列 + 扁平数据的模型（所有权归调用者）。
AuGridModel *makeFlatModel(int rowCount, QObject *parent);

/// 只读日志部件（固定高度，信号输出用）。
QPlainTextEdit *makeLog(int maxHeight = 110);

/// 在日志中追加一行。
void appendLog(QPlainTextEdit *log, const QString &text);

/// 演示页统一边距包装：给部件套上带边距的布局。
QWidget *padded(QWidget *content, int margin = 8);

/// 组装演示页：顶部控制条（可空）+ 中央主体 + 可选底部日志。
QWidget *composeDemo(QHBoxLayout *controls, QWidget *body, QWidget *footer = nullptr);

} // namespace PageHelpers

#endif // PAGEHELPERS_H
