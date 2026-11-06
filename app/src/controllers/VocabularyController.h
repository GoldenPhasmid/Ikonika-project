#pragma once

#include "core/FTCardStore.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <qqmlintegration.h>

#include <vector>

class VocabularyController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int notLearnedCount READ GetNotLearnedCount NOTIFY vocabularyChanged)
    Q_PROPERTY(int learningCount READ GetLearningCount NOTIFY vocabularyChanged)
    Q_PROPERTY(int learnedCount READ GetLearnedCount NOTIFY vocabularyChanged)

    Q_PROPERTY(QString search READ GetSearch WRITE SetSearch NOTIFY viewChanged)
    Q_PROPERTY(QStringList filterKeys READ GetFilterKeys WRITE SetFilterKeys NOTIFY viewChanged)
    Q_PROPERTY(QString sortKey READ GetSortKey WRITE SetSortKey NOTIFY viewChanged)
    Q_PROPERTY(bool sortDescending READ IsSortDescending WRITE SetSortDescending NOTIFY viewChanged)

    Q_PROPERTY(QVariantList visibleCards READ GetVisibleCards NOTIFY viewChanged)
    Q_PROPERTY(int filteredCount READ GetFilteredCount NOTIFY viewChanged)

    Q_PROPERTY(qint64 deckId READ GetDeckId WRITE SetDeckId NOTIFY vocabularyChanged)
    Q_PROPERTY(QString deckName READ GetDeckName NOTIFY vocabularyChanged)
    Q_PROPERTY(int deckNewCount READ GetDeckNewCount NOTIFY vocabularyChanged)

public:
    explicit VocabularyController(QObject* Parent = nullptr);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void resetView();

    Q_INVOKABLE void speakWord(const QString& Word) const;

    Q_INVOKABLE bool removeCard(const QString& CardId);

    Q_INVOKABLE bool resetCardProgress(const QString& CardId);

    Q_INVOKABLE bool completeCardProgress(const QString& CardId);

    Q_INVOKABLE bool addCard(const QString& Word, const QString& Translation,
                             const QString& PartOfSpeech = QString(), int Gender = -1,
                             bool bReadingChosen = false, const QString& MeaningGist = QString(), const QString& Hint = QString());

    Q_INVOKABLE bool updateCard(const QString& CardId, const QString& Word,
                                const QString& Translation, int Gender,
                                const QString& PartOfSpeech, const QString& MeaningGist, const QString& Hint = QString());

    Q_INVOKABLE int findWordTranslationStage(const QString& Word, const QString& Translation) const;

    Q_INVOKABLE bool importFromFile(const QString& Path);

    Q_INVOKABLE int practiceCount(const QStringList& Keys) const;

    static bool MatchesFilter(const FTCard& Card, const QString& Key);
    static bool MatchesAnyFilter(const FTCard& Card, const QStringList& Keys);

    int GetNotLearnedCount() const;
    int GetLearningCount() const;
    int GetLearnedCount() const;

    QString GetSearch() const;
    void SetSearch(const QString& Value);

    QStringList GetFilterKeys() const;
    void SetFilterKeys(const QStringList& Value);

    QString GetSortKey() const;
    void SetSortKey(const QString& Value);

    bool IsSortDescending() const;
    void SetSortDescending(bool Value);

    QVariantList GetVisibleCards() const;
    int GetFilteredCount() const;

    qint64 GetDeckId() const;
    void SetDeckId(qint64 Value);
    QString GetDeckName() const;
    int GetDeckNewCount() const;

signals:
    void vocabularyChanged();
    void viewChanged();

private:
    void Load();
    void ShowCards();
    void LoadDeck();
    void ClearView();
    void RebuildView();

    int CountMatching(const QString& Key) const;

    bool MatchesFilters(const FTCard& Card) const;
    bool MatchesSearch(const FTCard& Card, bool bIncludeTranslation) const;

    std::vector<FTCardHandle> VocabularyCards;
    std::vector<FTCardHandle> Cards;

    qint64 DeckId = 0;
    QString DeckName;
    int DeckNewCount = 0;

    QString Search;
    QStringList FilterKeys;
    QString SortKey = QStringLiteral("word");
    bool bSortDescending = false;

    QVariantList VisibleCards;
};
