#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "DictionaryController.h"

#include "AppDictionary.h"
#include "TestAppDatabase.h"

#include "core/FTCard.h"
#include "core/FTSettings.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QStringList>
#include <QThread>
#include <QVariantMap>

#include <algorithm>

namespace
{
    struct FAppDictionaryGuard
    {
        ~FAppDictionaryGuard()
        {
            AppDictionary::Get().Close();
        }
    };

    struct FDictionaryLanguageGuard
    {
        std::string TargetLanguage{FTSettings::Get().GetLanguageRules().TargetLanguage};
        std::string NativeLanguage{FTSettings::Get().GetLanguageRules().NativeLanguage};

        ~FDictionaryLanguageGuard()
        {
            FTSettings::Get().SetLanguagePair(TargetLanguage, NativeLanguage);
        }
    };

    QVariantMap FindTranslation(const QVariantList& Translations, const QString& Text)
    {
        for (const QVariant& Entry : Translations)
        {
            const QVariantMap Map = Entry.toMap();

            if (Map.value("text").toString().compare(Text, Qt::CaseInsensitive) == 0)
            {
                return Map;
            }
        }

        return {};
    }

    void WaitForResults(DictionaryController& Controller)
    {
        QElapsedTimer Timer;
        Timer.start();

        while (Controller.GetResults().isEmpty() && Timer.elapsed() < 5000)
        {
            QCoreApplication::processEvents();
            QThread::msleep(5);
        }
    }

    QVariantMap FindResultByPos(const QVariantList& Results, const QString& PartOfSpeech)
    {
        for (const QVariant& Entry : Results)
        {
            const QVariantMap Map = Entry.toMap();

            if (Map.value("partOfSpeech").toString() == PartOfSpeech)
            {
                return Map;
            }
        }

        return {};
    }

    QVariantList SearchFor(const QString& Text)
    {
        DictionaryController Controller;
        Controller.search(Text);
        WaitForResults(Controller);

        return Controller.GetResults();
    }

    QStringList ResultWords(const QVariantList& Results)
    {
        QStringList Words;

        for (const QVariant& Row : Results)
        {
            Words.push_back(Row.toMap().value("word").toString());
        }

        return Words;
    }
}

TEST_CASE("English dictionary displays retain the bare headword for navigation", "[dictionarycontroller][englishverbs]")
{
    FAppDatabaseGuard DatabaseGuard;
    FDictionaryLanguageGuard LanguageGuard;
    FAppDictionaryGuard DictionaryGuard;
    FTSettings::Get().SetTargetLanguage("en");
    FTSettings::Get().SetNativeLanguage("uk");
    REQUIRE(AppDictionary::Get().Initialise());

    DictionaryController Controller;
    Controller.openEntry("learn", "verb");
    REQUIRE(Controller.GetEntry().value("headword").toString() == "learn");
    CHECK(Controller.GetEntry().value("displayHeadword").toString() == "to learn");
    CHECK(Controller.GetEntry().value("forms").toMap().value("cues").toStringList()
          == QStringList{"I", "you", "he/she/it", "we", "you", "they"});

    Controller.openEntry("watch", "noun");
    CHECK(Controller.GetEntry().value("displayHeadword").toString() == "watch");

    const QVariantList Results = SearchFor("learn");
    REQUIRE_FALSE(Results.isEmpty());
    const QVariantMap Row = Results.front().toMap();
    CHECK(Row.value("word").toString() == "learn");
    CHECK(Row.value("displayWord").toString() == "to learn");
}

TEST_CASE("English dictionary verbs fill all six present and past cells", "[dictionarycontroller][englishverbs]")
{
    FAppDatabaseGuard DatabaseGuard;
    FDictionaryLanguageGuard LanguageGuard;
    FAppDictionaryGuard DictionaryGuard;
    REQUIRE(AppDictionary::Get().Initialise());
    REQUIRE(FTSettings::Get().SetLanguagePair("en", "uk"));
    AppDictionary::Get().Close();
    REQUIRE(AppDictionary::Get().Initialise());

    DictionaryController Controller;
    const QStringList Words{"work", "run", "have", "go", "be"};
    const QList<QStringList> Present{
        {"work", "work", "works", "work", "work", "work"},
        {"run", "run", "runs", "run", "run", "run"},
        {"have", "have", "has", "have", "have", "have"},
        {"go", "go", "goes", "go", "go", "go"},
        {"am", "are", "is", "are", "are", "are"}};
    const QList<QStringList> Past{
        {"worked", "worked", "worked", "worked", "worked", "worked"},
        {"ran", "ran", "ran", "ran", "ran", "ran"},
        {"had", "had", "had", "had", "had", "had"},
        {"went", "went", "went", "went", "went", "went"},
        {"was", "were", "was", "were", "were", "were"}};

    for (int WordIndex = 0; WordIndex < Words.size(); ++WordIndex)
    {
        INFO(Words[WordIndex].toStdString());
        Controller.openEntry(Words[WordIndex], "verb");
        const QVariantMap Forms = Controller.GetEntry().value("forms").toMap();
        CHECK(Forms.value("language").toString() == "en");
        bool bFoundPresent{false};
        bool bFoundPast{false};

        for (const QVariant& Mood : Forms.value("moods").toList())
        {
            if (Mood.toMap().value("mood").toInt() != 1)
            {
                continue;
            }

            for (const QVariant& Tense : Mood.toMap().value("tenses").toList())
            {
                const int Code = Tense.toMap().value("tense").toInt();
                REQUIRE((Code == 1 || Code == 3));
                bFoundPresent = bFoundPresent || Code == 1;
                bFoundPast = bFoundPast || Code == 3;
                const QVariantList Cells = Tense.toMap().value("cells").toList();
                REQUIRE(Cells.size() == 6);
                const QStringList& Expected = Code == 1 ? Present[WordIndex] : Past[WordIndex];

                for (int Person = 0; Person < Cells.size(); ++Person)
                {
                    CHECK(Cells[Person].toString().split(" / ").contains(Expected[Person]));
                }
            }
        }

        CHECK(bFoundPresent);
        CHECK(bFoundPast);
    }
}

TEST_CASE("An inflected lookup identifies every matching person and clears on another entry", "[dictionarycontroller][conjugationlookup]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;
    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");
    REQUIRE(AppDictionary::Get().Initialise());

    DictionaryController Controller;
    Controller.openEntry("voudrais", "verb");
    REQUIRE(Controller.GetEntry().value("headword").toString() == "vouloir");
    const QVariantMap Forms = Controller.GetEntry().value("forms").toMap();
    CHECK(Forms.value("matchedText").toString() == "voudrais");
    CHECK_FALSE(Forms.value("matchedReadings").toStringList().isEmpty());
    CHECK(Controller.GetEntry().value("verbGroup").toString() == "third");
    CHECK(Forms.value("matchedKey").toString() == "3-0");
    for (const QVariant& Mood : Forms.value("moods").toList())
    {
        if (Mood.toMap().value("mood").toInt() != 3)
        {
            continue;
        }
        const QVariantMap Tense = Mood.toMap().value("tenses").toList().front().toMap();
        CHECK(Tense.value("matches").toList() == QVariantList{true, true, false, false, false, false});
    }
    Controller.openEntry("vouloir", "verb", " VOUDRAIS ");
    CHECK(Controller.GetEntry().value("forms").toMap().value("matchedKey").toString() == "3-0");
    Controller.openEntry("parler", "verb");
    CHECK(Controller.GetEntry().value("forms").toMap().value("matchedText").toString().isEmpty());
}

TEST_CASE("Verb labels distinguish endings from irregularity", "[dictionarycontroller][conjugationlookup]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;
    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");
    REQUIRE(AppDictionary::Get().Initialise());
    DictionaryController Controller;
    Controller.openEntry("parler", "verb");
    CHECK(Controller.GetEntry().value("verbEnding").toString() == "-er");
    CHECK_FALSE(Controller.GetEntry().value("irregular").toBool());
    CHECK(Controller.GetEntry().value("verbGroup").toString() == "first");
    Controller.openEntry("aller", "verb");
    CHECK(Controller.GetEntry().value("verbEnding").toString() == "-er");
    CHECK(Controller.GetEntry().value("irregular").toBool());
    CHECK(Controller.GetEntry().value("verbGroup").toString() == "third");
    CHECK(Controller.GetEntry().value("conjugationUrl").toString() == "https://conjugator.reverso.net/conjugation-french-verb-aller.html");
}

TEST_CASE("A translation already collected shows its stage", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    FTCard Card;
    Card.TargetWord = "avoir";
    Card.Translation = "to have";

    REQUIRE(AddTestCards({Card}));

    DictionaryController Controller;
    Controller.openEntry("avoir", "verb");

    const QVariantMap Translation = FindTranslation(Controller.GetAllTranslations(), "to have");

    REQUIRE_FALSE(Translation.isEmpty());
    CHECK(Translation.value("learnedStage").toInt() >= 0);
}

TEST_CASE("A custom translation typed for a card still shows up on the word screen",
          "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    FTCard Card;
    Card.TargetWord = "avoir";
    Card.PartOfSpeech = "verb";
    Card.Translation = "to possess something";

    REQUIRE(AddTestCards({Card}));

    DictionaryController Controller;
    Controller.openEntry("avoir", "verb");

    const QVariantList Translations = Controller.GetAllTranslations();

    const QVariantMap Custom = FindTranslation(Translations, "to possess something");

    REQUIRE_FALSE(Custom.isEmpty());
    CHECK_FALSE(Custom.value("hasProbability").toBool());
    CHECK(Custom.value("learnedStage").toInt() >= 0);

    const int CustomIndex = Translations.indexOf(QVariant(Custom));
    REQUIRE(CustomIndex >= 0);
    CHECK(CustomIndex == 0);
}

TEST_CASE("A custom translation of a card with a meaning is listed under that meaning",
          "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    DictionaryController Controller;
    Controller.openEntry("avoir", "verb");

    const QVariantList Meanings = Controller.GetEntry().value("meanings").toList();
    REQUIRE_FALSE(Meanings.isEmpty());

    const QString Gist = Meanings.front().toMap().value("gist").toString();

    FTCard Card;
    Card.TargetWord = "avoir";
    Card.PartOfSpeech = "verb";
    Card.MeaningGist = Gist.toStdString();
    Card.Translation = "to possess something";

    REQUIRE(AddTestCards({Card}));

    Controller.openEntry("avoir", "verb");

    const QVariantList Reloaded = Controller.GetEntry().value("meanings").toList();
    const auto Matched = std::find_if(Reloaded.begin(), Reloaded.end(),
        [&Gist](const QVariant& Row) { return Row.toMap().value("gist").toString() == Gist; });

    REQUIRE(Matched != Reloaded.end());
    CHECK(Matched->toMap().value("translations").toStringList().contains("to possess something"));
}

TEST_CASE("A translation that was never collected has no stage", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    DictionaryController Controller;
    Controller.openEntry("avoir", "verb");

    const QVariantMap Translation = FindTranslation(Controller.GetAllTranslations(), "to have");

    REQUIRE_FALSE(Translation.isEmpty());
    CHECK(Translation.value("learnedStage").toInt() == -1);
}

TEST_CASE("Clearing the search shows recent search history", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    DictionaryController Controller;
    Controller.openEntry("avoir", "verb");
    Controller.recordSearch();

    Controller.clear();

    const QVariantList History = Controller.GetResults();

    const bool bFoundAvoir = std::any_of(History.begin(), History.end(),
        [](const QVariant& Row) { return Row.toMap().value("word").toString() == "avoir"; });

    CHECK(bFoundAvoir);
}

TEST_CASE("Searching empty text shows recent search history too", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    DictionaryController Controller;
    Controller.openEntry("avoir", "verb");
    Controller.recordSearch();

    Controller.search("");

    const QVariantList History = Controller.GetResults();

    const bool bFoundAvoir = std::any_of(History.begin(), History.end(),
        [](const QVariant& Row) { return Row.toMap().value("word").toString() == "avoir"; });

    CHECK(bFoundAvoir);
}

TEST_CASE("Learning one reading of a word does not mark its other part of speech learned",
          "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());


    FTCard Card;
    Card.TargetWord = "avoir";
    Card.PartOfSpeech = "verb";
    Card.Translation = "to have";
    Card.LeitnerStage = 2;

    REQUIRE(AddTestCards({Card}));

    DictionaryController Controller;
    Controller.openEntry("avoir", "verb");

    const QVariantMap VerbReading = FindResultByPos(Controller.GetReadings(), "verb");
    const QVariantMap NounReading = FindResultByPos(Controller.GetReadings(), "noun");

    REQUIRE_FALSE(VerbReading.isEmpty());
    REQUIRE_FALSE(NounReading.isEmpty());

    CHECK(VerbReading.value("stage").toInt() == 2);
    CHECK(NounReading.value("stage").toInt() == -1);
}

TEST_CASE("A search lists a word once across its parts of speech", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    FTCard Card;
    Card.TargetWord = "avoir";
    Card.PartOfSpeech = "verb";
    Card.Translation = "to have";
    Card.LeitnerStage = 2;

    REQUIRE(AddTestCards({Card}));

    DictionaryController Controller;
    Controller.search("avoir");
    WaitForResults(Controller);

    const QVariantList Results = Controller.GetResults();

    const auto IsAvoir = [](const QVariant& Row) { return Row.toMap().value("word").toString() == "avoir"; };

    CHECK(std::count_if(Results.begin(), Results.end(), IsAvoir) == 1);

    const auto Avoir = std::find_if(Results.begin(), Results.end(), IsAvoir);

    REQUIRE(Avoir != Results.end());
    CHECK(Avoir->toMap().value("stage").toInt() == 2);
}

TEST_CASE("A verb's forms carry the persons and mark the endings where training knows them", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    DictionaryController Controller;
    Controller.openEntry("venir", "verb");

    const QVariantMap Forms = Controller.GetEntry().value("forms").toMap();

    REQUIRE(Forms.value("kind").toString() == "verb");
    CHECK(Forms.value("cues").toStringList() == QStringList{"je", "tu", "il/elle", "nous", "vous", "ils/elles"});

    QVariantList Present;
    QVariantList Subjunctive;

    for (const QVariant& Mood : Forms.value("moods").toList())
    {
        for (const QVariant& Tense : Mood.toMap().value("tenses").toList())
        {
            const int MoodCode = Mood.toMap().value("mood").toInt();

            if (Tense.toMap().value("tense").toInt() == 1 && (MoodCode == 1 || MoodCode == 2))
            {
                (MoodCode == 1 ? Present : Subjunctive) = Tense.toMap().value("marked").toList();
            }
        }
    }

    REQUIRE(Present.size() == 6);
    CHECK(Present[0].toString() == "vien<a href=\"ending\">s</a>");
    CHECK(Present[3].toString() == "ven<a href=\"ending\">ons</a>");

    REQUIRE(Subjunctive.size() == 6);
    CHECK(Subjunctive[0].toString() == "vienne");
}

TEST_CASE("An adjective's forms carry the pronouns and fill the headword in", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    DictionaryController Controller;
    Controller.openEntry("beau", "adj");

    const QVariantMap Forms = Controller.GetEntry().value("forms").toMap();

    REQUIRE(Forms.value("kind").toString() == "agreement");
    CHECK(Forms.value("cues").toStringList() == QStringList{"il", "elle", "ils", "elles"});

    const QVariantList Marked = Forms.value("marked").toList();

    REQUIRE(Marked.size() == 2);
    CHECK(Marked[0].toList()[0].toString() == "beau");
    CHECK(Marked[0].toList()[1].toString() == "beau<a href=\"ending\">x</a>");
    CHECK(Marked[1].toList()[0].toString() == "be<a href=\"ending\">lle</a>");
}

TEST_CASE("A noun or adjective that does not qualify for forms training shows no forms", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    DictionaryController Controller;
    Controller.openEntry("jeune", "adj");

    CHECK(Controller.GetEntry().value("forms").toMap().value("kind").toString() == "none");
}

TEST_CASE("An inflected word is found by its headword with the headword's level", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    CHECK(ResultWords(SearchFor("parles")).contains("parler"));

    const QVariantList Results = SearchFor("inconnus");

    REQUIRE_FALSE(Results.isEmpty());
    CHECK(Results.front().toMap().value("word").toString() == "inconnu");
    CHECK(Results.front().toMap().value("cefrLevel").toString() == "A1");
}

TEST_CASE("Punctuation around a word does not hide it", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    CHECK(ResultWords(SearchFor("noir.")).contains("noir"));
    CHECK(ResultWords(SearchFor(QString::fromUtf8("\xC2\xAB" "Chien" "\xC2\xBB!"))).contains("chien"));
}

TEST_CASE("An elided or hyphenated word is searched part by part", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    QStringList Words = ResultWords(SearchFor("l'heure"));

    REQUIRE_FALSE(Words.isEmpty());
    CHECK(Words.front() == "heure");
    CHECK(Words.contains("le"));

    CHECK(ResultWords(SearchFor(QString::fromUtf8("l\xE2\x80\x99heure"))).contains("heure"));

    Words = ResultWords(SearchFor("n'est"));

    CHECK(Words.contains("ne"));
    CHECK(Words.contains(QString::fromUtf8("\xC3\xAAtre")));

    Words = ResultWords(SearchFor("Parlez-vous"));

    CHECK(Words.contains("parler"));
    CHECK(Words.contains("vous"));

    Words = ResultWords(SearchFor("Parle-t-il"));

    CHECK(Words.contains("parler"));
    CHECK(Words.contains("il"));
    CHECK_FALSE(Words.contains("t"));
}

TEST_CASE("A word spelled with an apostrophe or a hyphen stays whole when the dictionary has it",
          "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    CHECK(ResultWords(SearchFor("aujourd'hui")).front() == "aujourd'hui");
    CHECK(ResultWords(SearchFor(QString::fromUtf8("peut-\xC3\xAAtre"))).front() == QString::fromUtf8("peut-\xC3\xAAtre"));
}

TEST_CASE("Spanish attached pronouns fall back to a verb", "[dictionarycontroller][lookupfix]")
{
    FAppDatabaseGuard DatabaseGuard;
    FDictionaryLanguageGuard LanguageGuard;
    FAppDictionaryGuard DictionaryGuard;
    FTSettings::Get().SetTargetLanguage("es");
    FTSettings::Get().SetNativeLanguage("en");
    REQUIRE(AppDictionary::Get().Initialise());

    for (const QString& Text : {QString{"comerme"}, QString{"comermelo"}, QString::fromUtf8("comiéndome"),
                                QString{"dame"}, QString::fromUtf8("dámelo")})
    {
        CAPTURE(Text);
        const QVariantList Results = SearchFor(Text);
        REQUIRE_FALSE(Results.isEmpty());
        const QVariantMap First = Results.front().toMap();
        CHECK(First.value("word").toString() == (Text.startsWith("com") ? "comer" : "dar"));
        CHECK(First.value("partOfSpeech").toString() == "verb");
    }

    CHECK(ResultWords(SearchFor("vamos")).front() == "vamos");
    CHECK(ResultWords(SearchFor("caramelo")).front() == "caramelo");
}

TEST_CASE("Spanish pronoun fallback rejects indicative stems", "[dictionarycontroller][lookupfix]")
{
    FAppDatabaseGuard DatabaseGuard;
    FDictionaryLanguageGuard LanguageGuard;
    FAppDictionaryGuard DictionaryGuard;
    FTSettings::Get().SetTargetLanguage("es");
    FTSettings::Get().SetNativeLanguage("en");
    REQUIRE(AppDictionary::Get().Initialise());
    CHECK_FALSE(ResultWords(SearchFor("comome")).contains("comer"));
}

TEST_CASE("French t'aime restores te and finds aimer", "[dictionarycontroller][lookupfix]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;
    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");
    REQUIRE(AppDictionary::Get().Initialise());

    for (const QString& Text : {QString{"t'aime"}, QString::fromUtf8("t’aime")})
    {
        const QStringList Words = ResultWords(SearchFor(Text));
        REQUIRE_FALSE(Words.isEmpty());
        CHECK(Words.front() == "aimer");
        CHECK(Words.contains("te"));
        CHECK_FALSE(Words.contains("t"));
    }
}

TEST_CASE("Si opens its frequent A1 reading and allows selecting the noun", "[dictionarycontroller][lookupfix]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;
    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");
    REQUIRE(AppDictionary::Get().Initialise());
    DictionaryController Controller;
    Controller.openEntry("si", "");
    CHECK(Controller.GetEntry().value("partOfSpeech").toString() == "adv");
    const QVariantList Results = SearchFor("si");
    REQUIRE_FALSE(Results.isEmpty());
    CHECK(Results.front().toMap().value("partOfSpeech").toString() == "adv");
    Controller.openEntry("si", "noun");
    CHECK(Controller.GetEntry().value("partOfSpeech").toString() == "noun");
}

TEST_CASE("A word offers the parts of speech the dictionary has for it", "[dictionarycontroller]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());

    const auto Values = [](const DictionaryController& Controller)
    {
        QStringList Result;

        for (const QVariant& Choice : Controller.GetPartsOfSpeech())
        {
            Result << Choice.toMap().value("value").toString();
        }

        return Result;
    };

    DictionaryController Controller;
    Controller.openEntry("inconnu", "");

    QStringList Offered = Values(Controller);

    CHECK(Offered.size() == 2);
    CHECK(Offered.contains("adj"));
    CHECK(Offered.contains("noun"));

    Controller.openEntry("inconnu", "verb");

    Offered = Values(Controller);

    CHECK(Offered.size() == 3);
    CHECK(Offered.contains("verb"));

    Controller.openEntry("zzzzqq", "");

    CHECK(Values(Controller).contains("verb"));
    CHECK(Values(Controller).size() > 3);
}
