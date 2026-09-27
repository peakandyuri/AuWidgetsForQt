#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QHash>
#include <QMainWindow>
#include <QString>
#include <QVector>

#include "FeaturePage.h"

class QListWidget;
class QPlainTextEdit;
class QSplitter;
class QTabBar;
class QLabel;
class QVBoxLayout;

/**
 * @brief 功能细化演示主窗口。
 *
 * 布局：
 *  - 左侧：控件列表（AuGrid / AuSelect / AuDatePicker），单选互斥切换；
 *  - 右侧上方：当前控件支持的功能 TabBar（一个功能一个 Tab）；
 *  - 右侧下方：左右分栏 —— 左为该功能的独立演示，右为实现代码（PlainText）。
 *
 * 目的：把控件的每个可配置能力拆成单独的最小示例，边看演示边看实现代码。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    /// 切换左侧控件：重建右侧 TabBar 并加载第一个功能页。
    void switchWidget(int index);
    /// 切换功能 Tab：加载对应的功能演示与代码示例。
    void switchPage(int index);
    /// 把右侧代码复制到剪贴板。
    void copyCode();

    void buildLeftPanel();
    void buildRightPanel();
    void loadPage(int index);

    QListWidget *m_widgetList = nullptr;   ///< 左侧控件列表
    QWidget *m_rightHost = nullptr;        ///< 右侧整体面板（TabBar + 说明 + 分栏）
    QTabBar *m_tabBar = nullptr;           ///< 功能 TabBar
    QLabel *m_description = nullptr;       ///< 当前功能说明
    QWidget *m_demoHost = nullptr;         ///< 演示区宿主
    QVBoxLayout *m_demoLayout = nullptr;   ///< 演示区布局（每次换页替换内容）
    QWidget *m_demoWidget = nullptr;       ///< 当前演示部件（临时实例：切页销毁重建，不缓存）
    QPlainTextEdit *m_codeView = nullptr;  ///< 右侧代码示例
    QLabel *m_codeTitle = nullptr;         ///< 代码区标题

    /// 控件 key（grid/select/datepicker）→ 功能页列表。
    QHash<QString, QVector<FeaturePage>> m_pages;
    QStringList m_widgetKeys;              ///< 与左侧列表行序对应的 key
};

#endif // MAINWINDOW_H
