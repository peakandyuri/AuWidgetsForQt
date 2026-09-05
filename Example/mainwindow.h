#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class AgGridWidget;
class AgGridModel;
class AgGridSyncGroup;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    AgGridWidget *m_gridWidget;
    AgGridModel *m_gridModel;
    // 第二个相同数据的表格，用于多表格同步（数据对比）演示
    AgGridWidget *m_gridWidgetB;
    AgGridModel *m_gridModelB;
    AgGridSyncGroup *m_syncGroup;
};
#endif // MAINWINDOW_H
