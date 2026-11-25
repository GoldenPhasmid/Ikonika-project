#pragma once

#include <QObject>
#include <QPointer>
#include <QVariantMap>

#include <memory>

class QQmlEngine;
class QQuickItem;
class QQuickWindow;

class AppHostWindow : public QObject
{
    Q_OBJECT

public:
    AppHostWindow();
    ~AppHostWindow() override;

    QQuickWindow* GetWindow() const;
    QQuickItem* GetRoot() const;
    bool IsCreated() const;

    QQuickWindow& EnsureWindow();

    QQuickItem* Load(QQmlEngine& Engine, const QString& Screens, const QVariantMap& InitialProperties);
    void Unload();
    void DropQueuedInput();

protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;

private:
    bool ShouldDropInput(const QEvent& Event);

    std::unique_ptr<QQuickWindow> Window;
    QPointer<QQuickItem> Root;
    quint64 InputCutoff{0};
    bool bDroppingTouch{false};
    bool bDroppingMouse{false};
};
