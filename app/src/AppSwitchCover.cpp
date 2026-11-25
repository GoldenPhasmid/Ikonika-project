#include "AppSwitchCover.h"

#include "AppMetrics.h"
#include "core/FTLog.h"

#include <QPropertyAnimation>
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
    return Overlay != nullptr && !bFading;
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

    QObject::connect(&To, &QQuickWindow::frameSwapped, &To, [this] { FadeOut(); },
                     static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));
    To.update();
}

void AppSwitchCover::FadeOut()
{
    if (Overlay == nullptr || bFading)
    {
        return;
    }

    const AppMetrics* Metrics{Engine->singletonInstance<AppMetrics*>("Ikonika", "AppMetrics")};
    const int Duration{Metrics != nullptr ? Metrics->GetMotionBase() : 0};

    if (Duration <= 0)
    {
        Hide();

        return;
    }

    bFading = true;
    Overlay->setEnabled(false);

    QPropertyAnimation* Fade{new QPropertyAnimation{Overlay.data(), "opacity", Overlay.data()}};
    Fade->setDuration(Duration);
    Fade->setStartValue(1.0);
    Fade->setEndValue(0.0);
    Fade->setEasingCurve(QEasingCurve::OutCubic);

    const QPointer<QQuickItem> Faded{Overlay};

    QObject::connect(Fade, &QPropertyAnimation::finished, Overlay->window(), [this, Faded]()
    {
        if (Overlay == Faded)
        {
            Hide();
        }
    }, Qt::QueuedConnection);

    Fade->start();
}

void AppSwitchCover::Hide()
{
    bFading = false;

    if (Overlay != nullptr)
    {
        delete Overlay.data();
    }

    Engine.reset();
}
