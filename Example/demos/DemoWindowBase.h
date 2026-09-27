#ifndef DEMOWINDOWBASE_H
#define DEMOWINDOWBASE_H

#include <QDialog>
#include <QVector>
#include <QVariantMap>

class AuColumnDef;
class AuGridModel;
class QVBoxLayout;

/**
 * @brief 功能示例窗口基类：统一窗口尺寸、顶部说明与内容/备注分区。
 *
 * 每个功能示例独立一个窗口（一个功能一个界面），由 MainWindow 启动器打开。
 * 子类把控制按钮与表格加入 m_contentLayout，交互说明用 addNote() 追加到底部。
 */
class DemoWindowBase : public QDialog
{
    Q_OBJECT

public:
    explicit DemoWindowBase(const QString &title, const QString &description, QWidget *parent = nullptr);

    /// 底部追加灰字说明（可含 \n 换行）。
    void addNote(const QString &text);

    /// 共享工具：生成示例标准列定义（Fixed / AutoFit / Flex / 分组列 / 复选框数据列齐全）。
    static QVector<AuColumnDef> makeStandardColumns();
    /// 共享工具：生成 rowCount 行 "R{行}C{列}" 风格扁平数据（含下拉选项与 bool 列）。
    static QVector<QVariantMap> makeFlatRows(int rowCount);
    /// 共享工具：创建"标准列 + 扁平数据"的模型（所有权归调用者）。
    static AuGridModel *makeStandardModel(int rowCount, QObject *parent);

protected:
    QVBoxLayout *m_contentLayout;   ///< 内容区：控制行与表格（说明区之下、备注区之上）

private:
    QVBoxLayout *m_noteLayout;      ///< 底部备注区
};

#endif // DEMOWINDOWBASE_H
