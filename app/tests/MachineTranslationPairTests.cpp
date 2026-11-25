#include "AppMachineTranslation.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("A machine translation pack keeps the pair it was given", "[machine-translation]")
{
    AppMachineTranslation Pack{std::string("en"), std::string("uk")};

    const auto [Target, Native] = Pack.GetPair();

    REQUIRE(Target == "en");
    REQUIRE(Native == "uk");
    REQUIRE(Pack.GetPairLabel().startsWith(QStringLiteral("EN")));
    REQUIRE(Pack.GetPairLabel().endsWith(QStringLiteral("UK")));
}

TEST_CASE("Changing the pair of a pack announces it once", "[machine-translation]")
{
    AppMachineTranslation Pack{std::string("fr"), std::string("en")};
    int Changes{0};
    QObject::connect(&Pack, &AppMachineTranslation::changed, [&Changes]() { ++Changes; });

    Pack.SetPair("fr", "en");

    REQUIRE(Changes == 0);

    Pack.SetPair("en", "uk");

    REQUIRE(Changes == 1);
    REQUIRE(Pack.GetPair().first == "en");
    REQUIRE(Pack.GetPair().second == "uk");
}

TEST_CASE("Two packs do not share a pair", "[machine-translation]")
{
    AppMachineTranslation Reader{std::string("en"), std::string("uk")};
    AppMachineTranslation Learner{std::string("fr"), std::string("en")};

    REQUIRE(Reader.GetPair() != Learner.GetPair());
    REQUIRE(Reader.GetLanguageName() != Learner.GetLanguageName());
}
