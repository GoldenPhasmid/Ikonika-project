#pragma once

#include <string>

class FTDatabase;

struct FTFormsFlagStats
{
    int Eligible = 0;
    int NotEligible = 0;
    int Unresolved = 0;
};

/** Writes words.has_forms for every word of a part of speech the forms trainings read, and the forms_flags metadata
    row that tells the app the flags can be trusted. Run once at dictionary build time, after the translations are
    final. False when the dictionary has no has_forms column or a write failed. */
bool FillFormsFlags(FTDatabase& Dictionary, const std::string& TargetLanguage, const std::string& NativeLanguage,
                    FTFormsFlagStats& Stats);
