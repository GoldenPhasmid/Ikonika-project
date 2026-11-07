#include "AppTranslator.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QAtomicInteger>
#include <QCoreApplication>
#include <QMetaObject>

#if defined(Q_OS_ANDROID)
#include <QtCore/qjnienvironment.h>
#include <QtCore/qjniobject.h>
#elif defined(Q_OS_IOS)
#include "AppIosTranslationBridge.h"
#endif

namespace
{
    struct FPairState
    {
        QString Pair;
        bool bChecked = false;
        bool bOffered = false;
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

    void HandleModelState(const QString& Pair, bool bOffered, bool bDownloaded)
    {
        QMetaObject::invokeMethod(&AppTranslator::Notifier(), [Pair, bOffered, bDownloaded]()
        {
            if (Pair != State.Pair)
            {
                return;
            }

            State.bChecked = true;
            State.bOffered = bOffered;
            State.bDownloaded = bDownloaded;

            qCInfo(LogTranslate) << "Translation model for" << Pair
                                 << (bDownloaded ? "is downloaded" : bOffered ? "is not downloaded" : "is not offered");

            emit AppTranslator::Notifier().modelsChanged();
        }, Qt::QueuedConnection);
    }

    void HandleDownloadFinished(const QString& Pair, bool bSuccess, const QString& Message)
    {
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
                qCInfo(LogTranslate) << "Translation model for" << Pair << "downloaded";
            }
            else
            {
                qCWarning(LogTranslate) << "Translation model for" << Pair << "failed to download:" << Message;
            }

            emit AppTranslator::Notifier().downloadingChanged();
            emit AppTranslator::Notifier().modelsChanged();
        }, Qt::QueuedConnection);
    }

    void HandleTranslated(quint64 Generation, const QString& Source, const QString& Translation)
    {
        BusyCount.fetchAndAddOrdered(-1);

        if (Generation != PendingGeneration.loadRelaxed())
        {
            return;
        }

        QMetaObject::invokeMethod(&AppTranslator::Notifier(), [Source, Translation]()
        {
            emit AppTranslator::Notifier().translated(Source, Translation);
        }, Qt::QueuedConnection);
    }

#if defined(Q_OS_ANDROID)
    constexpr const char* PlatformTranslationClass = "com/goldenphasmid/ikonika/PlatformTranslation";

    QString ToString(jstring Value)
    {
        return QJniObject(Value).toString();
    }

    void NativeOnModelState(JNIEnv*, jclass, jstring Source, jstring Target, jboolean Downloaded)
    {
        const bool bDownloaded = Downloaded == JNI_TRUE;

        HandleModelState(PairKey(ToString(Source), ToString(Target)), bDownloaded, bDownloaded);
    }

    void NativeOnDownloadFinished(JNIEnv*, jclass, jstring Source, jstring Target, jboolean Success,
                                  jstring Error)
    {
        HandleDownloadFinished(PairKey(ToString(Source), ToString(Target)), Success == JNI_TRUE, ToString(Error));
    }

    void NativeOnTranslated(JNIEnv*, jclass, jlong Generation, jstring Text, jstring Result)
    {
        HandleTranslated(static_cast<quint64>(Generation), ToString(Text), ToString(Result));
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

    void PlatformCheckModel()
    {
        QJniObject::callStaticMethod<void>(PlatformTranslationClass, "checkModel",
            "(Ljava/lang/String;Ljava/lang/String;)V",
            QJniObject::fromString(SourceLanguage()).object<jstring>(),
            QJniObject::fromString(TargetLanguage()).object<jstring>());
    }

    void PlatformDownload()
    {
        QJniObject::callStaticMethod<void>(PlatformTranslationClass, "download",
            "(Ljava/lang/String;Ljava/lang/String;)V",
            QJniObject::fromString(SourceLanguage()).object<jstring>(),
            QJniObject::fromString(TargetLanguage()).object<jstring>());
    }

    void PlatformTranslate(quint64 Generation, const QString& Text)
    {
        QJniObject::callStaticMethod<void>(PlatformTranslationClass, "translate",
            "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
            static_cast<jlong>(Generation),
            QJniObject::fromString(Text).object<jstring>(),
            QJniObject::fromString(SourceLanguage()).object<jstring>(),
            QJniObject::fromString(TargetLanguage()).object<jstring>());
    }
#elif defined(Q_OS_IOS)
    constexpr int32_t AppleStatusSupported = 1;
    constexpr int32_t AppleStatusInstalled = 2;

    void OnAppleStatus(const char* Source, const char* Target, int32_t Status)
    {
        HandleModelState(PairKey(QString::fromUtf8(Source), QString::fromUtf8(Target)),
                         Status == AppleStatusSupported || Status == AppleStatusInstalled,
                         Status == AppleStatusInstalled);
    }

    void OnAppleDownload(const char* Source, const char* Target, bool bSuccess, const char* Error)
    {
        HandleDownloadFinished(PairKey(QString::fromUtf8(Source), QString::fromUtf8(Target)), bSuccess,
                               QString::fromUtf8(Error));
    }

    void OnAppleTranslated(uint64_t Generation, const char* Text, const char* Result)
    {
        HandleTranslated(Generation, QString::fromUtf8(Text), QString::fromUtf8(Result));
    }

    void EnsureBridge()
    {
        static bool bRegistered = false;

        if (bRegistered)
        {
            return;
        }

        bRegistered = true;

        IkonikaTranslationInstall(&OnAppleStatus, &OnAppleDownload, &OnAppleTranslated);
    }

    void PlatformCheckModel()
    {
        IkonikaTranslationCheck(SourceLanguage().toUtf8().constData(), TargetLanguage().toUtf8().constData());
    }

    void PlatformDownload()
    {
        IkonikaTranslationDownload(SourceLanguage().toUtf8().constData(), TargetLanguage().toUtf8().constData());
    }

    void PlatformTranslate(quint64 Generation, const QString& Text)
    {
        IkonikaTranslationTranslate(Generation, Text.toUtf8().constData(), SourceLanguage().toUtf8().constData(),
                                    TargetLanguage().toUtf8().constData());
    }
#endif

    void RequestModelState()
    {
        const QString Pair = CurrentPair();

        if (State.Pair == Pair)
        {
            return;
        }

        State = FPairState{Pair};

        EnsureBridge();
        PlatformCheckModel();
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

        PlatformTranslate(Generation, Trimmed);
    }

    void Reset()
    {
        State = FPairState{};

        qCInfo(LogTranslate) << "Translation model state forgotten, the next request checks it again";

        emit Notifier().modelsChanged();
    }

    bool IsPairOffered()
    {
#if defined(Q_OS_ANDROID)
        EnsureBridge();

        return QJniObject::callStaticMethod<jboolean>(PlatformTranslationClass, "isSupported",
            "(Ljava/lang/String;Ljava/lang/String;)Z",
            QJniObject::fromString(SourceLanguage()).object<jstring>(),
            QJniObject::fromString(TargetLanguage()).object<jstring>()) == JNI_TRUE;
#else
        RequestModelState();

        return State.bOffered;
#endif
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

        qCInfo(LogTranslate) << "Downloading the translation model for" << State.Pair;

        PlatformDownload();
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
#if defined(Q_OS_ANDROID)
        return QCoreApplication::translate("AppTranslator", "Translated by Google");
#else
        return QString();
#endif
    }
}
