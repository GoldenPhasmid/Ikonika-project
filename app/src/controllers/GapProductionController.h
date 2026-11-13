#pragma once

#include "SessionSentenceStage.h"
#include "WrittenProductionController.h"

#include "core/FTSentenceTree.h"

#include <functional>
#include <memory>

using FGapSentenceLookup = std::function<const FTTreeSentence*(const FTCard& Card)>;

FGapSentenceLookup MakeGapSentenceLookup(std::shared_ptr<const FGapSentences> Sentences);

class GapProductionController : public WrittenProductionController
{
    Q_OBJECT

public:
    explicit GapProductionController(QObject* Parent = nullptr);

    void SetSentenceLookup(FGapSentenceLookup InSentenceLookup);

protected:
    const FTTreeSentence* FindSentence(const FTCard& Card) const;
    const FTTreeSentence* FindCurrentSentence() const;

private:
    FGapSentenceLookup SentenceLookup;
};

class WordGapController : public GapProductionController
{
    Q_OBJECT

public:
    explicit WordGapController(QObject* Parent = nullptr);

protected:
    std::optional<FWrittenContent> BuildCardContent(const FTCard& Card) const override;
    FExerciseSpeech GetHiddenSpeech() const override;
    FExerciseSpeech GetRevealSpeech() const override;
};

class SentenceGapController : public GapProductionController
{
    Q_OBJECT

public:
    explicit SentenceGapController(QObject* Parent = nullptr);

protected:
    std::optional<FWrittenContent> BuildCardContent(const FTCard& Card) const override;
    FExerciseSpeech GetHiddenSpeech() const override;
    FExerciseSpeech GetRevealSpeech() const override;
};

class FormGapController : public GapProductionController
{
    Q_OBJECT

public:
    explicit FormGapController(QObject* Parent = nullptr);

protected:
    std::optional<FWrittenContent> BuildCardContent(const FTCard& Card) const override;
    FExerciseSpeech GetRevealSpeech() const override;
};
