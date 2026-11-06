#pragma once

#include "core/FTCard.h"
#include "core/FTCardRepository.h"
#include "core/FTDictionaryRepository.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <cstddef>
#include <optional>
#include <random>
#include <set>
#include <vector>

class TriageController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString word READ GetWord NOTIFY currentChanged)
    Q_PROPERTY(QString displayWord READ GetDisplayWord NOTIFY currentChanged)

    Q_PROPERTY(QString partOfSpeech READ GetPartOfSpeech NOTIFY currentChanged)

    Q_PROPERTY(int headwordGender READ GetHeadwordGender NOTIFY currentChanged)

    Q_PROPERTY(QString translation READ GetTranslation WRITE SetTranslation NOTIFY currentChanged)

    Q_PROPERTY(QVariantList translationOptions READ GetTranslationOptions NOTIFY currentChanged)

    Q_PROPERTY(QVariantList meaningOptions READ GetMeaningOptions NOTIFY currentChanged)

    Q_PROPERTY(QString cardMeta READ GetCardMeta NOTIFY currentChanged)

    Q_PROPERTY(int addedCount READ GetAddedCount NOTIFY progressChanged)
    Q_PROPERTY(QVariantList addedWords READ GetAddedWords NOTIFY progressChanged)
    Q_PROPERTY(bool addedListOpen READ IsAddedListOpen WRITE SetAddedListOpen NOTIFY addedListOpenChanged)
    Q_PROPERTY(bool rangePaneOpen READ IsRangePaneOpen WRITE SetRangePaneOpen NOTIFY rangePaneOpenChanged)

    Q_PROPERTY(int startRank READ GetStartRank WRITE SetStartRank NOTIFY startRankChanged)
    Q_PROPERTY(int maximumRank READ GetMaximumRank NOTIFY startRankChanged)
    Q_PROPERTY(QVariantList levelMarks READ GetLevelMarks NOTIFY startRankChanged)
    Q_PROPERTY(int targetCount READ GetTargetCount NOTIFY progressChanged)

    Q_PROPERTY(int knownCount READ GetKnownCount NOTIFY progressChanged)

    Q_PROPERTY(int skippedCount READ GetSkippedCount NOTIFY progressChanged)

    Q_PROPERTY(bool finished READ IsFinished NOTIFY progressChanged)

    Q_PROPERTY(bool deckMode READ IsDeckMode NOTIFY progressChanged)
    Q_PROPERTY(int progressCount READ GetProgressCount NOTIFY progressChanged)


public:
    explicit TriageController(QObject* Parent = nullptr);

    Q_INVOKABLE bool start(int WantedCount);

    Q_INVOKABLE bool startDeck(qint64 DeckId);

    Q_INVOKABLE void refreshStartRank();



    Q_INVOKABLE void markKnown();

    Q_INVOKABLE void markUnknown();

    Q_INVOKABLE void markSkipped();

    Q_INVOKABLE bool learnMeaning(const QString& Gist, const QString& Translation);

    Q_INVOKABLE void toggleTranslation(int Index);

    Q_INVOKABLE void speakWord();
    Q_INVOKABLE void openWebSearch();

    QString GetWord() const;
    QString GetDisplayWord() const;
    QString GetPartOfSpeech() const;
    QString GetGender() const;
    int GetHeadwordGender() const;
    QString GetCardMeta() const;
    void restart();
    QString GetTranslation() const;
    void SetTranslation(const QString& Text);
    QVariantList GetTranslationOptions() const;
    QVariantList GetMeaningOptions() const;
    int GetAddedCount() const;
    QVariantList GetAddedWords() const;
    bool IsAddedListOpen() const;
    void SetAddedListOpen(bool bOpen);
    bool IsRangePaneOpen() const;
    void SetRangePaneOpen(bool bOpen);
    int GetStartRank() const;
    void SetStartRank(int Rank);
    int GetMaximumRank() const;
    QVariantList GetLevelMarks() const;
    int GetTargetCount() const;
    int GetKnownCount() const;
    int GetSkippedCount() const;
    bool IsFinished() const;
    bool IsDeckMode() const;
    int GetProgressCount() const;

signals:
    void currentChanged();
    void progressChanged();
    void addedListOpenChanged();
    void rangePaneOpenChanged();
    void startRankChanged();

private:
    struct Cursor
    {
        std::string PartOfSpeech;
        int Share = 0;
        int Collected = 0;
        int Offset = 0;
        std::vector<FTLearnableWord> Queue;
        bool bExhausted = false;
    };

    bool BeginGrade(int Index);

    bool AdvanceGrade();

    FTLearnableWordFilter MakeFilter(int InGrade, const std::string& PartOfSpeech) const;

    int PickCursor() const;

    bool FillCursor(Cursor& Target);

    void MoveToNextWord();

    void SelectNextWord();

    void AnnounceCurrentWord();

    int LoadSavedStartRank() const;

    bool BeginRun(int WantedCount);

    void ResetRun();

    void SelectNextDeckWord();




    struct TranslationOption
    {
        std::string Text;
        bool bSelected = false;
        std::optional<double> Probability;
        int LearnedStage = -1;
    };

    void LoadTranslationOptions();

    void LoadMeanings();

    bool AddCardAndAdvance(const FTCard& Card);

    FTLearnableWord Current;

    std::vector<TranslationOption> TranslationOptions;

    std::vector<FTDictionarySense> Meanings;

    bool bTranslationEdited = false;
    QString TranslationOverride;

    int CurrentCursorIndex = -1;

    std::vector<Cursor> Cursors;

    std::set<QString> SkippedHeadwordsThisRun;

    std::set<QString> CollectedHeadwordsThisRun;

    int GradeIndex = 0;

    int StartRank = 0;

    int LastCursorIndex = -1;
    int ConsecutiveOffers = 0;

    int AddedCount = 0;
    QVariantList AddedWords;
    int TargetCount = 0;
    int KnownCount = 0;
    int SkippedCount = 0;

    bool bDeckMode = false;
    qint64 DeckId = 0;
    std::vector<FTDeckWord> DeckQueue;
    std::size_t DeckCursor = 0;
    std::string DeckGist;

    std::mt19937 Random;

};
