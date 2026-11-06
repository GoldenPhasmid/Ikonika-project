#pragma once

#include "ExerciseControllerBase.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

class ExerciseLauncherController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(ExerciseControllerBase* activeController READ GetActiveController NOTIFY activeControllerChanged)

public:
    explicit ExerciseLauncherController(QObject* Parent = nullptr);

    Q_INVOKABLE bool startExercise(const QString& Exercise, bool bReverse = false, int CardCount = -1,
        int RepetitionCount = -1, const QStringList& FilterKeys = {});

    ExerciseControllerBase* GetActiveController() const;

signals:
    void activeControllerChanged();

private:
    ExerciseControllerBase* CreateController(const QString& Exercise);

    ExerciseControllerBase* ActiveController = nullptr;
};
