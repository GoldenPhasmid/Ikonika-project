#pragma once

#include <functional>
#include <memory>

class QQmlApplicationEngine;
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
    QQuickWindow* GetWindow() const;

    std::unique_ptr<QQmlApplicationEngine> Engine;
};
