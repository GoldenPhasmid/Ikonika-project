#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppState.h"
#include "DailySessionController.h"
#include "FlashcardController.h"
#include "IntensiveLearningController.h"
#include "RepetitionController.h"
#include "SpeedMatchController.h"
#include "TestAppDatabase.h"
#include "TestAppSpeech.h"
#include "WrittenProductionController.h"

#include "core/FTCard.h"
#include "core/FTWordForms.h"
#include "core/FTCardStore.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <optional>
#include <string>
#include <vector>

static const QString EndingLink = QStringLiteral("<a href=\"ending\">");

static constexpr int StudyStage = 1;
static constexpr int TypingStage = 3;
static constexpr int MatchStage = 2;

struct FFormsTestBed
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;

    FFormsTestBed()
    {
        ForgetSessions();

        FTSettings::Get().SetTargetLanguage("fr");
        FTSettings::Get().SetNativeLanguage("en");

        AppDatabase::Get().GetCardRepository().SetActiveLanguages("fr", "en");

        REQUIRE(AppDictionary::Get().Initialise());
    }

    ~FFormsTestBed()
    {
        ForgetSessions();

        AppDictionary::Get().Close();
    }

    static void ForgetSessions()
    {
        AppState::Get().IntensiveLearningCheckpoints.clear();
        AppState::Get().RepetitionCheckpoint.clear();
    }
};

static FTCard MakeStoredCard(const std::string& Word, const std::string& Translation = "translation",
                             const std::string& Kind = FormsKindVerb)
{
    for (const FTDictionaryEntry& Entry : AppDictionary::Get().GetRepository().LookUp(Word))
    {
        if (std::optional<FTCard> Card = MakeFormsCard(Kind, Entry, "fr", Translation))
        {
            return *Card;
        }
    }

    FAIL("The dictionary has no clean forms for " << Word);

    return {};
}

struct FSeededForms
{
    QString TrainingId;
    QStringList CardIds;
};

static FSeededForms SeedForms(const std::string& Kind, const std::vector<std::string>& Words)
{
    std::vector<FTCard> Cards;
    FSeededForms Result;

    for (const std::string& Word : Words)
    {
        Cards.push_back(MakeStoredCard(Word, "translation", Kind));
        Result.CardIds.append(QString::fromStdString(Cards.back().Id));
    }

    REQUIRE(AppDatabase::Get().GetCardRepository().AddFormsCards(Cards));

    Result.TrainingId = QString::fromStdString(MakeFormsTrainingId(GetFormsTraining(Cards.front().Id), "fr", "en"));
    REQUIRE_FALSE(Result.TrainingId.isEmpty());

    return Result;
}

static FSeededForms SeedVerbs(const std::vector<std::string>& Verbs)
{
    return SeedForms(FormsKindVerb, Verbs);
}

static void MarkLearned(const QStringList& CardIds)
{
    std::vector<std::string> Ids;

    for (const QString& CardId : CardIds)
    {
        Ids.push_back(CardId.toStdString());
    }

    REQUIRE(AppDatabase::Get().GetCardRepository().MarkFormsCardsLearned(Ids, GetTodayDayNumber()));
}

static bool IsLearned(const QString& CardId)
{
    const std::optional<FTCardHandle> Loaded = AppDatabase::Get().GetCardRepository().LoadFormsCard(CardId.toStdString());
    REQUIRE(Loaded.has_value());

    return IsCompleted(**Loaded);
}

static int FindFirstUnmatched(const QList<bool>& Matched)
{
    for (int Row = 0; Row < Matched.size(); ++Row)
    {
        if (!Matched[Row])
        {
            return Row;
        }
    }

    return -1;
}

static QString ReadTableCell(const QVariantList& Table, int Column, int Row, const QString& Field)
{
    return Table[Row].toMap().value("cells").toList()[Column].toMap().value(Field).toString();
}

static QSet<QString> ToSet(const QStringList& Items)
{
    return QSet<QString>(Items.begin(), Items.end());
}

static void MatchScreen(SpeedMatchController& Match)
{
    while (Match.IsAwaitingAnswer())
    {
        const int LeftRow = FindFirstUnmatched(Match.GetLeftMatched());
        REQUIRE(LeftRow >= 0);

        bool bMatched = false;

        for (int RightRow = 0; RightRow < Match.GetRightItems().size() && !bMatched; ++RightRow)
        {
            if (Match.GetRightMatched()[RightRow])
            {
                continue;
            }

            Match.tapLeft(LeftRow);
            Match.tapRight(RightRow);

            bMatched = Match.GetLeftMatched()[LeftRow];
        }

        REQUIRE(bMatched);
    }

    Match.advance();
}

static QString ReadGapWord(const WrittenProductionController& Written)
{
    const QString GapMark = QStringLiteral("___");
    const QString Prompt = Written.GetPrompt();
    const QString Sentence = Written.GetAnswer();

    return Sentence.mid(Prompt.indexOf(GapMark), Sentence.size() - (Prompt.size() - GapMark.size()));
}

static QString ReadExpectedAnswer(const WrittenProductionController& Written)
{
    return Written.IsSentencePrompt() ? ReadGapWord(Written) : Written.GetAnswer();
}

static void AnswerActiveController(ExerciseControllerBase* Active)
{
    REQUIRE(Active != nullptr);

    if (auto* Flashcard = qobject_cast<FlashcardController*>(Active))
    {
        Flashcard->next();

        return;
    }

    if (auto* Written = qobject_cast<WrittenProductionController*>(Active))
    {
        Written->submitAnswer(ReadExpectedAnswer(*Written));
        Written->advance();

        return;
    }

    if (auto* Match = qobject_cast<SpeedMatchController*>(Active))
    {
        MatchScreen(*Match);

        return;
    }

    FAIL("Unexpected controller type in a forms session");
}

static void CompleteStage(StageSessionControllerBase& Session, int Stage)
{
    constexpr int MaximumSteps = 2000;

    int Steps = 0;

    while (!Session.IsFinished() && Session.GetStage() == Stage)
    {
        AnswerActiveController(Session.GetActiveController());

        REQUIRE(++Steps < MaximumSteps);
    }
}

static void CompleteStagesUpTo(StageSessionControllerBase& Session, int LastStage)
{
    for (int Stage = Session.GetStage(); Stage <= LastStage; ++Stage)
    {
        CompleteStage(Session, Stage);
    }
}

static QStringList ReadVerbForms(const std::string& Verb)
{
    QStringList Forms;

    for (const FTCardHandle& Form : AppCards::ExpandForms({MakeCardHandle(MakeStoredCard(Verb))}))
    {
        const std::optional<FTWordForm> Read = ReadWordForm(*Form);
        REQUIRE(Read.has_value());

        Forms.append(QString::fromStdString(Read->Form));
    }

    return Forms;
}

static QVariantMap FindTenseEntry(const QVariantList& Tenses, const QString& Key)
{
    for (const QVariant& Entry : Tenses)
    {
        if (Entry.toMap().value("key").toString() == Key)
        {
            return Entry.toMap();
        }
    }

    return {};
}

static std::string AddWord(const std::string& Headword, const std::string& Translation,
                           const std::string& PartOfSpeech)
{
    FTCard Word;
    Word.TargetWord = Headword;
    Word.Translation = Translation;
    Word.PartOfSpeech = PartOfSpeech;

    if (PartOfSpeech == "noun")
    {
        Word.Gender = GenderMasculine;
    }

    const std::optional<std::string> WordId = AppDatabase::Get().GetCardRepository().AddCard(Word);
    REQUIRE(WordId.has_value());

    return *WordId;
}

static void StudyWord(const std::string& Headword, const std::string& Translation,
                      const std::string& PartOfSpeech)
{
    const std::string WordId = AddWord(Headword, Translation, PartOfSpeech);

    REQUIRE(AppDatabase::Get().GetCardRepository().PromoteCards({WordId}));
}

static QString GetTrainingId(const char* Training)
{
    return QString::fromStdString(MakeFormsTrainingId(Training, "fr", "en"));
}

TEST_CASE("Expanding a stored verb reads its six forms from the dictionary", "[conjugation]")
{
    FFormsTestBed Bed;

    const std::vector<FTCardHandle> Forms = AppCards::ExpandForms({MakeCardHandle(MakeStoredCard("vouloir"))});

    REQUIRE(Forms.size() == 6);
    CHECK(Forms[0]->TargetWord == "je veux");
    CHECK(Forms[1]->TargetWord == "tu veux");
    CHECK(Forms[2]->TargetWord == "il veut");
    CHECK(Forms[3]->TargetWord == "nous voulons");
    CHECK(Forms[4]->TargetWord == "vous voulez");
    CHECK(Forms[5]->TargetWord == "ils veulent");

    for (const FTCardHandle& Form : Forms)
    {
        CHECK(Form->Translation == "vouloir");
        CHECK(IsFormsItemId(Form->Id));
    }
}

TEST_CASE("Expansion passes a word card through and leaves an unknown verb out", "[conjugation]")
{
    FFormsTestBed Bed;

    FTCard Word;
    Word.Id = "some-card";
    Word.TargetWord = "chien";
    Word.Translation = "dog";

    FTCard Unknown;
    Unknown.Id = "conjugation-fr-zzzzir-present";
    Unknown.TargetWord = "zzzzir";

    const FTCardHandle WordHandle = MakeCardHandle(Word);

    const std::vector<FTCardHandle> Expanded = AppCards::ExpandForms({WordHandle, MakeCardHandle(Unknown)});

    REQUIRE(Expanded.size() == 1);
    CHECK(Expanded[0].get() == WordHandle.get());
}

TEST_CASE("The ending is marked as a link after the stem", "[conjugation]")
{
    FFormsTestBed Bed;

    const std::vector<FTCardHandle> Vouloir = AppCards::ExpandForms({MakeCardHandle(MakeStoredCard("vouloir"))});
    REQUIRE(Vouloir.size() == 6);

    CHECK(AppCards::MarkFormWord(*Vouloir[3]) == "nous voul<a href=\"ending\">ons</a>");
    CHECK(AppCards::MarkFormWord(*Vouloir[0]) == "je veu<a href=\"ending\">x</a>");

    const std::vector<FTCardHandle> Aimer = AppCards::ExpandForms({MakeCardHandle(MakeStoredCard("aimer"))});
    REQUIRE(Aimer.size() == 6);

    CHECK(AppCards::MarkFormWord(*Aimer[0]) == "j'aim<a href=\"ending\">e</a>");

    FTCard Word;
    Word.Id = "some-card";
    Word.TargetWord = "chien";

    CHECK(AppCards::MarkFormWord(Word).isEmpty());
}

TEST_CASE("A verb's table holds the singular persons in one column and the plural ones in the other", "[conjugation]")
{
    FFormsTestBed Bed;

    const QVariantList Table = AppCards::BuildFormsTable(MakeStoredCard("vouloir"));

    REQUIRE(Table.size() == 3);

    CHECK(ReadTableCell(Table, 0, 0, "cue") == "je");
    CHECK(ReadTableCell(Table, 0, 1, "cue") == "tu");
    CHECK(ReadTableCell(Table, 0, 2, "cue") == "il/elle");
    CHECK(ReadTableCell(Table, 1, 0, "cue") == "nous");
    CHECK(ReadTableCell(Table, 1, 1, "cue") == "vous");
    CHECK(ReadTableCell(Table, 1, 2, "cue") == "ils/elles");

    for (int Column = 0; Column < 2; ++Column)
    {
        for (int Row = 0; Row < 3; ++Row)
        {
            CHECK(ReadTableCell(Table, Column, Row, "form").contains(EndingLink));
        }
    }

    CHECK(ReadTableCell(Table, 0, 0, "form") == "veu<a href=\"ending\">x</a>");
}

TEST_CASE("An adjective's table puts the pronoun of its gender and number before each form", "[conjugation][forms]")
{
    FFormsTestBed Bed;

    const QVariantList Table = AppCards::BuildFormsTable(MakeStoredCard("beau", "beautiful", FormsKindAdjective));

    REQUIRE(Table.size() == 2);

    CHECK(ReadTableCell(Table, 0, 0, "cue") == "il");
    CHECK(ReadTableCell(Table, 0, 1, "cue") == "elle");
    CHECK(ReadTableCell(Table, 1, 0, "cue") == "ils");
    CHECK(ReadTableCell(Table, 1, 1, "cue") == "elles");
    CHECK(ReadTableCell(Table, 0, 0, "form") == "beau");
    CHECK(ReadTableCell(Table, 0, 1, "form") == "be<a href=\"ending\">lle</a>");
    CHECK(ReadTableCell(Table, 1, 0, "form") == "beau<a href=\"ending\">x</a>");
    CHECK(ReadTableCell(Table, 1, 1, "form") == "be<a href=\"ending\">lles</a>");
}

TEST_CASE("A session card loads back from its id, whole or as one form", "[conjugation]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir"});
    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    const std::string StoredId = Seeded.CardIds[0].toStdString();

    const std::optional<FTCardHandle> Form = AppCards::LoadSessionCard(CardRepository, StoredId + "#1-2");
    REQUIRE(Form.has_value());
    CHECK((*Form)->TargetWord == "nous voulons");

    const std::optional<FTCardHandle> Stored = AppCards::LoadSessionCard(CardRepository, StoredId);
    REQUIRE(Stored.has_value());
    CHECK((*Stored)->TargetWord == "vouloir");

    CHECK_FALSE(AppCards::LoadSessionCard(CardRepository, "conjugation-fr-dire-present#1-1").has_value());
    CHECK_FALSE(AppCards::LoadSessionCard(CardRepository, StoredId + "#9-9").has_value());
}

TEST_CASE("A forms card is not stored among the word cards", "[conjugation]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir"});
    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    CHECK_FALSE(CardRepository.LoadCard(Seeded.CardIds[0].toStdString()).has_value());
    CHECK(CardRepository.CountCards() == 0);
    CHECK(CardRepository.LoadFormsCard(Seeded.CardIds[0].toStdString()).has_value());
}

TEST_CASE("A conjugation training shows each verb once with its table", "[conjugation]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir", "dire"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CHECK(Controller.IsFormsSession());
    CHECK(Controller.GetSessionWordCount() == 2);
    REQUIRE(Controller.GetStage() == StudyStage);

    auto* Study = qobject_cast<FlashcardController*>(Controller.GetActiveController());
    REQUIRE(Study != nullptr);
    CHECK(Study->GetMaxSessionCount() == 2);

    for (const QString& Verb : {QString("vouloir"), QString("dire")})
    {
        REQUIRE(Controller.GetStage() == StudyStage);
        REQUIRE(Study->GetCurrentCard() != nullptr);

        CHECK(QString::fromStdString(Study->GetCurrentCard()->TargetWord) == Verb);
        CHECK(Study->GetPrompt() == Verb);
        CHECK(Study->GetAnswer() == "translation");
        REQUIRE(Study->GetForms().size() == 3);
        CHECK(ReadTableCell(Study->GetForms(), 0, 0, "cue") == "je");
        CHECK(Study->GetTense() == "présent");

        Study->next();
    }

    CHECK(Controller.GetStage() == MatchStage);

    Controller.endSession();
}

TEST_CASE("A vocabulary session does not start a forms session", "[conjugation]")
{
    FFormsTestBed Bed;

    const std::string CardId = AddWord("chien", "dog", "noun");

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QStringLiteral("vocabulary"), {QString::fromStdString(CardId)}));

    CHECK_FALSE(Controller.IsFormsSession());
    CHECK(Controller.GetSessionWordCount() == 1);

    auto* Flashcard = qobject_cast<FlashcardController*>(Controller.GetActiveController());
    REQUIRE(Flashcard != nullptr);
    CHECK(Flashcard->GetForms().isEmpty());
    CHECK(Flashcard->GetTense().isEmpty());

    Controller.endSession();
}

TEST_CASE("A forms training has three stages: the table, the match, the typing", "[conjugation]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    REQUIRE(Controller.GetStage() == StudyStage);
    CHECK(qobject_cast<FlashcardController*>(Controller.GetActiveController()) != nullptr);

    CompleteStage(Controller, StudyStage);

    REQUIRE(Controller.GetStage() == MatchStage);
    CHECK(qobject_cast<SpeedMatchController*>(Controller.GetActiveController()) != nullptr);
    CHECK(Controller.GetActiveController()->GetTense() == "présent");

    CompleteStage(Controller, MatchStage);

    REQUIRE(Controller.GetStage() == TypingStage);
    CHECK(qobject_cast<WrittenProductionController*>(Controller.GetActiveController()) != nullptr);
    CHECK(Controller.GetActiveController()->GetTense() == "présent");

    CompleteStage(Controller, TypingStage);

    CHECK(Controller.IsFinished());
}

TEST_CASE("Typing shows the table of the word it asks", "[conjugation][forms]")
{
    FFormsTestBed Bed;

    const FSeededForms Verbs = SeedVerbs({"vouloir"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Verbs.TrainingId, Verbs.CardIds));

    CompleteStagesUpTo(Controller, MatchStage);
    REQUIRE(Controller.GetStage() == TypingStage);

    auto* Written = qobject_cast<WrittenProductionController*>(Controller.GetActiveController());
    REQUIRE(Written != nullptr);

    REQUIRE(Written->GetForms().size() == 3);
    CHECK(ReadTableCell(Written->GetForms(), 0, 0, "cue") == "je");
    CHECK(ReadTableCell(Written->GetForms(), 0, 0, "form") == "veu<a href=\"ending\">x</a>");

    Controller.endSession();
}

TEST_CASE("The match stage pairs each person with a form, and equal forms are interchangeable", "[conjugation]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStage(Controller, StudyStage);
    REQUIRE(Controller.GetStage() == MatchStage);

    auto* Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController());
    REQUIRE(Match != nullptr);

    const QStringList Left = Match->GetLeftItems();
    const QStringList Right = Match->GetRightItems();

    REQUIRE(Left.size() == 6);
    REQUIRE(Right.size() == 6);

    CHECK(QSet<QString>(Left.begin(), Left.end()) == QSet<QString>{"je", "tu", "il/elle", "nous", "vous", "ils/elles"});
    CHECK(Left == QStringList{"je", "tu", "il/elle", "nous", "vous", "ils/elles"});
    CHECK(Right.count("veux") == 2);
    CHECK(QSet<QString>(Right.begin(), Right.end())
          == QSet<QString>{"veux", "veut", "voulons", "voulez", "veulent"});

    REQUIRE(Match->GetRightMarkups().size() == 6);

    for (const QString& Markup : Match->GetRightMarkups())
    {
        CHECK(Markup.contains(EndingLink));
    }

    Match->tapLeft(static_cast<int>(Left.indexOf("je")));
    Match->tapRight(static_cast<int>(Right.lastIndexOf("veux")));

    Match->tapLeft(static_cast<int>(Left.indexOf("tu")));
    Match->tapRight(static_cast<int>(Right.indexOf("veux")));

    CHECK(Match->GetLeftMatched()[Left.indexOf("je")]);
    CHECK(Match->GetLeftMatched()[Left.indexOf("tu")]);
    CHECK(Match->GetWrongPairCount() == 0);

    Match->tapLeft(static_cast<int>(Left.indexOf("il/elle")));
    Match->tapRight(static_cast<int>(Right.indexOf("voulons")));

    CHECK_FALSE(Match->GetLeftMatched()[Left.indexOf("il/elle")]);
    CHECK(Match->GetWrongPairCount() == 1);

    Controller.endSession();
}

TEST_CASE("The match screen names its verb, and keeps the name once the first row is matched", "[conjugation]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStage(Controller, StudyStage);
    REQUIRE(Controller.GetStage() == MatchStage);

    auto* Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController());
    REQUIRE(Match != nullptr);

    CHECK(Match->GetHeading() == QStringLiteral("vouloir"));

    const QStringList Left = Match->GetLeftItems();
    const QStringList Right = Match->GetRightItems();

    REQUIRE(Left.first() == QStringLiteral("je"));

    Match->tapLeft(0);
    Match->tapRight(static_cast<int>(Right.indexOf("veux")));

    REQUIRE(Match->GetLeftMatched()[0]);
    CHECK(Match->GetHeading() == QStringLiteral("vouloir"));

    Controller.endSession();
}

TEST_CASE("The match offers the table of the word on its screen", "[conjugation]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStage(Controller, StudyStage);
    REQUIRE(Controller.GetStage() == MatchStage);

    auto* Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController());
    REQUIRE(Match != nullptr);

    REQUIRE(Match->GetForms().size() == 3);
    CHECK(ReadTableCell(Match->GetForms(), 0, 0, "cue") == "je");
    CHECK(ReadTableCell(Match->GetForms(), 0, 0, "form") == "veu<a href=\"ending\">x</a>");

    const QStringList Right = Match->GetRightItems();

    Match->tapLeft(0);
    Match->tapRight(static_cast<int>(Right.indexOf("veux")));

    REQUIRE(Match->GetLeftMatched()[0]);
    CHECK(Match->GetForms().size() == 3);

    Controller.endSession();
}

TEST_CASE("Each verb gets one match screen of its own", "[conjugation]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"prendre", "dire"});

    const QStringList PrendreForms = ReadVerbForms("prendre");
    const QStringList DireForms = ReadVerbForms("dire");

    const QSet<QString> Prendre(PrendreForms.begin(), PrendreForms.end());
    const QSet<QString> Dire(DireForms.begin(), DireForms.end());

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStage(Controller, StudyStage);
    REQUIRE(Controller.GetStage() == MatchStage);

    int Screens = 0;

    while (!Controller.IsFinished() && Controller.GetStage() == MatchStage)
    {
        auto* Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController());
        REQUIRE(Match != nullptr);

        const QStringList Right = Match->GetRightItems();
        const QSet<QString> Shown(Right.begin(), Right.end());

        REQUIRE(Right.size() == 6);
        CHECK((Shown == Prendre || Shown == Dire));

        MatchScreen(*Match);

        REQUIRE(++Screens < 100);
    }

    CHECK(Screens == 2);
    CHECK(Controller.GetStage() == TypingStage);

    Controller.endSession();
}

TEST_CASE("Each form is answered right in two rounds, with or without the person, and a miss does not count",
          "[conjugation]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStagesUpTo(Controller, MatchStage);
    REQUIRE(Controller.GetStage() == TypingStage);

    QSet<QString> Asked;
    QSet<QString> AskedForms;
    int WrittenAnswers = 0;

    while (!Controller.IsFinished() && Controller.GetStage() == TypingStage)
    {
        REQUIRE(WrittenAnswers < 40);

        auto* Written = qobject_cast<WrittenProductionController*>(Controller.GetActiveController());
        REQUIRE(Written != nullptr);
        REQUIRE(Written->GetCurrentCard() != nullptr);

        const FTCard Card = *Written->GetCurrentCard();
        const std::optional<FTWordForm> Read = ReadWordForm(Card);
        REQUIRE(Read.has_value());

        Asked.insert(QString::fromStdString(Card.Id));
        AskedForms.insert(QString::fromStdString(GetFormsItemId(Card.Id)));

        CHECK(Written->GetPromptHeading() == "vouloir");
        CHECK(Written->GetAnswerHint() == "Type the form...");

        if (Written->IsSentencePrompt())
        {
            CHECK(ReadGapWord(*Written).compare(QString::fromStdString(Read->Form), Qt::CaseInsensitive) == 0);
        }
        else
        {
            CHECK(Written->GetPrompt() == QString::fromStdString(MakeFormPrompt(Read->SubjectLabel)));
        }

        if (WrittenAnswers == 0)
        {
            Written->submitAnswer(QString::fromStdString(Read->Form));
            CHECK(Written->WasLastAnswerCorrect());
        }
        else if (WrittenAnswers == 1)
        {
            Written->submitAnswer(QString::fromStdString(Card.TargetWord));
            CHECK(Written->WasLastAnswerCorrect());
        }
        else if (WrittenAnswers == 2)
        {
            Written->submitAnswer("vouloir");
            CHECK_FALSE(Written->WasLastAnswerCorrect());

            Written->submitAnswer(QString::fromStdString(Read->Form));
            CHECK_FALSE(Written->IsRetypePending());
        }
        else
        {
            Written->submitAnswer(QString::fromStdString(Read->Form));
            CHECK(Written->WasLastAnswerCorrect());
        }

        ++WrittenAnswers;

        Written->advance();
    }

    CHECK(WrittenAnswers == 13);
    CHECK(Asked.size() == 12);
    CHECK(AskedForms.size() == 6);
    CHECK(Controller.IsFinished());

    Controller.endSession();
}

TEST_CASE("Finishing a forms training marks each of its words learned and nothing else", "[conjugation]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir", "dire", "voir"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, {Seeded.CardIds[0], Seeded.CardIds[1]}));

    bool bSessionEnded = false;
    QObject::connect(&Controller, &IntensiveLearningController::sessionEnded, &Controller,
        [&bSessionEnded](const QString&) { bSessionEnded = true; });

    CompleteStagesUpTo(Controller, TypingStage);

    REQUIRE(Controller.IsFinished());
    CHECK(bSessionEnded);

    CHECK(IsLearned(Seeded.CardIds[0]));
    CHECK(IsLearned(Seeded.CardIds[1]));
    CHECK_FALSE(IsLearned(Seeded.CardIds[2]));

    CHECK(AppDatabase::Get().GetCardRepository().LoadFormsCards().size() == 3);
}

TEST_CASE("A forms training counts toward neither learned nor reviewed words", "[conjugation][forms]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir", "dire"});
    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStagesUpTo(Controller, TypingStage);
    REQUIRE(Controller.IsFinished());

    const FTDayNumber Today = GetTodayDayNumber();
    const std::vector<FTDailyStats> Stats = CardRepository.LoadDailyStats(Today, Today);

    CHECK(Stats.empty());
}

TEST_CASE("A word session still counts toward the daily word goal", "[conjugation][forms]")
{
    FFormsTestBed Bed;

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();


    QStringList CardIds;

    for (const char* Word : {"chien", "chat", "maison"})
    {
        CardIds.append(QString::fromStdString(AddWord(Word, std::string("translation of ") + Word, "")));
    }

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(QStringLiteral("vocabulary"), CardIds));

    for (int Stage = 1; Stage <= 5 && !Controller.IsFinished(); ++Stage)
    {
        Controller.completeStage();
    }

    REQUIRE(Controller.IsFinished());

    const FTDayNumber Today = GetTodayDayNumber();
    const std::vector<FTDailyStats> Stats = CardRepository.LoadDailyStats(Today, Today);

    REQUIRE(Stats.size() == 1);
    CHECK(Stats[0].LearnedCount == 3);
    CHECK(Stats[0].ReviewedCount == 0);
}

TEST_CASE("A parked forms training resumes at the same stage and still marks its verbs learned", "[conjugation][slow]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir", "dire"});

    {
        IntensiveLearningController Controller;
        REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

        CompleteStagesUpTo(Controller, MatchStage);
        REQUIRE(Controller.GetStage() == TypingStage);

        AnswerActiveController(Controller.GetActiveController());
    }

    IntensiveLearningController Resumed;
    REQUIRE(Resumed.resume(Seeded.TrainingId));

    CHECK(Resumed.IsFormsSession());
    CHECK(Resumed.GetStage() == TypingStage);
    CHECK(Resumed.GetSessionWordCount() == 2);
    CHECK(qobject_cast<WrittenProductionController*>(Resumed.GetActiveController()) != nullptr);

    CompleteStagesUpTo(Resumed, TypingStage);

    REQUIRE(Resumed.IsFinished());
    CHECK(IsLearned(Seeded.CardIds[0]));
    CHECK(IsLearned(Seeded.CardIds[1]));
    CHECK_FALSE(Resumed.hasSessionFor(Seeded.TrainingId));
}

TEST_CASE("The two rounds of a form never come one after the other", "[conjugation][slow]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir"});

    for (int Run = 0; Run < 5; ++Run)
    {
        IntensiveLearningController Controller;
        REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

        CompleteStagesUpTo(Controller, MatchStage);
        REQUIRE(Controller.GetStage() == TypingStage);

        QStringList Order;

        while (!Controller.IsFinished() && Controller.GetStage() == TypingStage)
        {
            REQUIRE(Order.size() < 40);

            auto* Written = qobject_cast<WrittenProductionController*>(Controller.GetActiveController());
            REQUIRE(Written != nullptr);
            REQUIRE(Written->GetCurrentCard() != nullptr);

            Order.append(QString::fromStdString(GetFormsItemId(Written->GetCurrentCard()->Id)));

            AnswerActiveController(Written);
        }

        REQUIRE(Order.size() == 12);

        for (qsizetype Index = 1; Index < Order.size(); ++Index)
        {
            CHECK(Order[Index] != Order[Index - 1]);
        }

        Controller.endSession();
    }
}

TEST_CASE("Typing asks every form of one word, a miss included, before the next word, then each word again",
          "[conjugation][slow]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir", "dire", "prendre"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStagesUpTo(Controller, MatchStage);
    REQUIRE(Controller.GetStage() == TypingStage);

    QStringList Words;
    QList<int> Rounds;
    QList<int> Sizes;

    while (!Controller.IsFinished() && Controller.GetStage() == TypingStage)
    {
        REQUIRE(Sizes.size() < 20);

        auto* Written = qobject_cast<WrittenProductionController*>(Controller.GetActiveController());
        REQUIRE(Written != nullptr);
        REQUIRE(Written->GetCurrentCard() != nullptr);

        const std::string CardId = Written->GetCurrentCard()->Id;
        const QString Word = QString::fromStdString(GetFormsCardId(CardId));
        const int Round = CardId == MakeFormsRoundId(CardId, 1) ? 1 : 2;

        if (Words.isEmpty() || Words.back() != Word || Rounds.back() != Round)
        {
            Words.append(Word);
            Rounds.append(Round);
            Sizes.append(0);
        }

        ++Sizes.back();

        const QString Expected = ReadExpectedAnswer(*Written);

        if (Sizes.size() == 1 && Sizes.back() == 1)
        {
            Written->submitAnswer(QStringLiteral("xyz"));
            REQUIRE_FALSE(Written->WasLastAnswerCorrect());
        }

        Written->submitAnswer(Expected);
        Written->advance();
    }

    REQUIRE(Sizes == QList<int>{7, 6, 6, 6, 6, 6});
    CHECK(Rounds == QList<int>{1, 1, 1, 2, 2, 2});
    CHECK(ToSet(Words.mid(0, 3)).size() == 3);
    CHECK(ToSet(Words.mid(0, 3)) == ToSet(Words.mid(3)));
    CHECK(Words[3] != Words[2]);
    CHECK(Controller.IsFinished());

    Controller.endSession();
}

TEST_CASE("A form is asked in a dictionary sentence with the gap on the form", "[conjugation]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStagesUpTo(Controller, MatchStage);
    REQUIRE(Controller.GetStage() == TypingStage);

    auto* Written = qobject_cast<WrittenProductionController*>(Controller.GetActiveController());
    REQUIRE(Written != nullptr);
    REQUIRE(Written->GetCurrentCard() != nullptr);
    REQUIRE(Written->IsSentencePrompt());

    const std::optional<FTWordForm> Read = ReadWordForm(*Written->GetCurrentCard());
    REQUIRE(Read.has_value());

    const QString Sentence = Written->GetAnswer();

    CHECK(Written->GetPrompt().contains(QStringLiteral("___")));
    CHECK(ReadGapWord(*Written).compare(QString::fromStdString(Read->Form), Qt::CaseInsensitive) == 0);

    Written->submitAnswer(QString::fromStdString(Read->Form));

    CHECK(Written->WasLastAnswerCorrect());
    CHECK(Written->GetLastRevealedAnswer() == Sentence);
    CHECK_FALSE(Written->GetLastCardTranslation().isEmpty());
    CHECK(Written->GetLastCardTranslation() != QString::fromStdString(Read->Meaning));

    Controller.endSession();
}

TEST_CASE("A parked typing stage resumes on the same sentence", "[conjugation][slow]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir", "dire"});

    QString ParkedCardId;
    QString ParkedPrompt;

    {
        IntensiveLearningController Controller;
        REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

        CompleteStagesUpTo(Controller, MatchStage);
        REQUIRE(Controller.GetStage() == TypingStage);

        AnswerActiveController(Controller.GetActiveController());

        auto* Written = qobject_cast<WrittenProductionController*>(Controller.GetActiveController());
        REQUIRE(Written != nullptr);
        REQUIRE(Written->GetCurrentCard() != nullptr);

        ParkedCardId = QString::fromStdString(Written->GetCurrentCard()->Id);
        ParkedPrompt = Written->GetPrompt();
    }

    IntensiveLearningController Resumed;
    REQUIRE(Resumed.resume(Seeded.TrainingId));
    REQUIRE(Resumed.GetStage() == TypingStage);

    auto* Written = qobject_cast<WrittenProductionController*>(Resumed.GetActiveController());
    REQUIRE(Written != nullptr);
    REQUIRE(Written->GetCurrentCard() != nullptr);

    CHECK(QString::fromStdString(Written->GetCurrentCard()->Id) == ParkedCardId);
    CHECK(Written->GetPrompt() == ParkedPrompt);

    Resumed.endSession();
}

TEST_CASE("A forms training gives each word its own match screen, cues against forms",
          "[conjugation][forms]")
{
    FFormsTestBed Bed;

    const FSeededForms Nouns = SeedForms(FormsKindNoun, {"journal"});
    const FSeededForms Adjectives = SeedForms(FormsKindAdjective, {"beau"});

    CHECK(Nouns.TrainingId == Adjectives.TrainingId);
    CHECK(Nouns.TrainingId == GetTrainingId(FormsTrainingAgreement));

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Nouns.TrainingId, Nouns.CardIds + Adjectives.CardIds));

    CHECK(Controller.IsFormsSession());
    CHECK(Controller.GetSessionWordCount() == 2);

    auto* Study = qobject_cast<FlashcardController*>(Controller.GetActiveController());
    REQUIRE(Study != nullptr);
    REQUIRE(Study->GetForms().size() == 1);
    CHECK(ReadTableCell(Study->GetForms(), 0, 0, "cue") == "un");
    CHECK(ReadTableCell(Study->GetForms(), 1, 0, "cue") == "des");
    CHECK(ReadTableCell(Study->GetForms(), 0, 0, "form") == "journal");
    CHECK(ReadTableCell(Study->GetForms(), 1, 0, "form") == "journa<a href=\"ending\">ux</a>");
    CHECK(Study->GetTense().isEmpty());

    CompleteStage(Controller, StudyStage);
    REQUIRE(Controller.GetStage() == MatchStage);

    const QSet<QString> NounForms{"journal", "journaux"};

    int Screens = 0;

    while (!Controller.IsFinished() && Controller.GetStage() == MatchStage)
    {
        auto* Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController());
        REQUIRE(Match != nullptr);

        const QStringList Left = Match->GetLeftItems();
        const QStringList Right = Match->GetRightItems();
        const bool bNoun = ToSet(Right) == NounForms;

        CHECK(Left == (bNoun ? QStringList{"un", "des"}
                             : QStringList{"il", "elle", "ils", "elles"}));

        const QString Plural = bNoun ? "journaux" : "beaux";
        const QString SingularCue = bNoun ? "un" : "il";
        const QString PluralCue = bNoun ? "des" : "ils";

        CHECK(Match->GetRightMarkups()[Right.indexOf(Plural)].contains(EndingLink));

        if (Screens == 0)
        {
            Match->tapLeft(static_cast<int>(Left.indexOf(SingularCue)));
            Match->tapRight(static_cast<int>(Right.indexOf(Plural)));

            CHECK_FALSE(Match->GetLeftMatched()[Left.indexOf(SingularCue)]);
            CHECK(Match->GetWrongPairCount() == 1);

            Match->tapLeft(static_cast<int>(Left.indexOf(PluralCue)));
            Match->tapRight(static_cast<int>(Right.indexOf(Plural)));

            CHECK(Match->GetLeftMatched()[Left.indexOf(PluralCue)]);
        }

        MatchScreen(*Match);

        REQUIRE(++Screens < 100);
    }

    CHECK(Screens == 2);

    CompleteStagesUpTo(Controller, TypingStage);

    REQUIRE(Controller.IsFinished());
    CHECK(IsLearned(Nouns.CardIds[0]));
    CHECK(IsLearned(Adjectives.CardIds[0]));
}

TEST_CASE("Typing leaves out the form that is the word itself", "[conjugation][forms]")
{
    FFormsTestBed Bed;

    const FSeededForms Nouns = SeedForms(FormsKindNoun, {"journal"});
    const FSeededForms Adjectives = SeedForms(FormsKindAdjective, {"beau"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Nouns.TrainingId, Nouns.CardIds + Adjectives.CardIds));

    CompleteStagesUpTo(Controller, MatchStage);
    REQUIRE(Controller.GetStage() == TypingStage);

    QSet<QString> AskedForms;
    QSet<QString> Cues;
    int Answers = 0;

    while (!Controller.IsFinished() && Controller.GetStage() == TypingStage)
    {
        REQUIRE(++Answers < 40);

        auto* Written = qobject_cast<WrittenProductionController*>(Controller.GetActiveController());
        REQUIRE(Written != nullptr);
        REQUIRE(Written->GetCurrentCard() != nullptr);

        const std::optional<FTWordForm> Read = ReadWordForm(*Written->GetCurrentCard());
        REQUIRE(Read.has_value());

        AskedForms.insert(QString::fromStdString(GetFormsItemId(Written->GetCurrentCard()->Id)));
        Cues.insert(QString::fromStdString(Read->SubjectLabel));

        AnswerActiveController(Written);
    }

    CHECK(AskedForms.size() == 4);
    CHECK(Cues == QSet<QString>{"des", "elle", "ils", "elles"});
    CHECK(Controller.IsFinished());

    Controller.endSession();
}

TEST_CASE("An adjective form is revealed in its sentence, or with its pronoun as it is spoken", "[forms]")
{
    FFormsTestBed Bed;

    const FSeededForms Adjectives = SeedForms(FormsKindAdjective, {"beau"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Adjectives.TrainingId, Adjectives.CardIds));

    CompleteStagesUpTo(Controller, MatchStage);
    REQUIRE(Controller.GetStage() == TypingStage);

    auto* Written = qobject_cast<WrittenProductionController*>(Controller.GetActiveController());
    REQUIRE(Written != nullptr);
    REQUIRE(Written->GetCurrentCard() != nullptr);

    const FTCard Card = *Written->GetCurrentCard();
    const std::optional<FTWordForm> Read = ReadWordForm(Card);
    REQUIRE(Read.has_value());

    const QString Spoken = QString::fromStdString(Card.TargetWord);
    const QString Pronoun = Spoken.left(Spoken.size() - static_cast<int>(QString::fromStdString(Read->Form).size()));
    REQUIRE_FALSE(Pronoun.trimmed().isEmpty());

    const bool bSentence = Written->IsSentencePrompt();
    const QString Sentence = Written->GetAnswer();

    Written->submitAnswer(QString::fromStdString(Read->Form));

    CHECK(Written->GetLastRevealedAnswer() == (bSentence ? Sentence : Spoken));
    CHECK(Written->GetLastWordMarkup().startsWith(Pronoun));
    CHECK(Written->GetLastWordMarkup().contains(EndingLink));

    Controller.endSession();
}

TEST_CASE("An adjective's match lists the pronouns on the left, and two forms spelled alike take either",
          "[conjugation][forms]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedForms(FormsKindAdjective, {"heureux"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStage(Controller, StudyStage);

    auto* Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController());
    REQUIRE(Match != nullptr);

    const QStringList Left = Match->GetLeftItems();
    const QStringList Right = Match->GetRightItems();

    CHECK(Left == QStringList{"il", "elle", "ils", "elles"});
    CHECK(Right.count("heureux") == 2);
    CHECK(ToSet(Right) == QSet<QString>{"heureux", "heureuse", "heureuses"});

    Match->tapLeft(static_cast<int>(Left.indexOf("il")));
    Match->tapRight(static_cast<int>(Right.lastIndexOf("heureux")));

    Match->tapLeft(static_cast<int>(Left.indexOf("ils")));
    Match->tapRight(static_cast<int>(Right.indexOf("heureux")));

    CHECK(Match->GetLeftMatched()[Left.indexOf("il")]);
    CHECK(Match->GetLeftMatched()[Left.indexOf("ils")]);
    CHECK(Match->GetWrongPairCount() == 0);

    Controller.endSession();
}

TEST_CASE("A training takes the waiting words first, then studied words, and nothing when it cannot fill up",
          "[conjugation][forms]")
{
    FFormsTestBed Bed;

    DailySessionController Session;

    const QString Conjugation = Session.formsTrainingId("conjugation");
    const QString Forms = Session.formsTrainingId("forms");

    CHECK(Conjugation == GetTrainingId(FormsTrainingConjugation));
    CHECK(Forms == GetTrainingId(FormsTrainingAgreement));
    CHECK(Session.formsTrainingId("adverbs").isEmpty());

    CHECK(Session.formsTrainingSize(Conjugation) == 4);
    CHECK(Session.formsTrainingSize(Forms) == 8);

    SeedVerbs({"vouloir", "dire", "voir"});

    CHECK(Session.collectFormsCards(Conjugation).isEmpty());
    CHECK(Session.collectFormsCards(Forms).isEmpty());

    StudyWord("prendre", "to grab", "verb");
    StudyWord("faire", "do", "verb");

    const QStringList Taken = Session.collectFormsCards(Conjugation);

    REQUIRE(Taken.size() == 4);
    CHECK(Taken.mid(0, 3) == QStringList{"conjugation-fr-vouloir-present", "conjugation-fr-dire-present",
                                         "conjugation-fr-voir-present"});
    CHECK((Taken[3] == "conjugation-fr-prendre-present" || Taken[3] == "conjugation-fr-faire-present"));

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    CHECK(CardRepository.LoadFormsCards().size() == 4);
    CHECK(Session.formsWordCount(Conjugation) == 5);
}

TEST_CASE("A conjugation training takes the most frequent irregular verbs first, and a derived verb after its base",
          "[conjugation][forms]")
{
    FFormsTestBed Bed;

    StudyWord("aimer", "love", "verb");
    StudyWord("comprendre", "understand", "verb");
    StudyWord("lire", "read", "verb");
    StudyWord("dormir", "sleep", "verb");
    StudyWord("prendre", "take", "verb");
    StudyWord("être", "be", "verb");

    DailySessionController Session;

    CHECK(Session.collectFormsCards(GetTrainingId(FormsTrainingConjugation))
          == QStringList{"conjugation-fr-être-present", "conjugation-fr-prendre-present",
                         "conjugation-fr-dormir-present", "conjugation-fr-lire-present"});
}

TEST_CASE("A forms card shows the translation of the learner's own card, as it is now", "[conjugation][forms]")
{
    FFormsTestBed Bed;

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    const std::string WordId = AddWord("vouloir", "to long for", "verb");
    REQUIRE(CardRepository.PromoteCards({WordId}));

    StudyWord("dire", "say", "verb");
    StudyWord("voir", "see", "verb");
    StudyWord("prendre", "take", "verb");

    DailySessionController Session;

    REQUIRE(Session.collectFormsCards(GetTrainingId(FormsTrainingConjugation)).size() == 4);

    const std::optional<FTCardHandle> Stored = CardRepository.LoadFormsCard("conjugation-fr-vouloir-present");
    REQUIRE(Stored.has_value());
    CHECK((*Stored)->Translation == "to long for");

    REQUIRE(CardRepository.UpdateCardContent(WordId, "vouloir", "want, wish", std::nullopt, "verb", ""));

    const std::optional<FTCardHandle> Reloaded = CardRepository.LoadFormsCard("conjugation-fr-vouloir-present");
    REQUIRE(Reloaded.has_value());
    CHECK((*Reloaded)->Translation == "want, wish");
}

TEST_CASE("The forms lookup runs once per word, not on a card write, and again when the dictionary reopens",
          "[forms][appcards]")
{
    FFormsTestBed Bed;

    StudyWord("beau", "beautiful", "adj");
    StudyWord("grand", "big", "adj");

    DailySessionController Session;
    const QString Forms = GetTrainingId(FormsTrainingAgreement);

    const int Before = AppCards::GetFormsLookupCount();

    Session.formsWordCount(Forms);

    const int Studied = AppCards::GetFormsLookupCount() - Before;

    CHECK(Studied == 2);

    Session.formsWordCount(Forms);
    Session.canStartFormsTraining(Forms);

    CHECK(AppCards::GetFormsLookupCount() - Before == Studied);

    StudyWord("petit", "small", "adj");

    Session.formsWordCount(Forms);

    CHECK(AppCards::GetFormsLookupCount() - Before == Studied + 1);

    AppDictionary::Get().Close();

    REQUIRE(AppDictionary::Get().Initialise());

    const int Reopened = AppCards::GetFormsLookupCount();

    Session.formsWordCount(Forms);

    CHECK(AppCards::GetFormsLookupCount() - Reopened == Studied + 1);

    Session.formsWordCount(Forms);

    CHECK(AppCards::GetFormsLookupCount() - Reopened == Studied + 1);
}

TEST_CASE("A training is open once waiting and studied words fill a session", "[conjugation][forms]")
{
    FFormsTestBed Bed;

    DailySessionController Session;

    const QString Conjugation = GetTrainingId(FormsTrainingConjugation);
    const QString Forms = GetTrainingId(FormsTrainingAgreement);

    CHECK(Session.formsWordCount(Conjugation) == 0);
    CHECK_FALSE(Session.canStartFormsTraining(Conjugation));
    CHECK_FALSE(Session.canStartFormsTraining(Forms));

    SeedVerbs({"vouloir", "dire"});

    StudyWord("dire", "say", "verb");
    StudyWord("voir", "see", "verb");
    AddWord("faire", "do", "verb");

    CHECK(Session.formsWordCount(Conjugation) == 3);
    CHECK_FALSE(Session.canStartFormsTraining(Conjugation));

    StudyWord("prendre", "take", "verb");

    CHECK(Session.formsWordCount(Conjugation) == 4);
    CHECK(Session.canStartFormsTraining(Conjugation));

    StudyWord("beau", "beautiful", "adj");
    StudyWord("grand", "big", "adj");
    StudyWord("journal", "newspaper", "noun");

    CHECK(Session.formsWordCount(Forms) == 2);
    CHECK_FALSE(Session.canStartFormsTraining(Forms));
}

TEST_CASE("A learned word no longer waits for a training", "[conjugation][forms]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir", "dire"});
    MarkLearned({Seeded.CardIds[0]});

    DailySessionController Session;

    CHECK(Session.formsWordCount(Seeded.TrainingId) == 1);
}

TEST_CASE("Only the present is open until four verbs have been trained in it", "[conjugation][tenses]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir", "dire", "voir", "prendre"});

    DailySessionController Session;

    QVariantList Tenses = Session.formsTenses(Seeded.TrainingId);

    REQUIRE(Tenses.size() == 4);
    CHECK(Tenses[0].toMap().value("key").toString() == "present");
    CHECK(Tenses[0].toMap().value("label").toString() == "présent");
    CHECK(Tenses[0].toMap().value("enabled").toBool());
    CHECK(Tenses[0].toMap().value("count").toInt() == 4);
    CHECK(Tenses[0].toMap().value("wanted").toInt() == 4);

    for (const QString& Key : {QString("imperfect"), QString("future"), QString("perfect")})
    {
        const QVariantMap Entry = FindTenseEntry(Tenses, Key);

        CHECK_FALSE(Entry.value("enabled").toBool());
        CHECK(Entry.value("count").toInt() == 0);
    }

    CHECK(Session.collectTenseCards(Seeded.TrainingId, "imperfect").isEmpty());

    MarkLearned({Seeded.CardIds[0], Seeded.CardIds[1], Seeded.CardIds[2]});

    Tenses = Session.formsTenses(Seeded.TrainingId);

    CHECK(FindTenseEntry(Tenses, "present").value("count").toInt() == 1);
    CHECK(FindTenseEntry(Tenses, "imperfect").value("count").toInt() == 3);
    CHECK_FALSE(FindTenseEntry(Tenses, "imperfect").value("enabled").toBool());
    CHECK(Session.collectTenseCards(Seeded.TrainingId, "imperfect").isEmpty());
    CHECK(AppDatabase::Get().GetCardRepository().LoadFormsCards().size() == 4);

    MarkLearned({Seeded.CardIds[3]});

    Tenses = Session.formsTenses(Seeded.TrainingId);

    for (const QString& Key : {QString("imperfect"), QString("future"), QString("perfect")})
    {
        const QVariantMap Entry = FindTenseEntry(Tenses, Key);

        CHECK(Entry.value("enabled").toBool());
        CHECK(Entry.value("count").toInt() == 4);
    }
}

TEST_CASE("Only the conjugation training offers tenses", "[conjugation][tenses]")
{
    FFormsTestBed Bed;

    DailySessionController Session;

    CHECK(Session.formsTenses(GetTrainingId(FormsTrainingAgreement)).isEmpty());
    CHECK(Session.formsTenses("learning-pool-fr-en").isEmpty());
    CHECK(Session.formsTenses("").isEmpty());
}

TEST_CASE("Picking a tense creates its cards from the trained verbs, once, four at most", "[conjugation][tenses]")
{
    FFormsTestBed Bed;

    FTCardRepository& CardRepository = AppDatabase::Get().GetCardRepository();

    const FSeededForms Seeded = SeedVerbs({"vouloir", "dire", "voir", "prendre", "faire"});
    MarkLearned(Seeded.CardIds);

    DailySessionController Session;

    const QStringList Created = Session.collectTenseCards(Seeded.TrainingId, "imperfect");

    REQUIRE(Created == QStringList{"conjugation-fr-faire-imperfect", "conjugation-fr-dire-imperfect",
                                   "conjugation-fr-vouloir-imperfect", "conjugation-fr-voir-imperfect"});

    const std::optional<FTCardHandle> Stored = CardRepository.LoadFormsCard("conjugation-fr-vouloir-imperfect");
    REQUIRE(Stored.has_value());
    CHECK((*Stored)->TargetWord == "vouloir");
    CHECK((*Stored)->Translation == "translation");
    CHECK_FALSE(IsCompleted(**Stored));

    CHECK(Session.collectTenseCards(Seeded.TrainingId, "imperfect") == Created);
    CHECK(CardRepository.LoadFormsCards().size() == 9);

    const QVariantList Tenses = Session.formsTenses(Seeded.TrainingId);

    CHECK(FindTenseEntry(Tenses, "imperfect").value("count").toInt() == 5);
    CHECK(FindTenseEntry(Tenses, "future").value("count").toInt() == 5);

    CHECK(Session.formsWordCount(Seeded.TrainingId) == 0);
}

TEST_CASE("An imperfect training trains the imperfect and marks only that card learned", "[conjugation][tenses]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir", "partir", "voir", "prendre"});
    MarkLearned(Seeded.CardIds);

    DailySessionController Session;

    const QStringList CardIds = Session.collectTenseCards(Seeded.TrainingId, "imperfect");
    REQUIRE(CardIds.size() == 4);

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, CardIds));

    CHECK(Controller.IsFormsSession());
    CHECK(Controller.GetSessionWordCount() == 4);

    auto* Study = qobject_cast<FlashcardController*>(Controller.GetActiveController());
    REQUIRE(Study != nullptr);

    CHECK(Study->GetPrompt() == "vouloir");
    CHECK(Study->GetCardMeta() == "imparfait");
    CHECK(ReadTableCell(Study->GetForms(), 0, 0, "cue") == "je");
    CHECK(ReadTableCell(Study->GetForms(), 0, 0, "form") == "voul<a href=\"ending\">ais</a>");

    for (int Stage = StudyStage; Stage <= TypingStage; ++Stage)
    {
        REQUIRE(Controller.GetStage() == Stage);
        CHECK(Controller.GetActiveController()->GetTense() == "imparfait");

        CompleteStage(Controller, Stage);
    }

    REQUIRE(Controller.IsFinished());
    CHECK(IsLearned("conjugation-fr-vouloir-imperfect"));
    CHECK_FALSE(AppDatabase::Get().GetCardRepository().LoadFormsCard("conjugation-fr-vouloir-future").has_value());

    CHECK(FindTenseEntry(Session.formsTenses(Seeded.TrainingId), "imperfect").value("count").toInt() == 0);
    CHECK_FALSE(FindTenseEntry(Session.formsTenses(Seeded.TrainingId), "imperfect").value("enabled").toBool());
}

TEST_CASE("A parked tense training resumes in its tense", "[conjugation][tenses]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir", "dire", "voir", "prendre"});
    MarkLearned(Seeded.CardIds);

    DailySessionController Session;

    const QStringList CardIds = Session.collectTenseCards(Seeded.TrainingId, "future");
    REQUIRE(CardIds.size() == 4);

    {
        IntensiveLearningController Controller;
        REQUIRE(Controller.startCards(Seeded.TrainingId, CardIds));

        CompleteStage(Controller, StudyStage);
        AnswerActiveController(Controller.GetActiveController());
    }

    IntensiveLearningController Resumed;
    REQUIRE(Resumed.resume(Seeded.TrainingId));

    CHECK(Resumed.GetStage() == MatchStage);
    CHECK(Resumed.GetActiveController()->GetTense() == "futur");

    CompleteStagesUpTo(Resumed, TypingStage);

    REQUIRE(Resumed.IsFinished());
    CHECK(IsLearned("conjugation-fr-vouloir-future"));
}

TEST_CASE("Nothing about forms is ever due for review", "[conjugation][forms]")
{
    FFormsTestBed Bed;

    const FSeededForms Seeded = SeedVerbs({"vouloir", "dire"});
    MarkLearned(Seeded.CardIds);

    DailySessionController Session;
    Session.refresh();

    CHECK(Session.GetDueCount() == 0);

    RepetitionController Review;

    CHECK_FALSE(Review.startDueToday());
}

static std::vector<std::pair<std::string, std::string>> TakeStoredSessions()
{
    std::vector<std::pair<std::string, std::string>> Stored;

    for (const auto& [SessionKey, Json] : AppState::Get().IntensiveLearningCheckpoints)
    {
        Stored.emplace_back(SessionKey.toStdString(), Json.toStdString());
    }

    FFormsTestBed::ForgetSessions();

    return Stored;
}

TEST_CASE("Both parked forms trainings come back after a restart, the last one used on top",
          "[conjugation][forms]")
{
    FFormsTestBed Bed;

    const FSeededForms Verbs = SeedVerbs({"vouloir", "dire"});
    const FSeededForms Forms = SeedForms(FormsKindAdjective, {"beau", "blanc"});

    std::vector<std::pair<std::string, std::string>> Stored;

    {
        IntensiveLearningController Controller;

        REQUIRE(Controller.startCards(Verbs.TrainingId, Verbs.CardIds));
        CompleteStage(Controller, StudyStage);
        REQUIRE(Controller.GetStage() == MatchStage);

        REQUIRE(Controller.startCards(Forms.TrainingId, Forms.CardIds));
        CompleteStagesUpTo(Controller, MatchStage);
        REQUIRE(Controller.GetStage() == TypingStage);
        AnswerActiveController(Controller.GetActiveController());

        Stored = TakeStoredSessions();
    }

    REQUIRE(Stored.size() == 2);

    IntensiveLearningController Restarted;
    CHECK_FALSE(Restarted.hasSessionFor(Verbs.TrainingId));

    Restarted.RestoreCheckpoints(Stored);

    REQUIRE(Restarted.HasActiveSession());
    CHECK(Restarted.lastActiveKey() == Forms.TrainingId);
    CHECK(Restarted.GetStage() == TypingStage);
    CHECK(qobject_cast<WrittenProductionController*>(Restarted.GetActiveController()) != nullptr);

    CHECK(Restarted.hasSessionFor(Verbs.TrainingId));
    CHECK(Restarted.stageFor(Verbs.TrainingId) == MatchStage);
    CHECK(Restarted.wordCountFor(Verbs.TrainingId) == 2);

    CompleteStagesUpTo(Restarted, TypingStage);
    REQUIRE(Restarted.IsFinished());
    CHECK(IsLearned(Forms.CardIds[0]));
    CHECK(IsLearned(Forms.CardIds[1]));
    CHECK_FALSE(Restarted.hasSessionFor(Forms.TrainingId));

    REQUIRE(Restarted.resume(Verbs.TrainingId));
    CHECK(Restarted.GetStage() == MatchStage);
    CHECK(qobject_cast<SpeedMatchController*>(Restarted.GetActiveController()) != nullptr);

    Restarted.endSession();
}

TEST_CASE("A stored session whose cards are gone is dropped and the next one is restored", "[conjugation][forms]")
{
    FFormsTestBed Bed;

    const FSeededForms Verbs = SeedVerbs({"vouloir", "dire"});

    std::vector<std::pair<std::string, std::string>> Stored;

    {
        IntensiveLearningController Controller;

        REQUIRE(Controller.startCards(Verbs.TrainingId, Verbs.CardIds));
        CompleteStage(Controller, StudyStage);

        Stored = TakeStoredSessions();
    }

    Stored.emplace_back("gone-session",
        R"({"sessionKey":"gone-session","title":"Gone","cardIds":["no-such-card"],"stageIndex":0,"savedAt":9000000000000000})");

    IntensiveLearningController Restarted;
    Restarted.RestoreCheckpoints(Stored);

    REQUIRE(Restarted.HasActiveSession());
    CHECK(Restarted.lastActiveKey() == Verbs.TrainingId);
    CHECK(Restarted.GetStage() == MatchStage);
    CHECK_FALSE(Restarted.hasSessionFor("gone-session"));
}

TEST_CASE("A typing checkpoint that lands on the match stage starts the match fresh", "[conjugation][forms][slow]")
{
    FFormsTestBed Bed;

    const FSeededForms Verbs = SeedVerbs({"vouloir", "dire"});

    std::vector<std::pair<std::string, std::string>> Stored;

    {
        IntensiveLearningController Controller;

        REQUIRE(Controller.startCards(Verbs.TrainingId, Verbs.CardIds));
        CompleteStagesUpTo(Controller, MatchStage);
        REQUIRE(Controller.GetStage() == TypingStage);
        AnswerActiveController(Controller.GetActiveController());

        Stored = TakeStoredSessions();
    }

    REQUIRE(Stored.size() == 1);

    QJsonObject Root = QJsonDocument::fromJson(QByteArray::fromStdString(Stored[0].second)).object();
    REQUIRE(Root.contains("activeBatchCardIds"));
    Root["stageIndex"] = MatchStage - 1;
    Stored[0].second = QJsonDocument(Root).toJson(QJsonDocument::Compact).toStdString();

    IntensiveLearningController Restarted;
    Restarted.RestoreCheckpoints(Stored);

    REQUIRE(Restarted.GetStage() == MatchStage);

    auto* Match = qobject_cast<SpeedMatchController*>(Restarted.GetActiveController());
    REQUIRE(Match != nullptr);
    CHECK(Match->GetLeftItems().size() == 6);

    CompleteStagesUpTo(Restarted, TypingStage);
    CHECK(Restarted.IsFinished());
}

struct FSpanishFormsBed : FFormsTestBed
{
    FSpanishFormsBed()
    {
        FTSettings::Get().SetTargetLanguage("es");
        FTSettings::Get().SetOptionalPronoun(true);
        AppDatabase::Get().GetCardRepository().SetActiveLanguages("es", "en");

        REQUIRE(AppDictionary::Get().Open("es", "en"));
    }

    ~FSpanishFormsBed()
    {
        FTSettings::Get().SetTargetLanguage("fr");
        FTSettings::Get().SetOptionalPronoun(true);
    }
};

static FSeededForms SeedSpanishVerbs(const std::vector<std::string>& Verbs)
{
    std::vector<FTCard> Cards;
    FSeededForms Result;

    for (const std::string& Verb : Verbs)
    {
        for (const FTDictionaryEntry& Entry : AppDictionary::Get().GetRepository().LookUp(Verb))
        {
            if (std::optional<FTCard> Card = MakeFormsCard(FormsKindVerb, Entry, "es", "translation"))
            {
                Cards.push_back(*Card);
                Result.CardIds.append(QString::fromStdString(Card->Id));

                break;
            }
        }
    }

    REQUIRE(Cards.size() == Verbs.size());
    REQUIRE(AppDatabase::Get().GetCardRepository().AddFormsCards(Cards));

    Result.TrainingId = QString::fromStdString(MakeFormsTrainingId(GetFormsTraining(Cards.front().Id), "es", "en"));

    return Result;
}

TEST_CASE("A conjugation training started with vosotros off asks it in neither the match nor the typing",
          "[conjugation]")
{
    FSpanishFormsBed Bed;
    FTSettings::Get().SetOptionalPronoun(false);

    const FSeededForms Seeded = SeedSpanishVerbs({"pedir", "querer", "poner", "ir"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStage(Controller, StudyStage);

    int Steps = 0;

    while (!Controller.IsFinished() && Controller.GetStage() == MatchStage)
    {
        auto* Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController());
        REQUIRE(Match != nullptr);
        CHECK_FALSE(Match->GetLeftItems().contains("vosotros"));

        AnswerActiveController(Match);
        REQUIRE(++Steps < 2000);
    }

    while (!Controller.IsFinished() && Controller.GetStage() == TypingStage)
    {
        const FTCard* Asked = Controller.GetActiveController()->GetCurrentCard();
        REQUIRE(Asked != nullptr);
        CHECK_FALSE(IsOptionalForm(*Asked));

        AnswerActiveController(Controller.GetActiveController());
        REQUIRE(++Steps < 4000);
    }

    CHECK(Controller.IsFinished());
}

TEST_CASE("Turning vosotros off during a conjugation training drops it once the training is opened again",
          "[conjugation]")
{
    FSpanishFormsBed Bed;

    const FSeededForms Seeded = SeedSpanishVerbs({"hablar"});

    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStage(Controller, StudyStage);
    REQUIRE(Controller.GetStage() == MatchStage);

    auto* Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController());
    REQUIRE(Match != nullptr);
    CHECK(Match->GetLeftItems().contains("vosotros"));

    FTSettings::Get().SetOptionalPronoun(false);

    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));
    REQUIRE(Controller.GetStage() == MatchStage);

    Match = qobject_cast<SpeedMatchController*>(Controller.GetActiveController());
    REQUIRE(Match != nullptr);
    CHECK(Match->GetLeftItems().size() == 5);
    CHECK_FALSE(Match->GetLeftItems().contains("vosotros"));

    Controller.endSession();
}

TEST_CASE("A parked typing round saved before vosotros was turned off resumes without it", "[conjugation]")
{
    FSpanishFormsBed Bed;

    const FSeededForms Seeded = SeedSpanishVerbs({"hablar"});

    {
        IntensiveLearningController Controller;
        REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

        CompleteStagesUpTo(Controller, MatchStage);
        REQUIRE(Controller.GetStage() == TypingStage);
    }

    QJsonObject Parked = QJsonDocument::fromJson(AppState::Get().IntensiveLearningCheckpoints[Seeded.TrainingId]).object();
    Parked.remove("optionalPronoun");
    AppState::Get().IntensiveLearningCheckpoints[Seeded.TrainingId] = QJsonDocument(Parked).toJson(QJsonDocument::Compact);

    FTSettings::Get().SetOptionalPronoun(false);

    IntensiveLearningController Resumed;
    REQUIRE(Resumed.resume(Seeded.TrainingId));
    REQUIRE(Resumed.GetStage() == TypingStage);

    int Steps = 0;

    while (!Resumed.IsFinished())
    {
        const FTCard* Asked = Resumed.GetActiveController()->GetCurrentCard();
        REQUIRE(Asked != nullptr);
        CHECK_FALSE(IsOptionalForm(*Asked));

        AnswerActiveController(Resumed.GetActiveController());
        REQUIRE(++Steps < 2000);
    }
}

static QSet<QString> ListFormsWithoutSentence(const FSeededForms& Seeded)
{
    IntensiveLearningController Controller;
    REQUIRE(Controller.startCards(Seeded.TrainingId, Seeded.CardIds));

    CompleteStagesUpTo(Controller, MatchStage);
    REQUIRE(Controller.GetStage() == TypingStage);

    QSet<QString> Missing;
    int Steps = 0;

    while (!Controller.IsFinished())
    {
        auto* Written = qobject_cast<WrittenProductionController*>(Controller.GetActiveController());
        REQUIRE(Written != nullptr);

        const std::optional<FTWordForm> Read = ReadWordForm(*Written->GetCurrentCard());
        REQUIRE(Read.has_value());

        if (!Written->IsSentencePrompt())
        {
            Missing.insert(QString::fromStdString(Read->Form));
        }

        AnswerActiveController(Written);
        REQUIRE(++Steps < 2000);
    }

    return Missing;
}

TEST_CASE("Typing finds a sentence for a rare person when the dictionary has one", "[conjugation][slow]")
{
    SECTION("Spanish vosotros and tú")
    {
        FSpanishFormsBed Bed;

        CHECK(ListFormsWithoutSentence(SeedSpanishVerbs({"querer", "ir", "hablar", "poner"})).isEmpty());
    }

    SECTION("French")
    {
        FFormsTestBed Bed;

        CHECK(ListFormsWithoutSentence(SeedVerbs({"vouloir", "aller", "prendre", "dire"})).isEmpty());
    }
}
