#include "AppHostWindow.h"

#include "core/FTLog.h"

#include <QApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

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

bool AppHostWindow::eventFilter(QObject* Watched, QEvent* Event)
{
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
