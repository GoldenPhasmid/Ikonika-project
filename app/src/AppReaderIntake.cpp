#include "AppReaderIntake.h"

#include "core/FTLog.h"

#include <QTimer>
#include <QFileInfo>
#include <utility>

#if defined(Q_OS_ANDROID)
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>
#elif defined(Q_OS_IOS)
#include "AppIosReader.h"
#endif

#include <QCoreApplication>
#include <QFileOpenEvent>

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
#elif defined(Q_OS_IOS)
    QCoreApplication::instance()->installEventFilter(this);
#endif
}

bool AppReaderIntake::eventFilter(QObject* Watched, QEvent* Event)
{
    if (Event->type() != QEvent::FileOpen)
    {
        return QObject::eventFilter(Watched, Event);
    }

    OpenUrl(static_cast<QFileOpenEvent*>(Event)->url());

    return true;
}

void AppReaderIntake::OpenUrl(const QUrl& Url)
{
    qCInfo(LogApp) << "Reader intake: another app handed over" << Url.toString();

#if defined(Q_OS_IOS)
    const QString Kept{AppIosReader::ImportOpenedUrl(Url)};

    if (Kept.isEmpty())
    {
        return;
    }

    OpenPath(Kept);
#else
    OpenPath(Url.toLocalFile());
#endif
}

void AppReaderIntake::OpenPath(const QString& Path)
{
    PendingBook = {{QStringLiteral("path"), Path},
                   {QStringLiteral("name"), QFileInfo(Path).fileName()}};
    qCInfo(LogApp) << "Reader intake: received" << Path;
    emit bookArrived();
}

QVariantMap AppReaderIntake::takePending()
{
    QVariantMap Result{std::exchange(PendingBook, {})};

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
