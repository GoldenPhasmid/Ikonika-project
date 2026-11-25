#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "AppDictionary.h"
#include "AppPaths.h"
#include "FlashcardController.h"

#include "TestAppSpeech.h"

#include "core/FTDatabase.h"
#include "core/FTDictionaryPackage.h"
#include "core/FTSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QStringList>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
    struct FDictionaryTestGuard
    {
        FTLanguageRules SavedRules{FTSettings::Get().GetLanguageRules()};

        FDictionaryTestGuard()
        {
            AppDictionary::Get().Close();
            AppDictionary::Get().Discover();
            FTSettings::Get().SetTargetLanguage("fr");
            FTSettings::Get().SetNativeLanguage("en");
            REQUIRE(AppDictionary::Get().Initialise());
        }

        ~FDictionaryTestGuard()
        {
            AppDictionary::Get().Close();
            FTSettings::Get().SetTargetLanguage(SavedRules.TargetLanguage);
            FTSettings::Get().SetNativeLanguage(SavedRules.NativeLanguage);
        }
    };

    struct FNotifierLog
    {
        int LanguagesChanged{0};
        QStringList Closed;
        QMetaObject::Connection LanguagesConnection;
        QMetaObject::Connection ClosedConnection;

        FNotifierLog()
        {
            LanguagesConnection = QObject::connect(&AppDictionary::Notifier(), &AppDictionaryNotifier::languagesChanged,
                                                   [this]() { ++LanguagesChanged; });
            ClosedConnection = QObject::connect(&AppDictionary::Notifier(), &AppDictionaryNotifier::dictionaryClosed,
                                                [this](const QString& Language) { Closed.append(Language); });
        }

        ~FNotifierLog()
        {
            QObject::disconnect(LanguagesConnection);
            QObject::disconnect(ClosedConnection);
        }
    };

    void WriteDictionary(const QString& Path, const std::string& TargetLanguage, const std::string& NativeLanguage,
                         int SchemaVersion = DictionarySchemaVersion)
    {
        QFile::remove(Path);
        REQUIRE(QDir().mkpath(QFileInfo(Path).absolutePath()));

        std::optional<FTDatabase> Target{FTDatabase::Open(Path.toStdString())};
        REQUIRE(Target.has_value());

        REQUIRE(Target->Execute("CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT)"));
        REQUIRE(Target->Execute("CREATE TABLE languages (id INTEGER PRIMARY KEY, code TEXT)"));
        REQUIRE(Target->Execute("CREATE TABLE words (id INTEGER PRIMARY KEY)"));
        REQUIRE(Target->Execute("CREATE TABLE translations (id INTEGER PRIMARY KEY, language_id INTEGER)"));
        REQUIRE(Target->Execute("INSERT INTO metadata VALUES ('schema_version', '"
                                + std::to_string(SchemaVersion) + "')"));
        REQUIRE(Target->Execute("INSERT INTO metadata VALUES ('target_language', '" + TargetLanguage + "')"));
        REQUIRE(Target->Execute("INSERT INTO languages VALUES (1, '" + NativeLanguage + "')"));
        REQUIRE(Target->Execute("INSERT INTO words VALUES (1)"));
        REQUIRE(Target->Execute("INSERT INTO translations VALUES (1, 1)"));
    }
}

TEST_CASE("A language's dictionary opens on request without changing the learned language", "[appdictionary]")
{
    FDictionaryTestGuard Guard;
    FNotifierLog Log;

    AppLanguageDictionary& Spanish = AppDictionary::Get("es");

    REQUIRE(Spanish.IsReady());
    REQUIRE(Spanish.GetPackage() != nullptr);
    CHECK(Spanish.GetPackage()->TargetLanguage == "es");
    CHECK(Spanish.GetTargetLanguage() == "es");
    CHECK(Spanish.GetTranslationLanguage() == "en");

    AppLanguageDictionary& French = AppDictionary::GetActive();

    CHECK(French.GetTargetLanguage() == "fr");
    CHECK(French.IsOpen());
    CHECK(&French.GetRepository() != &Spanish.GetRepository());
    CHECK(Log.LanguagesChanged == 0);
    CHECK(Log.Closed.isEmpty());
}

TEST_CASE("A language with no installed dictionary is not ready", "[appdictionary]")
{
    FDictionaryTestGuard Guard;

    CHECK_FALSE(AppDictionary::Get("xx").IsReady());
    CHECK_FALSE(AppDictionary::Get("xx").IsOpen());
    CHECK(AppDictionary::Get("xx").GetPackage() == nullptr);
    CHECK(AppDictionary::GetActive().IsOpen());
}

TEST_CASE("Switching the learned language closes the previous dictionary", "[appdictionary]")
{
    FDictionaryTestGuard Guard;
    FNotifierLog Log;

    FTSettings::Get().SetTargetLanguage("es");
    REQUIRE(AppDictionary::Get().Open("es", "en"));

    CHECK(AppDictionary::GetActive().GetTargetLanguage() == "es");
    CHECK(AppDictionary::GetActive().IsOpen());
    CHECK_FALSE(AppDictionary::Get("fr").IsOpen());
    CHECK(Log.Closed == QStringList{QStringLiteral("fr")});
    CHECK(Log.LanguagesChanged == 1);
}

TEST_CASE("Closing one language leaves the others open", "[appdictionary]")
{
    FDictionaryTestGuard Guard;
    REQUIRE(AppDictionary::Get("es").IsReady());

    FNotifierLog Log;

    AppDictionary::Get().Close("es");

    CHECK(Log.Closed == QStringList{QStringLiteral("es")});
    CHECK(Log.LanguagesChanged == 0);
    CHECK_FALSE(AppDictionary::Get("es").IsOpen());
    CHECK(AppDictionary::GetActive().IsOpen());

    CHECK(AppDictionary::Get("es").IsReady());
}

TEST_CASE("Closing every dictionary reports each open language", "[appdictionary]")
{
    FDictionaryTestGuard Guard;
    REQUIRE(AppDictionary::Get("es").IsReady());

    FNotifierLog Log;

    AppDictionary::Get().Close();

    Log.Closed.sort();
    CHECK(Log.Closed == QStringList{QStringLiteral("es"), QStringLiteral("fr")});
    CHECK_FALSE(AppDictionary::Get("es").IsOpen());
    CHECK_FALSE(AppDictionary::Get("fr").IsOpen());
}

TEST_CASE("A language's dictionary keeps its address while others open", "[appdictionary]")
{
    FDictionaryTestGuard Guard;

    AppLanguageDictionary& French = AppDictionary::GetActive();
    FTDictionaryRepository& FrenchRepository = French.GetRepository();

    for (const std::string& Language : {"es", "de", "it", "pt"})
    {
        CHECK(AppDictionary::Get(Language).IsReady());
    }

    CHECK(&French == &AppDictionary::Get("fr"));
    CHECK(&FrenchRepository == &French.GetRepository());
    CHECK(French.IsOpen());
}

TEST_CASE("Closing a dictionary changes its generation", "[appdictionary]")
{
    FDictionaryTestGuard Guard;

    AppLanguageDictionary& Spanish = AppDictionary::Get("es");
    REQUIRE(Spanish.IsReady());

    const uint64_t Before{Spanish.GetGeneration()};

    AppDictionary::Get().Close("es");

    CHECK(Spanish.GetGeneration() != Before);
}

TEST_CASE("A dictionary without the user's native language opens with a language it has", "[appdictionary]")
{
    AppPaths::EnableHeadlessSandbox();

    const QString Path{QDir(AppPaths::InstalledDictionariesDirectory()).filePath(QStringLiteral("nl-dictionary.db"))};

    WriteDictionary(Path, "nl", "de");

    FDictionaryTestGuard Guard;

    AppLanguageDictionary& Dutch = AppDictionary::Get("nl");

    CHECK(Dutch.IsReady());
    CHECK(Dutch.GetTranslationLanguage() == "de");

    AppDictionary::Get().Close("nl");
    QFile::remove(Path);
    AppDictionary::Get().Discover();
}

TEST_CASE("A card shows its hint and no phonetic while the learned language has no usable dictionary", "[appdictionary]")
{
    FAppSpeechGuard SpeechGuard;
    FDictionaryTestGuard Guard;

    AppPaths::EnableHeadlessSandbox();

    const QString Path{QDir(AppPaths::InstalledDictionariesDirectory()).filePath(QStringLiteral("nl-dictionary.db"))};
    const auto Remove{qScopeGuard([&Path]
    {
        QFile::remove(Path);
        AppDictionary::Get().Discover();
    })};

    WriteDictionary(Path, "nl", "en", OldestDictionarySchemaVersion - 1);

    AppDictionary::Get().Close();
    FTSettings::Get().AdoptLanguagePair(FTLanguageRules{"nl", "en"});
    AppDictionary::Get().Discover();

    REQUIRE_FALSE(AppDictionary::GetActive().IsReady());

    FTCard Card;
    Card.Id = "card";
    Card.TargetWord = "woord";
    Card.Translation = "word";
    Card.PartOfSpeech = "noun";
    Card.MeaningGist = "gist";
    Card.Hint = "hint";

    FTSessionStartParams Params{};
    Params.CardState = std::make_shared<FCardState>(std::vector<FTCardHandle>{MakeCardHandle(Card)});
    Params.TargetLanguage = "nl";
    Params.FixedLength = 1;
    Params.Selector = std::make_shared<FEachOnceSelector>(1, true);

    FlashcardController Controller;
    Controller.Start(Params);

    CHECK(Controller.GetPhonetic().isEmpty());
    CHECK(Controller.GetExample() == "hint");
    CHECK(Controller.GetExampleTranslation().isEmpty());
    CHECK_FALSE(AppDictionary::GetActive().IsReady());
}

TEST_CASE("Dictionaries requested from several threads at once each open", "[appdictionary]")
{
    FDictionaryTestGuard Guard;

    std::atomic<int> Ready{0};
    std::vector<std::thread> Threads;

    for (int Index = 0; Index < 8; ++Index)
    {
        Threads.emplace_back([&Ready, Index]()
        {
            if (AppDictionary::Get(Index % 2 == 0 ? "es" : "de").IsReady())
            {
                ++Ready;
            }
        });
    }

    for (std::thread& Thread : Threads)
    {
        Thread.join();
    }

    CHECK(Ready == 8);
    CHECK(AppDictionary::Get("es").IsOpen());
    CHECK(AppDictionary::Get("de").IsOpen());
}
