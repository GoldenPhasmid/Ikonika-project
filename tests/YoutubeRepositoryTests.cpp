#include <catch2/catch_test_macros.hpp>

#include "core/FTDatabase.h"
#include "core/FTSchema.h"
#include "core/FTYoutubeRepository.h"

#include <memory>

namespace
{
    struct Fixture
    {
        Fixture()
        {
            std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
            REQUIRE(Opened.has_value());

            Target = std::make_unique<FTDatabase>(std::move(*Opened));
            REQUIRE(MigrateDeviceSchema(*Target));

            Repository = std::make_unique<FTYoutubeRepository>(*Target,
                                                                [this]() { return FakeNow; });
        }

        std::unique_ptr<FTDatabase> Target;
        std::unique_ptr<FTYoutubeRepository> Repository;

        int64_t FakeNow = 1000;
    };
}

TEST_CASE_METHOD(Fixture, "AddVideo adds a new row", "[youtube]")
{
    const auto Id = Repository->AddVideo("dQw4w9WgXcQ", "A talk");

    REQUIRE(Id.has_value());

    const std::vector<FTYoutubeVideoRow> Videos = Repository->ListVideos();

    REQUIRE(Videos.size() == 1);
    CHECK(Videos[0].VideoId == "dQw4w9WgXcQ");
    CHECK(Videos[0].Title == "A talk");
}

TEST_CASE_METHOD(Fixture, "AddVideo on the same video id updates the row instead of duplicating it",
                 "[youtube]")
{
    const auto First = Repository->AddVideo("dQw4w9WgXcQ", "A talk");
    REQUIRE(First.has_value());

    FakeNow = 2000;

    const auto Second = Repository->AddVideo("dQw4w9WgXcQ", "A renamed talk");
    REQUIRE(Second.has_value());
    CHECK(*Second == *First);

    const std::vector<FTYoutubeVideoRow> Videos = Repository->ListVideos();

    REQUIRE(Videos.size() == 1);
    CHECK(Videos[0].Title == "A renamed talk");
    CHECK(Videos[0].LastOpenedDate == 2000);
}

TEST_CASE_METHOD(Fixture, "RemoveVideo drops a video from the list", "[youtube]")
{
    const auto Id = Repository->AddVideo("dQw4w9WgXcQ", "A talk");
    REQUIRE(Id.has_value());

    CHECK(Repository->RemoveVideo(*Id));
    CHECK(Repository->ListVideos().empty());
}

TEST_CASE_METHOD(Fixture, "TouchVideo moves a video to the top by LastOpenedDate", "[youtube]")
{
    const auto First = Repository->AddVideo("aaaaaaaaaaa", "First");
    REQUIRE(First.has_value());

    FakeNow = 2000;

    const auto SecondId = Repository->AddVideo("bbbbbbbbbbb", "Second");
    REQUIRE(SecondId.has_value());

    FakeNow = 3000;

    CHECK(Repository->TouchVideo(*First));

    const std::vector<FTYoutubeVideoRow> Videos = Repository->ListVideos();

    REQUIRE(Videos.size() == 2);
    CHECK(Videos[0].VideoId == "aaaaaaaaaaa");
}
