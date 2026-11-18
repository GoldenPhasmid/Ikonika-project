#include "AppSwitchCover.h"

#include "core/FTLog.h"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

namespace
{
    constexpr int DrawTimeoutMilliseconds = 1000;
    constexpr qreal OverlayLayer = 1000000;
}

AppSwitchCover::AppSwitchCover() = default;

AppSwitchCover::~AppSwitchCover()
{
    Hide();
}

void AppSwitchCover::Show(QQuickWindow& Over, std::function<void()> Drawn)
{
    Hide();

    Engine = std::make_unique<QQmlEngine>();

    QQmlComponent Component{Engine.get()};
    Component.loadFromModule(QStringLiteral("Ikonika"), QStringLiteral("SwitchingOverlay"));

    QObject* Created{Component.isError() ? nullptr : Component.beginCreate(Engine->rootContext())};
    QQuickItem* Item{qobject_cast<QQuickItem*>(Created)};

    if (Item == nullptr)
    {
        qCWarning(LogApp) << "The switching overlay did not load, switching without it:" << Component.errorString();

        delete Created;
        Engine.reset();
        Drawn();

        return;
    }

    QQmlEngine::setObjectOwnership(Item, QQmlEngine::CppOwnership);
    Item->setParentItem(Over.contentItem());
    Item->setZ(OverlayLayer);
    Component.completeCreate();

    Overlay = Item;

    const auto Proceed = std::make_shared<std::function<void()>>(std::move(Drawn));
    const auto RunOnce = [Proceed]
    {
        if (*Proceed)
        {
            std::function<void()> Run{std::exchange(*Proceed, {})};
            Run();
        }
    };

    QObject::connect(&Over, &QQuickWindow::frameSwapped, &Over, RunOnce,
                     static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
    QTimer::singleShot(DrawTimeoutMilliseconds, &Over, RunOnce);

    Over.update();
}

bool AppSwitchCover::IsShown() const
{
    return Overlay != nullptr;
}

void AppSwitchCover::HandOver(QQuickWindow& To)
{
    if (Overlay == nullptr)
    {
        return;
    }

    if (!To.isVisible())
    {
        Hide();

        return;
    }

    QObject::connect(&To, &QQuickWindow::frameSwapped, &To, [this] { Hide(); },
                     static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
    To.update();
}

void AppSwitchCover::Hide()
{
    if (Overlay != nullptr)
    {
        delete Overlay.data();
    }

    Engine.reset();
}
