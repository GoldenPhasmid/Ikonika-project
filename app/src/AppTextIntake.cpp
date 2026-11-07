#include "AppTextIntake.h"

#include "core/FTLog.h"

#include <utility>

#if defined(Q_OS_ANDROID)
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>
#endif

AppTextIntake& AppTextIntake::Get()
{
    static AppTextIntake Instance;
    return Instance;
}

AppTextIntake::AppTextIntake(QObject* Parent)
    : QObject(Parent)
{
#if defined(Q_OS_ANDROID)
    static const JNINativeMethod Methods[] = {
        { "nativeOnTextArrived", "()V", reinterpret_cast<void*>(+[](JNIEnv*, jclass)
        {
            AppTextIntake& Intake = AppTextIntake::Get();
            QMetaObject::invokeMethod(&Intake, &AppTextIntake::ReceiveAndroidText, Qt::QueuedConnection);
        }) },
    };

    QJniEnvironment Environment;
    if (!Environment.registerNativeMethods("com/goldenphasmid/ikonika/TextIntake", Methods, 1))
    {
        qCWarning(LogApp) << "Text intake: could not register the Android bridge";
    }

    ReceiveAndroidText();
#endif
}

void AppTextIntake::ReceiveAndroidText()
{
#if defined(Q_OS_ANDROID)
    const QJniObject Text = QJniObject::callStaticObjectMethod(
        "com/goldenphasmid/ikonika/TextIntake", "takePending", "()Ljava/lang/String;");
    if (Text.isValid())
    {
        ReceiveText(Text.toString());
    }
#endif
}

void AppTextIntake::ReceiveText(const QString& Text)
{
    const QString Selection{Text.trimmed()};
    if (Selection.isEmpty())
    {
        return;
    }

    PendingText = Selection;
    qCInfo(LogApp) << "Text intake: received selection of" << Selection.size() << "characters";
    emit textArrived();
}

QString AppTextIntake::takePending()
{
    return std::exchange(PendingText, {});
}
