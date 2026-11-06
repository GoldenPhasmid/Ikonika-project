#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/FTSpeech.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <string>
#include <vector>

namespace
{
    const std::string PiperDirectory = PIPER_MODEL_DIRECTORY;
    const std::string EnglishDirectory = ENGLISH_MODEL_DIRECTORY;
    const std::string SpanishDirectory = SPANISH_MODEL_DIRECTORY;
    const std::string SupertonicDirectory = SUPERTONIC_MODEL_DIRECTORY;

    std::vector<std::string> BothModels()
    {
        return {PiperDirectory, SupertonicDirectory};
    }
}

TEST_CASE("A Piper directory is detected as Piper", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer = FTSpeechSynthesizer::Create(PiperDirectory);

    if (!Synthesizer.has_value())
    {
        WARN("the Piper model is absent, so it is untested here");

        return;
    }

    CHECK(Synthesizer->GetEngine() == "Piper");
    CHECK(Synthesizer->GetSampleRate() > 0);

    // One voice per model in this family, unlike Supertonic.
    CHECK(Synthesizer->GetSpeakerCount() == 1);
}

TEST_CASE("A Supertonic directory is detected as Supertonic", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer = FTSpeechSynthesizer::Create(SupertonicDirectory);

    if (!Synthesizer.has_value())
    {
        WARN("the Supertonic model is absent, so it is untested here");

        return;
    }

    CHECK(Synthesizer->GetEngine() == "Supertonic");
    CHECK(Synthesizer->GetSampleRate() > 0);
    CHECK(Synthesizer->GetSpeakerCount() > 1);
}

TEST_CASE("The two families run at different sample rates", "[speech]")
{
    // Which is the whole reason each voice owns its own audio device rather than sharing one.
    const std::optional<FTSpeechSynthesizer> Piper = FTSpeechSynthesizer::Create(PiperDirectory);
    const std::optional<FTSpeechSynthesizer> Supertonic = FTSpeechSynthesizer::Create(SupertonicDirectory);

    if (!Piper.has_value() || !Supertonic.has_value())
    {
        return;
    }

    CHECK(Piper->GetSampleRate() != Supertonic->GetSampleRate());
}

TEST_CASE("A directory holding neither family is refused rather than crashing", "[speech]")
{
    CHECK_FALSE(FTSpeechSynthesizer::Create("this/path/does/not/exist").has_value());
    CHECK_FALSE(FTSpeechSynthesizer::Create("").has_value());

    // A real directory with no model directly in it: the root both model folders sit under.
    CHECK_FALSE(FTSpeechSynthesizer::Create(PiperDirectory + "/..").has_value());
}

TEST_CASE("Both engines synthesise French, single words and sentences", "[speech]")
{
    for (const std::string& Directory : BothModels())
    {
        const std::optional<FTSpeechSynthesizer> Synthesizer = FTSpeechSynthesizer::Create(Directory);

        if (!Synthesizer.has_value())
        {
            continue;
        }

        const std::vector<int16_t> Word = Synthesizer->Synthesize("le chien");
        const std::vector<int16_t> Sentence =
            Synthesizer->Synthesize("Les amis de l'école ont un petit enfant.");

        REQUIRE_FALSE(Word.empty());
        REQUIRE_FALSE(Sentence.empty());

        // Sentences were the reason for moving off eSpeak in the first place.
        CHECK(Sentence.size() > Word.size());

        for (const char* Accented : {"le café", "l'école", "la fenêtre", "le garçon"})
        {
            CHECK_FALSE(Synthesizer->Synthesize(Accented).empty());
        }
    }
}

TEST_CASE("Supertonic is given a language, not left on its English default", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> French =
        FTSpeechSynthesizer::Create(SupertonicDirectory, "fr");
    const std::optional<FTSpeechSynthesizer> English =
        FTSpeechSynthesizer::Create(SupertonicDirectory, "en");

    if (!French.has_value() || !English.has_value())
    {
        return;
    }

    const std::vector<int16_t> AsFrench = French->Synthesize("second");
    const std::vector<int16_t> AsEnglish = English->Synthesize("second");

    REQUIRE_FALSE(AsFrench.empty());
    REQUIRE_FALSE(AsEnglish.empty());

    CHECK(AsFrench != AsEnglish);
}

TEST_CASE("The language code is ignored by Piper, whose model fixes it", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> First = FTSpeechSynthesizer::Create(PiperDirectory, "fr");
    const std::optional<FTSpeechSynthesizer> Second = FTSpeechSynthesizer::Create(PiperDirectory, "en");

    if (!First.has_value() || !Second.has_value())
    {
        return;
    }

    CHECK(First->Synthesize("second") == Second->Synthesize("second"));
}

TEST_CASE("Empty text produces no audio rather than failing", "[speech]")
{
    for (const std::string& Directory : BothModels())
    {
        const std::optional<FTSpeechSynthesizer> Synthesizer = FTSpeechSynthesizer::Create(Directory);

        if (Synthesizer.has_value())
        {
            CHECK(Synthesizer->Synthesize("").empty());
        }
    }
}

TEST_CASE("Speed changes the amount of audio", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer = FTSpeechSynthesizer::Create(PiperDirectory);

    if (!Synthesizer.has_value())
    {
        return;
    }

    const std::vector<int16_t> Fast = Synthesizer->Synthesize("le chien", 0, MaximumSpeechSpeed);
    const std::vector<int16_t> Slow = Synthesizer->Synthesize("le chien", 0, MinimumSpeechSpeed);

    REQUIRE_FALSE(Fast.empty());
    REQUIRE_FALSE(Slow.empty());

    CHECK(Slow.size() > Fast.size());
}

TEST_CASE("Speed is clamped rather than trusted", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer = FTSpeechSynthesizer::Create(PiperDirectory);

    if (!Synthesizer.has_value())
    {
        return;
    }

    // Zero or negative would be a division by zero inside the model's length scaling.
    CHECK_FALSE(Synthesizer->Synthesize("le chien", 0, 0.0f).empty());
    CHECK_FALSE(Synthesizer->Synthesize("le chien", 0, -5.0f).empty());
    CHECK_FALSE(Synthesizer->Synthesize("le chien", 0, 100.0f).empty());
}

TEST_CASE("Speed changes the amount of Supertonic audio", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer = FTSpeechSynthesizer::Create(SupertonicDirectory, "fr");

    if (!Synthesizer.has_value())
    {
        WARN("the Supertonic model is absent, so it is untested here");

        return;
    }

    const std::vector<int16_t> Natural = Synthesizer->Synthesize("le chien", 0, 1.0f);
    const std::vector<int16_t> Slow = Synthesizer->Synthesize("le chien", 0, 0.8f);

    REQUIRE_FALSE(Natural.empty());
    REQUIRE_FALSE(Slow.empty());

    CHECK(Slow.size() > Natural.size());
}

TEST_CASE("Supertonic renders the same text the same way twice", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer = FTSpeechSynthesizer::Create(SupertonicDirectory, "fr");

    if (!Synthesizer.has_value())
    {
        WARN("the Supertonic model is absent, so it is untested here");

        return;
    }

    const std::string Sentence = "Les commerçants ont été indemnisés après les inondations.";

    CHECK(Synthesizer->Synthesize(Sentence, 0, 0.8f) == Synthesizer->Synthesize(Sentence, 0, 0.8f));
}

TEST_CASE("A service loads every model it is given", "[speech]")
{
    const FTSpeechService Service(BothModels());

    if (Service.GetVoiceCount() == 0)
    {
        WARN("no models present, so the service is untested here");

        return;
    }

    for (int Index = 0; Index < Service.GetVoiceCount(); ++Index)
    {
        const FTVoiceInfo Info = Service.GetVoiceInfo(Index);

        CHECK_FALSE(Info.Name.empty());
        CHECK(Info.SampleRate > 0);
        CHECK(Info.SpeakerCount >= 1);
    }
}

TEST_CASE("Loading the same directory twice does not duplicate the voice", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (Service.GetVoiceCount() == 0)
    {
        WARN("no models present, so the service is untested here");

        return;
    }

    const int Before = Service.GetVoiceCount();

    Service.LoadVoices(BothModels());

    CHECK(Service.GetVoiceCount() == Before);
}

TEST_CASE("Loading picks up a directory that was not there on an earlier call", "[speech]")
{
    FTSpeechService Service({PiperDirectory});

    if (Service.GetVoiceCount() == 0)
    {
        WARN("no models present, so the service is untested here");

        return;
    }

    const int Before = Service.GetVoiceCount();

    Service.LoadVoices({PiperDirectory, SpanishDirectory});

    if (Service.GetVoiceCount() == Before)
    {
        WARN("the Spanish model is absent, so the new-arrival case is untested here");

        return;
    }

    CHECK(Service.GetVoiceCount() == Before + 1);
}

TEST_CASE("A service given nothing usable stays harmless", "[speech]")
{
    FTSpeechService Broken({"nowhere", "also/nowhere"});

    CHECK(Broken.GetVoiceCount() == 0);
    CHECK_FALSE(Broken.IsAvailable());
    CHECK_FALSE(Broken.Speak("le chien", "fr"));
    CHECK(Broken.GetCachedCount() == 0);
    CHECK(Broken.GetSpeakerId() == 0);

    Broken.SetActiveVoice(0);
    Broken.SetSpeakerId(3);
    Broken.Stop();

    CHECK(Broken.GetActiveVoice() == 0);
}

TEST_CASE("Switching voice is possible and out of range is ignored", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (Service.GetVoiceCount() < 2)
    {
        return;
    }

    CHECK(Service.GetActiveVoice() == 0);

    Service.SetActiveVoice(1);
    CHECK(Service.GetActiveVoice() == 1);

    Service.SetActiveVoice(-1);
    Service.SetActiveVoice(99);
    CHECK(Service.GetActiveVoice() == 1);
}

TEST_CASE("Each voice keeps its own cache across a switch", "[speech]")
{
    // The reason both models stay loaded: going back to a voice should not re-synthesise what it has
    // already produced, so comparing two voices on one card is instant either way.
    FTSpeechService Service(BothModels());

    if (Service.GetVoiceCount() < 2)
    {
        return;
    }

    Service.SetActiveVoice(0);
    Service.Speak("le chien", "fr");
    const int FirstVoiceCached = Service.GetCachedCount();

    Service.SetActiveVoice(1);
    CHECK(Service.GetCachedCount() == 0);

    Service.Speak("la maison", "fr");
    CHECK(Service.GetCachedCount() == 1);

    Service.SetActiveVoice(0);
    CHECK(Service.GetCachedCount() == FirstVoiceCached);
}

TEST_CASE("The cache keys on speaker and speed, not only text", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (Service.GetVoiceCount() == 0)
    {
        return;
    }

    Service.Speak("le chien", "fr");

    if (Service.GetCachedCount() == 0)
    {
        WARN("nothing was synthesised, so caching is untested here");

        return;
    }

    CHECK(Service.GetCachedCount() == 1);

    // Changing speed drops the caches outright, because everything held was rendered at the old speed.
    Service.SetSpeed(0.7f);
    CHECK(Service.GetCachedCount() == 0);

    Service.Speak("le chien", "fr");
    CHECK(Service.GetCachedCount() == 1);
}

TEST_CASE("A line spoken at a slower rate is rendered and cached on its own", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (Service.GetVoiceCount() == 0)
    {
        return;
    }

    Service.Speak("le chien dort", "fr");

    if (Service.GetCachedCount() == 0)
    {
        WARN("nothing was synthesised, so caching is untested here");

        return;
    }

    CHECK(Service.GetCachedCount() == 1);

    Service.Speak("le chien dort", "fr", 0.75f);
    CHECK(Service.GetCachedCount() == 2);

    Service.Speak("le chien dort", "fr", 0.75f);
    CHECK(Service.GetCachedCount() == 2);

    Service.Speak("le chien dort", "fr", 1.0f);
    CHECK(Service.GetCachedCount() == 2);
}

TEST_CASE("The cache is capped by entry count and evicts the least recently used", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (!Service.IsAvailable())
    {
        WARN("no voice model present, so this is untested here");

        return;
    }

    Service.SetMaximumCacheEntries(2);

    Service.Prepare("chien");
    Service.Prepare("chat");

    if (Service.GetCachedCount() < 2)
    {
        WARN("nothing was synthesised, so caching is untested here");

        return;
    }

    CHECK(Service.GetCachedCount() == 2);

    // A third line pushes the cache past its cap. Neither line has been asked for again since it
    // was rendered, so "chien" - the older of the two - is the one dropped.
    Service.Prepare("maison");

    CHECK(Service.GetCachedCount() == 2);
    CHECK_FALSE(Service.IsCached("chien"));
    CHECK(Service.IsCached("chat"));
    CHECK(Service.IsCached("maison"));
}

TEST_CASE("Re-speaking a cached line marks it recently used", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (!Service.IsAvailable())
    {
        WARN("no voice model present, so this is untested here");

        return;
    }

    Service.SetMaximumCacheEntries(2);

    Service.Prepare("chien");
    Service.Prepare("chat");

    if (Service.GetCachedCount() < 2)
    {
        WARN("nothing was synthesised, so caching is untested here");

        return;
    }

    // Asking for "chien" again moves it to the back of the eviction order, ahead of "chat", which
    // has not been touched since it was first rendered.
    Service.Prepare("chien");
    Service.Prepare("maison");

    CHECK(Service.IsCached("chien"));
    CHECK_FALSE(Service.IsCached("chat"));
    CHECK(Service.IsCached("maison"));
}

TEST_CASE("Shrinking the cache cap evicts immediately", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (!Service.IsAvailable())
    {
        WARN("no voice model present, so this is untested here");

        return;
    }

    Service.Prepare("chien");
    Service.Prepare("chat");
    Service.Prepare("maison");

    if (Service.GetCachedCount() < 3)
    {
        WARN("nothing was synthesised, so caching is untested here");

        return;
    }

    // Not just refused on the next render: dropping the cap evicts down to it right away.
    Service.SetMaximumCacheEntries(1);

    CHECK(Service.GetCachedCount() == 1);
    CHECK(Service.IsCached("maison"));
}

TEST_CASE("The cache key folds case", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (!Service.IsAvailable())
    {
        WARN("no voice model present, so this is untested here");

        return;
    }

    Service.Prepare("Chien");

    if (Service.GetCachedCount() == 0)
    {
        WARN("nothing was synthesised, so caching is untested here");

        return;
    }

    CHECK(Service.GetCachedCount() == 1);
    CHECK(Service.IsCached("chien"));
    CHECK(Service.IsCached("CHIEN"));
    CHECK(Service.IsCached("ChIeN"));

    // Same key as "Chien", so this is a cache hit rather than a second entry.
    Service.Prepare("chien");
    CHECK(Service.GetCachedCount() == 1);
}

TEST_CASE("Speed is clamped and reported back", "[speech]")
{
    FTSpeechService Service(BothModels());

    Service.SetSpeed(0.1f);
    CHECK(Service.GetSpeed() == MinimumSpeechSpeed);

    Service.SetSpeed(10.0f);
    CHECK(Service.GetSpeed() == MaximumSpeechSpeed);

    Service.SetSpeed(1.0f);
    CHECK(Service.GetSpeed() == 1.0f);
}

TEST_CASE("A speaker beyond the active voice's count is ignored", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (Service.GetVoiceCount() == 0)
    {
        return;
    }

    Service.SetSpeakerId(-1);
    Service.SetSpeakerId(9999);

    CHECK(Service.GetSpeakerId() == 0);
}

TEST_CASE("Muting stops speech being produced at all", "[speech]")
{
    FTSpeechService Service(BothModels());

    Service.SetEnabled(false);

    CHECK_FALSE(Service.IsEnabled());
    CHECK_FALSE(Service.Speak("le chien", "fr"));
    CHECK(Service.GetCachedCount() == 0);

    Service.SetEnabled(true);
    CHECK(Service.IsEnabled());
}

TEST_CASE("A single English word is spoken with a trailing pause", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer =
        FTSpeechSynthesizer::Create(EnglishDirectory, "en");

    if (!Synthesizer.has_value())
    {
        WARN("the English model is absent, so it is untested here");

        return;
    }

    FTSpeechService Service({EnglishDirectory});

    if (!Service.IsAvailable())
    {
        WARN("no voice model present, so this is untested here");

        return;
    }

    REQUIRE(Service.Speak("quick", "en"));
    CHECK(Service.GetCachedCount() == 1);

    REQUIRE(Service.Speak("quick,", "en"));
    CHECK(Service.GetCachedCount() == 1);

    REQUIRE(Service.Speak("quick fox", "en"));
    CHECK(Service.GetCachedCount() == 2);
}

TEST_CASE("A player refuses a sample rate it was not opened with", "[speech]")
{
    // The bug this replaced accepted a rate per call and ignored it, so a mismatch played at the wrong
    // pitch. That went unnoticed only while every model happened to run at 22050.
    FTAudioPlayer Player(44100);

    CHECK(Player.GetSampleRate() == 44100);

    if (Player.IsAvailable())
    {
        CHECK_FALSE(Player.Play(std::vector<int16_t>(1000, 0), 22050));
        CHECK(Player.Play(std::vector<int16_t>(1000, 0), 44100));
    }

    Player.Stop();
}

TEST_CASE("A player with no rate opens no device and stays harmless", "[speech]")
{
    FTAudioPlayer Player(0);

    CHECK_FALSE(Player.IsAvailable());
    CHECK(Player.GetSampleRate() == 0);
    CHECK_FALSE(Player.Play(std::vector<int16_t>(10, 0), 44100));
    CHECK_FALSE(Player.IsPlaying());

    Player.Stop();
}

/*
 * Prepare exists so the app can pay the synthesis cost on a worker thread and leave the interface
 * responsive. A sentence takes a few hundred milliseconds, which is a visible stall if it happens
 * where the interface is drawn.
 */

TEST_CASE("Preparing text caches it without playing", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (!Service.IsAvailable())
    {
        WARN("no voice model present, so this is untested here");

        return;
    }

    const std::string Word = "chien";

    CHECK_FALSE(Service.IsCached(Word));

    REQUIRE(Service.Prepare(Word));

    // The point of the split: the audio exists before anything asks to hear it.
    CHECK(Service.IsCached(Word));
    CHECK(Service.GetCachedCount() == 1);
}

TEST_CASE("Preparing the same text twice renders once", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (!Service.IsAvailable())
    {
        WARN("no voice model present, so this is untested here");

        return;
    }

    REQUIRE(Service.Prepare("chien"));
    REQUIRE(Service.Prepare("chien"));

    CHECK(Service.GetCachedCount() == 1);
}

TEST_CASE("Prepared text is not cached under a different speed", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (!Service.IsAvailable())
    {
        WARN("no voice model present, so this is untested here");

        return;
    }

    REQUIRE(Service.Prepare("chien"));
    REQUIRE(Service.IsCached("chien"));

    // Speed is part of the audio's identity, so the old rendering must not be reused.
    Service.SetSpeed(1.5f);

    CHECK_FALSE(Service.IsCached("chien"));
}

TEST_CASE("Nothing is prepared while muted or for empty text", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (!Service.IsAvailable())
    {
        WARN("no voice model present, so this is untested here");

        return;
    }

    CHECK_FALSE(Service.Prepare(""));

    Service.SetEnabled(false);

    CHECK_FALSE(Service.Prepare("chien"));
    CHECK_FALSE(Service.IsCached("chien"));
}

/*
 * Which voice can speak which language is written down in code, because neither engine reports it.
 * A wrong entry produces confident nonsense rather than an error, so these tests pin the lists and
 * the fallback rule rather than the audio.
 */

TEST_CASE("Piper claims only the language its model was built for", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer =
        FTSpeechSynthesizer::Create(PiperDirectory, "fr");

    if (!Synthesizer.has_value())
    {
        WARN("the Piper model is absent, so it is untested here");

        return;
    }

    REQUIRE(Synthesizer->GetLanguages().size() == 1);
    CHECK(Synthesizer->GetLanguages().front() == "fr");
}

TEST_CASE("Piper derives its language from the model, not the caller's default", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer =
        FTSpeechSynthesizer::Create(EnglishDirectory);

    if (!Synthesizer.has_value())
    {
        WARN("the English model is absent, so it is untested here");

        return;
    }

    REQUIRE(Synthesizer->GetLanguages().size() == 1);
    CHECK(Synthesizer->GetLanguages().front() == "en");
}

TEST_CASE("Each loaded voice reports its own language, not a shared default", "[speech]")
{
    FTSpeechService Service({EnglishDirectory, SpanishDirectory, PiperDirectory});

    if (Service.GetVoiceCount() < 3)
    {
        WARN("not every language's model is present, so this is untested here");

        return;
    }

    std::vector<std::string> Languages;

    for (int Index = 0; Index < Service.GetVoiceCount(); ++Index)
    {
        REQUIRE(Service.GetVoiceInfo(Index).Languages.size() == 1);
        Languages.push_back(Service.GetVoiceInfo(Index).Languages.front());
    }

    CHECK(std::find(Languages.begin(), Languages.end(), "en") != Languages.end());
    CHECK(std::find(Languages.begin(), Languages.end(), "es") != Languages.end());
    CHECK(std::find(Languages.begin(), Languages.end(), "fr") != Languages.end());
}

TEST_CASE("Supertonic claims the languages it ships with", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer =
        FTSpeechSynthesizer::Create(SupertonicDirectory, "fr");

    if (!Synthesizer.has_value())
    {
        WARN("the Supertonic model is absent, so it is untested here");

        return;
    }

    const std::vector<std::string>& Languages = Synthesizer->GetLanguages();

    // The documented set, so a model update that changes it fails here rather than mispronouncing.
    CHECK(Languages.size() == 31);

    for (const char* Code : {"en", "fr", "uk", "ja", "de"})
    {
        CHECK(std::find(Languages.begin(), Languages.end(), Code) != Languages.end());
    }

    // Not in the documented list, so nothing should claim it.
    CHECK(std::find(Languages.begin(), Languages.end(), "zz") == Languages.end());
}

TEST_CASE("A language the chosen voice cannot speak goes to the wider voice", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (Service.GetVoiceCount() < 2)
    {
        WARN("both models are needed to test the fallback");

        return;
    }

    // Find the narrow voice and select it, so the fallback is the one being exercised.
    int Narrow = -1;

    for (int Index = 0; Index < Service.GetVoiceCount(); ++Index)
    {
        if (Service.GetVoiceInfo(Index).Languages.size() == 1)
        {
            Narrow = Index;
        }
    }

    REQUIRE(Narrow >= 0);

    Service.SetActiveVoice(Narrow);

    // French it can do itself.
    CHECK(Service.FindVoiceForLanguage("fr") == Narrow);

    // English it cannot, so this has to land on the other voice.
    const int ForEnglish = Service.FindVoiceForLanguage("en");
    CHECK(ForEnglish != Narrow);
    CHECK(Service.GetVoiceInfo(ForEnglish).Languages.size() > 1);

    // Exactly one voice is the fallback, and it is the wider one.
    CHECK(Service.GetVoiceInfo(ForEnglish).bFallback);
    CHECK_FALSE(Service.GetVoiceInfo(Narrow).bFallback);
}

TEST_CASE("Switching the requested language finds the matching voice among several single-language "
          "ones, with no wide voice present", "[speech]")
{
    FTSpeechService Service({EnglishDirectory, SpanishDirectory, PiperDirectory});

    if (Service.GetVoiceCount() < 3)
    {
        WARN("not every language's model is present, so this is untested here");

        return;
    }

    auto IndexForLanguage = [&Service](const std::string& LanguageCode)
    {
        for (int Index = 0; Index < Service.GetVoiceCount(); ++Index)
        {
            if (Service.GetVoiceInfo(Index).Languages.front() == LanguageCode)
            {
                return Index;
            }
        }

        return -1;
    };

    const int FrenchIndex = IndexForLanguage("fr");
    const int SpanishIndex = IndexForLanguage("es");
    const int EnglishIndex = IndexForLanguage("en");

    REQUIRE(FrenchIndex >= 0);
    REQUIRE(SpanishIndex >= 0);
    REQUIRE(EnglishIndex >= 0);

    Service.SetActiveVoice(FrenchIndex);
    CHECK(Service.FindVoiceForLanguage("es") == SpanishIndex);
    CHECK(Service.FindVoiceForLanguage("fr") == FrenchIndex);

    Service.SetActiveVoice(SpanishIndex);
    CHECK(Service.FindVoiceForLanguage("fr") == FrenchIndex);
    CHECK(Service.FindVoiceForLanguage("en") == EnglishIndex);
}

TEST_CASE("A language nothing supports still speaks rather than going silent", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (!Service.IsAvailable())
    {
        WARN("no voice model present, so this is untested here");

        return;
    }

    // Nothing claims this code. Silence would leave the user with no idea why, so the active voice
    // takes it and mispronounces it.
    CHECK(Service.FindVoiceForLanguage("zz") == Service.GetActiveVoice());
}

TEST_CASE("The same text in two languages is cached separately", "[speech]")
{
    FTSpeechService Service(BothModels());

    if (Service.GetVoiceCount() < 2)
    {
        WARN("both models are needed, since one voice cannot do two languages");

        return;
    }

    // Pick the wide voice, so both languages go to the same one and the cache is the only thing
    // separating them.
    for (int Index = 0; Index < Service.GetVoiceCount(); ++Index)
    {
        if (Service.GetVoiceInfo(Index).Languages.size() > 1)
        {
            Service.SetActiveVoice(Index);
        }
    }

    // "day" is a word in both, so a key without the language would replay the wrong sounds.
    REQUIRE(Service.Speak("day", "en"));
    const int AfterEnglish = Service.GetCachedCount();

    REQUIRE(Service.Speak("day", "fr"));

    CHECK(Service.GetCachedCount() == AfterEnglish + 1);
}

TEST_CASE("scratch: Piper vs Supertonic across text lengths", "[speech][.]")
{
    const std::vector<std::pair<std::string, std::string>> Texts = {
        {"word_short", "chat"},
        {"word_medium", "montagne"},
        {"sentence_short", "Le chat dort."},
        {"sentence_long", "Le renard brun saute par-dessus le chien paresseux."}
    };

    for (const std::string& Engine : {std::string("Piper"), std::string("Supertonic")})
    {
        const std::string& Directory = Engine == "Piper" ? PiperDirectory : SupertonicDirectory;

        const std::optional<FTSpeechSynthesizer> Synthesizer =
            FTSpeechSynthesizer::Create(Directory, "fr", 2);

        if (!Synthesizer.has_value() || !Synthesizer->IsValid())
        {
            WARN(Engine << " is absent, cannot time it here");

            continue;
        }

        Synthesizer->Synthesize("a");

        for (const auto& [Label, Text] : Texts)
        {
            const auto Start = std::chrono::steady_clock::now();
            const std::vector<int16_t> Samples = Synthesizer->Synthesize(Text);
            const auto Elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - Start);

            REQUIRE(!Samples.empty());

            WARN(Engine << " " << Label << " (" << Text.size() << " chars): "
                 << Elapsed.count() << " ms");
        }
    }
}

TEST_CASE("A quiet clip is boosted to the target loudness", "[loudness]")
{
    std::vector<float> Quiet(1000);

    for (size_t Index = 0; Index < Quiet.size(); ++Index)
    {
        Quiet[Index] = Index % 2 == 0 ? 0.01f : -0.01f;
    }

    const float Gain = ComputeSpeechGain(Quiet);

    CHECK(Gain * 0.01f == Catch::Approx(TargetSpeechRms).epsilon(0.001));
}

TEST_CASE("A loud clip is attenuated toward the target loudness", "[loudness]")
{
    std::vector<float> Loud(1000);

    for (size_t Index = 0; Index < Loud.size(); ++Index)
    {
        Loud[Index] = Index % 2 == 0 ? 0.3f : -0.3f;
    }

    const float Gain = ComputeSpeechGain(Loud);

    CHECK(Gain < 1.0f);
    CHECK(Gain * 0.3f == Catch::Approx(TargetSpeechRms).epsilon(0.001));
}

TEST_CASE("The gain never pushes a peak past the ceiling", "[loudness]")
{
    std::vector<float> Spiky(1000, 0.001f);
    Spiky[500] = 0.5f;

    const float Gain = ComputeSpeechGain(Spiky);

    CHECK(Gain * 0.5f <= 0.9f + 1.0e-4f);
}

TEST_CASE("Silence and empty clips keep unit gain", "[loudness]")
{
    CHECK(ComputeSpeechGain({}) == 1.0f);

    const std::vector<float> Silent(1000, 0.0f);

    CHECK(ComputeSpeechGain(Silent) == 1.0f);
}

TEST_CASE("The gain is capped so a near-silent clip is not blown up", "[loudness]")
{
    const std::vector<float> Faint(1000, 0.0005f);

    CHECK(ComputeSpeechGain(Faint) <= 16.0f);
}

TEST_CASE("Leading silence is trimmed up to a short margin before the onset", "[loudness]")
{
    constexpr int SampleRate = 1000;
    std::vector<float> Clip(1000, 0.0f);
    std::fill(Clip.begin() + 400, Clip.end(), 0.5f);

    const size_t Margin = static_cast<size_t>(SampleRate * LeadingSilenceMarginSeconds);

    CHECK(FindSpeechStart(Clip, SampleRate) == 400 - Margin);
}

TEST_CASE("Sound below the silence threshold counts as silence", "[loudness]")
{
    constexpr int SampleRate = 1000;
    std::vector<float> Clip(1000, 0.5f * LeadingSilenceThreshold * 0.5f);
    std::fill(Clip.begin() + 300, Clip.end(), 0.5f);

    CHECK(FindSpeechStart(Clip, SampleRate) == 300 - static_cast<size_t>(SampleRate * LeadingSilenceMarginSeconds));
}

TEST_CASE("A clip that starts loud, or is silent throughout, is not trimmed", "[loudness]")
{
    const std::vector<float> Loud(1000, 0.5f);
    const std::vector<float> Silent(1000, 0.0f);

    CHECK(FindSpeechStart(Loud, 1000) == 0);
    CHECK(FindSpeechStart(Silent, 1000) == 0);
    CHECK(FindSpeechStart({}, 1000) == 0);
}

namespace
{
    std::vector<float> MakeTone(int SampleRate, double Frequency, double Seconds)
    {
        std::vector<float> Tone(static_cast<size_t>(SampleRate * Seconds));

        for (size_t Index = 0; Index < Tone.size(); ++Index)
        {
            Tone[Index] = static_cast<float>(0.5 * std::sin(2.0 * 3.14159265358979 * Frequency * static_cast<double>(Index) / SampleRate));
        }

        return Tone;
    }

    double MeasureFrequency(const std::vector<float>& Samples, int SampleRate)
    {
        const size_t Margin = static_cast<size_t>(SampleRate / 10);
        int RisingCrossings = 0;

        for (size_t Index = Margin + 1; Index < Samples.size() - Margin; ++Index)
        {
            if (Samples[Index - 1] < 0.0f && Samples[Index] >= 0.0f)
            {
                ++RisingCrossings;
            }
        }

        return RisingCrossings * static_cast<double>(SampleRate) / static_cast<double>(Samples.size() - 2 * Margin);
    }
}

TEST_CASE("Stretching changes the length by the tempo", "[stretch]")
{
    constexpr int SampleRate = 16000;
    const std::vector<float> Tone = MakeTone(SampleRate, 220.0, 1.0);

    CHECK(static_cast<double>(StretchSpeech(Tone, SampleRate, 0.8f).size()) == Catch::Approx(Tone.size() / 0.8).epsilon(0.01));
    CHECK(static_cast<double>(StretchSpeech(Tone, SampleRate, 1.25f).size()) == Catch::Approx(Tone.size() / 1.25).epsilon(0.01));
}

TEST_CASE("Stretching keeps the pitch", "[stretch]")
{
    constexpr int SampleRate = 16000;
    const std::vector<float> Tone = MakeTone(SampleRate, 220.0, 1.0);

    for (const float Tempo : {0.65f, 0.8f, 1.25f})
    {
        CHECK(MeasureFrequency(StretchSpeech(Tone, SampleRate, Tempo), SampleRate) == Catch::Approx(220.0).epsilon(0.02));
    }
}

TEST_CASE("A tempo of one leaves the samples untouched", "[stretch]")
{
    constexpr int SampleRate = 16000;
    const std::vector<float> Tone = MakeTone(SampleRate, 220.0, 0.5);

    CHECK(StretchSpeech(Tone, SampleRate, 1.0f) == Tone);
    CHECK(StretchSpeech({}, SampleRate, 0.8f).empty());
}

TEST_CASE("Synthesised speech starts without a long silence", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer =
        FTSpeechSynthesizer::Create(SupertonicDirectory, "fr");

    if (!Synthesizer.has_value())
    {
        WARN("the Supertonic model is absent, so it is untested here");

        return;
    }

    const std::vector<int16_t> Samples = Synthesizer->Synthesize("chat");

    REQUIRE(!Samples.empty());

    int Peak = 0;

    for (const int16_t Sample : Samples)
    {
        Peak = std::max(Peak, std::abs(static_cast<int>(Sample)));
    }

    const auto Onset = std::find_if(Samples.begin(), Samples.end(),
                                    [Peak](int16_t Sample) { return std::abs(static_cast<int>(Sample)) * 20 >= Peak; });
    const double LeadingSeconds =
        static_cast<double>(std::distance(Samples.begin(), Onset)) / Synthesizer->GetSampleRate();

    CHECK(LeadingSeconds < 0.2);
}

TEST_CASE("Synthesised speech lands at the target loudness", "[speech]")
{
    const std::optional<FTSpeechSynthesizer> Synthesizer = FTSpeechSynthesizer::Create(PiperDirectory);

    if (!Synthesizer.has_value())
    {
        WARN("the Piper model is absent, so it is untested here");

        return;
    }

    for (const std::string& Text : {std::string("chien"), std::string("Il a mangé trois croissants avec du fromage.")})
    {
        const std::vector<int16_t> Samples = Synthesizer->Synthesize(Text, 0, 1.0f, "fr");

        REQUIRE(!Samples.empty());

        double SumOfSquares = 0.0;

        for (const int16_t Sample : Samples)
        {
            const double Normalized = static_cast<double>(Sample) / 32767.0;
            SumOfSquares += Normalized * Normalized;
        }

        const double Rms = std::sqrt(SumOfSquares / static_cast<double>(Samples.size()));

        CHECK(Rms == Catch::Approx(TargetSpeechRms).epsilon(0.1));
    }
}
