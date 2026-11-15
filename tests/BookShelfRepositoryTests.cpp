#include <catch2/catch_test_macros.hpp>

#include "core/FTBookShelfRepository.h"
#include "core/FTCard.h"
#include "core/FTCardRepository.h"
#include "core/FTDatabase.h"
#include "core/FTSchema.h"

#include <algorithm>
#include <memory>
#include <string>

namespace
{
    struct Fixture
    {
        Fixture()
        {
            std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
            REQUIRE(Opened.has_value());

            Target = std::make_unique<FTDatabase>(std::move(*Opened));
            REQUIRE(MigrateSchema(*Target));

            Repository = std::make_unique<FTBookShelfRepository>(*Target, [this]() { return FakeNow; });
        }

        bool IsMember(const std::string& CollectionId, const std::string& BookKey) const
        {
            const std::vector<FTBookCollectionMember> Members = Repository->ListMembers();

            return std::any_of(Members.begin(), Members.end(), [&](const FTBookCollectionMember& Member)
            {
                return Member.CollectionId == CollectionId && Member.BookKey == BookKey;
            });
        }

        std::unique_ptr<FTDatabase> Target;
        std::unique_ptr<FTBookShelfRepository> Repository;

        int64_t FakeNow = 1000;
    };
}

TEST_CASE("A favourite book is listed until it is unmarked", "[reader][shelf]")
{
    Fixture Test;

    REQUIRE(Test.Repository->SetFavorite("cloud:a", true));
    REQUIRE(Test.Repository->SetFavorite("free:b", true));
    CHECK(Test.Repository->ListFavorites().size() == 2);

    REQUIRE(Test.Repository->SetFavorite("cloud:a", false));
    CHECK(Test.Repository->ListFavorites() == std::unordered_set<std::string>{"free:b"});
}

TEST_CASE("A collection holds the books added to it", "[reader][shelf]")
{
    Fixture Test;

    const std::optional<std::string> Id = Test.Repository->CreateCollection("Policier", 3);

    REQUIRE(Id.has_value());
    REQUIRE(Test.Repository->AddToCollection(*Id, "cloud:a"));
    REQUIRE(Test.Repository->AddToCollection(*Id, "free:b"));

    const std::vector<FTBookCollectionRow> Collections = Test.Repository->ListCollections();

    REQUIRE(Collections.size() == 1);
    CHECK(Collections[0].Name == "Policier");
    CHECK(Collections[0].Colour == 3);
    CHECK(Test.IsMember(*Id, "cloud:a"));

    REQUIRE(Test.Repository->RemoveFromCollection(*Id, "cloud:a"));
    CHECK_FALSE(Test.IsMember(*Id, "cloud:a"));
    CHECK(Test.IsMember(*Id, "free:b"));

    REQUIRE(Test.Repository->AddToCollection(*Id, "cloud:a"));
    CHECK(Test.IsMember(*Id, "cloud:a"));
}

TEST_CASE("A collection needs a name", "[reader][shelf]")
{
    Fixture Test;

    CHECK_FALSE(Test.Repository->CreateCollection("", 0).has_value());
}

TEST_CASE("A deleted collection takes its members with it", "[reader][shelf]")
{
    Fixture Test;

    const std::string Id = Test.Repository->CreateCollection("Vacances", 0).value();
    REQUIRE(Test.Repository->AddToCollection(Id, "cloud:a"));

    REQUIRE(Test.Repository->DeleteCollection(Id));

    CHECK(Test.Repository->ListCollections().empty());
    CHECK(Test.Repository->ListMembers().empty());
}

TEST_CASE("A renamed collection keeps its books", "[reader][shelf]")
{
    Fixture Test;

    const std::string Id = Test.Repository->CreateCollection("Vacances", 0).value();
    REQUIRE(Test.Repository->AddToCollection(Id, "cloud:a"));

    REQUIRE(Test.Repository->RenameCollection(Id, "Été"));

    CHECK(Test.Repository->ListCollections()[0].Name == "Été");
    CHECK(Test.IsMember(Id, "cloud:a"));
}

TEST_CASE("Uploading a book moves its marks to the cloud key", "[reader][shelf]")
{
    Fixture Test;

    const std::string Id = Test.Repository->CreateCollection("Vacances", 0).value();
    REQUIRE(Test.Repository->SetFavorite("local:doc", true));
    REQUIRE(Test.Repository->AddToCollection(Id, "local:doc"));

    REQUIRE(Test.Repository->RenameBookKey("local:doc", "cloud:abc"));

    CHECK(Test.Repository->ListFavorites() == std::unordered_set<std::string>{"cloud:abc"});
    CHECK(Test.IsMember(Id, "cloud:abc"));
    CHECK_FALSE(Test.IsMember(Id, "local:doc"));
}

TEST_CASE("A book lists the cards added while it was open, newest first", "[reader][shelf]")
{
    Fixture Test;
    FTCardRepository Cards(*Test.Target);

    FTCard Chien;
    Chien.TargetWord = "chien";
    Chien.Translation = "dog";
    FTCard Chat;
    Chat.TargetWord = "chat";
    Chat.Translation = "cat";

    const std::optional<std::string> ChienId = Cards.AddCard(Chien);
    const std::optional<std::string> ChatId = Cards.AddCard(Chat);
    REQUIRE(ChienId.has_value());
    REQUIRE(ChatId.has_value());

    REQUIRE(Test.Repository->AddBookCard("local:a", *ChienId));
    Test.FakeNow = 2000;
    REQUIRE(Test.Repository->AddBookCard("local:a", *ChatId));
    REQUIRE(Test.Repository->AddBookCard("free:b", *ChatId));

    CHECK(Test.Repository->ListBookCards("local:a") == std::vector<std::string>{*ChatId, *ChienId});
    CHECK(Test.Repository->ListBookCards("free:b").empty());
    CHECK(Test.Repository->CountBookCards() == std::unordered_map<std::string, int>{{"local:a", 2}});

    REQUIRE(Cards.DeleteCards({*ChienId}));
    CHECK(Test.Repository->ListBookCards("local:a") == std::vector<std::string>{*ChatId});

    REQUIRE(Test.Repository->RenameBookKey("local:a", "cloud:c"));
    CHECK(Test.Repository->ListBookCards("local:a").empty());
    CHECK(Test.Repository->ListBookCards("cloud:c") == std::vector<std::string>{*ChatId});
}
