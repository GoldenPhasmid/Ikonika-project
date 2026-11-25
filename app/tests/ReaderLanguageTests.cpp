#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "DictionaryController.h"
#include "VocabularyController.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppLocalization.h"
#include "AppReaderSettings.h"
#include "ReaderBookLanguage.h"
#include "ReaderVocabulary.h"
#include "TestAppDatabase.h"

#include "core/FTBookShelfRepository.h"
#include "core/FTCard.h"
#include "core/FTLanguageDetection.h"
#include "core/FTReaderRepository.h"
#include "core/FTSettings.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QStringList>
#include <QThread>
#include <QVariantMap>

#include <memory>

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    struct FLearningFrenchGuard
    {
        FTLanguageRules SavedRules{FTSettings::Get().GetLanguageRules()};
        FAppDatabaseGuard Database;

        FLearningFrenchGuard()
        {
            AppDictionary::Get().Close();
            AppDictionary::Get().Discover();
            FTSettings::Get().SetTargetLanguage("fr");
            FTSettings::Get().SetNativeLanguage("en");
            REQUIRE(AppDictionary::Get().Initialise());
            AppDatabase::Get().GetCardRepository().SetActiveLanguages("fr", "en");
        }

        ~FLearningFrenchGuard()
        {
            AppDictionary::Get().Close();
            FTSettings::Get().SetTargetLanguage(SavedRules.TargetLanguage);
            FTSettings::Get().SetNativeLanguage(SavedRules.NativeLanguage);
        }
    };

    struct FUkrainianSpeakerGuard
    {
        std::vector<FTLanguageRules> SavedPairs{FTSettings::GetAvailableLanguagePairs()};
        std::string SavedNative{FTSettings::Get().GetLanguageRules().NativeLanguage};

        FUkrainianSpeakerGuard()
        {
            std::vector<FTLanguageRules> Pairs{SavedPairs};
            FTLanguageRules Ukrainian{FTSettings::Get().GetLanguageRules()};
            Ukrainian.NativeLanguage = "uk";
            Pairs.push_back(Ukrainian);
            FTSettings::SetAvailableLanguagePairs(Pairs);
            FTSettings::Get().SetNativeLanguage("uk");
        }

        ~FUkrainianSpeakerGuard()
        {
            FTSettings::SetAvailableLanguagePairs(SavedPairs);
            FTSettings::Get().SetNativeLanguage(SavedNative);
        }
    };

    FTCardRepository& SpanishRepository()
    {
        return AppCards::GetCardRepository(AppDictionary::Get("es"));
    }

    FTCard MakeCard(const std::string& Word, const std::string& Translation, const std::string& PartOfSpeech)
    {
        FTCard Card;
        Card.TargetWord = Word;
        Card.Translation = Translation;
        Card.PartOfSpeech = PartOfSpeech;

        return Card;
    }

    QVariantList SearchFor(DictionaryController& Controller, const QString& Text)
    {
        Controller.search(Text);

        QElapsedTimer Timer;
        Timer.start();

        while (Controller.GetResults().isEmpty() && Timer.elapsed() < 5000)
        {
            QCoreApplication::processEvents();
            QThread::msleep(5);
        }

        return Controller.GetResults();
    }

    bool HasWord(const std::vector<FTCardHandle>& Cards, const std::string& Word)
    {
        return std::any_of(Cards.begin(), Cards.end(), [&Word](const FTCardHandle& Card)
        {
            return Card != nullptr && Card->TargetWord == Word;
        });
    }
}

TEST_CASE("The learned language's card repository serves its own language", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    CHECK(&AppDatabase::Get().GetCardRepository("fr", "en") == &AppDatabase::Get().GetCardRepository());
}

TEST_CASE("Another language's cards stay apart from the learned language's", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    FTCardRepository& Spanish{SpanishRepository()};

    CHECK(&Spanish != &AppDatabase::Get().GetCardRepository());
    CHECK(Spanish.GetTargetLanguage() == "es");
    CHECK(Spanish.GetNativeLanguage() == "en");

    REQUIRE(Spanish.AddCard(MakeCard("perro", "dog", "noun")).has_value());
    REQUIRE(AppDatabase::Get().GetCardRepository().AddCard(MakeCard("chien", "dog", "noun")).has_value());

    const std::vector<FTCardHandle> SpanishCards{Spanish.LoadCards()};
    const std::vector<FTCardHandle> FrenchCards{AppDatabase::Get().GetCardRepository().LoadCards()};

    CHECK(HasWord(SpanishCards, "perro"));
    CHECK_FALSE(HasWord(SpanishCards, "chien"));
    CHECK(HasWord(FrenchCards, "chien"));
    CHECK_FALSE(HasWord(FrenchCards, "perro"));
}

TEST_CASE("A lookup set to another language searches that language's dictionary", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    DictionaryController Controller;
    Controller.SetLanguage(QStringLiteral("es"));

    CHECK(Controller.GetTargetLanguageCode() == QStringLiteral("ES"));
    CHECK(Controller.GetNativeLanguageCode() == QStringLiteral("EN"));

    const QVariantList Results{SearchFor(Controller, QStringLiteral("perro"))};

    REQUIRE_FALSE(Results.isEmpty());
    CHECK(Results.front().toMap().value(QStringLiteral("word")).toString() == QStringLiteral("perro"));
    CHECK(AppDictionary::GetActive().GetTargetLanguage() == "fr");
}

TEST_CASE("A lookup in another language shows that language's cards", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    REQUIRE(SpanishRepository().AddCard(MakeCard("perro", "dog", "noun")).has_value());

    DictionaryController Controller;
    Controller.SetLanguage(QStringLiteral("es"));
    Controller.openEntry(QStringLiteral("perro"), QStringLiteral("noun"));

    CHECK(Controller.GetEntry().value(QStringLiteral("headword")).toString() == QStringLiteral("perro"));

    const QVariantList Cards{Controller.GetCards()};

    REQUIRE(Cards.size() == 1);
    CHECK(Cards.front().toMap().value(QStringLiteral("word")).toString() == QStringLiteral("perro"));

    const QVariantList Results{SearchFor(Controller, QStringLiteral("perro"))};

    REQUIRE_FALSE(Results.isEmpty());
    CHECK(Results.front().toMap().value(QStringLiteral("stage")).toInt() >= 0);
}

TEST_CASE("A lookup with no language set searches the learned language", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    DictionaryController Controller;

    CHECK(Controller.GetTargetLanguageCode() == QStringLiteral("FR"));

    const QVariantList Results{SearchFor(Controller, QStringLiteral("chien"))};

    REQUIRE_FALSE(Results.isEmpty());
    CHECK(Results.front().toMap().value(QStringLiteral("word")).toString() == QStringLiteral("chien"));
}

TEST_CASE("Clearing a lookup's language returns it to the learned language", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    DictionaryController Controller;
    Controller.SetLanguage(QStringLiteral("es"));
    Controller.SetLanguage(QString{});

    CHECK(Controller.GetTargetLanguageCode() == QStringLiteral("FR"));
}

TEST_CASE("A lookup in a language with no dictionary finds nothing", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    DictionaryController Controller;
    Controller.SetLanguage(QStringLiteral("ru"));
    Controller.search(QStringLiteral("dom"));

    CHECK(Controller.GetResults().isEmpty());
    CHECK(Controller.GetNativeLanguageCode().isEmpty());
}

TEST_CASE("Highlighting in another language marks that language's cards and their forms", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    const bool bWasHighlighting{AppReaderSettings::Get().IsHighlightingVocabulary()};
    AppReaderSettings::Get().SetHighlightVocabulary(true);

    REQUIRE(SpanishRepository().AddCard(MakeCard("perro", "dog", "noun")).has_value());
    REQUIRE(AppDatabase::Get().GetCardRepository().AddCard(MakeCard("chien", "dog", "noun")).has_value());

    ReaderVocabulary Vocabulary;
    Vocabulary.SetLanguage("es");
    Vocabulary.refresh();

    CHECK(Vocabulary.Find(QStringLiteral("los perros")).size() == 1);
    CHECK(Vocabulary.Find(QStringLiteral("le chien")).empty());

    Vocabulary.SetLanguage({});

    CHECK(Vocabulary.Find(QStringLiteral("le chien")).size() == 1);
    CHECK(Vocabulary.Find(QStringLiteral("los perros")).empty());

    AppReaderSettings::Get().SetHighlightVocabulary(bWasHighlighting);
}

TEST_CASE("A word added in another language joins that language's vocabulary", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    VocabularyController Controller;

    REQUIRE(Controller.addCard(QStringLiteral("perro"), QStringLiteral("dog"), QString{}, -1, false, QString{},
                               QString{}, QStringLiteral("es")));

    const std::vector<FTCardHandle> SpanishCards{SpanishRepository().LoadCards()};
    const std::vector<FTCardHandle> FrenchCards{AppDatabase::Get().GetCardRepository().LoadCards()};

    REQUIRE(HasWord(SpanishCards, "perro"));
    CHECK_FALSE(HasWord(FrenchCards, "perro"));

    const auto Perro = std::find_if(SpanishCards.begin(), SpanishCards.end(), [](const FTCardHandle& Card)
    {
        return Card->TargetWord == "perro";
    });

    CHECK((*Perro)->PartOfSpeech == "noun");
    CHECK((*Perro)->Gender == std::optional<int>{GenderMasculine});
}

TEST_CASE("A word added with no language joins the learned language's vocabulary", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    VocabularyController Controller;

    REQUIRE(Controller.addCard(QStringLiteral("chien"), QStringLiteral("dog")));

    CHECK(HasWord(AppDatabase::Get().GetCardRepository().LoadCards(), "chien"));
    CHECK_FALSE(HasWord(SpanishRepository().LoadCards(), "chien"));
}

TEST_CASE("A word added from a dictionary in another translation language keeps that language", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;
    FUkrainianSpeakerGuard Speaker;
    REQUIRE(FTSettings::Get().GetLanguageRules().NativeLanguage == "uk");

    VocabularyController Controller;

    REQUIRE(Controller.addCard(QStringLiteral("perro"), QStringLiteral("dog"), QString{}, -1, false, QString{},
                               QString{}, QStringLiteral("es")));

    CHECK(AppDictionary::Get("es").GetTranslationLanguage() == "en");
    CHECK(HasWord(AppDatabase::Get().GetCardRepository("es", "en").LoadCards(), "perro"));
    CHECK_FALSE(HasWord(AppDatabase::Get().GetCardRepository("es", "uk").LoadCards(), "perro"));
}

TEST_CASE("A binding to the book language sees the stored language", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    const std::optional<std::string> Portuguese{
        AppDatabase::Get().GetReaderLibrary().AddDocument("C:/books/lusiadas.epub", "Os Lusiadas", "epub")};
    const std::optional<std::string> Unknown{
        AppDatabase::Get().GetReaderLibrary().AddDocument("C:/books/numbers.epub", "Numbers", "epub")};

    REQUIRE(Portuguese.has_value());
    REQUIRE(Unknown.has_value());
    REQUIRE(AppDatabase::Get().GetReaderLibrary().SetDetails(*Portuguese, FTReaderDocumentDetails{"pt"}));
    REQUIRE(AppDatabase::Get().GetReaderLibrary().SetDetails(*Unknown, FTReaderDocumentDetails{UndeterminedLanguage}));

    ReaderBookLanguage Book;
    QQmlEngine Engine;
    Engine.rootContext()->setContextProperty(QStringLiteral("book"), &Book);

    QQmlComponent Component{&Engine};
    Component.setData("import QtQml\nQtObject { property string seen: book.code }", QUrl{});

    const std::unique_ptr<QObject> Watcher{Component.create()};

    REQUIRE(Watcher != nullptr);
    CHECK(Watcher->property("seen").toString().isEmpty());

    Book.Update(QString::fromStdString(*Portuguese));

    CHECK(Watcher->property("seen").toString() == QStringLiteral("pt"));

    Book.Update(QString::fromStdString(*Unknown));

    CHECK(Watcher->property("seen").toString().isEmpty());
}

TEST_CASE("A book's word list shows each card with its own language's article", "[readerlanguage]")
{
    FLearningFrenchGuard Guard;

    FTCard Spanish{MakeCard("perro", "dog", "noun")};
    Spanish.Gender = GenderMasculine;
    FTCard French{MakeCard("chien", "dog", "noun")};
    French.Gender = GenderMasculine;

    const std::optional<std::string> SpanishId{SpanishRepository().AddCard(Spanish)};
    const std::optional<std::string> FrenchId{AppDatabase::Get().GetCardRepository().AddCard(French)};

    REQUIRE(SpanishId.has_value());
    REQUIRE(FrenchId.has_value());
    REQUIRE(AppDatabase::Get().GetBookShelf().AddBookCard("mixed-book", *SpanishId));
    REQUIRE(AppDatabase::Get().GetBookShelf().AddBookCard("mixed-book", *FrenchId));

    QStringList Words;

    for (const QVariant& Entry : AppCards::ListBookWords("mixed-book"))
    {
        Words.append(Entry.toMap().value(QStringLiteral("word")).toString());
    }

    CHECK(Words.contains(QStringLiteral("el perro")));
    CHECK(Words.contains(QStringLiteral("le chien")));
}
