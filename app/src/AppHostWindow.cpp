#include "AppHostWindow.h"

#include "core/FTLog.h"

#include <QApplication>
#include <QInputEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

#if defined(Q_OS_ANDROID)
#include <qpa/qwindowsysteminterface_p.h>

namespace
{
    constexpr quint64 QueuedInputMilliseconds = 300;
}
#endif

AppHostWindow::AppHostWindow() = default;

AppHostWindow::~AppHostWindow()
{
    Unload();
}

QQuickWindow* AppHostWindow::GetWindow() const
{
    return Window.get();
}

QQuickItem* AppHostWindow::GetRoot() const
{
    return Root.data();
}

bool AppHostWindow::IsCreated() const
{
    return Window != nullptr;
}

QQuickWindow& AppHostWindow::EnsureWindow()
{
    if (Window == nullptr)
    {
        Window = std::make_unique<QQuickWindow>();
        Window->setTitle(QStringLiteral("Ikonika"));
        Window->setIcon(QApplication::windowIcon());
        Window->installEventFilter(this);

        qCInfo(LogApp) << "Created the window the screens live in";
    }

    return *Window;
}

QQuickItem* AppHostWindow::Load(QQmlEngine& Engine, const QString& Screens, const QVariantMap& InitialProperties)
{
    Unload();
    EnsureWindow();

    QQmlComponent Component{&Engine};
    Component.loadFromModule(QStringLiteral("Ikonika"), Screens);

    if (Component.isError())
    {
        qCCritical(LogApp) << "The screens" << Screens << "did not load:" << Component.errorString();

        return nullptr;
    }

    QObject* Created{Component.beginCreate(Engine.rootContext())};
    QQuickItem* Item{qobject_cast<QQuickItem*>(Created)};

    if (Item == nullptr)
    {
        qCCritical(LogApp) << "The screens" << Screens << "are not an item:" << Component.errorString();

        delete Created;

        return nullptr;
    }

    if (!InitialProperties.isEmpty())
    {
        Component.setInitialProperties(Item, InitialProperties);
    }

    QQmlEngine::setObjectOwnership(Item, QQmlEngine::CppOwnership);
    Item->setParentItem(Window->contentItem());
    Component.completeCreate();
    Item->setFocus(true);

    Root = Item;

    return Item;
}

void AppHostWindow::Unload()
{
    if (Root == nullptr)
    {
        return;
    }

    delete Root.data();
}

void AppHostWindow::DropQueuedInput()
{
#if defined(Q_OS_ANDROID)
    InputCutoff = QWindowSystemInterfacePrivate::eventTime.elapsed() + QueuedInputMilliseconds;
#endif
}

bool AppHostWindow::ShouldDropInput(const QEvent& Event)
{
    const QEvent::Type Type{Event.type()};
    const bool bTouch{Type == QEvent::TouchBegin || Type == QEvent::TouchUpdate || Type == QEvent::TouchEnd || Type == QEvent::TouchCancel};
    const bool bMouse{Type == QEvent::MouseButtonPress || Type == QEvent::MouseButtonDblClick || Type == QEvent::MouseMove
                      || Type == QEvent::MouseButtonRelease};

    if (!bTouch && !bMouse)
    {
        return false;
    }

    const bool bTooSoon{static_cast<const QInputEvent&>(Event).timestamp() < InputCutoff};
    bool& bDropping{bTouch ? bDroppingTouch : bDroppingMouse};

    if ((Type == QEvent::TouchBegin || Type == QEvent::MouseButtonPress || Type == QEvent::MouseButtonDblClick) && bTooSoon)
    {
        qCInfo(LogApp) << "Dropped" << (bTouch ? "a touch" : "a click") << "aimed at the screens before the layout swap";

        bDropping = true;
    }

    const bool bDrop{bDropping || bTooSoon};

    if (Type == QEvent::TouchEnd || Type == QEvent::TouchCancel || Type == QEvent::MouseButtonRelease)
    {
        bDropping = false;
    }

    return bDrop;
}

bool AppHostWindow::eventFilter(QObject* Watched, QEvent* Event)
{
    if (Watched == Window.get() && ShouldDropInput(*Event))
    {
        return true;
    }

    if (Watched != Window.get() || Event->type() != QEvent::Close || Root == nullptr)
    {
        return QObject::eventFilter(Watched, Event);
    }

    QVariant Accepted;
    QMetaObject::invokeMethod(Root.data(), "handleClose", Q_RETURN_ARG(QVariant, Accepted));

    if (Accepted.toBool())
    {
        return false;
    }

    Event->ignore();

    return true;
}
