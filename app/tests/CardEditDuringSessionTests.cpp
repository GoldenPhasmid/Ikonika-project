#include <catch2/catch_test_macros.hpp>

#include "CatchQtTypes.h"

#include "FlashcardController.h"

#include "AppDatabase.h"
#include "TestAppSpeech.h"
#include "TestAppDatabase.h"

#include <string>


TEST_CASE("A card edited mid-session shows its new text, not what was loaded at session start",
         "[cardeditduringsession]")
{
    FAppDatabaseGuard DatabaseGuard;
    FAppSpeechGuard SpeechGuard;


    FTCard Card;
    Card.TargetWord = "original";
    Card.Translation = "translation-original";

    REQUIRE(AddTestCards({Card}));

    const std::vector<FTCardHandle> Loaded = AppDatabase::Get().GetCardRepository().LoadCards();
    const std::string CardId = Loaded.at(0)->Id;

    const std::optional<FTCardHandle> Handle = AppDatabase::Get().GetCardRepository().LoadCard(CardId);
    REQUIRE(Handle.has_value());

    FTSessionStartParams Params{};
    Params.CardState = std::make_shared<FCardState>(std::vector<FTCardHandle>{*Handle});
    Params.Title = "Test Session";
    Params.TargetLanguage = "fr";
    Params.FixedLength = 1;
    Params.Selector = std::make_shared<FEachOnceSelector>(1, true);

    FlashcardController Controller;
    Controller.Start(Params);

    REQUIRE(Controller.GetPrompt() == "original");

    REQUIRE(AppDatabase::Get().GetCardRepository().UpdateCardContent(CardId, "edited", "translation-edited", std::nullopt, "", ""));

    CHECK(Controller.GetPrompt() == "edited");
    CHECK(Controller.GetAnswer() == "translation-edited");
}
