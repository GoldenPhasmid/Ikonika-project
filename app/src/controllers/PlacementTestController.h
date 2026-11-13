#pragma once

#include "core/FTPlacementTest.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class PlacementTestController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString prompt READ GetPrompt NOTIFY stateChanged)
    Q_PROPERTY(QString lastRevealedAnswer READ GetLastRevealedAnswer NOTIFY stateChanged)
    Q_PROPERTY(bool answered READ IsAnswered NOTIFY stateChanged)
    Q_PROPERTY(QString partOfSpeech READ GetPartOfSpeech NOTIFY stateChanged)
    Q_PROPERTY(QString gender READ GetGender NOTIFY stateChanged)
    Q_PROPERTY(int headwordGender READ GetHeadwordGender NOTIFY stateChanged)
    Q_PROPERTY(QString headwordVerbGroup READ GetHeadwordVerbGroup NOTIFY stateChanged)
    Q_PROPERTY(QString cardMeta READ GetCardMeta NOTIFY stateChanged)
    Q_PROPERTY(QString definition READ GetDefinition CONSTANT)
    Q_PROPERTY(QString example READ GetExample CONSTANT)
    Q_PROPERTY(QString exampleTranslation READ GetExampleTranslation CONSTANT)
    Q_PROPERTY(int currentSessionCount READ GetCurrentSessionCount NOTIFY stateChanged)
    Q_PROPERTY(int maxSessionCount READ GetMaxSessionCount CONSTANT)
    Q_PROPERTY(double exerciseProgress READ GetExerciseProgress NOTIFY stateChanged)
    Q_PROPERTY(int elapsedSeconds READ GetElapsedSeconds CONSTANT)
    Q_PROPERTY(bool finished READ IsFinished NOTIFY stateChanged)
    Q_PROPERTY(int knownWordCount READ GetKnownWordCount NOTIFY stateChanged)
    Q_PROPERTY(QString cefrLevel READ GetCefrLevel NOTIFY stateChanged)
    Q_PROPERTY(int answeredCount READ GetAnsweredCount NOTIFY stateChanged)
    Q_PROPERTY(int knownAnswerCount READ GetKnownAnswerCount NOTIFY stateChanged)
    Q_PROPERTY(QVariantList answers READ GetAnswers NOTIFY stateChanged)

public:
    explicit PlacementTestController(QObject* Parent = nullptr);

    Q_INVOKABLE bool isOffered(const QString& TargetLanguage, const QString& NativeLanguage) const;

    static bool HasWordListFor(const QString& TargetLanguage, const QString& NativeLanguage);

    static bool IsOfferedFor(const QString& TargetLanguage, const QString& NativeLanguage);

    Q_INVOKABLE bool start(const QString& TargetLanguage, const QString& NativeLanguage, int Level);

    Q_INVOKABLE void markNew(const QString& TargetLanguage);

    Q_INVOKABLE void reveal();
    Q_INVOKABLE void answerYes();
    Q_INVOKABLE void answerNo();
    Q_INVOKABLE void restart();

    QString GetPrompt() const;
    QString GetLastRevealedAnswer() const;
    bool IsAnswered() const;
    QString GetPartOfSpeech() const;
    QString GetGender() const;
    int GetHeadwordGender() const;
    QString GetHeadwordVerbGroup() const;
    QString GetCardMeta() const;
    QString GetDefinition() const { return {}; }
    QString GetExample() const { return {}; }
    QString GetExampleTranslation() const { return {}; }
    int GetCurrentSessionCount() const;
    int GetMaxSessionCount() const;
    double GetExerciseProgress() const;
    int GetElapsedSeconds() const;
    bool IsFinished() const;
    int GetKnownWordCount() const;
    QString GetCefrLevel() const;
    int GetAnsweredCount() const;
    int GetKnownAnswerCount() const;
    QVariantList GetAnswers() const;

signals:
    void stateChanged();

private:
    void Answer(bool bKnown);

    void SpeakPrompt() const;

    FTPlacementTest Test;

    QString TargetLanguage;
    QString NativeLanguage;

    int Level = 1;

    bool bRevealed = false;
};
