#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "TriageController.h"

#include "AppDatabase.h"
#include "AppDictionary.h"
#include "TestAppDatabase.h"
#include "TestAppSpeech.h"

#include "core/FTSettings.h"

#include <QVariantMap>

#include <set>

namespace
{
    struct FAppDictionaryGuard
    {
        ~FAppDictionaryGuard()
        {
            AppDictionary::Get().Close();
        }
    };

    bool IsSelected(const QVariantList& Options, const QString& Text)
    {
        for (const QVariant& Entry : Options)
        {
            const QVariantMap Map = Entry.toMap();

            if (Map.value("text").toString().compare(Text, Qt::CaseInsensitive) == 0)
            {
                return Map.value("selected").toBool();
            }
        }

        return false;
    }

    int SelectedCount(const QVariantList& Options)
    {
        int Count = 0;

        for (const QVariant& Entry : Options)
        {
            if (Entry.toMap().value("selected").toBool())
            {
                ++Count;
            }
        }

        return Count;
    }
}

TEST_CASE("Retyping the current translation keeps its chip selected", "[triage]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    TriageController Controller;
    REQUIRE(Controller.start(5));
    REQUIRE_FALSE(Controller.GetTranslationOptions().isEmpty());

    const QString OriginalTranslation = Controller.GetTranslation();
    REQUIRE(SelectedCount(Controller.GetTranslationOptions()) > 0);

    Controller.SetTranslation(OriginalTranslation);

    CHECK(SelectedCount(Controller.GetTranslationOptions()) > 0);

    for (const QVariant& Entry : Controller.GetTranslationOptions())
    {
        const QVariantMap Map = Entry.toMap();

        if (OriginalTranslation.contains(Map.value("text").toString(), Qt::CaseInsensitive))
        {
            CHECK(Map.value("selected").toBool());
        }
    }
}

TEST_CASE("Clearing the translation box deselects every chip", "[triage]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    TriageController Controller;
    REQUIRE(Controller.start(5));
    REQUIRE_FALSE(Controller.GetTranslationOptions().isEmpty());
    REQUIRE(SelectedCount(Controller.GetTranslationOptions()) > 0);

    Controller.SetTranslation("");

    CHECK(SelectedCount(Controller.GetTranslationOptions()) == 0);
}

TEST_CASE("Typing a chip's own text selects exactly that chip", "[triage]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    TriageController Controller;
    REQUIRE(Controller.start(5));

    const QVariantList Options = Controller.GetTranslationOptions();
    REQUIRE_FALSE(Options.isEmpty());

    const QString FirstOptionText = Options.first().toMap().value("text").toString();

    Controller.SetTranslation(FirstOptionText);

    const QVariantList Updated = Controller.GetTranslationOptions();
    CHECK(IsSelected(Updated, FirstOptionText));
    CHECK(SelectedCount(Updated) == 1);
}

TEST_CASE("Triage offers nothing at or below the placement cutoff", "[triage][placement][slow]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    constexpr int CommonWordCount = 300;
    constexpr int OfferedWordCount = 12;

    std::set<QString> CommonWords;

    for (const FTLearnableWord& Word : AppDictionary::Get().GetRepository().LoadLearnableWords(CommonWordCount, 0))
    {
        CommonWords.insert(QString::fromStdString(Word.Headword + " " + Word.PartOfSpeech));
    }

    REQUIRE_FALSE(CommonWords.empty());

    FTLearnableWordFilter AboveCommon;
    AboveCommon.AboveTeachingRank = 3000;

    for (const FTLearnableWord& Word : AppDictionary::Get().GetRepository().LoadLearnableWords(CommonWordCount, 0, AboveCommon))
    {
        CHECK_FALSE(CommonWords.contains(QString::fromStdString(Word.Headword + " " + Word.PartOfSpeech)));
    }

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    CardRepository.SetActiveLanguages("fr", "en");
    REQUIRE(CardRepository.SetPlacementCutoff("fr", 3000));

    TriageController Controller;
    REQUIRE(Controller.start(OfferedWordCount));

    for (int Offer = 0; Offer < OfferedWordCount; ++Offer)
    {
        REQUIRE_FALSE(Controller.GetWord().isEmpty());
        CHECK_FALSE(CommonWords.contains(Controller.GetWord() + QLatin1Char(' ') + Controller.GetPartOfSpeech()));

        Controller.markSkipped();
    }
}

TEST_CASE("Learning a meaning adds a card for it and moves to the next word", "[triage]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    AppDatabase::Get().GetCardRepository().SetActiveLanguages("fr", "en");

    REQUIRE(AppDictionary::Get().Initialise());

    if (!AppDictionary::Get().GetRepository().HasMeaningExamples())
    {
        SKIP("The built dictionary has no meanings");
    }

    TriageController Controller;
    REQUIRE(Controller.start(5));

    const QVariantList Meanings = Controller.GetMeaningOptions();
    REQUIRE_FALSE(Meanings.isEmpty());

    const QVariantMap Meaning = Meanings.front().toMap();
    const QString Word = Controller.GetWord();

    CHECK(Meaning.value("learnedStage").toInt() == -1);
    CHECK_FALSE(Meaning.value("nativeDefinition").toString().isEmpty());

    REQUIRE(Controller.learnMeaning(Meaning.value("gist").toString(),
                                    Meaning.value("translations").toStringList().front()));

    CHECK(Controller.GetAddedCount() == 1);
    CHECK(Controller.GetWord() != Word);
}

TEST_CASE("Moving the start rank refills triage and becomes the placement value", "[triage][slow]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    CardRepository.SetActiveLanguages("fr", "en");
    REQUIRE(CardRepository.SetPlacementCutoff("fr", 500));

    constexpr int StartRank = 20000;
    constexpr int OfferedWordCount = 8;
    constexpr int CommonWordCount = 300;

    FTLearnableWordFilter AbovePlacement;
    AbovePlacement.AboveTeachingRank = 500;

    std::set<QString> CommonWords;

    for (const FTLearnableWord& Word :
         AppDictionary::Get().GetRepository().LoadLearnableWords(CommonWordCount, 0, AbovePlacement))
    {
        CommonWords.insert(QString::fromStdString(Word.Headword + " " + Word.PartOfSpeech));
    }

    TriageController Controller;
    CHECK(Controller.GetStartRank() == 500);
    REQUIRE(Controller.start(OfferedWordCount));

    Controller.SetStartRank(StartRank);

    CHECK(Controller.GetStartRank() == StartRank);

    for (int Offer = 0; Offer < OfferedWordCount; ++Offer)
    {
        REQUIRE_FALSE(Controller.GetWord().isEmpty());
        CHECK_FALSE(CommonWords.contains(Controller.GetWord() + QLatin1Char(' ') + Controller.GetPartOfSpeech()));

        Controller.markSkipped();
    }

    CHECK(CardRepository.GetPlacementCutoff() == StartRank);

    TriageController Reopened;
    CHECK(Reopened.GetStartRank() == StartRank);
    CHECK(Reopened.GetMaximumRank() > StartRank);
    CHECK_FALSE(Reopened.GetLevelMarks().isEmpty());
}

TEST_CASE("Starting triage announces a start rank saved after the screen first read it", "[triage]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    CardRepository.SetActiveLanguages("fr", "en");
    REQUIRE(CardRepository.SetPlacementCutoff("fr", 0));

    TriageController Controller;
    REQUIRE(Controller.GetStartRank() == 0);

    int Announcements = 0;
    QObject::connect(&Controller, &TriageController::startRankChanged, [&Announcements]() { ++Announcements; });

    REQUIRE(CardRepository.SetPlacementCutoff("fr", 500));
    REQUIRE(Controller.start(3));

    CHECK(Controller.GetStartRank() == 500);
    CHECK(Announcements > 0);
}

TEST_CASE("The start rank stops short of the rarest words, so triage always has words to offer", "[triage]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();
    CardRepository.SetActiveLanguages("fr", "en");

    TriageController Controller;
    const int Ceiling = Controller.GetMaximumRank();
    REQUIRE(Ceiling > 0);

    REQUIRE(CardRepository.SetPlacementCutoff("fr", Ceiling * 10));
    CHECK(Controller.GetStartRank() == Ceiling);

    REQUIRE(Controller.start(5));

    Controller.SetStartRank(Ceiling * 10);

    CHECK(Controller.GetStartRank() == Ceiling);
    CHECK_FALSE(Controller.IsFinished());
    CHECK_FALSE(Controller.GetWord().isEmpty());
}


TEST_CASE("Triage still starts when nothing is left above the start rank", "[triage][slow]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();
    CardRepository.SetActiveLanguages("fr", "en");

    TriageController Controller;
    const int Ceiling = Controller.GetMaximumRank();
    REQUIRE(CardRepository.SetPlacementCutoff("fr", Ceiling));

    FTLearnableWordFilter AboveCeiling;
    AboveCeiling.AboveTeachingRank = Ceiling;

    for (const FTLearnableWord& Word :
         AppDictionary::Get().GetRepository().LoadLearnableWords(100000, 0, AboveCeiling))
    {
        CardRepository.MarkWordTriaged(Word.Headword, Word.PartOfSpeech, Word.Gender);
    }

    REQUIRE(Controller.start(3));

    CHECK_FALSE(Controller.GetWord().isEmpty());
    CHECK(Controller.GetStartRank() == 0);
    CHECK(CardRepository.GetPlacementCutoff() == Ceiling);
}
