#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "DecksController.h"
#include "DiscoveryController.h"
#include "TriageController.h"
#include "VocabularyController.h"

#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppPaths.h"
#include "TestAppDatabase.h"
#include "TestAppSpeech.h"

#include "core/FTCard.h"
#include "core/FTDatabase.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTSettings.h"

#include <QDir>
#include <QFile>
#include <QVariantMap>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

namespace
{
    struct FAppDictionaryGuard
    {
        ~FAppDictionaryGuard()
        {
            AppDictionary::Get().Close();
        }
    };

    struct FDeckFixture
    {
        FDeckFixture()
            : SavedRules(FTSettings::Get().GetLanguageRules())
        {
            FTSettings::Get().SetLanguagePair("en", "uk");

            AppDatabase::Get().GetCardRepository().SetActiveLanguages("en", "uk");

            if (!AppDictionary::Get().Initialise() || !AppDictionary::Get().GetRepository().HasDecks())
            {
                return;
            }

            const std::vector<FTDeck> Decks = AppDictionary::Get().GetRepository().ListDecks();

            if (Decks.empty())
            {
                return;
            }

            Deck = Decks.front();
            Words = AppDictionary::Get().GetRepository().LoadDeckWords(Deck.Id);
        }

        ~FDeckFixture()
        {
            FTSettings::Get().SetLanguagePair(SavedRules.TargetLanguage, SavedRules.NativeLanguage);
        }

        bool HasDeck() const
        {
            return Words.size() >= 4;
        }

        void AddCard(const FTDeckWord& Word) const
        {
            FTCard Card;
            Card.TargetWord = Word.Headword;
            Card.PartOfSpeech = Word.PartOfSpeech;
            Card.Translation = Word.Translation.empty() ? "translation" : Word.Translation;

            REQUIRE(AppDatabase::Get().GetCardRepository().AddCard(Card).has_value());
        }

        void MarkKnown(const FTDeckWord& Word) const
        {
            REQUIRE(AppDatabase::Get().GetCardRepository().MarkWordTriaged(Word.Headword, Word.PartOfSpeech,
                                                                         Word.Gender));
        }

        FTLanguageRules SavedRules;

        FAppDatabaseGuard DatabaseGuard;
        FAppSpeechGuard SpeechGuard;
        FAppDictionaryGuard DictionaryGuard;

        FTDeck Deck;
        std::vector<FTDeckWord> Words;
    };

    struct FTestDeckFixture
    {
        FTestDeckFixture()
            : SavedRules(FTSettings::Get().GetLanguageRules())
        {
            AppPaths::EnableHeadlessSandbox();
            AppDictionary::Get().Close();

            QDir().mkpath(AppPaths::InstalledDictionariesDirectory());

            const QString Built = QStringLiteral(PROJECT_ROOT_DIRECTORY "/data/built/en/en-dictionary.db");

            if (!QFile::exists(Built))
            {
                return;
            }

            QFile::remove(InstalledPath());
            REQUIRE(QFile::copy(Built, InstalledPath()));

            {
                std::optional<FTDatabase> Copy = FTDatabase::Open(InstalledPath().toStdString());
                REQUIRE(Copy.has_value());
                REQUIRE(Copy->Execute(
                    "DELETE FROM deck_words; DELETE FROM decks;"
                    "INSERT INTO decks (id, name) VALUES (1, 'Test deck');"
                    "INSERT INTO deck_words (deck_id, headword, part_of_speech, gist) VALUES "
                    "  (1, 'watch', 'noun', 'duty'), (1, 'watch', 'verb', NULL), (1, 'abeyance', 'noun', NULL);"));
            }

            AppDictionary::Get().Discover();

            FTSettings::Get().SetLanguagePair("en", "uk");
            AppDatabase::Get().GetCardRepository().SetActiveLanguages("en", "uk");

            bReady = AppDictionary::Get().Initialise() && AppDictionary::Get().GetRepository().HasDecks();
        }

        ~FTestDeckFixture()
        {
            AppDictionary::Get().Close();
            QFile::remove(InstalledPath());

            FTSettings::Get().SetLanguagePair(SavedRules.TargetLanguage, SavedRules.NativeLanguage);

            AppDictionary::Get().Discover();
        }

        static QString InstalledPath()
        {
            return QDir(AppPaths::InstalledDictionariesDirectory()).filePath(QStringLiteral("en-dictionary.db"));
        }

        FTLanguageRules SavedRules;

        FAppDatabaseGuard DatabaseGuard;
        FAppSpeechGuard SpeechGuard;

        bool bReady = false;
    };

    QVariantMap RowAt(const VocabularyController& Controller, int Index)
    {
        return Controller.GetVisibleCards().at(Index).toMap();
    }
}

TEST_CASE("A deck opens in deck order with every word new", "[decks]")
{
    FDeckFixture Fixture;

    if (!Fixture.HasDeck())
    {
        SKIP("The built dictionary has no decks");
    }

    VocabularyController Controller;
    Controller.SetDeckId(Fixture.Deck.Id);

    CHECK(Controller.GetDeckName() == QString::fromStdString(Fixture.Deck.Name));
    REQUIRE(Controller.GetFilteredCount() == static_cast<int>(Fixture.Words.size()));
    CHECK(Controller.GetDeckNewCount() == static_cast<int>(Fixture.Words.size()));
    CHECK(Controller.GetSortKey() == QStringLiteral("deck"));

    for (int Index = 0; Index < 4; ++Index)
    {
        const QVariantMap Row = RowAt(Controller, Index);

        CHECK(Row.value("targetWord").toString() == QString::fromStdString(Fixture.Words[Index].Headword));
        CHECK(Row.value("word").toString() == QString::fromStdString(GetDisplayHeadword(
            Fixture.Words[Index].Headword, Fixture.Words[Index].PartOfSpeech, "en")));
        CHECK_FALSE(Row.value("inVocabulary").toBool());
        CHECK_FALSE(Row.value("completed").toBool());
    }
}

TEST_CASE("A deck word shows its card, and a word marked known shows as learned", "[decks]")
{
    FDeckFixture Fixture;

    if (!Fixture.HasDeck())
    {
        SKIP("The built dictionary has no decks");
    }

    Fixture.AddCard(Fixture.Words[0]);
    Fixture.MarkKnown(Fixture.Words[1]);

    VocabularyController Controller;
    Controller.SetDeckId(Fixture.Deck.Id);

    CHECK(Controller.GetDeckNewCount() == static_cast<int>(Fixture.Words.size()) - 2);

    const QVariantMap Carded = RowAt(Controller, 0);
    CHECK(Carded.value("inVocabulary").toBool());
    CHECK_FALSE(Carded.value("id").toString().isEmpty());

    const QVariantMap Known = RowAt(Controller, 1);
    CHECK_FALSE(Known.value("inVocabulary").toBool());
    CHECK(Known.value("completed").toBool());

    CHECK_FALSE(RowAt(Controller, 2).value("inVocabulary").toBool());
}

TEST_CASE("A deck word whose card was deleted is new again", "[decks]")
{
    FDeckFixture Fixture;

    if (!Fixture.HasDeck())
    {
        SKIP("The built dictionary has no decks");
    }

    Fixture.AddCard(Fixture.Words[0]);
    Fixture.MarkKnown(Fixture.Words[0]);

    std::vector<std::string> CardIds;

    for (const FTCardHandle& Card : AppDatabase::Get().GetCardRepository().LoadCards())
    {
        CardIds.push_back(Card->Id);
    }

    REQUIRE(AppDatabase::Get().GetCardRepository().DeleteCards(CardIds));

    VocabularyController Controller;
    Controller.SetDeckId(Fixture.Deck.Id);

    CHECK_FALSE(RowAt(Controller, 0).value("completed").toBool());
    CHECK(Controller.GetDeckNewCount() == static_cast<int>(Fixture.Words.size()));
}

TEST_CASE("Deck triage offers the deck's new words most common first", "[decks][triage]")
{
    FDeckFixture Fixture;

    if (!Fixture.HasDeck())
    {
        SKIP("The built dictionary has no decks");
    }

    Fixture.AddCard(Fixture.Words[0]);
    Fixture.MarkKnown(Fixture.Words[1]);

    std::vector<FTDeckWord> Expected(Fixture.Words.begin() + 2, Fixture.Words.end());
    std::stable_sort(Expected.begin(), Expected.end(), [](const FTDeckWord& Left, const FTDeckWord& Right)
    {
        return Left.TeachingRank.value_or(std::numeric_limits<int>::max())
            < Right.TeachingRank.value_or(std::numeric_limits<int>::max());
    });

    TriageController Controller;
    REQUIRE(Controller.startDeck(Fixture.Deck.Id));

    CHECK(Controller.IsDeckMode());
    CHECK(Controller.GetTargetCount() == static_cast<int>(Fixture.Words.size()) - 2);
    CHECK(Controller.GetWord() == QString::fromStdString(Expected[0].Headword));
    CHECK(Controller.GetPartOfSpeech() == QString::fromStdString(Expected[0].PartOfSpeech));

    Controller.markKnown();

    CHECK(Controller.GetWord() == QString::fromStdString(Expected[1].Headword));
    CHECK(Controller.GetProgressCount() == 1);
    CHECK_FALSE(Controller.IsFinished());
}

TEST_CASE("Deck triage ends once every deck word is a card or known", "[decks][triage]")
{
    FDeckFixture Fixture;

    if (!Fixture.HasDeck())
    {
        SKIP("The built dictionary has no decks");
    }

    for (std::size_t Index = 0; Index + 2 < Fixture.Words.size(); ++Index)
    {
        Fixture.MarkKnown(Fixture.Words[Index]);
    }

    TriageController Controller;
    REQUIRE(Controller.startDeck(Fixture.Deck.Id));
    REQUIRE(Controller.GetTargetCount() == 2);

    Controller.markUnknown();

    CHECK(Controller.GetAddedCount() == 1);
    CHECK_FALSE(Controller.IsFinished());

    Controller.markKnown();

    CHECK(Controller.IsFinished());
    CHECK(Controller.GetProgressCount() == 2);

    TriageController Again;
    CHECK_FALSE(Again.startDeck(Fixture.Deck.Id));
}

TEST_CASE("The decks list counts new and learned words per deck", "[decks]")
{
    FDeckFixture Fixture;

    if (!Fixture.HasDeck())
    {
        SKIP("The built dictionary has no decks");
    }

    Fixture.AddCard(Fixture.Words[0]);
    Fixture.MarkKnown(Fixture.Words[1]);

    DecksController Controller;

    REQUIRE(Controller.IsAvailable());

    const QVariantMap First = Controller.GetDecks().front().toMap();

    CHECK(First.value("name").toString() == QString::fromStdString(Fixture.Deck.Name));
    CHECK(First.value("wordCount").toInt() == Fixture.Deck.WordCount);
    CHECK(First.value("learnedCount").toInt() == 1);
    CHECK(First.value("learningCount").toInt() == 0);
    CHECK(First.value("newCount").toInt() == Fixture.Deck.WordCount - 1);
}

TEST_CASE("Learning a deck word that names a meaning adds a card for that meaning", "[decks][triage]")
{
    FTestDeckFixture Fixture;

    if (!Fixture.bReady)
    {
        SKIP("The built English dictionary is missing");
    }

    TriageController Controller;
    REQUIRE(Controller.startDeck(1));
    REQUIRE(Controller.GetWord() == QStringLiteral("watch"));
    REQUIRE(Controller.GetPartOfSpeech() == QStringLiteral("verb"));
    CHECK(Controller.GetDisplayWord() == QStringLiteral("to watch"));

    Controller.markKnown();

    REQUIRE(Controller.GetWord() == QStringLiteral("watch"));
    REQUIRE(Controller.GetPartOfSpeech() == QStringLiteral("noun"));
    CHECK(Controller.GetDisplayWord() == QStringLiteral("watch"));

    const QVariantList Meanings = Controller.GetMeaningOptions();
    REQUIRE(Meanings.size() == 1);
    CHECK(Meanings.front().toMap().value("gist").toString() == QStringLiteral("duty"));

    Controller.markUnknown();
    REQUIRE(Controller.GetAddedCount() == 1);

    const std::vector<FTCardHandle> Cards = AppDatabase::Get().GetCardRepository().LoadCards();
    REQUIRE(Cards.size() == 1);
    CHECK(Cards.front()->MeaningGist == "duty");

    VocabularyController Deck;
    Deck.SetDeckId(1);

    CHECK(RowAt(Deck, 0).value("inVocabulary").toBool());
    CHECK(Deck.GetDeckNewCount() == 1);
}

TEST_CASE("Discover adds a card for Learn and marks the meaning known for Know", "[decks][discovery]")
{
    FDeckFixture Fixture;

    if (!Fixture.HasDeck())
    {
        SKIP("The built dictionary has no decks");
    }

    FTCardRepository& Cards = AppDatabase::Get().GetCardRepository();
    REQUIRE(Cards.SetPlacementCutoff("en", FTDiscoveryPlanner::BeginnerWordCount));

    DiscoveryController Controller;
    REQUIRE(Controller.startDeck(Fixture.Deck.Id));
    REQUIRE_FALSE(Controller.IsRoundOver());

    const std::string KnownWord = Controller.GetWord().toStdString();

    Controller.choose(Controller.GetCorrectOptionIndex());
    REQUIRE(Controller.GetOutcome() == QStringLiteral("known"));
    CHECK_FALSE(Controller.IsLearn());

    Controller.commit(false);

    const std::vector<FTKnownWord> Known = Cards.ListKnownWords();
    CHECK(std::any_of(Known.begin(), Known.end(), [&KnownWord](const FTKnownWord& Word) { return Word.TargetWord == KnownWord; }));
    CHECK(Cards.FindCardsByHeadword(KnownWord, 10).empty());

    REQUIRE_FALSE(Controller.IsRoundOver());

    const std::string NewWord = Controller.GetWord().toStdString();

    while (!Controller.IsAnswered())
    {
        Controller.askClue();
    }

    CHECK(Controller.GetOutcome() == QStringLiteral("new"));
    CHECK(Controller.IsLearn());

    Controller.commit(true);

    CHECK_FALSE(Cards.FindCardsByHeadword(NewWord, 10).empty());

    while (!Controller.IsRoundOver())
    {
        Controller.choose(Controller.GetCorrectOptionIndex());
        Controller.commit(Controller.IsLearn());
    }

    CHECK(Controller.GetAskedCount() <= Controller.GetQuestionCount());
    CHECK(Controller.GetResults().size() >= 2);
}
