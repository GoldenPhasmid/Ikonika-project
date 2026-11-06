#include "AppSwitchCover.h"

#include "core/FTLog.h"

#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTimer>

namespace
{
    constexpr int DrawTimeoutMilliseconds = 1000;
}

AppSwitchCover::AppSwitchCover() = default;

AppSwitchCover::~AppSwitchCover() = default;

void AppSwitchCover::Show(QQuickWindow& Over, std::function<void()> Drawn)
{
    Engine = std::make_unique<QQmlApplicationEngine>();
    Engine->setInitialProperties({{QStringLiteral("visible"), false}});
    Engine->loadFromModule("Ikonika", "SwitchingScreen");

    QQuickWindow* Cover{GetWindow()};

    if (Cover == nullptr)
    {
        qCWarning(LogApp) << "The switching screen did not load, switching without it";

        Engine.reset();
        Drawn();

        return;
    }

    const auto Proceed = std::make_shared<std::function<void()>>(std::move(Drawn));
    const auto RunOnce = [Proceed]
    {
        if (*Proceed)
        {
            std::function<void()> Run{std::exchange(*Proceed, {})};
            Run();
        }
    };

    QObject::connect(Cover, &QQuickWindow::frameSwapped, Cover, RunOnce,
                     static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
    QTimer::singleShot(DrawTimeoutMilliseconds, Cover, RunOnce);

    Cover->setIcon(Over.icon());
    Cover->setGeometry(Over.geometry());
    Cover->setVisibility(Over.visibility());
}

bool AppSwitchCover::IsShown() const
{
    return GetWindow() != nullptr;
}

void AppSwitchCover::HandOver(QQuickWindow& To)
{
    QQuickWindow* Cover{GetWindow()};

    if (Cover == nullptr)
    {
        To.show();

        return;
    }

    To.setGeometry(Cover->geometry());
    To.setVisibility(Cover->visibility());

    QObject::connect(&To, &QQuickWindow::frameSwapped, &To, [this] { Engine.reset(); },
                     static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
}

QQuickWindow* AppSwitchCover::GetWindow() const
{
    if (Engine == nullptr || Engine->rootObjects().isEmpty())
    {
        return nullptr;
    }

    return qobject_cast<QQuickWindow*>(Engine->rootObjects().constFirst());
}
