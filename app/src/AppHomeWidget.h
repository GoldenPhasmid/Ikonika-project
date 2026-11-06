#pragma once

#include <QColor>
#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QtQml/qqmlregistration.h>

#include <array>

class AppHomeWidget : public QObject
{
    Q_OBJECT

public:
    static AppHomeWidget& Get();

    explicit AppHomeWidget(QObject* Parent = nullptr);

    Q_INVOKABLE void publish(int Done, int Goal, int Reviewed, int Due, int DueTomorrow, const QColor& Panel,
                             const QColor& Muted, const QColor& Review, const QColor& Learn);

    Q_INVOKABLE QString takePendingSession();

signals:
    void sessionRequested();

private:
    std::array<long long, 10> PublishedState{};
    bool bPublished = false;
};

struct AppHomeWidgetSingleton
{
    Q_GADGET
    QML_FOREIGN(AppHomeWidget)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppHomeWidget)

public:
    static AppHomeWidget* create(QQmlEngine* Engine, QJSEngine*)
    {
        Q_UNUSED(Engine);

        AppHomeWidget& Instance = AppHomeWidget::Get();

        QJSEngine::setObjectOwnership(&Instance, QJSEngine::CppOwnership);

        return &Instance;
    }
};
