#pragma once

#include "core/FTCardStore.h"

#include "ExerciseControllerBase.h"

#include "core/sessions/FTExerciseSession.h"

#include <QVariantList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <string>
#include <vector>

class FlashcardController : public ExerciseControllerBase
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit FlashcardController(QObject* Parent = nullptr);

    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void replay();

    Q_PROPERTY(bool canGoNext READ CanGoNext NOTIFY stateChanged)
    bool CanGoNext() const;
    
    Q_PROPERTY(bool canGoPrevious READ CanGoPrevious NOTIFY stateChanged)
    bool CanGoPrevious() const;

    const FTCard* GetCurrentCard() const override;
    QString GetPrompt() const override;
    QString GetAnswer() const override;
    int GetCurrentSessionCount() const override;
    bool removeCard(const QString& CardId) override;

private:
    void InitializeSession() override;

    QJsonObject SaveExtraState() const override;
    void RestoreExtraState(const QJsonObject& Json) override;

    int CurrentCardIndex() const;

    mutable std::vector<int> History;
    int HistoryPosition = -1;
};
