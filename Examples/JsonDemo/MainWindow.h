#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QByteArray>
#include <QMainWindow>

class QCheckBox;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

namespace aujson { class Value; }

/**
 * @brief AuJson 速度测试窗口。
 *
 * 左侧录入 JSON 文本，输入停顿后自动解析为右侧树结构，并显示最近一次解析耗时；
 * 底部"速度测试"区可对同一份数据连续解析 N 次，统计最快 / 平均 / 最慢，
 * 并可选与 QJsonDocument 同机对比。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    /// 拦截输入框 Ctrl+Enter，等效于点击"解析"。
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    /// 立即解析当前输入并刷新树与统计。
    void onParse();
    /// 输入变化：自动解析模式下重启防抖定时器。
    void onTextChanged();
    /// 载入组合框选中的内置示例文本。
    void onLoadSample(int index);
    /// 执行 N 次连续解析测速。
    void onBench();

private:
    /// 递归构建树节点，返回该子树节点总数（含自身）。
    int buildTree(const aujson::Value &v, QTreeWidgetItem *parent, const QString &name);
    /// 递归统计子树最大深度（cur 为当前层号，根为 1）。
    int maxDepth(const aujson::Value &v, int cur) const;
    /// 值列摘要文本（容器显示项数，字符串截断）。
    QString valueSummary(const aujson::Value &v) const;
    /// 生成内置示例 JSON 文本（0 = 配置对象，1 = 大数组，2 = 深嵌套）。
    QString sampleText(int index) const;

    QPlainTextEdit *m_input;      ///< JSON 输入框
    QTreeWidget    *m_tree;       ///< 解析结果树
    QLabel         *m_timeLabel;  ///< 最近一次解析耗时（大号醒目）
    QLabel         *m_statLabel;  ///< 字节数 / 节点数 / 最大深度
    QLabel         *m_errLabel;   ///< 解析失败信息（红字）
    QPlainTextEdit *m_log;        ///< 测速结果输出
    QComboBox      *m_sampleCombo;
    QSpinBox       *m_runsSpin;
    QCheckBox      *m_autoCheck;  ///< 输入后自动解析
    QCheckBox      *m_cmpCheck;   ///< 测速时与 QJsonDocument 对比
    QTimer         *m_autoTimer;  ///< 自动解析防抖定时器
    QByteArray      m_lastData;   ///< 最近一次成功解析的输入（测速复用）
};

#endif // MAINWINDOW_H
