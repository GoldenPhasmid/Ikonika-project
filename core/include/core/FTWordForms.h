#pragma once

#include "core/FTCard.h"
#include "core/FTCardStore.h"
#include "core/FTDictionaryEntry.h"
#include "core/FTSentenceTree.h"

#include <optional>
#include <string>
#include <vector>

inline constexpr const char* FormsKindVerb = "verb";
inline constexpr const char* FormsKindAdjective = "adj";
inline constexpr const char* FormsKindNoun = "noun";

inline constexpr const char* FormsTrainingConjugation = "conjugation";
inline constexpr const char* FormsTrainingAgreement = "forms";

inline constexpr const char* FormsTensePresent = "present";

struct FTWordForm
{
    std::string Subject;
    std::string SubjectLabel;
    std::string Base;
    std::string Meaning;
    std::string Form;
    std::string Stem;
    std::string Ending;
    std::string Headword;
    bool bFeminine = false;
    bool bPlural = false;
};

std::vector<std::string> ListFormsKinds();

bool SupportsForms(const std::string& Kind, const std::string& TargetLanguage);

std::vector<std::string> ListFormsTrainings();

std::vector<std::string> ListFormsTrainingKinds(const std::string& Training);

bool SupportsFormsTraining(const std::string& Training, const std::string& TargetLanguage);

int GetFormsTrainingSize(const std::string& Training);

std::string MakeFormsTrainingId(const std::string& Training, const std::string& TargetLanguage,
                                const std::string& NativeLanguage);

bool IsFormsTrainingId(const std::string& TrainingId);

std::string GetFormsTraining(const std::string& CardOrTrainingId);

std::string GetFormsKind(const std::string& CardId);

std::string GetFormsCardIdPrefix(const std::string& Kind);

std::vector<std::string> ListFormsTenses(const std::string& TargetLanguage);

std::string GetFormsTenseLabel(const std::string& TargetLanguage, const std::string& Tense);

std::string GetFormsTenseRequirement(const std::string& TargetLanguage, const std::string& Tense);

int GetFormsTenseCode(const std::string& TargetLanguage, const std::string& Tense);

std::string GetFormsTense(const std::string& CardId);

std::string GetFormsCardTenseLabel(const std::string& CardId);

bool IsFormsCard(const FTCard& Subject);

bool IsFormsItemId(const std::string& CardId);

std::string GetFormsCardId(const std::string& ItemId);

std::string MakeFormsRoundId(const std::string& ItemId, int Round);

std::string GetFormsItemId(const std::string& CardId);

struct FTFormGap
{
    int Start = -1;
    int Length = 0;
    bool bSubjectSpoken = false;
};

std::optional<FTFormGap> LocateFormGap(const FTCard& Item, const FTTreeSentence& Sentence);

bool PlaceFormGap(const FTCard& Item, FTTreeSentence& Sentence);

std::optional<FTWordForm> ReadWordForm(const FTCard& Item);

struct FTFormsSentenceKey
{
    std::string Headword;
    std::string Kind;
    std::string Tense;
    int First = 0;
    int Number = 0;
};

std::optional<FTFormsSentenceKey> ReadFormsSentenceKey(const FTCard& Item);

std::vector<std::string> ListAcceptedForms(const FTCard& Item);

std::string GetOptionalFormsPronoun(const std::string& TargetLanguage);

bool IsOptionalForm(const FTCard& Item);

std::string GetVerbGroup(const std::string& TargetLanguage, const FTDictionaryEntry& Entry);

bool IsIrregularVerb(const std::string& TargetLanguage, const FTDictionaryEntry& Entry);

bool IsDerivedVerb(const std::string& TargetLanguage, const std::string& Tense, const FTDictionaryEntry& Entry,
                   const FTDictionaryEntry& Base);

std::string MakeFormPrompt(const std::string& Cue);

std::optional<FTCard> MakeFormsCard(const std::string& Kind, const FTDictionaryEntry& Entry,
                                    const std::string& TargetLanguage, const std::string& Translation,
                                    const std::string& Tense = FormsTensePresent);

std::vector<FTCard> ExpandFormsCard(const FTCard& Stored, const FTDictionaryEntry& Entry,
                                    const std::string& TargetLanguage);

std::vector<std::vector<int>> GroupCardsByWord(const std::vector<FTCardHandle>& Cards);

std::vector<FTCardHandle> TakeFormsSession(const std::string& Training, const std::vector<FTCardHandle>& Cards);

std::vector<std::string> ListFormsCues(const std::string& Kind, const std::string& TargetLanguage);

std::string FindFormEnding(const std::string& TargetLanguage, const std::string& Headword, int Mood, int Tense,
                           int CellIndex, const std::string& Form);
