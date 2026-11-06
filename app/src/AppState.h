#pragma once

#include <QByteArray>
#include <QString>

#include <map>

class AppState
{
public:
    static AppState& Get();

    std::map<QString, QByteArray> IntensiveLearningCheckpoints;
    QByteArray RepetitionCheckpoint;
    std::map<QString, QByteArray> SentenceCheckpoints;

private:
    AppState() = default;
};
