#pragma once

#include "core/FTCardStore.h"
#include "core/FTSentenceTree.h"
#include "core/sessions/FTExerciseSession.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class FCardState;
class SentenceTilesController;
class SessionSettings;

struct FSessionSentence
{
    FTCardHandle Card;
    FTTreeSentence Sentence;
    std::string WordCardId;
};

struct FSessionSentenceStage
{
    std::vector<SentenceTilesController*> Controllers;
    std::vector<FSessionSentence> Sentences;
};

using FGapSentences = std::unordered_map<std::string, FTTreeSentence>;

std::optional<FTTreeSentence> PickGapSentence(const std::vector<FTTreeSentence>& Candidates, const FTCard& Card,
                                             const std::vector<std::string>& Spellings, bool bPrioritizeTranslation);

FGapSentences PickGapSentences(const FCardState& WordCards);

QJsonObject GapSentencesToJson(const FGapSentences& Sentences);

FGapSentences GapSentencesFromJson(const QJsonObject& Saved);

FSessionSentenceStage BuildSessionSentenceStage(QObject* Parent, const FTSessionStartParams& Params,
                                                SessionSettings& Options, std::vector<FSessionSentence> Saved = {});

QJsonArray SessionSentencesToJson(const std::vector<FSessionSentence>& Sentences);

std::vector<FSessionSentence> SessionSentencesFromJson(const QJsonArray& Saved);
