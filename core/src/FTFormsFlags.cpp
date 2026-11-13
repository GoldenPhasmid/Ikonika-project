#include "core/FTFormsFlags.h"

#include "core/FTDatabase.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTLog.h"
#include "core/FTWordForms.h"

#include <QString>

#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{
    struct FFlagCandidate
    {
        int64_t WordId = 0;
        std::string Headword;
        std::string PartOfSpeech;
    };

    std::vector<std::string> ListReadKinds()
    {
        std::vector<std::string> Kinds;

        for (const char* Training : {FormsTrainingConjugation, FormsTrainingAgreement})
        {
            for (std::string& Kind : ListFormsTrainingKinds(Training))
            {
                Kinds.push_back(std::move(Kind));
            }
        }

        return Kinds;
    }

    bool HasFlagColumn(FTDatabase& Dictionary)
    {
        FTStatement Column = Dictionary.Prepare("SELECT COUNT(*) FROM pragma_table_info('words') WHERE name = 'has_forms'");

        return Column.IsValid() && Column.Step() && Column.GetInt(0) > 0;
    }

    std::vector<FFlagCandidate> ListCandidates(FTDatabase& Dictionary, const std::vector<std::string>& Kinds)
    {
        std::vector<FFlagCandidate> Candidates;

        FTStatement Query = Dictionary.Prepare(
            "SELECT words.id, words.headword, parts_of_speech.code FROM words "
            "JOIN parts_of_speech ON parts_of_speech.id = words.part_of_speech_id");

        if (!Query.IsValid())
        {
            return Candidates;
        }

        while (Query.Step())
        {
            std::string PartOfSpeech = Query.GetText(2);

            if (std::find(Kinds.begin(), Kinds.end(), PartOfSpeech) == Kinds.end())
            {
                continue;
            }

            Candidates.push_back({Query.GetInt64(0), Query.GetText(1), std::move(PartOfSpeech)});
        }

        return Candidates;
    }
}

bool FillFormsFlags(FTDatabase& Dictionary, const std::string& TargetLanguage, const std::string& NativeLanguage,
                    FTFormsFlagStats& Stats)
{
    Stats = {};

    if (!HasFlagColumn(Dictionary))
    {
        qCWarning(LogDatabase) << "Forms flags: the dictionary has no has_forms column";

        return false;
    }

    const bool bSupported = SupportsFormsTraining(FormsTrainingConjugation, TargetLanguage)
        || SupportsFormsTraining(FormsTrainingAgreement, TargetLanguage);

    if (!bSupported)
    {
        qCInfo(LogDatabase) << "Forms flags: the forms trainings do not support" << QString::fromStdString(TargetLanguage)
                            << "so no flags are written";

        return true;
    }

    const FTDictionaryRepository Repository(Dictionary, NativeLanguage);
    const std::vector<FFlagCandidate> Candidates = ListCandidates(Dictionary, ListReadKinds());

    if (!Dictionary.BeginTransaction())
    {
        return false;
    }

    FTStatement Clear = Dictionary.Prepare("UPDATE words SET has_forms = NULL");

    if (!Clear.IsValid() || !Clear.Run())
    {
        Dictionary.Rollback();

        return false;
    }

    FTStatement Write = Dictionary.Prepare("UPDATE words SET has_forms = ?1 WHERE id = ?2");

    if (!Write.IsValid())
    {
        Dictionary.Rollback();

        return false;
    }

    for (const FFlagCandidate& Candidate : Candidates)
    {
        const std::optional<FTDictionaryEntry> Entry = Repository.LookUpForForms(Candidate.Headword, Candidate.PartOfSpeech);

        if (!Entry.has_value() || Entry->Id != Candidate.WordId)
        {
            ++Stats.Unresolved;

            continue;
        }

        const bool bEligible = IsFormsEligible(*Entry, TargetLanguage);

        Write.Reset();
        Write.BindInt(1, bEligible ? 1 : 0);
        Write.BindInt64(2, Candidate.WordId);

        if (!Write.Run())
        {
            Dictionary.Rollback();

            return false;
        }

        ++(bEligible ? Stats.Eligible : Stats.NotEligible);
    }

    FTStatement Marker = Dictionary.Prepare("INSERT OR REPLACE INTO metadata (key, value) VALUES ('forms_flags', ?1)");

    if (!Marker.IsValid())
    {
        Dictionary.Rollback();

        return false;
    }

    Marker.BindText(1, TargetLanguage);

    if (!Marker.Run() || !Dictionary.Commit())
    {
        Dictionary.Rollback();

        return false;
    }

    qCInfo(LogDatabase) << "Forms flags:" << Stats.Eligible << "eligible," << Stats.NotEligible << "not eligible,"
                        << Stats.Unresolved << "left to the app's own lookup";

    return true;
}
