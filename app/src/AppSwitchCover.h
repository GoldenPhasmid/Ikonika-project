#pragma once

#include <QPointer>

#include <functional>
#include <memory>

class QQmlEngine;
class QQuickItem;
class QQuickWindow;

class AppSwitchCover
{
public:
    AppSwitchCover();
    ~AppSwitchCover();

    void Show(QQuickWindow& Over, std::function<void()> Drawn);
    bool IsShown() const;
    void HandOver(QQuickWindow& To);

private:
    void Hide();

    std::unique_ptr<QQmlEngine> Engine;
    QPointer<QQuickItem> Overlay;
};
