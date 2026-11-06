#include <catch2/catch_test_macros.hpp>

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTErasure.h"
#include "core/FTJournal.h"
#include "core/FTSchema.h"

#include <memory>
#include <string>

namespace
{
    struct Fixture
    {
        Fixture()
        {
            std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
            REQUIRE(Opened.has_value());

            Target = std::make_unique<FTDatabase>(std::move(*Opened));
            REQUIRE(MigrateSchema(*Target));

            Repository = std::make_unique<FTCardRepository>(*Target, [this] { return Clock; });
            Repository->SetActiveLanguages("fr", "en");
        }

        std::unique_ptr<FTDatabase> Target;
        std::unique_ptr<FTCardRepository> Repository;
        int64_t Clock = 1000;
    };

    std::string SeedCard(Fixture& Test, const std::string& Word)
    {
        FTCard Card;
        Card.TargetWord = Word;
        Card.Translation = "translation";

        const std::optional<std::string> CardId = Test.Repository->AddCard(Card);
        REQUIRE(CardId.has_value());

        return *CardId;
    }

    int CountRows(Fixture& Test, const std::string& Table, const std::string& Condition = "1")
    {
        FTStatement Query = Test.Target->Prepare("SELECT COUNT(*) FROM " + Table + " WHERE " + Condition);
        REQUIRE(Query.IsValid());
        REQUIRE(Query.Step());

        return Query.GetInt(0);
    }

    void RecordErasure(Fixture& Test, int64_t At)
    {
        FTStatement Insert = Test.Target->Prepare(
            "INSERT INTO Erasures (Scope, ErasedDate) VALUES ('everything', ?1) "
            "ON CONFLICT (Scope) DO UPDATE SET ErasedDate = excluded.ErasedDate");
        REQUIRE(Insert.IsValid());

        Insert.BindInt64(1, At);
        REQUIRE(Insert.Run());
    }

    void RecordLanguageErasure(Fixture& Test, const std::string& Language, int64_t At)
    {
        FTStatement Insert = Test.Target->Prepare("INSERT INTO Erasures (Scope, ErasedDate) VALUES (?1, ?2)");
        REQUIRE(Insert.IsValid());

        Insert.BindText(1, MakeLanguageErasureScope(Language));
        Insert.BindInt64(2, At);
        REQUIRE(Insert.Run());
    }

    void InsertEvent(Fixture& Test, const std::string& CardId)
    {
        REQUIRE(AppendJournalEvent(*Test.Target, MakeJournalEvent(CardId, JournalLearned, 0, 20000)));
    }

    void SeedEveryTable(Fixture& Test, const std::string& Language)
    {
        Test.Repository->SetActiveLanguages(Language, "en");

        SeedCard(Test, Language + "-word");

        FTCard Forms;
        Forms.Id = "conjugation-" + Language + "-verb-present";
        Forms.TargetWord = Language + "-verb";
        Forms.Translation = "do";
        REQUIRE(Test.Repository->AddFormsCards({Forms}));

        REQUIRE(Test.Repository->MarkWordTriaged(Language + "-known", "noun", 1));
        REQUIRE(Test.Repository->RecordSearch(Language + "-searched", "noun", 2));
        REQUIRE(Test.Repository->SetPlacementCutoff(Language, 300));
        REQUIRE(Test.Repository->RecordDailyProgress(20000, 5, 10, 25));
    }

    constexpr const char* LanguageTables[]{"Cards", "WordForms", "TriagedWords", "SearchHistory", "Placements",
                                           "DailyStats"};
}

TEST_CASE("Erasing everything records when it happened", "[erase]")
{
    Fixture Test;
    Test.Clock = 5000;

    REQUIRE(Test.Repository->DeleteEverything());

    CHECK(ReadErasedDate(*Test.Target) == 5000);
}

TEST_CASE("A database never erased has no erase time", "[erase]")
{
    Fixture Test;

    CHECK_FALSE(ReadErasedDate(*Test.Target).has_value());
}

TEST_CASE("Erasing everything clears the journal and every synced table but the settings", "[erase]")
{
    Fixture Test;

    const std::string CardId = SeedCard(Test, "avoir");
    REQUIRE(Test.Repository->PromoteCards({CardId}));
    REQUIRE(Test.Repository->MarkWordTriaged("maison", "noun", 1));
    REQUIRE(Test.Repository->RecordSearch("chien", "noun", 2));
    REQUIRE(Test.Repository->SetPlacementCutoff("fr", 300));
    REQUIRE(Test.Repository->RecordDailyProgress(20000, 5, 10, 25));
    REQUIRE(Test.Target->Execute(
        "INSERT INTO SyncedSettings (Key, Value, LastUpdatedDate) VALUES ('DailyGoal', '20', 1000)"));

    Test.Clock = 5000;
    REQUIRE(Test.Repository->DeleteEverything());

    for (const char* Table : {"Journal", "Cards", "WordForms", "Placements",
                              "DailyStats", "TriagedWords", "SearchHistory"})
    {
        INFO(Table);
        CHECK(CountRows(Test, Table) == 0);
    }

    CHECK(CountRows(Test, "SyncedSettings") == 1);
}

TEST_CASE("Erasing everything keeps this device's id", "[erase]")
{
    Fixture Test;

    const std::string DeviceId = ReadLocalDeviceId(*Test.Target);

    REQUIRE(Test.Repository->DeleteEverything());

    CHECK(ReadLocalDeviceId(*Test.Target) == DeviceId);
}

TEST_CASE("Erasing twice keeps the later time", "[erase]")
{
    Fixture Test;

    Test.Clock = 5000;
    REQUIRE(Test.Repository->DeleteEverything());

    Test.Clock = 7000;
    REQUIRE(Test.Repository->DeleteEverything());

    CHECK(ReadErasedDate(*Test.Target) == 7000);
    CHECK(CountRows(Test, "Erasures") == 1);
}

TEST_CASE("Removing erased data drops rows older than the erase and keeps newer ones", "[erase]")
{
    Fixture Test;

    Test.Clock = 2000;
    const std::string Old = SeedCard(Test, "avoir");
    REQUIRE(Test.Repository->MarkWordTriaged("maison", "noun", 1));
    REQUIRE(Test.Repository->RecordSearch("chien", "noun", 2));
    REQUIRE(Test.Repository->SetPlacementCutoff("fr", 300));

    Test.Clock = 4000;
    const std::string New = SeedCard(Test, "être");
    REQUIRE(Test.Repository->MarkWordTriaged("chat", "noun", 0));
    REQUIRE(Test.Repository->RecordSearch("pain", "noun", 3));

    RecordErasure(Test, 3000);

    REQUIRE(RemoveErasedData(*Test.Target));

    CHECK(CountRows(Test, "Cards", "Id = '" + Old + "'") == 0);
    CHECK(CountRows(Test, "Cards", "Id = '" + New + "'") == 1);
    CHECK(CountRows(Test, "Placements") == 0);

    CHECK_FALSE(Test.Repository->IsWordTriaged("maison", "noun", 1));
    CHECK(Test.Repository->IsWordTriaged("chat", "noun", 0));
    CHECK(CountRows(Test, "SearchHistory", "Term = 'chien'") == 0);
    CHECK(CountRows(Test, "SearchHistory", "Term = 'pain'") == 1);
}

TEST_CASE("Removing erased data keeps the settings older than the erase", "[erase]")
{
    Fixture Test;

    REQUIRE(Test.Target->Execute(
        "INSERT INTO SyncedSettings (Key, Value, LastUpdatedDate) VALUES ('DailyGoal', '20', 2000)"));
    RecordErasure(Test, 3000);

    REQUIRE(RemoveErasedData(*Test.Target));

    CHECK(CountRows(Test, "SyncedSettings") == 1);
}

TEST_CASE("Removing erased data drops a deleted language's rows older than the deletion", "[erase]")
{
    Fixture Test;

    Test.Clock = 2000;
    const std::string Old = SeedCard(Test, "avoir");
    REQUIRE(Test.Repository->MarkWordTriaged("maison", "noun", 1));
    REQUIRE(Test.Repository->RecordSearch("chien", "noun", 2));
    REQUIRE(Test.Repository->SetPlacementCutoff("fr", 300));
    InsertEvent(Test, Old);

    Test.Repository->SetActiveLanguages("es", "en");
    const std::string Spanish = SeedCard(Test, "tener");
    REQUIRE(Test.Repository->MarkWordTriaged("casa", "noun", 1));

    Test.Repository->SetActiveLanguages("fr", "en");
    Test.Clock = 4000;
    const std::string New = SeedCard(Test, "être");
    REQUIRE(Test.Repository->MarkWordTriaged("chat", "noun", 0));

    REQUIRE(Test.Target->Execute("INSERT INTO Erasures (Scope, ErasedDate) VALUES ('language:fr', 3000)"));

    REQUIRE(RemoveErasedData(*Test.Target));

    CHECK(CountRows(Test, "Cards", "Id = '" + Old + "'") == 0);
    CHECK(CountRows(Test, "Cards", "Id = '" + New + "'") == 1);
    CHECK(CountRows(Test, "Cards", "Id = '" + Spanish + "'") == 1);
    CHECK(CountRows(Test, "Journal", "CardId = '" + Old + "'") == 0);
    CHECK_FALSE(Test.Repository->IsWordTriaged("maison", "noun", 1));
    CHECK(Test.Repository->IsWordTriaged("chat", "noun", 0));
    CHECK(CountRows(Test, "TriagedWords", "TargetLanguage = 'es'") == 1);
    CHECK(CountRows(Test, "SearchHistory") == 0);
    CHECK(CountRows(Test, "Placements") == 0);
}

TEST_CASE("Removing erased data with no erase keeps every row", "[erase]")
{
    Fixture Test;

    SeedCard(Test, "avoir");

    REQUIRE(RemoveErasedData(*Test.Target));

    CHECK(CountRows(Test, "Cards") == 1);
}

TEST_CASE("Removing erased data drops the events of cards that no longer exist", "[erase]")
{
    Fixture Test;

    const std::string Kept = SeedCard(Test, "avoir");
    InsertEvent(Test, Kept);
    InsertEvent(Test, "card-that-is-gone");

    REQUIRE(RemoveErasedData(*Test.Target));

    CHECK(CountRows(Test, "Journal", "CardId = '" + Kept + "'") == 1);
    CHECK(CountRows(Test, "Journal", "CardId = 'card-that-is-gone'") == 0);
}

TEST_CASE("Removing erased data keeps the events of a soft-deleted card", "[erase]")
{
    Fixture Test;

    const std::string CardId = SeedCard(Test, "avoir");
    InsertEvent(Test, CardId);
    REQUIRE(Test.Repository->DeleteCards({CardId}));
    REQUIRE(CountRows(Test, "Cards", "Deleted = 1") == 1);

    REQUIRE(RemoveErasedData(*Test.Target));

    CHECK(CountRows(Test, "Journal") == 1);
}

TEST_CASE("Removing erased data keeps the cards edited after the erase", "[erase]")
{
    Fixture Test;

    const std::string Edited = SeedCard(Test, "avoir");
    const std::string Untouched = SeedCard(Test, "être");

    Test.Clock = 4000;
    REQUIRE(Test.Repository->UpdateCardContent(Edited, "avoir", "to own", std::nullopt, "verb", ""));

    RecordErasure(Test, 3000);

    REQUIRE(RemoveErasedData(*Test.Target));

    CHECK(CountRows(Test, "Cards", "Id = '" + Edited + "'") == 1);
    CHECK(CountRows(Test, "Cards", "Id = '" + Untouched + "'") == 0);
    CHECK(Test.Repository->LoadCards().size() == 1);
}

TEST_CASE("Each deleted language removes its own rows by its own deletion time", "[erase]")
{
    Fixture Test;

    Test.Clock = 2000;
    SeedEveryTable(Test, "fr");
    SeedEveryTable(Test, "es");

    RecordLanguageErasure(Test, "fr", 3000);
    RecordLanguageErasure(Test, "es", 5000);

    Test.Clock = 4000;
    Test.Repository->SetActiveLanguages("fr", "en");
    const std::string French = SeedCard(Test, "fr-later");
    Test.Repository->SetActiveLanguages("es", "en");
    const std::string Spanish = SeedCard(Test, "es-later");

    REQUIRE(RemoveErasedData(*Test.Target));

    CHECK(CountRows(Test, "Cards", "Id = '" + French + "'") == 1);
    CHECK(CountRows(Test, "Cards") == 1);

    for (const char* Table : LanguageTables)
    {
        INFO(Table);
        CHECK(CountRows(Test, Table, "TargetLanguage = 'es'") == 0);
    }
}

TEST_CASE("A language deletion newer than erasing everything removes that language up to its own time",
          "[erase]")
{
    Fixture Test;

    Test.Clock = 2000;
    SeedEveryTable(Test, "fr");
    SeedEveryTable(Test, "es");

    RecordErasure(Test, 1500);
    RecordLanguageErasure(Test, "fr", 3000);

    REQUIRE(RemoveErasedData(*Test.Target));

    for (const char* Table : LanguageTables)
    {
        INFO(Table);
        CHECK(CountRows(Test, Table, "TargetLanguage = 'fr'") == 0);
        CHECK(CountRows(Test, Table, "TargetLanguage = 'es'") == 1);
    }
}

TEST_CASE("Rows of a deleted language written after its deletion stay in every table", "[erase]")
{
    Fixture Test;

    Test.Clock = 2000;
    SeedEveryTable(Test, "es");

    Test.Clock = 4000;
    SeedEveryTable(Test, "fr");

    RecordLanguageErasure(Test, "fr", 3000);

    REQUIRE(RemoveErasedData(*Test.Target));

    for (const char* Table : LanguageTables)
    {
        INFO(Table);
        CHECK(CountRows(Test, Table, "TargetLanguage = 'fr'") == 1);
        CHECK(CountRows(Test, Table, "TargetLanguage = 'es'") == 1);
    }
}

TEST_CASE("Deleting a language again replaces its deletion time", "[erase]")
{
    Fixture Test;

    Test.Clock = 5000;
    REQUIRE(Test.Repository->DeleteLanguage("fr"));

    Test.Clock = 7000;
    REQUIRE(Test.Repository->DeleteLanguage("fr"));

    CHECK(CountRows(Test, "Erasures") == 1);
    CHECK(CountRows(Test, "Erasures", "Scope = 'language:fr' AND ErasedDate = 7000") == 1);
    CHECK_FALSE(ReadErasedDate(*Test.Target).has_value());
}

TEST_CASE("Words added again on this device after their language was deleted stay", "[erase]")
{
    Fixture Test;

    Test.Clock = 2000;
    SeedEveryTable(Test, "fr");

    Test.Clock = 3000;
    REQUIRE(Test.Repository->DeleteLanguage("fr"));

    Test.Clock = 4000;
    SeedEveryTable(Test, "fr");

    REQUIRE(RemoveErasedData(*Test.Target));

    for (const char* Table : LanguageTables)
    {
        INFO(Table);
        CHECK(CountRows(Test, Table, "TargetLanguage = 'fr'") == 1);
    }

    CHECK(Test.Repository->IsWordTriaged("fr-known", "noun", 1));
}
