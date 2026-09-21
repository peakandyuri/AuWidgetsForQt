#ifndef AGSELECTLISTVIEW_H
#define AGSELECTLISTVIEW_H

#include <QListView>
#include <QMargins>

/// 下拉面板内的列表视图：暴露 setViewportMargins 以实现内容区内边距。
class AgSelectListView : public QListView
{
    Q_OBJECT

public:
    explicit AgSelectListView(QWidget *parent = nullptr);

    void setInnerMargins(const QMargins &margins);
};

#endif // AGSELECTLISTVIEW_H
