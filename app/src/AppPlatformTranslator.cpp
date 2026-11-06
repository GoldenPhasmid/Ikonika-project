#include "AppTranslator.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QAtomicInteger>
#include <QCoreApplication>
#include <QMetaObject>
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>

namespace
{
    constexpr const char* PlatformTranslationClass = "com/goldenphasmid/ikonika/PlatformTranslation";

    struct FPairState
    {
        QString Pair;
        bool bChecked = false;
        bool bDownloaded = false;
        bool bDownloading = false;
    };

    FPairState State;

    QAtomicInteger<quint64> PendingGeneration = 0;

    QAtomicInteger<int> BusyCount = 0;

    QString SourceLanguage()
    {
        return QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage);
    }

    QString TargetLanguage()
    {
        return QString::fromStdString(FTSettings::Get().GetLanguageRules().NativeLanguage);
    }

    QString PairKey(const QString& Source, const QString& Target)
    {
        return Source + QLatin1Char('-') + Target;
    }

    QString CurrentPair()
    {
        return PairKey(SourceLanguage(), TargetLanguage());
    }

    QString ToString(jstring Value)
    {
        return QJniObject(Value).toString();
    }

    void NativeOnModelState(JNIEnv*, jclass, jstring Source, jstring Target, jboolean Downloaded)
    {
        const QString Pair = PairKey(ToString(Source), ToString(Target));
        const bool bDownloaded = Downloaded == JNI_TRUE;

        QMetaObject::invokeMethod(&AppTranslator::Notifier(), [Pair, bDownloaded]()
        {
            if (Pair != State.Pair)
            {
                return;
            }

            State.bChecked = true;
            State.bDownloaded = bDownloaded;

            qCInfo(LogTranslate) << "ML Kit model for" << Pair
                                 << (bDownloaded ? "is downloaded" : "is not downloaded");

            emit AppTranslator::Notifier().modelsChanged();
        }, Qt::QueuedConnection);
    }

    void NativeOnDownloadFinished(JNIEnv*, jclass, jstring Source, jstring Target, jboolean Success,
                                  jstring Error)
    {
        const QString Pair = PairKey(ToString(Source), ToString(Target));
        const bool bSuccess = Success == JNI_TRUE;
        const QString Message = ToString(Error);

        QMetaObject::invokeMethod(&AppTranslator::Notifier(), [Pair, bSuccess, Message]()
        {
            if (Pair != State.Pair)
            {
                return;
            }

            State.bDownloading = false;
            State.bDownloaded = bSuccess;

            if (bSuccess)
            {
                qCInfo(LogTranslate) << "ML Kit model for" << Pair << "downloaded";
            }
            else
            {
                qCWarning(LogTranslate) << "ML Kit model for" << Pair << "failed to download:"
                                        << Message;
            }

            emit AppTranslator::Notifier().downloadingChanged();
            emit AppTranslator::Notifier().modelsChanged();
        }, Qt::QueuedConnection);
    }

    void NativeOnTranslated(JNIEnv*, jclass, jlong Generation, jstring Text, jstring Result)
    {
        const QString Source = ToString(Text);
        const QString Translation = ToString(Result);
        const quint64 Requested = static_cast<quint64>(Generation);

        BusyCount.fetchAndAddOrdered(-1);

        if (Requested != PendingGeneration.loadRelaxed())
        {
            return;
        }

        QMetaObject::invokeMethod(&AppTranslator::Notifier(), [Source, Translation]()
        {
            emit AppTranslator::Notifier().translated(Source, Translation);
        }, Qt::QueuedConnection);
    }

    void EnsureBridge()
    {
        static bool bRegistered = false;

        if (bRegistered)
        {
            return;
        }

        bRegistered = true;

        static const JNINativeMethod Methods[] = {
            { "nativeOnModelState", "(Ljava/lang/String;Ljava/lang/String;Z)V",
              reinterpret_cast<void*>(&NativeOnModelState) },
            { "nativeOnDownloadFinished", "(Ljava/lang/String;Ljava/lang/String;ZLjava/lang/String;)V",
              reinterpret_cast<void*>(&NativeOnDownloadFinished) },
            { "nativeOnTranslated", "(JLjava/lang/String;Ljava/lang/String;)V",
              reinterpret_cast<void*>(&NativeOnTranslated) },
        };

        QJniEnvironment Env;

        if (!Env.registerNativeMethods(PlatformTranslationClass, Methods, 3))
        {
            qCWarning(LogTranslate) << "Could not register the ML Kit translation bridge";
        }
    }

    void RequestModelState()
    {
        const QString Pair = CurrentPair();

        if (State.Pair == Pair)
        {
            return;
        }

        State = FPairState{Pair};

        EnsureBridge();

        QJniObject::callStaticMethod<void>(PlatformTranslationClass, "checkModel",
            "(Ljava/lang/String;Ljava/lang/String;)V",
            QJniObject::fromString(SourceLanguage()).object<jstring>(),
            QJniObject::fromString(TargetLanguage()).object<jstring>());
    }
}

namespace AppTranslator
{
    AppTranslatorNotifier& Notifier()
    {
        static AppTranslatorNotifier Instance;

        return Instance;
    }

    bool IsSupportedPair()
    {
        RequestModelState();

        return State.bDownloaded;
    }

    bool IsAvailable()
    {
        return IsSupportedPair();
    }

    bool IsBusy()
    {
        return BusyCount.loadRelaxed() > 0;
    }

    void Translate(const QString& Text)
    {
        const QString Trimmed = Text.trimmed();

        if (Trimmed.isEmpty() || !IsSupportedPair())
        {
            return;
        }

        const quint64 Generation = ++PendingGeneration;

        BusyCount.fetchAndAddOrdered(1);

        QJniObject::callStaticMethod<void>(PlatformTranslationClass, "translate",
            "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
            static_cast<jlong>(Generation),
            QJniObject::fromString(Trimmed).object<jstring>(),
            QJniObject::fromString(SourceLanguage()).object<jstring>(),
            QJniObject::fromString(TargetLanguage()).object<jstring>());
    }

    void Reset()
    {
        State = FPairState{};

        qCInfo(LogTranslate) << "ML Kit model state forgotten, the next request checks it again";

        emit Notifier().modelsChanged();
    }

    bool IsPairOffered()
    {
        EnsureBridge();

        return QJniObject::callStaticMethod<jboolean>(PlatformTranslationClass, "isSupported",
            "(Ljava/lang/String;Ljava/lang/String;)Z",
            QJniObject::fromString(SourceLanguage()).object<jstring>(),
            QJniObject::fromString(TargetLanguage()).object<jstring>()) == JNI_TRUE;
    }

    bool IsDownloading()
    {
        RequestModelState();

        return State.bDownloading;
    }

    void Download()
    {
        RequestModelState();

        if (State.bDownloading || State.bDownloaded)
        {
            return;
        }

        State.bDownloading = true;

        emit Notifier().downloadingChanged();

        QJniObject::callStaticMethod<void>(PlatformTranslationClass, "download",
            "(Ljava/lang/String;Ljava/lang/String;)V",
            QJniObject::fromString(SourceLanguage()).object<jstring>(),
            QJniObject::fromString(TargetLanguage()).object<jstring>());
    }

    qint64 GetDownloadBytes()
    {
        return 0;
    }

    double GetDownloadProgress()
    {
        return 0.0;
    }

    QString GetAttribution()
    {
        return QCoreApplication::translate("AppTranslator", "Translated by Google");
    }
}
