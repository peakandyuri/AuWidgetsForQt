#include "AuSelectListView.h"

#include "AuSelectItemDelegate.h"

#include <QMouseEvent>

AuSelectListView::AuSelectListView(QWidget *parent)
    : QListView(parent)
{
}

void AuSelectListView::setInnerMargins(const QMargins &margins)
{
    setViewportMargins(margins);
}

bool AuSelectListView::isActionableRow(const QModelIndex &index) const
{
    return index.isValid()
           && index.data(AuSelectItemRole::RowType).toInt() == AuSelectItemRole::RowItem
           && index.data(AuSelectItemRole::ItemEnabled).toBool();
}

void AuSelectListView::mousePressEvent(QMouseEvent *event)
{
    // 组名行 / 禁用项 / 空白区：点击无效果。内置模型对所有行开放 ItemIsEnabled
    // （保证委托绘制与 hover 正常），QListView 会把这些行的点击当普通行派发，
    // 这里在视图层显式吞掉，确保 clicked 不会发出、行高亮也不会跳到不可选行。
    if (!isActionableRow(indexAt(event->pos()))) {
        event->accept();
        return;
    }
    QListView::mousePressEvent(event);
}

void AuSelectListView::mouseReleaseEvent(QMouseEvent *event)
{
    if (!isActionableRow(indexAt(event->pos()))) {
        event->accept();
        return;
    }
    QListView::mouseReleaseEvent(event);
}
