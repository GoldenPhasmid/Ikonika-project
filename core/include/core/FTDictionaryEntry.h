#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct FTDictionaryExample
{
    std::string Text;
    std::string Translation;
    std::string LanguageCode;
};

struct FTDictionaryForm
{
    std::string Text;

    int Mood = 0;
    int Tense = 0;
    int Person = 0;
    int Gender = 0;
    int Number = 0;
};

struct FTDictionaryTranslation
{
    std::string Text;

    std::optional<double> Probability;
};

struct FTDictionarySense
{
    std::string Definition;

    std::string Gloss;

    std::string Gist;

    std::string NativeDefinition;

    std::string Example;

    std::string ExampleTranslation;

    std::optional<int> Gender;

    std::vector<FTDictionaryTranslation> Translations;
};

struct FTDictionaryEntry
{
    int64_t Id = 0;

    std::string Headword;
    std::string PartOfSpeech;

    std::string Phonetic;

    std::vector<std::string> Tags;

    std::optional<int> FrequencyRank;

    std::optional<int> TeachingRank;

    std::optional<int> CefrLevel;

    bool bLlmComplete = false;

    std::vector<FTDictionarySense> Senses;

    std::vector<FTDictionaryTranslation> Translations;

    std::vector<FTDictionaryExample> Examples;

    std::vector<FTDictionaryForm> Forms;
};
