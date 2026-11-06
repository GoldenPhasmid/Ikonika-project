#pragma once

#include "ExerciseControllerBase.h"

#include "core/FTSentenceTree.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <string>
#include <vector>

using FSentenceSource = std::function<FTTreeSentence(const FTCard& Card)>;

class SentenceTilesController : public ExerciseControllerBase
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QStringList tiles READ GetTiles NOTIFY stateChanged)
    Q_PROPERTY(QVariantList placed READ GetPlaced NOTIFY stateChanged)
    Q_PROPERTY(QString lastTranslation READ GetLastTranslation NOTIFY stateChanged)
    Q_PROPERTY(bool typingAllowed READ IsTypingAllowed CONSTANT)
    Q_PROPERTY(bool typing READ IsTyping WRITE SetTyping NOTIFY typingChanged)
    Q_PROPERTY(bool buildsTranslation READ IsBuildingTranslation CONSTANT)
    Q_PROPERTY(bool canSubmit READ CanSubmit NOTIFY stateChanged)

public:
    static constexpr int DistractorCount = 3;

    explicit SentenceTilesController(QObject* Parent = nullptr);

    void SetSentenceSource(FSentenceSource InSentenceSource);
    void SetTypingAllowed(bool bAllowed);
    void SetBuildsTranslation(bool bBuilds);

    Q_INVOKABLE void startSession();
    Q_INVOKABLE void placeTile(int TileIndex);
    Q_INVOKABLE void removeTile(int TileIndex);
    Q_INVOKABLE void submit();
    Q_INVOKABLE void skip();
    Q_INVOKABLE void advance() override;
    Q_INVOKABLE void replay();
    Q_INVOKABLE void submitTyped(const QString& Given);

    QString GetPrompt() const override;
    QString GetAnswer() const override;
    bool ShowsTargetLanguage() const override;

    QStringList GetTiles() const;
    QVariantList GetPlaced() const;
    QString GetLastTranslation() const;
    bool IsTypingAllowed() const;
    bool IsBuildingTranslation() const;
    bool CanSubmit() const;
    bool IsTyping() const;
    void SetTyping(bool bTyping);

signals:
    void typingChanged();

private:
    FTTreeSentence GetCurrentSentence() const;

    FSentenceSource SentenceSource;

    void InitializeSession() override;
    void ApplySessionSettings() override;

    QJsonObject SaveExtraState() const override;
    void RestoreExtraState(const QJsonObject& Json) override;

    std::vector<std::string> PickDistractors() const;
    void BuildTiles(const std::vector<std::string>& InDistractors);
    bool IsPlacedAnswerCorrect() const;
    void Reveal(bool bCorrect, const QString& Given);

    std::vector<std::string> Words;
    std::vector<std::string> Distractors;
    size_t AnswerWordCount = 0;
    std::vector<int> TileOrder;
    std::vector<int> Placed;
    QString LastTranslation;
    bool bTypingAllowed = false;
    bool bBuildsTranslation = false;
};
