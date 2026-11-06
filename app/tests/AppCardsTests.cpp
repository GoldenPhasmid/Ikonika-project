#include <catch2/catch_test_macros.hpp>

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "TestAppDatabase.h"

#include "core/FTCard.h"
#include "core/FTCardStore.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTSettings.h"

namespace
{
    struct FAppDictionaryGuard
    {
        ~FAppDictionaryGuard()
        {
            AppDictionary::Get().Close();
        }
    };
}

TEST_CASE("An added word gets its part of speech from the dictionary when it is made", "[appcards]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());
    REQUIRE(AppDictionary::Get().IsReady());

    const FTCard Card = AppCards::MakeAddedWord("avoir", "to have", "", -1, false, "");

    CHECK_FALSE(Card.PartOfSpeech.empty());
}

TEST_CASE("An added word keeps the part of speech it was given", "[appcards]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());
    REQUIRE(AppDictionary::Get().IsReady());

    const FTCard Card = AppCards::MakeAddedWord("avoir", "to have", "custom", -1, false, "");

    CHECK(Card.PartOfSpeech == "custom");
}

TEST_CASE("An added word gets the meaning its translation matches when it is made", "[appcards]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());
    REQUIRE(AppDictionary::Get().IsReady());
    REQUIRE(AppDictionary::Get().GetRepository().HasMeaningExamples());

    const FTCard Card = AppCards::MakeAddedWord("avoir", "to have", "", -1, false, "");

    CHECK_FALSE(Card.MeaningGist.empty());
}

TEST_CASE("An added word keeps the meaning it was given", "[appcards]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());
    REQUIRE(AppDictionary::Get().IsReady());

    const FTCard Card = AppCards::MakeAddedWord("avoir", "to have", "", -1, false, "chosen");

    CHECK(Card.MeaningGist == "chosen");
}

TEST_CASE("Annotate batches h-elision and noun/verb overlap correctly across many cards at once",
          "[appcards]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppDictionaryGuard DictionaryGuard;

    FTSettings::Get().SetTargetLanguage("fr");
    FTSettings::Get().SetNativeLanguage("en");

    REQUIRE(AppDictionary::Get().Initialise());
    REQUIRE(AppDictionary::Get().IsReady());

    FTCard MuteH;
    MuteH.TargetWord = "homme";
    MuteH.PartOfSpeech = "noun";

    FTCard AspiratedH;
    AspiratedH.TargetWord = "héros";
    AspiratedH.PartOfSpeech = "noun";

    FTCard PlainNoun;
    PlainNoun.TargetWord = "table";
    PlainNoun.PartOfSpeech = "noun";

    FTCard NounAlsoVerb;
    NounAlsoVerb.TargetWord = "dîner";
    NounAlsoVerb.PartOfSpeech = "noun";

    const std::vector<FTCardHandle> Batch{
        MakeCardHandle(MuteH), MakeCardHandle(AspiratedH),
        MakeCardHandle(PlainNoun), MakeCardHandle(NounAlsoVerb)};

    AppCards::Annotate(Batch);

    CHECK(Batch[0]->bElidesArticle);
    CHECK_FALSE(Batch[1]->bElidesArticle);
    CHECK_FALSE(Batch[2]->bElidesArticle);

    CHECK_FALSE(Batch[0]->bSpellingIsAlsoAVerb);
    CHECK(Batch[3]->bSpellingIsAlsoAVerb);
}
