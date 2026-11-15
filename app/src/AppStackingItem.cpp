#include "AppStackingItem.h"

#include "core/FTLog.h"

AppStackingItem::AppStackingItem(QQuickItem* Parent)
    : QQuickItem{Parent}
{
}

void AppStackingItem::placeBefore(QQuickItem* Sibling)
{
    if (Sibling == nullptr || Sibling == this || Sibling->parentItem() != parentItem())
    {
        qCWarning(LogApp) << "AppStackingItem: placeBefore needs a sibling";

        return;
    }

    stackBefore(Sibling);
}
