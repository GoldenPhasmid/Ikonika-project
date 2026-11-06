#include <catch2/catch_test_macros.hpp>

#include "core/FTCatalog.h"
#include "core/FTDictionaryPackage.h"

#include <string>

namespace
{
    const std::string GoodHash = std::string(64, 'a');

    std::string OnePackage(const std::string& Fields)
    {
        return R"({"schema":1,"packages":[{)" + Fields + R"(}]})";
    }

    std::string DictionaryFields(int Schema = DictionarySchemaVersion)
    {
        return R"("id":"dictionary-fr","kind":"dictionary","target":"fr","natives":["en"],)"
               R"("version":1,"dictionarySchema":)" + std::to_string(Schema) +
               R"(,"file":"fr-dictionary.zip","unpackTo":".",)"
               R"("url":"https://example.invalid/fr-dictionary.zip",)"
               R"("bytes":26387439,"unpackedBytes":51798016,"sha256":")" + GoodHash + R"(")";
    }
}

TEST_CASE("A catalog for an unknown schema is refused whole", "[catalog]")
{
    REQUIRE_FALSE(ParseCatalog(R"({"schema":2,"packages":[]})").has_value());
    REQUIRE_FALSE(ParseCatalog(R"({"packages":[]})").has_value());
}

TEST_CASE("Unreadable JSON is refused rather than read partially", "[catalog]")
{
    REQUIRE_FALSE(ParseCatalog("not json at all").has_value());
    REQUIRE_FALSE(ParseCatalog(R"({"schema":1,"packages":[)").has_value());
    REQUIRE_FALSE(ParseCatalog("[]").has_value());
}

TEST_CASE("An empty catalog reads as empty rather than failing", "[catalog]")
{
    const std::optional<FTCatalog> Catalog = ParseCatalog(R"({"schema":1,"packages":[]})");

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->Packages.empty());
    REQUIRE_FALSE(Catalog->App.has_value());
}

TEST_CASE("A package is read field for field", "[catalog]")
{
    const std::optional<FTCatalog> Catalog = ParseCatalog(OnePackage(DictionaryFields()));

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->Packages.size() == 1);

    const FTCatalogPackage& Package = Catalog->Packages.front();

    REQUIRE(Package.Id == "dictionary-fr");
    REQUIRE(Package.Kind == "dictionary");
    REQUIRE(Package.TargetLanguage == "fr");
    REQUIRE(Package.NativeLanguages == std::vector<std::string>{"en"});
    REQUIRE(Package.Version == 1);
    REQUIRE(Package.UnpackTo == ".");
    REQUIRE(Package.Bytes == 26387439);
    REQUIRE(Package.UnpackedBytes == 51798016);
    REQUIRE(Package.Sha256 == GoodHash);
    REQUIRE(Package.IsCompatible());
}

TEST_CASE("Sizes past the range of an int survive the read", "[catalog]")
{
    const std::string Fields =
        R"("id":"big","kind":"translation","version":1,)"
        R"("url":"https://example.invalid/big.zip",)"
        R"("bytes":5368709120,"unpackedBytes":6442450944,"sha256":")" + GoodHash + R"(")";

    const std::optional<FTCatalog> Catalog = ParseCatalog(OnePackage(Fields));

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->Packages.size() == 1);
    REQUIRE(Catalog->Packages.front().Bytes == 5368709120LL);
    REQUIRE(Catalog->Packages.front().UnpackedBytes == 6442450944LL);
}

TEST_CASE("An entry that could never be downloaded is dropped, not the file", "[catalog]")
{
    const std::string Good =
        R"({"id":"good","kind":"voice","version":1,)"
        R"("url":"https://example.invalid/good.zip","sha256":")" + GoodHash + R"("})";

    const auto WithBad = [&Good](const std::string& Bad)
    {
        return R"({"schema":1,"packages":[)" + Bad + "," + Good + "]}";
    };

    SECTION("no url")
    {
        const std::string Bad = R"({"id":"x","kind":"voice","version":1,"sha256":")" + GoodHash + R"("})";
        const std::optional<FTCatalog> Catalog = ParseCatalog(WithBad(Bad));

        REQUIRE(Catalog.has_value());
        REQUIRE(Catalog->Packages.size() == 1);
        REQUIRE(Catalog->Packages.front().Id == "good");
    }

    SECTION("no version")
    {
        const std::string Bad =
            R"({"id":"x","kind":"voice","url":"https://example.invalid/x.zip","sha256":")" +
            GoodHash + R"("})";

        REQUIRE(ParseCatalog(WithBad(Bad))->Packages.size() == 1);
    }

    SECTION("unknown kind")
    {
        const std::string Bad =
            R"({"id":"x","kind":"wallpaper","version":1,)"
            R"("url":"https://example.invalid/x.zip","sha256":")" + GoodHash + R"("})";

        REQUIRE(ParseCatalog(WithBad(Bad))->Packages.size() == 1);
    }

    SECTION("a checksum that could never verify")
    {
        const std::string Bad =
            R"({"id":"x","kind":"voice","version":1,)"
            R"("url":"https://example.invalid/x.zip","sha256":"abc123"})";

        REQUIRE(ParseCatalog(WithBad(Bad))->Packages.size() == 1);
    }

    SECTION("an upper case checksum, which would never match a computed one")
    {
        const std::string Bad =
            R"({"id":"x","kind":"voice","version":1,)"
            R"("url":"https://example.invalid/x.zip","sha256":")" + std::string(64, 'A') + R"("})";

        REQUIRE(ParseCatalog(WithBad(Bad))->Packages.size() == 1);
    }
}

TEST_CASE("A dictionary built for another schema is read but marked incompatible", "[catalog]")
{
    const std::optional<FTCatalog> Catalog =
        ParseCatalog(OnePackage(DictionaryFields(DictionarySchemaVersion + 1)));

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->Packages.size() == 1);
    REQUIRE_FALSE(Catalog->Packages.front().IsCompatible());
}

TEST_CASE("A language resolves to the dictionary this app can read", "[catalog]")
{
    const std::string Old = DictionaryFields(OldestDictionarySchemaVersion - 1);
    std::string Current = DictionaryFields();
    Current.replace(Current.find("dictionary-fr"), 13, "dictionary-fr-current");

    const std::optional<FTCatalog> Catalog =
        ParseCatalog(R"({"schema":1,"packages":[{)" + Old + "},{" + Current + "}]}");

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->Packages.size() == 2);

    const FTCatalogPackage* Found = Catalog->FindCompatibleDictionary("fr");

    REQUIRE(Found != nullptr);
    CHECK(Found->Id == "dictionary-fr-current");
    CHECK(Catalog->FindCompatibleDictionary("es") == nullptr);
}

TEST_CASE("No dictionary is offered when none is built for this app's schema", "[catalog]")
{
    const std::optional<FTCatalog> Catalog =
        ParseCatalog(OnePackage(DictionaryFields(OldestDictionarySchemaVersion - 1)));

    REQUIRE(Catalog.has_value());
    CHECK(Catalog->FindCompatibleDictionary("fr") == nullptr);
}

TEST_CASE("Of two readable dictionaries the newer schema is offered", "[catalog]")
{
    const std::string Older = DictionaryFields(OldestDictionarySchemaVersion);
    std::string Newer = DictionaryFields(DictionarySchemaVersion);
    Newer.replace(Newer.find("dictionary-fr"), 13, "dictionary-fr-current");

    const std::optional<FTCatalog> Catalog =
        ParseCatalog(R"({"schema":1,"packages":[{)" + Older + "},{" + Newer + "}]}");

    REQUIRE(Catalog.has_value());

    const FTCatalogPackage* Found = Catalog->FindCompatibleDictionary("fr");

    REQUIRE(Found != nullptr);
    CHECK(Found->Id == "dictionary-fr-current");
}

TEST_CASE("The newest readable dictionary is offered whichever order the catalog lists them in",
          "[catalog]")
{
    std::string Older = DictionaryFields(OldestDictionarySchemaVersion);
    Older.replace(Older.find("dictionary-fr"), 13, "dictionary-fr-older");
    const std::string Newer = DictionaryFields(DictionarySchemaVersion);

    const std::vector<std::string> Catalogues =
    {
        R"({"schema":1,"packages":[{)" + Newer + "},{" + Older + "}]}",
        R"({"schema":1,"packages":[{)" + Older + "},{" + Newer + "}]}",
    };

    for (const std::string& Text : Catalogues)
    {
        const std::optional<FTCatalog> Catalog = ParseCatalog(Text);

        REQUIRE(Catalog.has_value());

        const FTCatalogPackage* Found = Catalog->FindCompatibleDictionary("fr");

        REQUIRE(Found != nullptr);
        CHECK(Found->Id == "dictionary-fr");
        CHECK(Found->DictionarySchema == DictionarySchemaVersion);
    }
}

TEST_CASE("Only dictionaries are held to a schema", "[catalog]")
{
    const std::string Fields =
        R"("id":"piper-fr","kind":"voice","version":1,)"
        R"("url":"https://example.invalid/piper-fr.zip","sha256":")" + GoodHash + R"(")";

    const std::optional<FTCatalog> Catalog = ParseCatalog(OnePackage(Fields));

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->Packages.size() == 1);
    REQUIRE(Catalog->Packages.front().IsCompatible());
}

TEST_CASE("A package with no target language serves every language", "[catalog]")
{
    const std::string Fields =
        R"("id":"piper-fr","kind":"voice","target":null,"version":1,)"
        R"("url":"https://example.invalid/piper-fr.zip","sha256":")" + GoodHash + R"(")";

    const std::optional<FTCatalog> Catalog = ParseCatalog(OnePackage(Fields));

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->Packages.size() == 1);

    const FTCatalogPackage& Package = Catalog->Packages.front();

    REQUIRE(Package.TargetLanguage.empty());
    REQUIRE(Package.ServesTarget("fr"));
    REQUIRE(Package.ServesTarget("es"));
    REQUIRE(Package.SupportsNative("uk"));
}

TEST_CASE("A package names the language it serves", "[catalog]")
{
    const std::optional<FTCatalog> Catalog = ParseCatalog(OnePackage(DictionaryFields()));

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->Packages.size() == 1);

    const FTCatalogPackage& Package = Catalog->Packages.front();

    REQUIRE(Package.ServesTarget("fr"));
    REQUIRE_FALSE(Package.ServesTarget("es"));
    REQUIRE(Package.SupportsNative("en"));
    REQUIRE_FALSE(Package.SupportsNative("uk"));
}

TEST_CASE("The app entry is read when one is published", "[catalog]")
{
    const std::string Json =
        R"({"schema":1,"app":{"version":"0.1.0","file":"Ikonika-Setup-0.1.0.exe",)"
        R"("url":"https://example.invalid/Ikonika-Setup-0.1.0.exe","bytes":40122048,)"
        R"("sha256":")" + GoodHash + R"(","silentArguments":["/VERYSILENT"]},"packages":[]})";

    const std::optional<FTCatalog> Catalog = ParseCatalog(Json);

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->App.has_value());
    REQUIRE(Catalog->App->Version == "0.1.0");
    REQUIRE(Catalog->App->Bytes == 40122048);
    REQUIRE(Catalog->App->SilentArguments == std::vector<std::string>{"/VERYSILENT"});
}

TEST_CASE("Installer arguments keep their spaces", "[catalog]")
{
    const std::string Json =
        R"({"schema":1,"app":{"version":"0.2.0","file":"setup.exe",)"
        R"("url":"https://example.invalid/setup.exe","bytes":1,)"
        R"("sha256":")" + GoodHash +
        R"(","silentArguments":["/VERYSILENT","/DIR=C:\\Program Files\\Ikonika"]},"packages":[]})";

    const std::optional<FTCatalog> Catalog = ParseCatalog(Json);

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->App.has_value());
    REQUIRE(Catalog->App->SilentArguments.size() == 2);
    REQUIRE(Catalog->App->SilentArguments[1] == "/DIR=C:\\Program Files\\Ikonika");
}

TEST_CASE("An app entry that could never be installed is ignored", "[catalog]")
{
    const std::string NoHash =
        R"({"schema":1,"app":{"version":"0.2.0",)"
        R"("url":"https://example.invalid/setup.exe"},"packages":[]})";

    const std::optional<FTCatalog> Catalog = ParseCatalog(NoHash);

    REQUIRE(Catalog.has_value());
    REQUIRE_FALSE(Catalog->App.has_value());
}

TEST_CASE("Version comparison decides whether an update is offered", "[catalog]")
{
    SECTION("a higher part anywhere is newer")
    {
        REQUIRE(IsNewerVersion("0.1.0", "0.1.1"));
        REQUIRE(IsNewerVersion("0.1.0", "0.2.0"));
        REQUIRE(IsNewerVersion("0.1.0", "1.0.0"));
    }

    SECTION("the same version is not an update")
    {
        REQUIRE_FALSE(IsNewerVersion("0.1.0", "0.1.0"));
    }

    SECTION("an older published version is not an update")
    {
        REQUIRE_FALSE(IsNewerVersion("0.2.0", "0.1.9"));
        REQUIRE_FALSE(IsNewerVersion("1.0.0", "0.9.9"));
    }

    SECTION("parts compare as numbers, not as text")
    {
        REQUIRE(IsNewerVersion("0.9.0", "0.10.0"));
        REQUIRE_FALSE(IsNewerVersion("0.10.0", "0.9.0"));
        REQUIRE(IsNewerVersion("1.2.9", "1.2.10"));
    }

    SECTION("a missing part counts as zero")
    {
        REQUIRE_FALSE(IsNewerVersion("0.1", "0.1.0"));
        REQUIRE_FALSE(IsNewerVersion("0.1.0", "0.1"));
        REQUIRE(IsNewerVersion("0.1.9", "0.2"));
        REQUIRE(IsNewerVersion("0.1", "0.1.1"));
    }

    SECTION("an unreadable published version never forces an update")
    {
        REQUIRE_FALSE(IsNewerVersion("0.1.0", "v0.2.0"));
        REQUIRE_FALSE(IsNewerVersion("0.1.0", "0.2.0-beta"));
        REQUIRE_FALSE(IsNewerVersion("0.1.0", ""));
        REQUIRE_FALSE(IsNewerVersion("0.1.0", "..."));
    }

    SECTION("an unreadable installed version takes any published one")
    {
        REQUIRE(IsNewerVersion("", "0.1.0"));
        REQUIRE(IsNewerVersion("unknown", "0.1.0"));
    }
}

TEST_CASE("Web search sources are read from the catalog and broken ones dropped", "[catalog]")
{
    const std::string Json =
        R"({"schema":1,"packages":[],"webSearch":[)"
        R"({"id":"reverso","name":"Reverso","url":"https://context.reverso.net/translation/{target}-{native}/{word}",)"
        R"("languages":{"fr":"french","en":"english"}},)"
        R"({"id":"plain","name":"Plain","url":"http://example.invalid/{word}","languages":{"fr":"fr"}},)"
        R"({"id":"noword","name":"No word","url":"https://example.invalid/","languages":{"fr":"fr"}},)"
        R"({"id":"nolanguages","name":"No languages","url":"https://example.invalid/{word}"})"
        R"(]})";

    const std::optional<FTCatalog> Catalog = ParseCatalog(Json);

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->WebSearchSources.size() == 1);

    const FTWebSearchSource& Source = Catalog->WebSearchSources.front();

    REQUIRE(Source.Id == "reverso");
    REQUIRE(Source.Name == "Reverso");
    REQUIRE(Source.LanguageNames.at("fr") == "french");
}

TEST_CASE("A catalog without web search sources still reads", "[catalog]")
{
    const std::optional<FTCatalog> Catalog = ParseCatalog(R"({"schema":1,"packages":[]})");

    REQUIRE(Catalog.has_value());
    REQUIRE(Catalog->WebSearchSources.empty());
}

TEST_CASE("A web search address spells the pair the way its site does", "[catalog]")
{
    const std::vector<FTWebSearchSource> Sources = GetBuiltInWebSearchSources();
    const int Reverso = FindWebSearchSourceIndex(Sources, "reverso");
    const int Google = FindWebSearchSourceIndex(Sources, "google");

    REQUIRE(Sources[Reverso].Id == "reverso");
    REQUIRE(Sources[Google].Id == "google");

    REQUIRE(Sources[Reverso].BuildUrl("fr", "en", "élève") ==
            "https://context.reverso.net/translation/french-english/%C3%A9l%C3%A8ve");
    REQUIRE(Sources[Google].BuildUrl("fr", "en", "pomme de terre") ==
            "https://translate.google.com/?sl=fr&tl=en&text=pomme%20de%20terre&op=translate");
}

TEST_CASE("A web search source not serving the pair builds nothing and is left out", "[catalog]")
{
    const FTWebSearchSource Source{"one", "One", "https://example.invalid/{target}/{native}/{word}",
                                   {{"fr", "fr"}, {"en", "en"}}};

    REQUIRE(Source.BuildUrl("fr", "uk", "chien").empty());
    REQUIRE(SelectWebSearchSourcesForPair({Source}, "fr", "uk").empty());
    REQUIRE(SelectWebSearchSourcesForPair({Source}, "fr", "en").size() == 1);
}

TEST_CASE("An unknown preferred web search source falls back to the first one", "[catalog]")
{
    const std::vector<FTWebSearchSource> Sources = GetBuiltInWebSearchSources();

    REQUIRE(FindWebSearchSourceIndex(Sources, "google") == 1);
    REQUIRE(FindWebSearchSourceIndex(Sources, "gone") == 0);
    REQUIRE(FindWebSearchSourceIndex({}, "reverso") == -1);
}
