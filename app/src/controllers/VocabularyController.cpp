#include "VocabularyController.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppImport.h"
#include "AppSpeech.h"

#include "core/FTCard.h"
#include "core/FTDeckProgress.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"
#include "core/FTTextNormalization.h"
#include "core/FTUserDeckRepository.h"

#include <QFile>
#include <QFileInfo>
#include <QVariantMap>

#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <limits>
#include <memory>

VocabularyController::VocabularyController(QObject* Parent)
    : QObject(Parent)
{
    connect(&AppDictionary::Notifier(), &AppDictionaryNotifier::languagesChanged, this,
            &VocabularyController::HandleLanguagesChanged);

    Load();
}

void VocabularyController::HandleLanguagesChanged()
{
    qCInfo(LogApp) << "Vocabulary: languages changed, dropping deck" << DeckId;

    DeckId = 0;
    UserDeckId.clear();

    ClearView();
    Load();
}

void VocabularyController::refresh()
{
    Load();
}

void VocabularyController::resetView()
{
    ClearView();
    RebuildView();
}

void VocabularyController::ClearView()
{
    Search.clear();
    FilterKeys.clear();
    SortKey = DeckId > 0 || !UserDeckId.isEmpty() ? QStringLiteral("deck") : QStringLiteral("word");
    bSortDescending = false;
}

void VocabularyController::speakWord(const QString& Word) const
{
    qCInfo(LogSpeech) << "VocabularyController speaks:" << Word;

    AppSpeech::Get().Speak(Word,
        QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage));
}

bool VocabularyController::removeCard(const QString& CardId)
{
    if (CardId.isEmpty() || !AppDatabase::Get().IsReady())
    {
        return false;
    }

    if (!AppDatabase::Get().GetCardRepository().DeleteCards({CardId.toStdString()}))
    {
        return false;
    }

    qCInfo(LogApp) << "Word removed from My Vocabulary -" << CardId;

    Load();

    return true;
}

bool VocabularyController::resetCardProgress(const QString& CardId)
{
    if (CardId.isEmpty() || !AppDatabase::Get().IsReady())
    {
        return false;
    }

    if (!AppDatabase::Get().GetCardRepository().ReturnCardToPool(CardId.toStdString()))
    {
        return false;
    }

    qCInfo(LogApp) << "Progress reset for card" << CardId;

    Load();

    return true;
}

bool VocabularyController::completeCardProgress(const QString& CardId)
{
    if (CardId.isEmpty() || !AppDatabase::Get().IsReady())
    {
        return false;
    }

    if (!AppDatabase::Get().GetCardRepository().MarkCardCompleted(CardId.toStdString()))
    {
        return false;
    }

    qCInfo(LogApp) << "Card marked learned" << CardId;

    Load();

    return true;
}

bool VocabularyController::addCard(const QString& Word, const QString& Translation, const QString& PartOfSpeech,
                                   int Gender, bool bReadingChosen, const QString& MeaningGist, const QString& Hint,
                                   const QString& Language)
{
    if (!AppDatabase::Get().IsReady())
    {
        return false;
    }

    const QString TrimmedWord = Word.trimmed();
    const QString TrimmedTranslation = Translation.trimmed();

    if (TrimmedWord.isEmpty() || TrimmedTranslation.isEmpty())
    {
        return false;
    }

    const std::string TargetLanguage{Language.isEmpty() ? FTSettings::Get().GetLanguageRules().TargetLanguage
                                                        : Language.toStdString()};

    AppLanguageDictionary& Dictionary{AppDictionary::Get(TargetLanguage)};

    FTCard Card = AppCards::MakeAddedWord(Dictionary, TrimmedWord.toStdString(), TrimmedTranslation.toStdString(),
                                          PartOfSpeech.trimmed().toStdString(), Gender, bReadingChosen,
                                          MeaningGist.toStdString());
    Card.Hint = Hint.trimmed().toStdString();

    const std::optional<std::string> Added = AppCards::GetCardRepository(Dictionary).AddCard(Card);

    if (!Added.has_value())
    {
        qCWarning(LogApp) << "Could not add" << TrimmedWord << "to the"
                          << QString::fromStdString(TargetLanguage) << "vocabulary";

        return false;
    }

    if (!UserDeckId.isEmpty() && !AppDatabase::Get().GetUserDecks().AddCard(UserDeckId.toStdString(), *Added))
    {
        qCWarning(LogApp) << "Could not put" << TrimmedWord << "into the deck" << UserDeckId;
    }

    qCInfo(LogApp) << "Added" << TrimmedWord << Card.PartOfSpeech.c_str() << "to the"
                   << QString::fromStdString(TargetLanguage) << "vocabulary";

    Load();

    emit cardAdded(QString::fromStdString(*Added));
    emit AppCards::Notifier().cardAdded(QString::fromStdString(*Added));

    return true;
}

bool VocabularyController::updateCard(const QString& CardId, const QString& Word,
                                      const QString& Translation, int Gender,
                                      const QString& PartOfSpeech, const QString& MeaningGist, const QString& Hint)
{
    if (!AppCards::UpdateCardContent(CardId, Word, Translation, Gender, PartOfSpeech, MeaningGist, Hint))
    {
        return false;
    }

    Load();

    return true;
}

int VocabularyController::findWordTranslationStage(const QString& Word,
                                                    const QString& Translation) const
{
    return AppCards::FindWordTranslationStage(Word, Translation);
}

bool VocabularyController::importFromFile(const QString& Path)
{
    if (!AppDatabase::Get().IsReady())
    {
        return false;
    }

    const std::unique_ptr<FTImportSource> Source = AppImport::Parse(Path);

    if (!Source)
    {
        return false;
    }

    int Added = 0;

    for (const FTImportRow& Row : Source->GetRows())
    {
        const FTCard Card = AppCards::MakeAddedWord(AppDictionary::GetActive(), Row.TargetWord, Row.Translation, {}, -1, false, {});

        if (AppDatabase::Get().GetCardRepository().AddCard(Card).has_value())
        {
            ++Added;
        }
    }

    qCInfo(LogApp) << "Imported" << Added << "of" << static_cast<int>(Source->GetRows().size())
                   << "words from" << Path << "into My Vocabulary";

    Load();

    return Added > 0;
}

static QString QuoteCsvField(const std::string& Value)
{
    QString Field = QString::fromStdString(Value).simplified();
    Field.replace(QLatin1Char('"'), QStringLiteral("\"\""));

    return QLatin1Char('"') + Field + QLatin1Char('"');
}

bool VocabularyController::exportToFile(const QUrl& File) const
{
    QFile Target{File.isLocalFile() ? File.toLocalFile() : File.toString()};

    if (!Target.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qCWarning(LogApp) << "Could not export to" << File << ":" << Target.errorString();

        return false;
    }

    QByteArray Content{"Word,Translation\n"};

    for (const FTCardHandle& Card : Cards)
    {
        Content += (QuoteCsvField(Card->TargetWord) + QLatin1Char(',') + QuoteCsvField(Card->Translation) + QLatin1Char('\n')).toUtf8();
    }

    if (Target.write(Content) != Content.size() || !Target.flush())
    {
        qCWarning(LogApp) << "Could not finish the export to" << File << ":" << Target.errorString();

        return false;
    }

    qCInfo(LogApp) << "Exported" << static_cast<int>(Cards.size()) << "words of" << DeckName << "to" << File;

    return true;
}

int VocabularyController::practiceCount(const QStringList& Keys) const
{
    return static_cast<int>(std::count_if(Cards.begin(), Cards.end(), [&Keys](const FTCardHandle& Card)
    {
        return Card && MatchesAnyFilter(*Card, Keys);
    }));
}

int VocabularyController::GetNotLearnedCount() const
{
    return CountMatching(QStringLiteral("notlearned"));
}

int VocabularyController::GetLearningCount() const
{
    return CountMatching(QStringLiteral("learning"));
}

int VocabularyController::GetLearnedCount() const
{
    return CountMatching(QStringLiteral("completed"));
}

int VocabularyController::CountMatching(const QString& Key) const
{
    return static_cast<int>(std::count_if(Cards.begin(), Cards.end(), [&Key](const FTCardHandle& Card)
    {
        return Card && MatchesFilter(*Card, Key);
    }));
}

QString VocabularyController::GetSearch() const
{
    return Search;
}

void VocabularyController::SetSearch(const QString& Value)
{
    if (Search == Value)
    {
        return;
    }

    Search = Value;

    RebuildView();
}

QStringList VocabularyController::GetFilterKeys() const
{
    return FilterKeys;
}

void VocabularyController::SetFilterKeys(const QStringList& Value)
{
    if (FilterKeys == Value)
    {
        return;
    }

    FilterKeys = Value;

    RebuildView();
}

QString VocabularyController::GetSortKey() const
{
    return SortKey;
}

void VocabularyController::SetSortKey(const QString& Value)
{
    if (SortKey == Value)
    {
        return;
    }

    SortKey = Value;

    RebuildView();
}

bool VocabularyController::IsSortDescending() const
{
    return bSortDescending;
}

void VocabularyController::SetSortDescending(bool Value)
{
    if (bSortDescending == Value)
    {
        return;
    }

    bSortDescending = Value;

    RebuildView();
}

QVariantList VocabularyController::GetVisibleCards() const
{
    return VisibleCards;
}

int VocabularyController::GetFilteredCount() const
{
    return VisibleCards.size();
}

void VocabularyController::Load()
{
    VocabularyCards = AppCards::LoadCards();

    ShowCards();
}

void VocabularyController::ShowCards()
{
    if (DeckId > 0)
    {
        LoadDeck();
    }
    else if (!UserDeckId.isEmpty())
    {
        LoadUserDeck();
    }
    else
    {
        Cards = VocabularyCards;
    }

    RebuildView();

    emit vocabularyChanged();
}

void VocabularyController::LoadDeck()
{
    Cards.clear();
    DeckName.clear();
    DeckNewCount = 0;

    if (!AppDictionary::GetActive().IsReady() || !AppDatabase::Get().IsReady())
    {
        qCWarning(LogApp) << "Cannot show deck" << DeckId << "without both a dictionary and a database";

        return;
    }

    FTDictionaryRepository& Dictionary = AppDictionary::GetActive().GetRepository();

    for (const FTDeck& Deck : Dictionary.ListDecks())
    {
        if (Deck.Id == DeckId)
        {
            DeckName = QString::fromStdString(Deck.Name);
        }
    }

    const std::vector<FTDeckWord> Words = Dictionary.LoadDeckWords(DeckId);
    const std::vector<FTDeckWordProgress> Progress = MatchDeckWords(
        Words, VocabularyCards, AppDatabase::Get().GetCardRepository().ListKnownWords());

    Cards.reserve(Words.size());

    for (std::size_t Index = 0; Index < Words.size(); ++Index)
    {
        if (Progress[Index].Card)
        {
            Cards.push_back(Progress[Index].Card);

            continue;
        }

        auto Placeholder = std::make_shared<FTCard>();
        Placeholder->TargetWord = Words[Index].Headword;
        Placeholder->PartOfSpeech = Words[Index].PartOfSpeech;
        Placeholder->Gender = Words[Index].Gender;
        Placeholder->Translation = Words[Index].Translation;
        Placeholder->MeaningGist = Words[Index].Gist;

        if (Progress[Index].bKnown)
        {
            Placeholder->CompletedDate = 0;
        }
        else
        {
            ++DeckNewCount;
        }

        Cards.push_back(std::move(Placeholder));
    }

    qCInfo(LogApp) << "Deck" << DeckName << "shows" << static_cast<int>(Cards.size()) << "words,"
                   << DeckNewCount << "not yet triaged";
}

void VocabularyController::LoadUserDeck()
{
    Cards.clear();
    DeckName.clear();
    DeckNewCount = 0;

    if (!AppDatabase::Get().IsReady())
    {
        qCWarning(LogApp) << "Cannot show the deck" << UserDeckId << "without a database";

        return;
    }

    FTUserDeckRepository& Repository = AppDatabase::Get().GetUserDecks();
    const std::string Id = UserDeckId.toStdString();

    for (const FTUserDeckRow& Deck : Repository.ListDecks())
    {
        if (Deck.Id == Id)
        {
            DeckName = QString::fromStdString(Deck.Name);
        }
    }

    std::unordered_map<std::string, FTCardHandle> CardsById;
    CardsById.reserve(VocabularyCards.size());

    for (const FTCardHandle& Card : VocabularyCards)
    {
        CardsById.emplace(Card->Id, Card);
    }

    for (const std::string& CardId : Repository.ListCardIds(Id))
    {
        const auto Found = CardsById.find(CardId);

        if (Found != CardsById.end())
        {
            Cards.push_back(Found->second);
        }
    }

    qCInfo(LogApp) << "The deck" << DeckName << "shows" << static_cast<int>(Cards.size()) << "cards";
}

void VocabularyController::RebuildView()
{
    std::vector<FTCardHandle> Filtered;
    Filtered.reserve(Cards.size());

    for (const FTCardHandle& Card : Cards)
    {
        if (Card && MatchesFilters(*Card))
        {
            Filtered.push_back(Card);
        }
    }

    const std::string FoldedNeedle = FoldAccents(Search.trimmed().toStdString());
    bool bMatchTranslations = false;

    if (!FoldedNeedle.empty())
    {
        bMatchTranslations = std::none_of(Filtered.begin(), Filtered.end(),
            [&](const FTCardHandle& Card) { return MatchesSearch(*Card, false); });

        Filtered.erase(std::remove_if(Filtered.begin(), Filtered.end(),
            [&](const FTCardHandle& Card) { return !MatchesSearch(*Card, bMatchTranslations); }),
            Filtered.end());
    }

    const std::string TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;
    const QString ActiveSortKey = SortKey;

    std::stable_sort(Filtered.begin(), Filtered.end(),
        [&](const FTCardHandle& A, const FTCardHandle& B)
        {
            if (ActiveSortKey == QStringLiteral("deck"))
            {
                return false;
            }

            if (ActiveSortKey == QStringLiteral("created"))
            {
                return A->CreatedDate > B->CreatedDate;
            }

            if (ActiveSortKey == QStringLiteral("lastupdated"))
            {
                return A->LastUpdatedDate > B->LastUpdatedDate;
            }

            if (ActiveSortKey == QStringLiteral("due"))
            {
                const FTDayNumber ADue = A->DueDate.value_or(std::numeric_limits<FTDayNumber>::max());
                const FTDayNumber BDue = B->DueDate.value_or(std::numeric_limits<FTDayNumber>::max());

                return ADue < BDue;
            }

            return A->TargetWord < B->TargetWord;
        });

    if (bSortDescending)
    {
        std::reverse(Filtered.begin(), Filtered.end());
    }

    if (!FoldedNeedle.empty())
    {
        std::stable_partition(Filtered.begin(), Filtered.end(),
            [&](const FTCardHandle& Card)
            {
                const std::string& Text = bMatchTranslations ? Card->Translation : Card->TargetWord;

                return FoldAccents(Text).rfind(FoldedNeedle, 0) == 0;
            });
    }

    VisibleCards.clear();
    VisibleCards.reserve(static_cast<int>(Filtered.size()));

    for (const FTCardHandle& Card : Filtered)
    {
        QVariantMap Entry;
        Entry[QStringLiteral("id")] = QString::fromStdString(Card->Id);
        Entry[QStringLiteral("word")] = QString::fromStdString(GetDisplayHeadword(
            Card->TargetWord, Card->PartOfSpeech, FTSettings::Get().GetLanguageRules().TargetLanguage));
        Entry[QStringLiteral("translation")] =
            QString::fromStdString(GetDisplayTranslation(
                *Card, FTSettings::Get().GetLanguageRules().NativeLanguage));
        Entry[QStringLiteral("targetWord")] = QString::fromStdString(Card->TargetWord);
        Entry[QStringLiteral("partOfSpeech")] = QString::fromStdString(Card->PartOfSpeech);
        Entry[QStringLiteral("gender")] = Card->Gender.value_or(-1);
        Entry[QStringLiteral("box")] = Card->LeitnerStage;
        Entry[QStringLiteral("completed")] = IsCompleted(*Card);
        Entry[QStringLiteral("inVocabulary")] = !Card->Id.empty();

        VisibleCards.append(Entry);
    }

    emit viewChanged();
}

qint64 VocabularyController::GetDeckId() const
{
    return DeckId;
}

void VocabularyController::SetDeckId(qint64 Value)
{
    if (DeckId == Value)
    {
        return;
    }

    DeckId = Value;

    ClearView();
    Load();
}

QString VocabularyController::GetUserDeckId() const
{
    return UserDeckId;
}

void VocabularyController::SetUserDeckId(const QString& Value)
{
    if (UserDeckId == Value)
    {
        return;
    }

    UserDeckId = Value;

    ClearView();
    Load();
}

QString VocabularyController::GetDeckName() const
{
    return DeckName;
}

int VocabularyController::GetDeckNewCount() const
{
    return DeckNewCount;
}

bool VocabularyController::MatchesFilters(const FTCard& Card) const
{
    return MatchesAnyFilter(Card, FilterKeys);
}

bool VocabularyController::MatchesAnyFilter(const FTCard& Card, const QStringList& Keys)
{
    if (Keys.isEmpty())
    {
        return true;
    }

    return std::any_of(Keys.begin(), Keys.end(), [&Card](const QString& Key)
    {
        return MatchesFilter(Card, Key);
    });
}

bool VocabularyController::MatchesFilter(const FTCard& Card, const QString& Key)
{
    if (Key == QStringLiteral("notlearned"))
    {
        return IsInPool(Card);
    }

    if (Key == QStringLiteral("learning"))
    {
        return Card.LeitnerStage > PoolStage && !IsCompleted(Card);
    }

    return Key == QStringLiteral("completed") && IsCompleted(Card);
}

bool VocabularyController::MatchesSearch(const FTCard& Card, bool bIncludeTranslation) const
{
    const std::string FoldedNeedle = FoldAccents(Search.trimmed().toStdString());

    if (FoldedNeedle.empty())
    {
        return true;
    }

    if (bIncludeTranslation)
    {
        return FoldAccents(Card.Translation).find(FoldedNeedle) != std::string::npos;
    }

    return FoldAccents(Card.TargetWord).find(FoldedNeedle) != std::string::npos;
}
