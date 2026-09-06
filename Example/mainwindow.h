#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHash>
#include <QMainWindow>
#include <QPointer>

#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QDialog;
class QGridLayout;

/**
 * @brief 示例启动器主窗口：每个功能示例一个独立窗口，点击按钮打开查看。
 *
 * 已打开的示例窗口再次点击时抬升置前；窗口关闭时自动销毁（WA_DeleteOnClose）。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    /// 向启动按钮区添加一个示例按钮。
    /// create 为示例窗口工厂：返回未显示的 QDialog（内部以 this 为父对象）。
    void addDemoButton(QGridLayout *grid, int row, int column,
                       const QString &key, const QString &buttonText,
                       const std::function<QDialog *()> &create);

    Ui::MainWindow *ui;
    QHash<QString, QPointer<QDialog> > m_openDemos;   ///< 已打开示例窗口（关闭后 QPointer 自动置空）
};

#endif // MAINWINDOW_H
