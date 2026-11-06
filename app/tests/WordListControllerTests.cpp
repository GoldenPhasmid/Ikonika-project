#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "WordListController.h"

#include "AppDatabase.h"
#include "AppDictionary.h"
#include "TestAppSpeech.h"
#include "TestAppDatabase.h"

#include "core/FTCard.h"
#include "core/FTSchedule.h"
#include "core/FTSettings.h"

#include <string>


TEST_CASE("Loading history lists recent searches, newest first", "[wordlist]")
{
    FAppDatabaseGuard DatabaseGuard;

    AppDatabase::Get().GetCardRepository().RecordSearch("marche", "verb", 0);
    AppDatabase::Get().GetCardRepository().RecordSearch("chien", "noun", 0);

    WordListController Controller;
    Controller.load("history");

    const QVariantList Entries = Controller.GetEntries();
    REQUIRE(Entries.size() == 2);
    CHECK(Entries[0].toMap().value("word").toString() == QStringLiteral("chien"));
    CHECK(Entries[0].toMap().value("stage").toInt() == -1);
    CHECK_FALSE(Entries[0].toMap().value("completed").toBool());
}

TEST_CASE("Counting a list gives the number of rows loading it would list", "[wordlist]")
{
    FAppDatabaseGuard DatabaseGuard;

    AppDatabase::Get().GetCardRepository().RecordSearch("marche", "verb", 0);
    AppDatabase::Get().GetCardRepository().RecordSearch("chien", "noun", 0);


    FTCard Card;
    Card.TargetWord = "chat";
    Card.Translation = "cat";
    REQUIRE(AddTestCards({Card}));

    FTCard Promoted = *AppDatabase::Get().GetCardRepository().LoadCards().front();
    Promote(Promoted, 1000);
    REQUIRE(AppDatabase::Get().GetCardRepository().UpdateCardSchedule(Promoted));

    WordListController Controller;

    for (const QString Source : {QStringLiteral("history"), QStringLiteral("studied")})
    {
        Controller.load(Source);

        CHECK(Controller.count(Source) == Controller.GetEntries().size());
    }

    CHECK(Controller.count("history") == 2);
    CHECK(Controller.count("studied") == 1);
}

TEST_CASE("Loading studied lists a completed card with real translation and stage data",
          "[wordlist]")
{
    FAppDatabaseGuard DatabaseGuard;


    FTCard Card;
    Card.TargetWord = "chien";
    Card.Translation = "dog";
    REQUIRE(AddTestCards({Card}));

    FTCard Completed = *AppDatabase::Get().GetCardRepository().LoadCards().front();
    MarkCompleted(Completed, 1000);
    REQUIRE(AppDatabase::Get().GetCardRepository().UpdateCardSchedule(Completed));

    WordListController Controller;
    Controller.load("studied");

    const QVariantList Entries = Controller.GetEntries();
    REQUIRE(Entries.size() == 1);

    const QVariantMap Entry = Entries.front().toMap();
    CHECK(Entry.value("translation").toString() == QStringLiteral("dog"));
    CHECK(Entry.value("completed").toBool());
    CHECK(Entry.value("stage").toInt() == GetLastStage());
}

TEST_CASE("Loading studied lists a word promoted without being completed", "[wordlist]")
{
    FAppDatabaseGuard DatabaseGuard;


    FTCard Card;
    Card.TargetWord = "chat";
    Card.Translation = "cat";
    REQUIRE(AddTestCards({Card}));

    FTCard Promoted = *AppDatabase::Get().GetCardRepository().LoadCards().front();
    Promote(Promoted, 1000);
    REQUIRE(AppDatabase::Get().GetCardRepository().UpdateCardSchedule(Promoted));

    WordListController Controller;
    Controller.load("studied");

    const QVariantList Entries = Controller.GetEntries();
    REQUIRE(Entries.size() == 1);

    const QVariantMap Entry = Entries.front().toMap();
    CHECK(Entry.value("translation").toString() == QStringLiteral("cat"));
    CHECK_FALSE(Entry.value("completed").toBool());
    CHECK(Entry.value("stage").toInt() == FirstLadderStage);
}

TEST_CASE("A studied noun starting with h takes the article the dictionary gives it", "[wordlist]")
{
    struct FDictionaryGuard
    {
        ~FDictionaryGuard()
        {
            AppDictionary::Get().Close();
        }
    };

    FAppDatabaseGuard DatabaseGuard;
    FDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();
    CardRepository.SetActiveLanguages("fr", "en");


    std::vector<FTCard> Cards;

    for (const char* Headword : {"homme", "hasard", "chien", "ami"})
    {
        FTCard Card;
        Card.TargetWord = Headword;
        Card.Translation = Headword;
        Card.PartOfSpeech = "noun";
        Card.Gender = GenderMasculine;
        Cards.push_back(std::move(Card));
    }

    REQUIRE(AddTestCards(Cards));
    std::vector<std::string> CardIds;

    for (const FTCardHandle& Card : CardRepository.LoadCards())
    {
        CardIds.push_back(Card->Id);
    }

    REQUIRE(CardRepository.PromoteCards(CardIds));

    WordListController Controller;
    Controller.load("studied");

    QStringList Words;

    for (const QVariant& Entry : Controller.GetEntries())
    {
        Words.push_back(Entry.toMap().value("word").toString());
    }

    Words.sort();

    CHECK(Words == QStringList{"l'ami", "l'homme", "le chien", "le hasard"});

    Controller.load("due:0");

    Words.clear();

    for (const QVariant& Entry : Controller.GetEntries())
    {
        Words.push_back(Entry.toMap().value("word").toString());
    }

    CHECK_FALSE(Words.contains("le homme"));
}

TEST_CASE("Speaking a word reaches AppSpeech", "[wordlist]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    WordListController Controller;
    Controller.speakWord("chien");

    CHECK(SpeechGuard.Mock.SpeakCount == 1);
    CHECK(SpeechGuard.Mock.LastText == QStringLiteral("chien"));
}
