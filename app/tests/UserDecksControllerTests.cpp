#include <catch2/catch_test_macros.hpp>

#include "DailySessionController.h"
#include "UserDecksController.h"
#include "VocabularyController.h"

#include "AppDatabase.h"
#include "TestAppDatabase.h"

#include "core/FTCard.h"

#include <QFile>
#include <QTemporaryDir>
#include <QUrl>

namespace
{
    QUrl WriteWordList(const QTemporaryDir& Folder, const QByteArray& Content)
    {
        const QString Path = Folder.filePath(QStringLiteral("Cafe.txt"));

        QFile File(Path);
        REQUIRE(File.open(QIODevice::WriteOnly));
        File.write(Content);
        File.close();

        return QUrl::fromLocalFile(Path);
    }

    void UseFrenchPair()
    {
        AppDatabase::Get().GetCardRepository().SetActiveLanguages("fr", "en");
        AppDatabase::Get().GetUserDecks().SetActiveLanguages("fr", "en");
    }
}

TEST_CASE("Importing a word list creates a deck named after the file and holding its cards", "[userdeck][import]")
{
    FAppDatabaseGuard Guard;
    UseFrenchPair();

    QTemporaryDir Folder;
    REQUIRE(Folder.isValid());

    UserDecksController Controller;

    const QVariantMap Result = Controller.importFromFile(WriteWordList(Folder, "chien\tdog\nchat\tcat\n"), QString());

    REQUIRE(Result.value(QStringLiteral("ok")).toBool());
    CHECK(Result.value(QStringLiteral("name")).toString() == QStringLiteral("Cafe"));
    CHECK(Result.value(QStringLiteral("added")).toInt() == 2);
    CHECK(Result.value(QStringLiteral("reused")).toInt() == 0);

    REQUIRE(Controller.GetDecks().size() == 1);
    CHECK(Controller.GetDecks()[0].toMap().value(QStringLiteral("wordCount")).toInt() == 2);
    CHECK(AppDatabase::Get().GetCardRepository().LoadCards().size() == 2);
}

TEST_CASE("Importing a word already in My Vocabulary reuses its card", "[userdeck][import]")
{
    FAppDatabaseGuard Guard;
    UseFrenchPair();

    FTCard Chien;
    Chien.TargetWord = "chien";
    Chien.Translation = "dog";
    REQUIRE(AppDatabase::Get().GetCardRepository().AddCard(Chien).has_value());

    QTemporaryDir Folder;
    REQUIRE(Folder.isValid());

    UserDecksController Controller;

    const QVariantMap Result = Controller.importFromFile(WriteWordList(Folder, "chien\tdog\nchat\tcat\n"), QString());

    REQUIRE(Result.value(QStringLiteral("ok")).toBool());
    CHECK(Result.value(QStringLiteral("added")).toInt() == 1);
    CHECK(Result.value(QStringLiteral("reused")).toInt() == 1);
    CHECK(AppDatabase::Get().GetCardRepository().LoadCards().size() == 2);
    CHECK(Controller.GetDecks()[0].toMap().value(QStringLiteral("wordCount")).toInt() == 2);
}

TEST_CASE("Importing the same file twice makes a second deck without doubling the cards", "[userdeck][import]")
{
    FAppDatabaseGuard Guard;
    UseFrenchPair();

    QTemporaryDir Folder;
    REQUIRE(Folder.isValid());

    UserDecksController Controller;
    const QUrl File = WriteWordList(Folder, "chien\tdog\nchat\tcat\n");

    REQUIRE(Controller.importFromFile(File, QString()).value(QStringLiteral("ok")).toBool());
    REQUIRE(Controller.importFromFile(File, QString()).value(QStringLiteral("ok")).toBool());

    CHECK(Controller.GetDecks().size() == 2);
    CHECK(AppDatabase::Get().GetCardRepository().LoadCards().size() == 2);
}

TEST_CASE("Importing into a deck adds its words there and moves the cards of other decks", "[userdeck][import]")
{
    FAppDatabaseGuard Guard;
    UseFrenchPair();

    QTemporaryDir Folder;
    REQUIRE(Folder.isValid());

    UserDecksController Controller;

    REQUIRE(Controller.importFromFile(WriteWordList(Folder, "chien\tdog\nchat\tcat\n"), QString()).value(QStringLiteral("ok")).toBool());

    const QString DeckId = Controller.createDeck(QStringLiteral("Animals"), QString());
    REQUIRE_FALSE(DeckId.isEmpty());

    const QVariantMap Result = Controller.importIntoDeck(WriteWordList(Folder, "chien\tdog\noiseau\tbird\n"), DeckId);

    REQUIRE(Result.value(QStringLiteral("ok")).toBool());
    CHECK(Result.value(QStringLiteral("added")).toInt() == 1);
    CHECK(Result.value(QStringLiteral("reused")).toInt() == 1);
    CHECK(Result.value(QStringLiteral("moved")).toInt() == 1);
    CHECK(AppDatabase::Get().GetUserDecks().ListCardIds(DeckId.toStdString()).size() == 2);
    CHECK(Controller.GetDecks().size() == 2);
    CHECK(Controller.collectSessionCards(DeckId).size() == 2);

    CHECK_FALSE(Controller.importIntoDeck(WriteWordList(Folder, "lait\tmilk\n"), QStringLiteral("missing")).value(QStringLiteral("ok")).toBool());
}

TEST_CASE("Daily learning skips the new cards of a deck taken out of it", "[userdeck]")
{
    FAppDatabaseGuard Guard;
    UseFrenchPair();

    QTemporaryDir Folder;
    REQUIRE(Folder.isValid());

    UserDecksController Controller;

    const QVariantMap Result = Controller.importFromFile(WriteWordList(Folder, "chien\tdog\nchat\tcat\n"), QString());
    const QString DeckId = Result.value(QStringLiteral("deckId")).toString();

    DailySessionController Daily;
    CHECK(Daily.collectSessionCards().size() == 2);

    REQUIRE(Controller.setDailyLearning(DeckId, false));
    CHECK(Daily.collectSessionCards().isEmpty());
    CHECK(Controller.collectSessionCards(DeckId).size() == 2);
}

TEST_CASE("A card created while a deck is open joins it, and the deck exports to a list that imports back", "[userdeck][import]")
{
    FAppDatabaseGuard Guard;
    UseFrenchPair();

    QTemporaryDir Folder;
    REQUIRE(Folder.isValid());

    UserDecksController Controller;
    const QString DeckId = Controller.createDeck(QStringLiteral("Animals"), QString());

    VocabularyController Vocabulary;
    Vocabulary.SetUserDeckId(DeckId);

    REQUIRE(Vocabulary.addCard(QStringLiteral("chien"), QStringLiteral("dog, hound")));
    CHECK(AppDatabase::Get().GetUserDecks().ListCardIds(DeckId.toStdString()).size() == 1);

    const QUrl File = QUrl::fromLocalFile(Folder.filePath(QStringLiteral("Animals.csv")));
    REQUIRE(Vocabulary.exportToFile(File));

    const QVariantMap Result = Controller.importFromFile(File, QString());
    CHECK(Result.value(QStringLiteral("added")).toInt() == 0);
    CHECK(Result.value(QStringLiteral("reused")).toInt() == 1);
    CHECK(Result.value(QStringLiteral("moved")).toInt() == 1);
}

TEST_CASE("A file with no words creates no deck", "[userdeck][import]")
{
    FAppDatabaseGuard Guard;
    UseFrenchPair();

    QTemporaryDir Folder;
    REQUIRE(Folder.isValid());

    UserDecksController Controller;

    const QVariantMap Result = Controller.importFromFile(WriteWordList(Folder, "\n"), QString());

    CHECK_FALSE(Result.value(QStringLiteral("ok")).toBool());
    CHECK(Controller.GetDecks().isEmpty());
}
