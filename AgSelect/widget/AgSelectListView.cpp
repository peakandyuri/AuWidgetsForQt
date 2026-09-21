#include "AgSelectListView.h"

AgSelectListView::AgSelectListView(QWidget *parent)
    : QListView(parent)
{
}

void AgSelectListView::setInnerMargins(const QMargins &margins)
{
    setViewportMargins(margins);
}
