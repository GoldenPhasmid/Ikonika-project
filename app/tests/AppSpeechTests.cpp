#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "TestAppSpeech.h"

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
