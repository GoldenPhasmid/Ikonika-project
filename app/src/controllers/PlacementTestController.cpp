#include "PlacementTestController.h"

#include "AppDatabase.h"
#include "AppSpeech.h"
#include "CefrFormatting.h"

#include "core/FCardSelectorBase.h"
#include "core/FTCard.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QFile>

#include <algorithm>

namespace
{
    QString MakeWordListPath(const QString& TargetLanguage, const QString& NativeLanguage)
    {
        return QStringLiteral(":/placement/%1-%2.tsv").arg(TargetLanguage, NativeLanguage);
    }

    constexpr int FineRoundingLimit = 1000;
    constexpr int FineRoundingStep = 50;
    constexpr int CoarseRoundingStep = 100;

    QString ResolveNativeLanguage(const QString& NativeLanguage)
    {
        if (!NativeLanguage.isEmpty())
        {
            return NativeLanguage;
        }

        return QString::fromStdString(FTSettings::Get().GetLanguageRules().NativeLanguage);
    }

    FTCard MakeCard(const FTPlacementWord& Word)
    {
        FTCard Card;
        Card.TargetWord = Word.Word;
        Card.Translation = Word.Translation;
        Card.PartOfSpeech = Word.PartOfSpeech;
        Card.Gender = Word.Gender;

        return Card;
    }
}

PlacementTestController::PlacementTestController(QObject* Parent)
    : QObject(Parent)
{
}

bool PlacementTestController::isOffered(const QString& InTargetLanguage, const QString& InNativeLanguage) const
{
    if (InTargetLanguage.isEmpty() || !AppDatabase::Get().IsReady())
    {
        return false;
    }

    if (!QFile::exists(MakeWordListPath(InTargetLanguage, ResolveNativeLanguage(InNativeLanguage))))
    {
        return false;
    }

    return !AppDatabase::Get().GetCardRepository().FindPlacementCutoff(InTargetLanguage.toStdString()).has_value();
}

bool PlacementTestController::start(const QString& InTargetLanguage, const QString& InNativeLanguage)
{
    const QString ResolvedNative = ResolveNativeLanguage(InNativeLanguage);

    QFile WordList(MakeWordListPath(InTargetLanguage, ResolvedNative));

    if (!WordList.open(QIODevice::ReadOnly))
    {
        qCWarning(LogApp) << "Placement test has no word list for" << InTargetLanguage << "->" << InNativeLanguage;

        return false;
    }

    std::vector<FTPlacementWord> Words = ParsePlacementWords(WordList.readAll().toStdString());

    if (Words.empty())
    {
        qCWarning(LogApp) << "Placement test word list is empty for" << InTargetLanguage << "->" << InNativeLanguage;

        return false;
    }

    qCInfo(LogApp) << "Placement test started for" << InTargetLanguage << "->" << InNativeLanguage
                   << "with" << Words.size() << "words";

    TargetLanguage = InTargetLanguage;
    NativeLanguage = ResolvedNative;
    Test = FTPlacementTest(std::move(Words), MakeRandomSeed());
    bRevealed = false;

    emit stateChanged();

    SpeakPrompt();

    return true;
}

void PlacementTestController::skip(const QString& InTargetLanguage)
{
    qCInfo(LogApp) << "Placement test skipped for" << InTargetLanguage;

    if (!AppDatabase::Get().GetCardRepository().SetPlacementCutoff(InTargetLanguage.toStdString(), 0))
    {
        qCWarning(LogApp) << "Could not record the skipped placement test for" << InTargetLanguage;
    }
}

void PlacementTestController::reveal()
{
    if (Test.IsFinished() || bRevealed)
    {
        return;
    }

    bRevealed = true;

    emit stateChanged();
}

void PlacementTestController::answerYes()
{
    Answer(true);
}

void PlacementTestController::answerNo()
{
    Answer(false);
}

void PlacementTestController::restart()
{
    start(TargetLanguage, NativeLanguage);
}

void PlacementTestController::Answer(bool bKnown)
{
    if (Test.IsFinished() || !bRevealed)
    {
        return;
    }

    Test.Answer(bKnown);
    bRevealed = false;

    if (Test.IsFinished())
    {
        qCInfo(LogApp) << "Placement test finished for" << TargetLanguage << ": cutoff rank" << Test.GetCutoffRank()
                       << "after" << Test.GetAnsweredCount() << "words," << Test.GetKnownCount() << "known";

        if (!AppDatabase::Get().GetCardRepository().SetPlacementCutoff(TargetLanguage.toStdString(), Test.GetCutoffRank()))
        {
            qCWarning(LogApp) << "Could not record the placement test result for" << TargetLanguage;
        }
    }

    emit stateChanged();

    SpeakPrompt();
}

void PlacementTestController::SpeakPrompt() const
{
    const FTPlacementWord* Word = Test.GetCurrentWord();

    if (Word == nullptr || !AppSpeech::Get().AllowsAutomaticSpeech())
    {
        return;
    }

    AppSpeech::Get().Speak(QString::fromStdString(Word->Word), TargetLanguage);
}

QString PlacementTestController::GetPrompt() const
{
    const FTPlacementWord* Word = Test.GetCurrentWord();

    if (Word == nullptr)
    {
        return {};
    }

    return QString::fromStdString(GetDisplayHeadword(
        Word->Word, Word->PartOfSpeech, TargetLanguage.toStdString()));
}

QString PlacementTestController::GetLastRevealedAnswer() const
{
    const FTPlacementWord* Word = Test.GetCurrentWord();

    if (Word == nullptr || !bRevealed)
    {
        return {};
    }

    return QString::fromStdString(GetDisplayTranslation(MakeCard(*Word), NativeLanguage.toStdString()));
}

bool PlacementTestController::IsAnswered() const
{
    return bRevealed;
}

QString PlacementTestController::GetPartOfSpeech() const
{
    const FTPlacementWord* Word = Test.GetCurrentWord();

    return Word != nullptr ? QString::fromStdString(Word->PartOfSpeech) : QString();
}

QString PlacementTestController::GetGender() const
{
    const FTPlacementWord* Word = Test.GetCurrentWord();

    if (Word == nullptr || !Word->Gender.has_value())
    {
        return {};
    }

    return *Word->Gender == GenderMasculine ? tr("masculine") : tr("feminine");
}

int PlacementTestController::GetHeadwordGender() const
{
    const FTPlacementWord* Word = Test.GetCurrentWord();

    return Word != nullptr ? Word->Gender.value_or(-1) : -1;
}

QString PlacementTestController::GetCardMeta() const
{
    const QString Speech = GetPartOfSpeech();
    const QString Gender = GetGender();

    if (Speech.isEmpty() || Gender.isEmpty())
    {
        return Speech.isEmpty() ? Gender : Speech;
    }

    return QStringLiteral("%1 · %2").arg(Speech, Gender);
}

int PlacementTestController::GetCurrentSessionCount() const
{
    return std::min(Test.GetAnsweredCount() + 1, FTPlacementTest::MaximumQuestions);
}

int PlacementTestController::GetMaxSessionCount() const
{
    return FTPlacementTest::MaximumQuestions;
}

double PlacementTestController::GetExerciseProgress() const
{
    if (Test.IsFinished())
    {
        return 1.0;
    }

    return static_cast<double>(Test.GetAnsweredCount()) / FTPlacementTest::MaximumQuestions;
}

int PlacementTestController::GetElapsedSeconds() const
{
    return 0;
}

bool PlacementTestController::IsFinished() const
{
    return Test.GetAnsweredCount() > 0 && Test.IsFinished();
}

int PlacementTestController::GetKnownWordCount() const
{
    const int Size = Test.GetCutoffRank();
    const int Step = Size < FineRoundingLimit ? FineRoundingStep : CoarseRoundingStep;

    return (Size + Step / 2) / Step * Step;
}

QString PlacementTestController::GetCefrLevel() const
{
    return CefrLetter(GetPlacementCefrLevel(GetKnownWordCount()));
}

int PlacementTestController::GetAnsweredCount() const
{
    return Test.GetAnsweredCount();
}

int PlacementTestController::GetKnownAnswerCount() const
{
    return Test.GetKnownCount();
}

QVariantList PlacementTestController::GetAnswers() const
{
    QVariantList Result;

    for (const FTPlacementAnswer& Answer : Test.GetAnswersByRank())
    {
        Result.append(Answer.bKnown);
    }

    return Result;
}
