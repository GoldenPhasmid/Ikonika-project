#include "AppHomeWidget.h"

#include "core/FTLog.h"
#include "core/FTSchema.h"

#include <QTimer>

#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>
#endif

namespace
{
#if defined(Q_OS_ANDROID)
    constexpr const char* AndroidWidgetClass = "com/goldenphasmid/ikonika/HomeWidget";

    void AndroidNativeOnSessionRequested(JNIEnv*, jclass)
    {
        QMetaObject::invokeMethod(&AppHomeWidget::Get(), &AppHomeWidget::sessionRequested, Qt::QueuedConnection);
    }
#endif
}

AppHomeWidget& AppHomeWidget::Get()
{
    static AppHomeWidget Instance;

    return Instance;
}

AppHomeWidget::AppHomeWidget(QObject* Parent)
    : QObject(Parent)
{
#if defined(Q_OS_ANDROID)
    static const JNINativeMethod Methods[] = {
        { "nativeOnSessionRequested", "()V", reinterpret_cast<void*>(&AndroidNativeOnSessionRequested) },
    };

    QJniEnvironment Env;

    if (!Env.registerNativeMethods(AndroidWidgetClass, Methods, 1))
    {
        qCWarning(LogApp) << "home widget: could not register the Android bridge";
    }

    QTimer::singleShot(0, this, &AppHomeWidget::sessionRequested);
#endif
}

QString AppHomeWidget::takePendingSession()
{
#if defined(Q_OS_ANDROID)
    const QString Session = QJniObject::callStaticObjectMethod(
        AndroidWidgetClass, "takePendingSession", "()Ljava/lang/String;").toString();

    if (!Session.isEmpty())
    {
        qCInfo(LogApp) << "home widget: opening" << Session;
    }

    return Session;
#else
    return {};
#endif
}

void AppHomeWidget::publish(int Done, int Goal, int Reviewed, int Due, int DueTomorrow, const QColor& Panel,
                            const QColor& Muted, const QColor& Review, const QColor& Learn)
{
    const std::array<long long, 10> State{
        GetTodayDayNumber(), Done, Goal, Reviewed, Due, DueTomorrow,
        Panel.rgba(), Muted.rgba(), Review.rgba(), Learn.rgba()
    };

    if (bPublished && State == PublishedState)
    {
        return;
    }

    PublishedState = State;
    bPublished = true;

    qCInfo(LogApp) << "home widget:" << Done << "of" << Goal << "done," << Reviewed << "reviewed,"
                   << Due << "due," << DueTomorrow << "due tomorrow";

#if defined(Q_OS_ANDROID)
    QJniObject Context = QNativeInterface::QAndroidApplication::context();

    QJniObject::callStaticMethod<void>(AndroidWidgetClass, "publish",
        "(Landroid/content/Context;IIIIIIIII)V", Context.object<jobject>(),
        jint{Done}, jint{Goal}, jint{Reviewed}, jint{Due}, jint{DueTomorrow},
        static_cast<jint>(Panel.rgba()), static_cast<jint>(Muted.rgba()),
        static_cast<jint>(Review.rgba()), static_cast<jint>(Learn.rgba()));
#else
    Q_UNUSED(Panel);
    Q_UNUSED(Muted);
    Q_UNUSED(Review);
    Q_UNUSED(Learn);
#endif
}
