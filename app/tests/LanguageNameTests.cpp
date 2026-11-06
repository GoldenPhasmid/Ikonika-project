#include <catch2/catch_test_macros.hpp>

#include "AppLocalization.h"

TEST_CASE("A language from the name table shows its English name", "[localization]")
{
    CHECK(LanguageDisplayName("it") == QStringLiteral("Italian"));
    CHECK(LanguageDisplayName("nl") == QStringLiteral("Dutch"));
    CHECK(LanguageDisplayName("vi") == QStringLiteral("Vietnamese"));
}

TEST_CASE("A language outside the name table takes Qt's name for it", "[localization]")
{
    CHECK(LanguageDisplayName("fi") == QStringLiteral("Finnish"));
}

TEST_CASE("A code no locale knows shows as the code itself", "[localization]")
{
    CHECK(LanguageDisplayName("zz") == QStringLiteral("zz"));
}
