#include "AppTranslator.h"

#include "AppTranslationRequests.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QAtomicInteger>
#include <QCoreApplication>
#include <QHash>
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
        bool bOffered = false;
        bool bDownloaded = false;
        bool bDownloading = false;
    };

    QHash<QString, FPairState> States;

    FAppTranslationRequests Requests;

    QAtomicInteger<int> BusyCount = 0;

    QString PairKey(const QString& Source, const QString& Target)
    {
        return Source + QLatin1Char('-') + Target;
    }

    void HandleModelState(const QString& Pair, bool bOffered, bool bDownloaded)
    {
        QMetaObject::invokeMethod(&AppTranslator::Notifier(), [Pair, bOffered, bDownloaded]()
        {
            FPairState& State = States[Pair];

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
            FPairState& State = States[Pair];

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

        if (!Requests.Finish(Generation))
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

    void PlatformCheckModel(const QString& Source, const QString& Target)
    {
        QJniObject::callStaticMethod<void>(PlatformTranslationClass, "checkModel",
            "(Ljava/lang/String;Ljava/lang/String;)V",
            QJniObject::fromString(Source).object<jstring>(),
            QJniObject::fromString(Target).object<jstring>());
    }

    void PlatformDownload(const QString& Source, const QString& Target)
    {
        QJniObject::callStaticMethod<void>(PlatformTranslationClass, "download",
            "(Ljava/lang/String;Ljava/lang/String;)V",
            QJniObject::fromString(Source).object<jstring>(),
            QJniObject::fromString(Target).object<jstring>());
    }

    void PlatformTranslate(quint64 Generation, const QString& Text, const QString& Source, const QString& Target)
    {
        QJniObject::callStaticMethod<void>(PlatformTranslationClass, "translate",
            "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
            static_cast<jlong>(Generation),
            QJniObject::fromString(Text).object<jstring>(),
            QJniObject::fromString(Source).object<jstring>(),
            QJniObject::fromString(Target).object<jstring>());
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

    void PlatformCheckModel(const QString& Source, const QString& Target)
    {
        IkonikaTranslationCheck(Source.toUtf8().constData(), Target.toUtf8().constData());
    }

    void PlatformDownload(const QString& Source, const QString& Target)
    {
        IkonikaTranslationDownload(Source.toUtf8().constData(), Target.toUtf8().constData());
    }

    void PlatformTranslate(quint64 Generation, const QString& Text, const QString& Source, const QString& Target)
    {
        IkonikaTranslationTranslate(Generation, Text.toUtf8().constData(), Source.toUtf8().constData(),
                                    Target.toUtf8().constData());
    }
#endif

    void RequestModelState(const QString& Source, const QString& Target)
    {
        const QString Pair = PairKey(Source, Target);

        if (States.contains(Pair))
        {
            return;
        }

        States.insert(Pair, FPairState{});

        EnsureBridge();
        PlatformCheckModel(Source, Target);
    }

    FPairState& StateFor(const QString& Source, const QString& Target)
    {
        RequestModelState(Source, Target);

        return States[PairKey(Source, Target)];
    }

    QString ActiveLearned()
    {
        return QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage);
    }

    QString ActiveNative()
    {
        return QString::fromStdString(FTSettings::Get().GetLanguageRules().NativeLanguage);
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
        return StateFor(ActiveLearned(), ActiveNative()).bDownloaded;
    }

    bool IsSupportedPair(const std::string& LearnedLanguage, const std::string& NativeLanguage)
    {
        return StateFor(QString::fromStdString(LearnedLanguage), QString::fromStdString(NativeLanguage)).bDownloaded;
    }

    bool IsAvailable()
    {
        return IsSupportedPair();
    }

    bool IsBusy()
    {
        return BusyCount.loadRelaxed() > 0;
    }

    void Translate(const QString& Text, quintptr Owner)
    {
        Translate(Text, Owner, FTSettings::Get().GetLanguageRules().TargetLanguage,
                  FTSettings::Get().GetLanguageRules().NativeLanguage);
    }

    void Translate(const QString& Text, quintptr Owner, const std::string& LearnedLanguage,
                   const std::string& NativeLanguage)
    {
        const QString Trimmed = Text.trimmed();

        if (Trimmed.isEmpty() || !IsSupportedPair(LearnedLanguage, NativeLanguage))
        {
            return;
        }

        const quint64 Generation = Requests.Begin(Owner);

        BusyCount.fetchAndAddOrdered(1);

        PlatformTranslate(Generation, Trimmed, QString::fromStdString(LearnedLanguage),
                          QString::fromStdString(NativeLanguage));
    }

    void Reset()
    {
        States.clear();

        qCInfo(LogTranslate) << "Translation model state forgotten, the next request checks it again";

        emit Notifier().modelsChanged();
    }

    bool IsPairOffered()
    {
        return IsPairOffered(FTSettings::Get().GetLanguageRules().TargetLanguage,
                             FTSettings::Get().GetLanguageRules().NativeLanguage);
    }

    bool IsPairOffered(const std::string& LearnedLanguage, const std::string& NativeLanguage)
    {
        const QString Source = QString::fromStdString(LearnedLanguage);
        const QString Target = QString::fromStdString(NativeLanguage);

#if defined(Q_OS_ANDROID)
        EnsureBridge();

        return QJniObject::callStaticMethod<jboolean>(PlatformTranslationClass, "isSupported",
            "(Ljava/lang/String;Ljava/lang/String;)Z",
            QJniObject::fromString(Source).object<jstring>(),
            QJniObject::fromString(Target).object<jstring>()) == JNI_TRUE;
#else
        return StateFor(Source, Target).bOffered;
#endif
    }

    bool IsDownloading()
    {
        return IsDownloading(FTSettings::Get().GetLanguageRules().TargetLanguage,
                             FTSettings::Get().GetLanguageRules().NativeLanguage);
    }

    bool IsDownloading(const std::string& LearnedLanguage, const std::string& NativeLanguage)
    {
        return StateFor(QString::fromStdString(LearnedLanguage), QString::fromStdString(NativeLanguage)).bDownloading;
    }

    void Download()
    {
        Download(FTSettings::Get().GetLanguageRules().TargetLanguage,
                 FTSettings::Get().GetLanguageRules().NativeLanguage);
    }

    void Download(const std::string& LearnedLanguage, const std::string& NativeLanguage)
    {
        const QString Source = QString::fromStdString(LearnedLanguage);
        const QString Target = QString::fromStdString(NativeLanguage);

        FPairState& State = StateFor(Source, Target);

        if (State.bDownloading || State.bDownloaded)
        {
            return;
        }

        State.bDownloading = true;

        emit Notifier().downloadingChanged();

        qCInfo(LogTranslate) << "Downloading the translation model for" << PairKey(Source, Target);

        PlatformDownload(Source, Target);
    }

    qint64 GetDownloadBytes()
    {
        return 0;
    }

    qint64 GetDownloadBytes(const std::string&, const std::string&)
    {
        return 0;
    }

    double GetDownloadProgress()
    {
        return 0.0;
    }

    double GetDownloadProgress(const std::string&, const std::string&)
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
