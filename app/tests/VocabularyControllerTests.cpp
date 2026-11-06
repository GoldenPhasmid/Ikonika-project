#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "VocabularyController.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "TestAppDatabase.h"

#include "core/FTCard.h"
#include "core/FTCardStore.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTSchedule.h"
#include "core/FTSettings.h"

#include <QElapsedTimer>

#include <string>

namespace
{
    struct FAppDictionaryGuard
    {
        ~FAppDictionaryGuard()
        {
            AppDictionary::Get().Close();
        }
    };

    void SeedThreeStages()
    {
        FTCard NotLearned;
        NotLearned.TargetWord = "chien";
        NotLearned.Translation = "dog";

        FTCard Learning;
        Learning.TargetWord = "chat";
        Learning.Translation = "cat";

        FTCard Completed;
        Completed.TargetWord = "oiseau";
        Completed.Translation = "bird";

        for (const FTCard& Card : {NotLearned, Learning, Completed})
        {
            REQUIRE(AppDatabase::Get().GetCardRepository().AddCard(Card).has_value());
        }

        std::vector<FTCardHandle> Cards = AppDatabase::Get().GetCardRepository().LoadCards();
        REQUIRE(Cards.size() == 3);

        FTCard LearningCard = *Cards[1];
        LearningCard.LeitnerStage = 2;
        REQUIRE(AppDatabase::Get().GetCardRepository().UpdateCardSchedule(LearningCard));

        FTCard CompletedCard = *Cards[2];
        CompletedCard.LeitnerStage = GetLastStage();
        CompletedCard.CompletedDate = 1000;
        REQUIRE(AppDatabase::Get().GetCardRepository().UpdateCardSchedule(CompletedCard));
    }

    QStringList WordsIn(const VocabularyController& Controller)
    {
        QStringList Words;

        for (const QVariant& Value : Controller.GetVisibleCards())
        {
            Words.append(Value.toMap().value(QStringLiteral("targetWord")).toString());
        }

        return Words;
    }
}

TEST_CASE("The vocabulary lists only the cards of the language pair being learned", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    FTCardRepository& Repository = AppDatabase::Get().GetCardRepository();

    Repository.SetActiveLanguages("es", "en");
    FTCard Spanish;
    Spanish.TargetWord = "perro";
    Spanish.Translation = "dog";
    REQUIRE(Repository.AddCard(Spanish).has_value());

    Repository.SetActiveLanguages("fr", "en");
    FTCard French;
    French.TargetWord = "chien";
    French.Translation = "dog";
    REQUIRE(Repository.AddCard(French).has_value());

    VocabularyController Controller;

    CHECK(WordsIn(Controller) == QStringList{QStringLiteral("chien")});
}

TEST_CASE("With no filter keys set every stage shows", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    CHECK(WordsIn(Controller).size() == 3);
}

TEST_CASE("A single filter key shows only that stage", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    Controller.SetFilterKeys({QStringLiteral("learning")});

    const QStringList Words = WordsIn(Controller);
    REQUIRE(Words.size() == 1);
    CHECK(Words.front() == QStringLiteral("chat"));
}

TEST_CASE("Two filter keys show the union of both stages", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    Controller.SetFilterKeys({QStringLiteral("notlearned"), QStringLiteral("completed")});

    const QStringList Words = WordsIn(Controller);
    REQUIRE(Words.size() == 2);
    CHECK(Words.contains(QStringLiteral("chien")));
    CHECK(Words.contains(QStringLiteral("oiseau")));
    CHECK_FALSE(Words.contains(QStringLiteral("chat")));
}

TEST_CASE("Clearing filter keys shows every stage again", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    Controller.SetFilterKeys({QStringLiteral("learning")});
    CHECK(WordsIn(Controller).size() == 1);

    Controller.SetFilterKeys({});
    CHECK(WordsIn(Controller).size() == 3);
}

TEST_CASE("The stage counts cover the whole vocabulary whatever the filter", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    Controller.SetFilterKeys({QStringLiteral("learning")});

    CHECK(Controller.GetNotLearnedCount() == 1);
    CHECK(Controller.GetLearningCount() == 1);
    CHECK(Controller.GetLearnedCount() == 1);
}

TEST_CASE("practiceCount is not capped by the training word count", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    const int Total = FTSettings::Get().GetIntensiveLearningRules().TrainingWordCount + 10;

    for (int Index = 0; Index < Total; ++Index)
    {
        FTCard Card;
        Card.TargetWord = "word" + std::to_string(Index);
        Card.Translation = "meaning" + std::to_string(Index);
        REQUIRE(AppDatabase::Get().GetCardRepository().AddCard(Card).has_value());
    }

    VocabularyController Controller;

    CHECK(Controller.practiceCount({}) == Total);
    CHECK(Controller.practiceCount({QStringLiteral("notlearned")}) == Total);
}

TEST_CASE("practiceCount counts the words of the chosen stages", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    CHECK(Controller.practiceCount({QStringLiteral("notlearned")}) == 1);
    CHECK(Controller.practiceCount({QStringLiteral("learning"), QStringLiteral("completed")}) == 2);
    CHECK(Controller.practiceCount({}) == 3);
}

TEST_CASE("Search narrows the visible cards by word or translation", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    Controller.SetSearch(QStringLiteral("dog"));
    CHECK(WordsIn(Controller) == QStringList{QStringLiteral("chien")});

    Controller.SetSearch(QStringLiteral("oise"));
    CHECK(WordsIn(Controller) == QStringList{QStringLiteral("oiseau")});
}

TEST_CASE("Reloading the vocabulary keeps the search, resetting the view clears it", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;
    Controller.SetSearch(QStringLiteral("dog"));

    Controller.refresh();

    CHECK(Controller.GetSearch() == QStringLiteral("dog"));
    CHECK(WordsIn(Controller) == QStringList{QStringLiteral("chien")});

    Controller.resetView();

    CHECK(Controller.GetSearch().isEmpty());
    CHECK(WordsIn(Controller).size() == 3);
}

TEST_CASE("Search ignores missing accents", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    FTCard Accented;
    Accented.TargetWord = "élève";
    Accented.Translation = "pupil";

    REQUIRE(AppDatabase::Get().GetCardRepository().AddCard(Accented).has_value());

    VocabularyController Controller;

    Controller.SetSearch(QStringLiteral("eleve"));
    CHECK(WordsIn(Controller) == QStringList{QStringLiteral("élève")});
}

TEST_CASE("Sorting by word is alphabetical and reverses when descending", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    Controller.SetSortKey(QStringLiteral("word"));

    CHECK(WordsIn(Controller) == QStringList({QStringLiteral("chat"), QStringLiteral("chien"),
                                              QStringLiteral("oiseau")}));

    Controller.SetSortDescending(true);

    CHECK(WordsIn(Controller) == QStringList({QStringLiteral("oiseau"), QStringLiteral("chien"),
                                              QStringLiteral("chat")}));
}

TEST_CASE("addCard keeps the part of speech the word was picked with", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    REQUIRE(Controller.addCard(QStringLiteral("devoir"), QStringLiteral("homework"), QStringLiteral("noun")));

    bool bFound = false;

    for (const QVariant& Value : Controller.GetVisibleCards())
    {
        const QVariantMap Entry = Value.toMap();

        if (Entry.value(QStringLiteral("targetWord")).toString() == QStringLiteral("devoir"))
        {
            bFound = true;
            CHECK(Entry.value(QStringLiteral("partOfSpeech")).toString() == QStringLiteral("noun"));
        }
    }

    CHECK(bFound);
}

TEST_CASE("addCard keeps the gender chosen with the part of speech", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    REQUIRE(Controller.addCard(QStringLiteral("tour"), QStringLiteral("tower"), QStringLiteral("noun"),
                               GenderFeminine, true));

    bool bFound = false;

    for (const QVariant& Value : Controller.GetVisibleCards())
    {
        const QVariantMap Entry = Value.toMap();

        if (Entry.value(QStringLiteral("targetWord")).toString() == QStringLiteral("tour"))
        {
            bFound = true;
            CHECK(Entry.value(QStringLiteral("partOfSpeech")).toString() == QStringLiteral("noun"));
            CHECK(Entry.value(QStringLiteral("gender")).toInt() == GenderFeminine);
        }
    }

    CHECK(bFound);
}

TEST_CASE("addCard adds to the vocabulary and removeCard deletes the card", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    REQUIRE(Controller.addCard(QStringLiteral("poisson"), QStringLiteral("fish")));
    CHECK(WordsIn(Controller).contains(QStringLiteral("poisson")));

    QString AddedCardId;

    for (const QVariant& Value : Controller.GetVisibleCards())
    {
        const QVariantMap Entry = Value.toMap();

        if (Entry.value(QStringLiteral("targetWord")).toString() == QStringLiteral("poisson"))
        {
            AddedCardId = Entry.value(QStringLiteral("id")).toString();
        }
    }

    REQUIRE(!AddedCardId.isEmpty());

    CHECK(Controller.removeCard(AddedCardId));
    CHECK_FALSE(WordsIn(Controller).contains(QStringLiteral("poisson")));
    CHECK_FALSE(AppDatabase::Get().GetCardRepository().LoadCard(AddedCardId.toStdString()).has_value());
}

TEST_CASE("updateCard rewrites a card's word and translation", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    QString TargetCardId;

    for (const QVariant& Value : Controller.GetVisibleCards())
    {
        const QVariantMap Entry = Value.toMap();

        if (Entry.value(QStringLiteral("targetWord")).toString() == QStringLiteral("chien"))
        {
            TargetCardId = Entry.value(QStringLiteral("id")).toString();
        }
    }

    REQUIRE(!TargetCardId.isEmpty());

    CHECK(Controller.updateCard(TargetCardId, QStringLiteral("chiot"),
                                QStringLiteral("puppy"), -1, QString(), QString()));
    CHECK(WordsIn(Controller).contains(QStringLiteral("chiot")));
    CHECK_FALSE(WordsIn(Controller).contains(QStringLiteral("chien")));
}

TEST_CASE("completeCardProgress sends a pool card straight to the last stage", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    QString TargetCardId;

    for (const QVariant& Value : Controller.GetVisibleCards())
    {
        const QVariantMap Entry = Value.toMap();

        if (Entry.value(QStringLiteral("targetWord")).toString() == QStringLiteral("chien"))
        {
            TargetCardId = Entry.value(QStringLiteral("id")).toString();
        }
    }

    REQUIRE(!TargetCardId.isEmpty());

    CHECK(Controller.completeCardProgress(TargetCardId));

    for (const QVariant& Value : Controller.GetVisibleCards())
    {
        const QVariantMap Entry = Value.toMap();

        if (Entry.value(QStringLiteral("id")).toString() == TargetCardId)
        {
            CHECK(Entry.value(QStringLiteral("completed")).toBool());
            CHECK(Entry.value(QStringLiteral("box")).toInt() == GetLastStage());
        }
    }
}

TEST_CASE("resetCardProgress returns a learning card to the pool", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    QString TargetCardId;

    for (const QVariant& Value : Controller.GetVisibleCards())
    {
        const QVariantMap Entry = Value.toMap();

        if (Entry.value(QStringLiteral("targetWord")).toString() == QStringLiteral("chat"))
        {
            TargetCardId = Entry.value(QStringLiteral("id")).toString();
        }
    }

    REQUIRE(!TargetCardId.isEmpty());

    CHECK(Controller.resetCardProgress(TargetCardId));

    for (const QVariant& Value : Controller.GetVisibleCards())
    {
        const QVariantMap Entry = Value.toMap();

        if (Entry.value(QStringLiteral("id")).toString() == TargetCardId)
        {
            CHECK_FALSE(Entry.value(QStringLiteral("completed")).toBool());
            CHECK(Entry.value(QStringLiteral("box")).toInt() == PoolStage);
        }
    }
}

TEST_CASE("completeCardProgress and resetCardProgress fail for an unknown card", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    CHECK_FALSE(Controller.completeCardProgress(QStringLiteral("not-a-real-id")));
    CHECK_FALSE(Controller.resetCardProgress(QStringLiteral("not-a-real-id")));
}

TEST_CASE("findWordTranslationStage answers -1 for a pair never added", "[vocabulary]")
{
    FAppDatabaseGuard DatabaseGuard;

    SeedThreeStages();
    VocabularyController Controller;

    CHECK(Controller.findWordTranslationStage(QStringLiteral("chat"),
                                              QStringLiteral("cat")) == 2);
    CHECK(Controller.findWordTranslationStage(QStringLiteral("renard"),
                                              QStringLiteral("fox")) == -1);
}

TEST_CASE("Opening a 500-word vocabulary completes quickly", "[vocabulary][.][performance]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    FTLearnableWordFilter Filter;
    Filter.PartOfSpeech = "noun";

    const std::vector<FTLearnableWord> Words =
        AppDictionary::Get().GetRepository().LoadLearnableWords(500, 0, Filter);

    REQUIRE(Words.size() == 500);

    std::vector<FTCard> SeedCards;
    SeedCards.reserve(Words.size());

    for (const FTLearnableWord& Word : Words)
    {
        FTCard Card;
        Card.TargetWord = Word.Headword;
        Card.PartOfSpeech = Word.PartOfSpeech;
        Card.Gender = Word.Gender;
        Card.Translation = Word.Translation;
        SeedCards.push_back(Card);
    }

    for (const FTCard& Card : SeedCards)
    {
        REQUIRE(AppDatabase::Get().GetCardRepository().AddCard(Card).has_value());
    }

    QElapsedTimer Timer;

    Timer.start();
    std::vector<FTCardHandle> RawLoaded = AppDatabase::Get().GetCardRepository().LoadCards();
    const qint64 RawLoadMs = Timer.elapsed();
    REQUIRE(RawLoaded.size() == SeedCards.size());

    Timer.restart();
    AppCards::Prepare(RawLoaded);
    const qint64 PrepareMs = Timer.elapsed();

    VocabularyController Controller;

    Timer.restart();
    Controller.refresh();
    const qint64 ControllerOpenMs = Timer.elapsed();

    WARN("raw DB load: " << RawLoadMs << " ms, AppCards::Prepare: " << PrepareMs
        << " ms, VocabularyController::refresh (caches now warm): " << ControllerOpenMs << " ms");

    CHECK(Controller.GetVisibleCards().size() == static_cast<int>(SeedCards.size()));
}

TEST_CASE("Vocabulary saves and edits the source sentence hint", "[vocabulary][hint]")
{
    FAppDatabaseGuard DatabaseGuard;
    VocabularyController Controller;
    REQUIRE(Controller.addCard("chat", "cat", "noun", 0, true, "", "Le chat dort."));
    auto Cards = AppDatabase::Get().GetCardRepository().LoadCards();
    REQUIRE(Cards.size() == 1);
    CHECK(Cards[0]->Hint == "Le chat dort.");
    REQUIRE(Controller.updateCard(QString::fromStdString(Cards[0]->Id), "chat", "cat", 0, "noun", "", "Le chat joue."));
    CHECK(Cards[0]->Hint == "Le chat joue.");
}
