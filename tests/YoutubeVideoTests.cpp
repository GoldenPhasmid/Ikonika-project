#include <catch2/catch_test_macros.hpp>

#include "core/FTYoutubeVideo.h"

TEST_CASE("ExtractYoutubeVideoId reads a watch URL", "[youtube]")
{
    const auto Id = ExtractYoutubeVideoId("https://www.youtube.com/watch?v=dQw4w9WgXcQ");

    REQUIRE(Id.has_value());
    CHECK(*Id == "dQw4w9WgXcQ");
}

TEST_CASE("ExtractYoutubeVideoId reads a watch URL with extra parameters", "[youtube]")
{
    const auto Id = ExtractYoutubeVideoId(
        "https://www.youtube.com/watch?v=dQw4w9WgXcQ&t=42s&list=PL123");

    REQUIRE(Id.has_value());
    CHECK(*Id == "dQw4w9WgXcQ");
}

TEST_CASE("ExtractYoutubeVideoId reads a youtu.be short link", "[youtube]")
{
    const auto Id = ExtractYoutubeVideoId("https://youtu.be/dQw4w9WgXcQ");

    REQUIRE(Id.has_value());
    CHECK(*Id == "dQw4w9WgXcQ");
}

TEST_CASE("ExtractYoutubeVideoId reads a youtu.be short link with a query", "[youtube]")
{
    const auto Id = ExtractYoutubeVideoId("https://youtu.be/dQw4w9WgXcQ?si=abc123");

    REQUIRE(Id.has_value());
    CHECK(*Id == "dQw4w9WgXcQ");
}

TEST_CASE("ExtractYoutubeVideoId reads an embed URL", "[youtube]")
{
    const auto Id = ExtractYoutubeVideoId("https://www.youtube.com/embed/dQw4w9WgXcQ");

    REQUIRE(Id.has_value());
    CHECK(*Id == "dQw4w9WgXcQ");
}

TEST_CASE("ExtractYoutubeVideoId reads a shorts URL", "[youtube]")
{
    const auto Id = ExtractYoutubeVideoId("https://www.youtube.com/shorts/dQw4w9WgXcQ");

    REQUIRE(Id.has_value());
    CHECK(*Id == "dQw4w9WgXcQ");
}

TEST_CASE("ExtractYoutubeVideoId accepts a bare id", "[youtube]")
{
    const auto Id = ExtractYoutubeVideoId("dQw4w9WgXcQ");

    REQUIRE(Id.has_value());
    CHECK(*Id == "dQw4w9WgXcQ");
}

TEST_CASE("ExtractYoutubeVideoId trims surrounding whitespace off a bare id", "[youtube]")
{
    const auto Id = ExtractYoutubeVideoId("  dQw4w9WgXcQ\n");

    REQUIRE(Id.has_value());
    CHECK(*Id == "dQw4w9WgXcQ");
}

TEST_CASE("ExtractYoutubeVideoId rejects garbage", "[youtube]")
{
    CHECK_FALSE(ExtractYoutubeVideoId("").has_value());
    CHECK_FALSE(ExtractYoutubeVideoId("not a link").has_value());
    CHECK_FALSE(ExtractYoutubeVideoId("https://example.com/watch?v=short").has_value());
}
