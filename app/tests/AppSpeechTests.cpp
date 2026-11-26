#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "TestAppSpeech.h"
#include "core/FTSettings.h"

#include <QScopeGuard>
#include <QTemporaryDir>

TEST_CASE("Text of four or more words is a sentence", "[appspeech]")
{
    CHECK_FALSE(AppSpeech::IsSentence(""));
    CHECK_FALSE(AppSpeech::IsSentence("pomme"));
    CHECK_FALSE(AppSpeech::IsSentence("une pomme rouge"));
    CHECK(AppSpeech::IsSentence("une pomme bien rouge"));
    CHECK(AppSpeech::IsSentence("  Je   mange une pomme rouge. "));
}

TEST_CASE("Plain speech uses the speech rate for short text and the sentence speed for sentences", "[appspeech]")
{
    FAppSpeechGuard SpeechGuard;
    SpeechGuard.Mock.SentenceSpeed = 0.7f;

    AppSpeech::Get().Speak("pomme", "fr");
    CHECK(SpeechGuard.Mock.LastRateScale == 1.0f);

    AppSpeech::Get().Speak("une pomme rouge", "fr");
    CHECK(SpeechGuard.Mock.LastRateScale == 1.0f);

    AppSpeech::Get().Speak("Je mange une pomme rouge.", "fr");
    CHECK(SpeechGuard.Mock.LastRateScale == Catch::Approx(0.7f));
}

TEST_CASE("Platform voice preferences are local and independent for each language", "[appspeech]")
{
    QTemporaryDir Directory;
    REQUIRE(Directory.isValid());

    const QString FileName{Directory.filePath(QStringLiteral("settings.ini"))};
    FTSettings Workspace{FileName};
    FTSettings::SetActive(&Workspace);
    const auto ResetSettings{qScopeGuard([] { FTSettings::SetActive(nullptr); })};

    AppSpeech& Speech{AppSpeech::Get()};
    CHECK(Speech.GetPlatformVoiceId(QStringLiteral("fr")).isEmpty());
    Speech.SetPlatformVoiceId(QStringLiteral("fr"), QStringLiteral("fr-ca-x-caa-local"));
    Speech.SetPlatformVoiceId(QStringLiteral("en"), QStringLiteral("en-US-language"));

    FTSettings Reopened{FileName};
    FTSettings::SetActive(&Reopened);
    CHECK(Speech.GetPlatformVoiceId(QStringLiteral("fr")) == QStringLiteral("fr-ca-x-caa-local"));
    CHECK(Speech.GetPlatformVoiceId(QStringLiteral("en")) == QStringLiteral("en-US-language"));
    CHECK(Reopened.ReadSyncedValues().empty());

    Speech.SetPlatformVoiceId(QStringLiteral("fr"), QString());
    CHECK(Speech.GetPlatformVoiceId(QStringLiteral("fr")).isEmpty());
    CHECK(Speech.GetPlatformVoiceId(QStringLiteral("en")) == QStringLiteral("en-US-language"));
}
