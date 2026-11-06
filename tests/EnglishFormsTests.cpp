#include <catch2/catch_test_macros.hpp>

#include "core/FTEnglishForms.h"

TEST_CASE("Regular English forms match their base", "[englishforms]")
{
    CHECK(IsEnglishForm("worlds", "world"));
    CHECK(IsEnglishForm("died", "die"));
    CHECK(IsEnglishForm("dying", "die"));
    CHECK(IsEnglishForm("making", "make"));
    CHECK(IsEnglishForm("stopped", "stop"));
    CHECK(IsEnglishForm("cries", "cry"));
    CHECK(IsEnglishForm("knives", "knife"));
    CHECK_FALSE(IsEnglishForm("ending", "die"));
    CHECK_FALSE(IsEnglishForm("diet", "die"));
}

TEST_CASE("Irregular English forms match their base", "[englishforms]")
{
    CHECK(IsEnglishForm("went", "go"));
    CHECK(IsEnglishForm("is", "be"));
    CHECK(IsEnglishForm("children", "child"));
    CHECK(IsEnglishForm("better", "good"));
    CHECK_FALSE(IsEnglishForm("went", "come"));
}

TEST_CASE("A sentence translation mentions a meaning translation in any of its forms", "[englishforms]")
{
    const std::vector<std::string> Die{"to die", "dying", "to expire", "pass away", "bite the dust"};

    CHECK(MentionsTranslation("Many people die of this disease.", Die));
    CHECK(MentionsTranslation("He died yesterday.", Die));
    CHECK(MentionsTranslation("Her grandfather passed away.", Die));
    CHECK(MentionsTranslation("The villain bit the dust.", Die));
    CHECK_FALSE(MentionsTranslation("Our world is ending.", Die));
    CHECK(MentionsTranslation("Our world is ending.", {"world", "earth"}));
    CHECK(MentionsTranslation("The world's oceans are warm.", {"world"}));
    CHECK(MentionsTranslation("It\xE2\x80\x99s real leather.", {"real (not imitation)", "genuine"}));
    CHECK(MentionsTranslation("Come on, let's go!", {"let's go"}));
}

TEST_CASE("A placeholder in a translation stands for a few words", "[englishforms]")
{
    CHECK(MentionsTranslation("She kept her anger in check.", {"to keep something in check"}));
    CHECK(MentionsTranslation("He cashed in his chips.", {"cash in one's chips"}));
    CHECK_FALSE(MentionsTranslation("He cashed his chips.", {"cash in one's chips"}));
}
