#include <catch2/catch_test_macros.hpp>

#include "AppVoiceChoices.h"
#include "TestAppSpeech.h"

namespace
{
    FTVoiceInfo MakeVoice(const std::string& Engine, const std::string& Name,
                          const std::vector<std::string>& Languages)
    {
        FTVoiceInfo Info;
        Info.Engine = Engine;
        Info.Name = Name;
        Info.Languages = Languages;

        return Info;
    }

    FTCatalogPackage MakeVoicePackage(const std::string& Id, const std::string& Target)
    {
        FTCatalogPackage Package;
        Package.Id = Id;
        Package.Kind = CatalogKindVoice;
        Package.TargetLanguage = Target;
        Package.UnpackTo = Id;
        Package.Version = 1;
        Package.Bytes = 1000;

        return Package;
    }

    QVariantMap EntryAt(const QVariantList& Choices, int Row)
    {
        return Choices.at(Row).toMap();
    }
}

TEST_CASE("Installed voices that speak the target language are listed, the rest are not", "[voices]")
{
    FAppSpeechGuard Guard;
    Guard.Mock.Voices = {
        MakeVoice("Piper", "piper-es", {"es"}),
        MakeVoice("Piper", "piper-fr", {"fr"}),
        MakeVoice("Supertonic", "supertonic-3", {"fr", "es", "pt"})
    };
    Guard.Mock.ActiveVoice = 2;

    const QVariantList Choices = AppVoiceChoices::Build("fr", Guard.Mock, nullptr);

    REQUIRE(Choices.size() == 2);

    CHECK(EntryAt(Choices, 0).value("title").toString() == "Piper");
    CHECK(EntryAt(Choices, 0).value("index").toInt() == 1);
    CHECK_FALSE(EntryAt(Choices, 0).value("selected").toBool());

    CHECK(EntryAt(Choices, 1).value("title").toString() == "Supertonic");
    CHECK(EntryAt(Choices, 1).value("index").toInt() == 2);
    CHECK(EntryAt(Choices, 1).value("selected").toBool());
}

TEST_CASE("A catalog voice that is not installed is offered for download", "[voices]")
{
    FAppSpeechGuard Guard;
    Guard.Mock.Voices = {MakeVoice("Supertonic", "supertonic-3", {"fr", "pt"})};

    FTCatalog Catalog;
    Catalog.Packages = {
        MakeVoicePackage("supertonic-3", ""),
        MakeVoicePackage("piper-fr", "fr"),
        MakeVoicePackage("piper-es", "es")
    };

    const QVariantList Choices = AppVoiceChoices::Build("fr", Guard.Mock, &Catalog);

    REQUIRE(Choices.size() == 2);

    CHECK(EntryAt(Choices, 0).value("installed").toBool());

    const QVariantMap Download = EntryAt(Choices, 1);

    CHECK(Download.value("title").toString() == "Piper");
    CHECK_FALSE(Download.value("installed").toBool());
    CHECK(Download.value("packageId").toString() == "piper-fr");
    CHECK(Download.value("bytes").toLongLong() == 1000);
    CHECK(Download.value("index").toInt() == -1);
}

TEST_CASE("A package whose directory is already loaded is not offered again", "[voices]")
{
    FAppSpeechGuard Guard;
    Guard.Mock.Voices = {MakeVoice("Piper", "piper-fr", {"fr"})};

    FTCatalog Catalog;
    Catalog.Packages = {MakeVoicePackage("piper-fr", "fr")};

    CHECK(AppVoiceChoices::Build("fr", Guard.Mock, &Catalog).size() == 1);
}

TEST_CASE("A voice with no alternative leaves the list at one entry", "[voices]")
{
    FAppSpeechGuard Guard;
    Guard.Mock.Voices = {MakeVoice("Piper", "piper-fr", {"fr"})};

    CHECK(AppVoiceChoices::Build("fr", Guard.Mock, nullptr).size() == 1);
    CHECK(AppVoiceChoices::Build("pt", Guard.Mock, nullptr).isEmpty());
}
