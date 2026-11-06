#include "AppSoundEffects.h"

#include "AppSpeech.h"

#include "core/FTAudioPlayer.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QDate>
#include <QFile>
#include <QMutexLocker>
#include <QSettings>
#include <QtEndian>

#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>
#endif

namespace
{
    const QString EnabledKey = QStringLiteral("sound/effects");
    const QString GoalDayKey = QStringLiteral("sound/goalDay");

    bool IsSpeechMuted()
    {
        const QMutexLocker Guard(&AppSpeech::Get().Lock());

        return !AppSpeech::Get().IsEnabled();
    }

#if defined(Q_OS_ANDROID)
    constexpr const char* AndroidSoundEffectsClass = "com/goldenphasmid/ikonika/PlatformSoundEffects";
#else
    const QStringList SoundNames{QStringLiteral("sfx_incorrect"), QStringLiteral("sfx_complete"),
                                 QStringLiteral("sfx_goal")};

    constexpr int SoundSampleRate = 44100;

    std::vector<int16_t> ReadWave(const QString& Path)
    {
        QFile File(Path);

        if (!File.open(QIODevice::ReadOnly))
        {
            qCWarning(LogApp) << "sound effect file missing:" << Path;

            return {};
        }

        const QByteArray Bytes = File.readAll();

        if (Bytes.size() < 12 || !Bytes.startsWith("RIFF") || Bytes.mid(8, 4) != "WAVE")
        {
            qCWarning(LogApp) << "sound effect file is not a wave file:" << Path;

            return {};
        }

        bool bPlayableFormat = false;
        qsizetype Offset = 12;

        while (Offset + 8 <= Bytes.size())
        {
            const QByteArray ChunkId = Bytes.mid(Offset, 4);
            const qsizetype ChunkSize = qFromLittleEndian<quint32>(Bytes.constData() + Offset + 4);
            const char* Body = Bytes.constData() + Offset + 8;

            if (Offset + 8 + ChunkSize > Bytes.size())
            {
                break;
            }

            if (ChunkId == "fmt " && ChunkSize >= 16)
            {
                const quint16 Format = qFromLittleEndian<quint16>(Body);
                const quint16 Channels = qFromLittleEndian<quint16>(Body + 2);
                const quint32 SampleRate = qFromLittleEndian<quint32>(Body + 4);
                const quint16 Bits = qFromLittleEndian<quint16>(Body + 14);

                bPlayableFormat = Format == 1 && Channels == 1 && Bits == 16 && SampleRate == SoundSampleRate;
            }
            else if (ChunkId == "data")
            {
                if (!bPlayableFormat)
                {
                    break;
                }

                std::vector<int16_t> Samples(static_cast<size_t>(ChunkSize / 2));
                qFromLittleEndian<qint16>(Body, static_cast<qsizetype>(Samples.size()), Samples.data());

                return Samples;
            }

            Offset += 8 + ChunkSize + (ChunkSize & 1);
        }

        qCWarning(LogApp) << "sound effect file is not 16-bit mono" << SoundSampleRate << "Hz PCM:" << Path;

        return {};
    }
#endif
}

AppSoundEffects& AppSoundEffects::Get()
{
    static AppSoundEffects Instance;

    return Instance;
}

AppSoundEffects::AppSoundEffects(QObject* Parent)
    : QObject(Parent)
{
}

AppSoundEffects::~AppSoundEffects() = default;

void AppSoundEffects::Prewarm()
{
#if defined(Q_OS_ANDROID)
    QJniObject Activity = QNativeInterface::QAndroidApplication::context();

    QJniObject::callStaticMethod<void>(AndroidSoundEffectsClass, "initialize",
        "(Landroid/content/Context;)V", Activity.object<jobject>());
#else
    if (Player)
    {
        return;
    }

    for (const QString& Name : SoundNames)
    {
        std::vector<int16_t> Samples = ReadWave(QStringLiteral(":/sounds/%1.wav").arg(Name));

        if (!Samples.empty())
        {
            Sounds.insert(Name, std::move(Samples));
        }
    }

    Player = std::make_unique<FTAudioPlayer>(SoundSampleRate);

    if (!Player->IsAvailable())
    {
        qCWarning(LogApp) << "sound effects have no audio device";
    }
#endif
}

bool AppSoundEffects::IsEnabled() const
{
    return FTSettings::Get().OpenStore().value(EnabledKey, true).toBool();
}

void AppSoundEffects::SetEnabled(bool bValue)
{
    if (IsEnabled() == bValue)
    {
        return;
    }

    FTSettings::Get().OpenStore().setValue(EnabledKey, bValue);

    qCInfo(LogApp) << "sound effects" << (bValue ? "enabled" : "disabled");

    emit enabledChanged();
}

void AppSoundEffects::PlayIncorrect()
{
    Play(QStringLiteral("sfx_incorrect"));
}

void AppSoundEffects::PlayComplete()
{
    Play(QStringLiteral("sfx_complete"));
}

void AppSoundEffects::celebrateDailyGoal()
{
    if (!ClaimDailyGoal())
    {
        return;
    }

    Play(QStringLiteral("sfx_goal"));
}

bool AppSoundEffects::ClaimDailyGoal()
{
    const qint64 Today = QDate::currentDate().toJulianDay();

    QSettings Settings(FTSettings::Get().OpenStore());

    if (Settings.value(GoalDayKey, 0).toLongLong() == Today)
    {
        return false;
    }

    Settings.setValue(GoalDayKey, Today);

    return true;
}

void AppSoundEffects::Play(const QString& Name)
{
    if (!IsEnabled() || IsSpeechMuted())
    {
        return;
    }

    qCDebug(LogApp) << "sound effect:" << Name;

    Prewarm();

#if defined(Q_OS_ANDROID)
    QJniObject JniName = QJniObject::fromString(Name);

    QJniObject::callStaticMethod<void>(AndroidSoundEffectsClass, "play",
        "(Ljava/lang/String;)V", JniName.object<jstring>());
#else
    const auto Found = Sounds.constFind(Name);

    if (Found == Sounds.constEnd())
    {
        qCWarning(LogApp) << "sound effect not loaded:" << Name;

        return;
    }

    if (!Player->Play(Found.value(), SoundSampleRate))
    {
        qCWarning(LogApp) << "sound effect did not play:" << Name;
    }
#endif
}
