#include "AppReaderIntake.h"

#include "core/FTLog.h"

#include <QTimer>

#if defined(Q_OS_ANDROID)
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>
#endif

namespace
{
#if defined(Q_OS_ANDROID)
    constexpr const char* AndroidIntakeClass = "com/goldenphasmid/ikonika/ReaderIntake";

    void AndroidNativeOnBookArrived(JNIEnv*, jclass)
    {
        QMetaObject::invokeMethod(&AppReaderIntake::Get(), &AppReaderIntake::bookArrived,
                                  Qt::QueuedConnection);
    }
#endif
}

AppReaderIntake& AppReaderIntake::Get()
{
    static AppReaderIntake Instance;

    return Instance;
}

AppReaderIntake::AppReaderIntake(QObject* Parent)
    : QObject(Parent)
{
#if defined(Q_OS_ANDROID)
    static const JNINativeMethod Methods[] = {
        { "nativeOnBookArrived", "()V", reinterpret_cast<void*>(&AndroidNativeOnBookArrived) },
    };

    QJniEnvironment Env;

    if (!Env.registerNativeMethods(AndroidIntakeClass, Methods, 1))
    {
        qCWarning(LogApp) << "Reader intake: could not register the Android bridge";
    }

    QTimer::singleShot(0, this, &AppReaderIntake::bookArrived);
#endif
}

QVariantMap AppReaderIntake::takePending()
{
    QVariantMap Result;

#if defined(Q_OS_ANDROID)
    const QJniObject Pending = QJniObject::callStaticObjectMethod(
        AndroidIntakeClass, "takePending", "()[Ljava/lang/String;");

    if (!Pending.isValid())
    {
        return Result;
    }

    QJniEnvironment Env;
    const jobjectArray Values = Pending.object<jobjectArray>();

    if (Env->GetArrayLength(Values) < 2)
    {
        return Result;
    }

    const QString Path = QJniObject(Env->GetObjectArrayElement(Values, 0)).toString();
    const QString Name = QJniObject(Env->GetObjectArrayElement(Values, 1)).toString();

    qCInfo(LogApp) << "Reader intake: opening" << Name << "from" << Path;

    Result[QStringLiteral("path")] = Path;
    Result[QStringLiteral("name")] = Name;
#endif

    return Result;
}
