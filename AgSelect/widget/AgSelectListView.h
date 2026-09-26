#ifndef AGSELECTLISTVIEW_H
#define AGSELECTLISTVIEW_H

#include <QListView>
#include <QMargins>

/// 下拉面板内的列表视图：暴露 setViewportMargins 以实现内容区内边距；
/// 点击不可激活的行（组名行 / 禁用项 / 空白区）不产生任何交互（不吞滚动条等
/// 其它事件），避免误发 clicked 触发上层收起面板。
class AgSelectListView : public QListView
{
    Q_OBJECT

public:
    explicit AgSelectListView(QWidget *parent = nullptr);

    void setInnerMargins(const QMargins &margins);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    bool isActionableRow(const QModelIndex &index) const;
};

#endif // AGSELECTLISTVIEW_H
