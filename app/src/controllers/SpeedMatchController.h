#pragma once

#include "core/FTCardStore.h"

#include "ExerciseControllerBase.h"

#include "core/sessions/FTExerciseSession.h"

#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>

struct FMatchPair
{
    std::string Left;
    std::string Right;
    QString RightMarkup;
    QString LeftMarkup;
};

using FMatchPairText = std::function<FMatchPair(const FTCard& Card)>;
using FMatchScreenKey = std::function<std::string(const FTCard& Card)>;
using FMatchHeading = std::function<QString(const FTCard& Card)>;

class SpeedMatchController : public ExerciseControllerBase
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QStringList leftVerbGroups READ GetLeftVerbGroups NOTIFY stateChanged)
    Q_PROPERTY(QStringList leftItems READ GetLeftItems NOTIFY stateChanged)
    Q_PROPERTY(QStringList rightItems READ GetRightItems NOTIFY stateChanged)
    Q_PROPERTY(QStringList rightMarkups READ GetRightMarkups NOTIFY stateChanged)
    Q_PROPERTY(QStringList leftMarkups READ GetLeftMarkups NOTIFY stateChanged)

    Q_PROPERTY(QList<bool> leftMatched READ GetLeftMatched NOTIFY stateChanged)
    Q_PROPERTY(QList<bool> rightMatched READ GetRightMatched NOTIFY stateChanged)

    Q_PROPERTY(int selectedLeftRow READ GetSelectedLeftRow NOTIFY stateChanged)
    Q_PROPERTY(int selectedRightRow READ GetSelectedRightRow NOTIFY stateChanged)

    Q_PROPERTY(int flashedLeftRow READ GetFlashedLeftRow NOTIFY stateChanged)
    Q_PROPERTY(int flashedRightRow READ GetFlashedRightRow NOTIFY stateChanged)

    Q_PROPERTY(bool voiceLeft READ IsVoiceLeft NOTIFY stateChanged)

    Q_PROPERTY(bool awaitingAnswer READ IsAwaitingAnswer NOTIFY stateChanged)
    Q_PROPERTY(bool showingFeedback READ IsShowingFeedback NOTIFY stateChanged)

    Q_PROPERTY(int wrongPairCount READ GetWrongPairCount NOTIFY stateChanged)

    Q_PROPERTY(QString heading READ GetHeading NOTIFY stateChanged)

public:
    explicit SpeedMatchController(QObject* Parent = nullptr);

    void SetPairText(FMatchPairText InPairText);
    void SetPairCount(int InPairCount);

    /** Cards sharing a key fill one screen together, and a screen never holds two keys. */
    void SetScreenKey(FMatchScreenKey InScreenKey);

    void SetScreenHeading(FMatchHeading InHeading);

    /** A side kept in the order of the cards instead of shuffled, so its rows stay where they were. */
    void SetOrderedSides(bool bInLeftOrdered, bool bInRightOrdered);
    
    Q_INVOKABLE void tapLeft(int Row);
    Q_INVOKABLE void tapRight(int Row);

    Q_INVOKABLE void advance() override;
    Q_INVOKABLE void speakHeading();

    bool IsFinished() const override;
    int GetCurrentSessionCount() const override;

    QStringList GetLeftItems() const;
    QStringList GetLeftVerbGroups() const;
    QStringList GetRightItems() const;
    QStringList GetRightMarkups() const;
    QStringList GetLeftMarkups() const;
    QList<bool> GetLeftMatched() const;
    QList<bool> GetRightMatched() const;

    int GetSelectedLeftRow() const;
    int GetSelectedRightRow() const;
    int GetFlashedLeftRow() const;
    int GetFlashedRightRow() const;

    bool IsVoiceLeft() const;
    bool SupportsVoicePrompt() const override;
    bool IsAwaitingAnswer() const;
    bool IsShowingFeedback() const;

    int GetWrongPairCount() const;
    QString GetHeading() const;

protected:
    const FTCard* GetFormsCard() const override;

private:
    void InitializeSession() override;
    void ApplySessionSettings() override;
    void BuildScreen();
    void TryPendingPair();
    void RememberRightTexts();
    void UpdateHeading();
    const FTCard* FindScreenCard() const;
    int CountEntriesLeftInPass() const;
    bool IsBatchComplete() const;

    QJsonObject SaveExtraState() const override;
    void RestoreExtraState(const QJsonObject& Json) override;

    std::mt19937 ScreenGenerator{std::random_device{}()};

    int MatchPairCount = 5;

    FMatchPairText PairText;
    FMatchScreenKey ScreenKey;
    FMatchHeading HeadingText;
    QString Heading;
    std::unordered_map<int, QString> RightTextByCard;

    std::vector<int> LeftCardIndices;
    std::vector<int> RightCardIndices;
    QStringList LeftItems;
    QStringList RightItems;
    QStringList RightMarkups;
    QStringList LeftMarkups;
    bool bLeftOrdered = false;
    bool bRightOrdered = false;

    int SelectedLeftRow = -1;
    int SelectedRightRow = -1;

    int FlashedLeftRow = -1;
    int FlashedRightRow = -1;
    
    int MatchedCount = 0;
    int TotalMatchedCount = 0;
};
