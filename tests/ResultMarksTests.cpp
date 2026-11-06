#include <catch2/catch_test_macros.hpp>

#include "ResultMarks.h"

#include <vector>

TEST_CASE("Correct and wrong answers map to opposite marks", "[resultmarks]")
{
    const QVariantList Marks = ToResultMarks({true, false});

    REQUIRE(Marks.size() == 2);
    CHECK(Marks[0].toDouble() == 1.0);
    CHECK(Marks[1].toDouble() == 0.0);
}

TEST_CASE("Marks come back in the order the results were given", "[resultmarks]")
{
    const QVariantList Marks = ToResultMarks({true, true, false, true});

    REQUIRE(Marks.size() == 4);
    CHECK(Marks[0].toDouble() == 1.0);
    CHECK(Marks[1].toDouble() == 1.0);
    CHECK(Marks[2].toDouble() == 0.0);
    CHECK(Marks[3].toDouble() == 1.0);
}

TEST_CASE("No answers yet is an empty row of ticks", "[resultmarks]")
{
    CHECK(ToResultMarks({}).isEmpty());
}
