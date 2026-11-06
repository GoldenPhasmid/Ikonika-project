#include "SessionSettings.h"

#include "core/FTLog.h"

#include <QJSEngine>
#include <QVariant>

SessionSettings::SessionSettings(std::string InGroup)
    : Group(std::move(InGroup))
{
    QJSEngine::setObjectOwnership(this, QJSEngine::CppOwnership);
}

SessionSettings& SessionSettings::Training()
{
    static SessionSettings Instance(TrainingSessionGroup);

    return Instance;
}

SessionSettings& SessionSettings::Review()
{
    static SessionSettings Instance(ReviewSessionGroup);

    return Instance;
}

const FTSessionExerciseRules& SessionSettings::GetRules() const
{
    return FTSettings::Get().GetSessionExercises(Group);
}

const std::string& SessionSettings::GetGroup() const
{
    return Group;
}

bool SessionSettings::IsReview() const
{
    return Group == ReviewSessionGroup;
}

void SessionSettings::Update(const char* Name, const QVariant& Value,
                             const std::function<void(FTSessionExerciseRules&)>& Change)
{
    FTSessionExerciseRules Wanted = GetRules();
    Change(Wanted);

    if (!Wanted.bTypeWordExercise && !Wanted.bGapExercise && !Wanted.bAudioExercise)
    {
        qCInfo(LogApp) << "Setting refused:" << QString::fromStdString(Group) << Name
                       << "would leave no typing exercise";

        return;
    }

    if (Wanted == GetRules())
    {
        return;
    }

    FTSettings::Get().SetSessionExercises(Group, Wanted);

    qCInfo(LogApp) << "Setting changed:" << QString::fromStdString(Group) << Name << "->" << Value;

    emit changed();
}

int SessionSettings::GetFlipCount() const
{
    return GetRules().FlipCount;
}

void SessionSettings::SetFlipCount(int Value)
{
    Update("flip count", Value, [Value](FTSessionExerciseRules& Rules) { Rules.FlipCount = Value; });
}

int SessionSettings::GetMaximumFlipCount() const
{
    return MaximumFlipCount;
}

bool SessionSettings::IsChoiceStage() const
{
    return GetRules().bChoiceStage;
}

void SessionSettings::SetChoiceStage(bool bValue)
{
    Update("choice stage", bValue, [bValue](FTSessionExerciseRules& Rules) { Rules.bChoiceStage = bValue; });
}

int SessionSettings::GetChoiceCount() const
{
    return GetRules().ChoiceCount;
}

void SessionSettings::SetChoiceCount(int Value)
{
    Update("choice count", Value, [Value](FTSessionExerciseRules& Rules) { Rules.ChoiceCount = Value; });
}

int SessionSettings::GetChoiceSide() const
{
    return GetRules().ChoiceSide;
}

void SessionSettings::SetChoiceSide(int Value)
{
    if (Value < 0 || Value >= QuestionSideCount)
    {
        return;
    }

    Update("choice side", Value, [Value](FTSessionExerciseRules& Rules) { Rules.ChoiceSide = Value; });
}

bool SessionSettings::IsChoiceVoice() const
{
    return GetRules().bChoiceVoice;
}

void SessionSettings::SetChoiceVoice(bool bValue)
{
    Update("choice voice", bValue, [bValue](FTSessionExerciseRules& Rules) { Rules.bChoiceVoice = bValue; });
}

int SessionSettings::GetRepeatsToMaster() const
{
    return GetRules().RepeatsToMaster;
}

void SessionSettings::SetRepeatsToMaster(int Value)
{
    Update("repeats to master", Value, [Value](FTSessionExerciseRules& Rules) { Rules.RepeatsToMaster = Value; });
}

bool SessionSettings::IsTypeWord() const
{
    return GetRules().bTypeWordExercise;
}

void SessionSettings::SetTypeWord(bool bValue)
{
    Update("type word", bValue, [bValue](FTSessionExerciseRules& Rules) { Rules.bTypeWordExercise = bValue; });
}

bool SessionSettings::IsGapExercise() const
{
    return GetRules().bGapExercise;
}

void SessionSettings::SetGapExercise(bool bValue)
{
    Update("gap exercise", bValue, [bValue](FTSessionExerciseRules& Rules) { Rules.bGapExercise = bValue; });
}

bool SessionSettings::IsAudioExercise() const
{
    return GetRules().bAudioExercise;
}

void SessionSettings::SetAudioExercise(bool bValue)
{
    Update("audio exercise", bValue, [bValue](FTSessionExerciseRules& Rules) { Rules.bAudioExercise = bValue; });
}

bool SessionSettings::AreExactAccentsRequired() const
{
    return GetRules().bRequireExactAccents;
}

void SessionSettings::SetRequireExactAccents(bool bValue)
{
    Update("require exact accents", bValue,
           [bValue](FTSessionExerciseRules& Rules) { Rules.bRequireExactAccents = bValue; });
}

bool SessionSettings::IsSentenceStage() const
{
    return GetRules().bSentenceStage;
}

void SessionSettings::SetSentenceStage(bool bValue)
{
    Update("sentence stage", bValue, [bValue](FTSessionExerciseRules& Rules) { Rules.bSentenceStage = bValue; });
}

bool SessionSettings::IsSentenceTyping() const
{
    return GetRules().bSentenceTyping;
}

void SessionSettings::SetSentenceTyping(bool bValue)
{
    Update("sentence typing", bValue, [bValue](FTSessionExerciseRules& Rules) { Rules.bSentenceTyping = bValue; });
}

bool SessionSettings::IsMatchStage() const
{
    return GetRules().bMatchStage;
}

void SessionSettings::SetMatchStage(bool bValue)
{
    Update("match stage", bValue, [bValue](FTSessionExerciseRules& Rules) { Rules.bMatchStage = bValue; });
}

int SessionSettings::GetMatchCount() const
{
    return GetRules().MatchCount;
}

void SessionSettings::SetMatchCount(int Value)
{
    Update("match count", Value, [Value](FTSessionExerciseRules& Rules) { Rules.MatchCount = Value; });
}

bool SessionSettings::IsMatchVoice() const
{
    return GetRules().bMatchVoice;
}

void SessionSettings::SetMatchVoice(bool bValue)
{
    Update("match voice", bValue, [bValue](FTSessionExerciseRules& Rules) { Rules.bMatchVoice = bValue; });
}

int SessionSettings::GetMaximumStageRepeatCount() const
{
    return MaximumStageRepeatCount;
}

void SessionSettings::NotifyDisplayChanged()
{
    emit Training().changed();
    emit Review().changed();
}
