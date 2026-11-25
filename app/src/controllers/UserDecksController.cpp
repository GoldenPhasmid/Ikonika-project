#include "UserDecksController.h"

#include "AppDatabase.h"
#include "AppCards.h"
#include "AppDictionary.h"
#include "AppImport.h"

#include "core/FTCard.h"
#include "core/FTCardRepository.h"
#include "core/FTLog.h"
#include "core/FTRepetitionBatch.h"
#include "core/FTSettings.h"
#include "core/FTUserDeckRepository.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QVariantMap>

#include <algorithm>
#include <map>
#include <unordered_map>
#include <unordered_set>

static QString StageFile(const QUrl& File, QTemporaryDir& Staging)
{
    if (!Staging.isValid())
    {
        return QString();
    }

    const QString Suffix = QFileInfo(File.fileName()).suffix();
    const QString Target = Staging.filePath(Suffix.isEmpty() ? QStringLiteral("import") : QStringLiteral("import.") + Suffix);

    QFile Source(File.toString());

    if (!Source.copy(Target))
    {
        return QString();
    }

    return Target;
}

static std::unique_ptr<FTImportSource> ReadWordList(const QUrl& File)
{
    QTemporaryDir Staging;
    const QString Path = File.isLocalFile() ? File.toLocalFile() : StageFile(File, Staging);

    if (Path.isEmpty())
    {
        qCWarning(LogApp) << "Could not read the file" << File;

        return nullptr;
    }

    std::unique_ptr<FTImportSource> Source = AppImport::Parse(Path);

    if (!Source || Source->GetRows().empty())
    {
        qCWarning(LogApp) << "Nothing to import from" << Path;

        return nullptr;
    }

    return Source;
}

static QVariantMap MakeFailedImport()
{
    QVariantMap Result;
    Result[QStringLiteral("ok")] = false;

    return Result;
}

UserDecksController::UserDecksController(QObject* Parent)
    : QObject(Parent)
{
    connect(&AppDictionary::Notifier(), &AppDictionaryNotifier::languagesChanged, this, &UserDecksController::refresh);

    refresh();
}

void UserDecksController::refresh()
{
    QVariantList UpdatedCategories;
    QVariantList UpdatedDecks;

    if (AppDatabase::Get().IsReady())
    {
        FTUserDeckRepository& Repository = AppDatabase::Get().GetUserDecks();

        std::map<std::string, int> DeckCounts;

        for (const FTUserDeckRow& Deck : Repository.ListDecks())
        {
            ++DeckCounts[Deck.CategoryId];

            QVariantMap Entry;
            Entry[QStringLiteral("id")] = QString::fromStdString(Deck.Id);
            Entry[QStringLiteral("name")] = QString::fromStdString(Deck.Name);
            Entry[QStringLiteral("description")] = QString::fromStdString(Deck.Description);
            Entry[QStringLiteral("categoryId")] = QString::fromStdString(Deck.CategoryId);
            Entry[QStringLiteral("dailyLearning")] = Deck.bDailyLearning;
            Entry[QStringLiteral("createdDate")] = static_cast<qlonglong>(Deck.CreatedDate);
            Entry[QStringLiteral("icon")] = QStringLiteral("nav_decks");
            Entry[QStringLiteral("wordCount")] = Deck.CardCount;
            Entry[QStringLiteral("newCount")] = Deck.PoolCount;
            Entry[QStringLiteral("learningCount")] = Deck.LearningCount;
            Entry[QStringLiteral("learnedCount")] = Deck.LearnedCount;
            Entry[QStringLiteral("untriagedCount")] = 0;

            UpdatedDecks.append(Entry);
        }

        for (const FTUserDeckCategoryRow& Category : Repository.ListCategories())
        {
            QVariantMap Entry;
            Entry[QStringLiteral("id")] = QString::fromStdString(Category.Id);
            Entry[QStringLiteral("name")] = QString::fromStdString(Category.Name);
            Entry[QStringLiteral("colour")] = Category.Colour;
            Entry[QStringLiteral("count")] = DeckCounts[Category.Id];

            UpdatedCategories.append(Entry);
        }
    }

    if (UpdatedCategories == Categories && UpdatedDecks == Decks)
    {
        return;
    }

    Categories = std::move(UpdatedCategories);
    Decks = std::move(UpdatedDecks);

    qCInfo(LogApp) << "User decks:" << Decks.size() << "decks in" << Categories.size() << "categories";

    emit changed();
}

QString UserDecksController::createCategory(const QString& Name, int Colour)
{
    if (!AppDatabase::Get().IsReady())
    {
        return {};
    }

    FTUserDeckRepository& Repository = AppDatabase::Get().GetUserDecks();
    const std::optional<std::string> Id = Repository.CreateCategory(Name.trimmed().toStdString(), Colour);

    if (!Id.has_value())
    {
        qCWarning(LogApp) << "Could not create the deck category" << Name << ":" << QString::fromStdString(Repository.GetLastError());

        return {};
    }

    qCInfo(LogApp) << "Created the deck category" << Name;

    refresh();

    return QString::fromStdString(*Id);
}

bool UserDecksController::renameCategory(const QString& Id, const QString& Name)
{
    if (!AppDatabase::Get().IsReady() || !AppDatabase::Get().GetUserDecks().RenameCategory(Id.toStdString(), Name.trimmed().toStdString()))
    {
        qCWarning(LogApp) << "Could not rename the deck category" << Id;

        return false;
    }

    refresh();

    return true;
}

bool UserDecksController::deleteCategory(const QString& Id)
{
    if (!AppDatabase::Get().IsReady() || !AppDatabase::Get().GetUserDecks().DeleteCategory(Id.toStdString()))
    {
        qCWarning(LogApp) << "Could not delete the deck category" << Id;

        return false;
    }

    qCInfo(LogApp) << "Deleted the deck category" << Id << "and returned its decks to My Decks";

    refresh();

    return true;
}

QString UserDecksController::createDeck(const QString& Name, const QString& CategoryId, const QString& Description)
{
    if (!AppDatabase::Get().IsReady())
    {
        return {};
    }

    FTUserDeckRepository& Repository = AppDatabase::Get().GetUserDecks();
    const std::optional<std::string> Id{Repository.CreateDeck(Name.trimmed().toStdString(), CategoryId.toStdString(),
                                                             Description.trimmed().toStdString())};

    if (!Id.has_value())
    {
        qCWarning(LogApp) << "Could not create the deck" << Name << ":" << QString::fromStdString(Repository.GetLastError());

        return {};
    }

    qCInfo(LogApp) << "Created the deck" << Name;

    refresh();

    return QString::fromStdString(*Id);
}

bool UserDecksController::renameDeck(const QString& Id, const QString& Name)
{
    if (!AppDatabase::Get().IsReady() || !AppDatabase::Get().GetUserDecks().RenameDeck(Id.toStdString(), Name.trimmed().toStdString()))
    {
        qCWarning(LogApp) << "Could not rename the deck" << Id;

        return false;
    }

    refresh();

    return true;
}

bool UserDecksController::editDeck(const QString& Id, const QString& Name, const QString& Description)
{
    if (!AppDatabase::Get().IsReady()
        || !AppDatabase::Get().GetUserDecks().EditDeck(Id.toStdString(), Name.trimmed().toStdString(),
                                                       Description.trimmed().toStdString()))
    {
        qCWarning(LogApp) << "Could not edit the deck" << Id;

        return false;
    }

    qCInfo(LogApp) << "Edited the deck" << Id;

    refresh();

    return true;
}

bool UserDecksController::setDailyLearning(const QString& Id, bool bDailyLearning)
{
    if (!AppDatabase::Get().IsReady()
        || !AppDatabase::Get().GetUserDecks().SetDailyLearning(Id.toStdString(), bDailyLearning))
    {
        qCWarning(LogApp) << "Could not change daily learning of the deck" << Id;

        return false;
    }

    qCInfo(LogApp) << "Daily learning of the deck" << Id << "is now" << bDailyLearning;

    refresh();

    return true;
}

bool UserDecksController::moveDeck(const QString& Id, const QString& CategoryId)
{
    if (!AppDatabase::Get().IsReady() || !AppDatabase::Get().GetUserDecks().MoveDeck(Id.toStdString(), CategoryId.toStdString()))
    {
        qCWarning(LogApp) << "Could not move the deck" << Id;

        return false;
    }

    refresh();

    return true;
}

bool UserDecksController::deleteDeck(const QString& Id)
{
    if (!AppDatabase::Get().IsReady() || !AppDatabase::Get().GetUserDecks().DeleteDeck(Id.toStdString()))
    {
        qCWarning(LogApp) << "Could not delete the deck" << Id;

        return false;
    }

    qCInfo(LogApp) << "Deleted the deck" << Id << "and kept its cards in My Vocabulary";

    refresh();

    return true;
}

QVariantMap UserDecksController::importFromFile(const QUrl& File, const QString& CategoryId)
{
    if (!AppDatabase::Get().IsReady())
    {
        return MakeFailedImport();
    }

    const std::unique_ptr<FTImportSource> Source = ReadWordList(File);

    if (!Source)
    {
        return MakeFailedImport();
    }

    QString Name = QFileInfo(File.isLocalFile() ? File.toLocalFile() : File.fileName()).completeBaseName();

    if (Name.isEmpty())
    {
        Name = tr("Imported deck");
    }

    return ImportWords(*Source, Name, [&Name, &CategoryId]()
    {
        return AppDatabase::Get().GetUserDecks().CreateDeck(Name.toStdString(), CategoryId.toStdString());
    });
}

QVariantMap UserDecksController::importIntoDeck(const QUrl& File, const QString& DeckId)
{
    if (!AppDatabase::Get().IsReady())
    {
        return MakeFailedImport();
    }

    const std::vector<FTUserDeckRow> Rows = AppDatabase::Get().GetUserDecks().ListDecks();
    const auto Deck = std::find_if(Rows.begin(), Rows.end(), [Id = DeckId.toStdString()](const FTUserDeckRow& Row)
    {
        return Row.Id == Id;
    });

    if (Deck == Rows.end())
    {
        qCWarning(LogApp) << "Cannot import into the missing deck" << DeckId;

        return MakeFailedImport();
    }

    const std::unique_ptr<FTImportSource> Source = ReadWordList(File);

    if (!Source)
    {
        return MakeFailedImport();
    }

    return ImportWords(*Source, QString::fromStdString(Deck->Name), [&Deck]()
    {
        return std::optional<std::string>{Deck->Id};
    });
}

QVariantMap UserDecksController::ImportWords(const FTImportSource& Source, const QString& Name,
                                             const std::function<std::optional<std::string>()>& ChooseDeck)
{
    const auto MakeKey = [](const std::string& Word, const std::string& Translation)
    {
        return Word + '	' + Translation;
    };

    std::unordered_map<std::string, std::string> CardIds;

    for (const FTCardHandle& Card : AppCards::LoadCards())
    {
        CardIds.emplace(MakeKey(Card->TargetWord, Card->Translation), Card->Id);
    }

    FTDatabase& Database = AppDatabase::Get().GetConnection();
    FTUserDeckRepository& Decks = AppDatabase::Get().GetUserDecks();
    FTCardRepository& Cards = AppDatabase::Get().GetCardRepository();

    if (!Database.BeginTransaction())
    {
        return MakeFailedImport();
    }

    const std::optional<std::string> DeckId = ChooseDeck();

    if (!DeckId.has_value())
    {
        Database.Rollback();

        qCWarning(LogApp) << "Could not create the deck" << Name << ":" << QString::fromStdString(Decks.GetLastError());

        return MakeFailedImport();
    }

    int Added = 0;
    int Reused = 0;
    int Moved = 0;

    for (const FTImportRow& Row : Source.GetRows())
    {
        const std::string Key = MakeKey(Row.TargetWord, Row.Translation);
        const auto Existing = CardIds.find(Key);

        std::string CardId;

        if (Existing != CardIds.end())
        {
            CardId = Existing->second;
            ++Reused;

            const std::optional<std::string> PreviousDeck = Decks.FindDeckOf(CardId);

            if (PreviousDeck.has_value() && *PreviousDeck != *DeckId)
            {
                ++Moved;
            }
        }
        else
        {
            const std::optional<std::string> Created = Cards.AddCard(AppCards::MakeAddedWord(AppDictionary::GetActive(), Row.TargetWord, Row.Translation, {}, -1, false, {}));

            if (!Created.has_value())
            {
                continue;
            }

            CardId = *Created;
            CardIds.emplace(Key, CardId);
            ++Added;
        }

        Decks.AddCard(*DeckId, CardId);
    }

    if (!Database.Commit())
    {
        Database.Rollback();

        qCWarning(LogApp) << "Could not save the import into the deck" << Name;

        return MakeFailedImport();
    }

    qCInfo(LogApp) << "Imported into the deck" << Name << ":" << Added << "new cards," << Reused
                   << "already in My Vocabulary," << Moved << "of them moved from another deck";

    refresh();

    QVariantMap Result;
    Result[QStringLiteral("ok")] = true;
    Result[QStringLiteral("deckId")] = QString::fromStdString(*DeckId);
    Result[QStringLiteral("name")] = Name;
    Result[QStringLiteral("added")] = Added;
    Result[QStringLiteral("reused")] = Reused;
    Result[QStringLiteral("moved")] = Moved;

    return Result;
}

QString UserDecksController::sessionKey(const QString& DeckId) const
{
    return QStringLiteral("deck:") + DeckId;
}

QStringList UserDecksController::collectSessionCards(const QString& DeckId) const
{
    QStringList Result;

    if (!AppDatabase::Get().IsReady())
    {
        return Result;
    }

    const std::vector<std::string> DeckCards = AppDatabase::Get().GetUserDecks().ListCardIds(DeckId.toStdString());
    const std::unordered_set<std::string> InDeck{DeckCards.begin(), DeckCards.end()};

    std::vector<FTCardHandle> Pool = AppDatabase::Get().GetCardRepository().LoadUnlearnedCards();

    std::erase_if(Pool, [&InDeck](const FTCardHandle& Card)
    {
        return !InDeck.contains(Card->Id);
    });

    const int WordCount = FTSettings::Get().GetIntensiveLearningRules().TrainingWordCount;

    for (const FTCardHandle& Card : PickTrainingCards(Pool, static_cast<size_t>(std::max(WordCount, 0))))
    {
        Result.append(QString::fromStdString(Card->Id));
    }

    qCInfo(LogSession) << "Learning the deck" << DeckId << "picks" << Result.size() << "cards";

    return Result;
}

QVariantList UserDecksController::GetCategories() const
{
    return Categories;
}

QVariantList UserDecksController::GetDecks() const
{
    return Decks;
}
