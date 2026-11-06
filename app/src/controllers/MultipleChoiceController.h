#pragma once

#include "core/FTCardStore.h"

#include "ExerciseControllerBase.h"

#include "core/sessions/FTExerciseSession.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <string>
#include <vector>

struct FChoiceContent
{
    std::string Prompt;
    std::vector<std::string> Options;
    std::string CorrectOption;
    std::string AnswerSpeech;
    bool bShowsTargetLanguage = false;
};

using FChoiceContentBuilder = std::function<FChoiceContent(const FTExerciseSession& Session, int CardIndex)>;

class MultipleChoiceController : public ExerciseControllerBase
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int correctOptionIndex READ GetCorrectOptionIndex NOTIFY stateChanged)
    Q_PROPERTY(int chosenOptionIndex READ GetChosenOptionIndex NOTIFY stateChanged)
    Q_PROPERTY(QStringList optionVerbGroups READ GetOptionVerbGroups NOTIFY stateChanged)
    Q_PROPERTY(QVariantList options READ GetOptions NOTIFY stateChanged)
    Q_PROPERTY(QString title READ GetTitle CONSTANT)

public:
    explicit MultipleChoiceController(QObject* Parent = nullptr);

    void SetContentBuilder(FChoiceContentBuilder InContentBuilder, const QString& InTitle);

    QString GetTitle() const;
    
    Q_INVOKABLE void advance() override;
    
    Q_INVOKABLE void choose(int OptionIndex);
    Q_INVOKABLE void skip();
    Q_INVOKABLE void replay();

    QVariantList GetOptions() const;
    QStringList GetOptionVerbGroups() const;

    int GetCorrectOptionIndex() const;
    int GetChosenOptionIndex() const;

    QString GetPrompt() const override;
    QString GetAnswer() const override;
    bool ShowsTargetLanguage() const override;
    bool IsVoicePrompt() const override;
    bool SupportsVoicePrompt() const override;

private:
    void InitializeSession() override;
    void ApplySessionSettings() override;
    void BuildOptions();
    void BuildOptionsFromContent(int CurrentIndex);
    void SpeakVoicePrompt() const;
    void SpeakWordAfterReveal();

    QJsonObject SaveExtraState() const override;
    void RestoreExtraState(const QJsonObject& Json) override;

    std::vector<std::string> Options;
    int CorrectOptionIndex = -1;
    int ChosenOptionIndex = -1;

    bool bShowWord = false;

    FChoiceContentBuilder ContentBuilder;
    std::string ContentPrompt;
    std::string ContentAnswerSpeech;
    QString Title;

    std::mt19937 OptionGenerator{std::random_device{}()};
};