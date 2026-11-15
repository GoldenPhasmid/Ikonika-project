#pragma once

#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

class AppStackingItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit AppStackingItem(QQuickItem* Parent = nullptr);

    Q_INVOKABLE void placeBefore(QQuickItem* Sibling);
};
