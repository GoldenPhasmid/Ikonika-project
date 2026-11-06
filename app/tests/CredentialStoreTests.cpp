#include <catch2/catch_test_macros.hpp>

#include "AppCredentialStore.h"

#if defined(Q_OS_WIN)

TEST_CASE("Credential Manager keeps a secret until it is removed", "[credentials]")
{
    AppWindowsCredentialStore Store(QStringLiteral("IkonikaTests/google/"));
    Store.Remove(QStringLiteral("1001"));

    REQUIRE(Store.Write(QStringLiteral("1001"), QStringLiteral("alice-refresh")));
    CHECK(Store.Read(QStringLiteral("1001")) == "alice-refresh");

    REQUIRE(Store.Write(QStringLiteral("1001"), QStringLiteral("alice-refresh-2")));
    CHECK(Store.Read(QStringLiteral("1001")) == "alice-refresh-2");

    Store.Remove(QStringLiteral("1001"));
    CHECK(Store.Read(QStringLiteral("1001")).isEmpty());
}

TEST_CASE("Credential Manager entries under different prefixes never see each other", "[credentials]")
{
    AppWindowsCredentialStore Debug(QStringLiteral("IkonikaTests-debug/google/"));
    AppWindowsCredentialStore Release(QStringLiteral("IkonikaTests/google/"));
    Debug.Remove(QStringLiteral("1001"));
    Release.Remove(QStringLiteral("1001"));

    REQUIRE(Debug.Write(QStringLiteral("1001"), QStringLiteral("debug-refresh")));

    CHECK(Release.Read(QStringLiteral("1001")).isEmpty());
    CHECK(Debug.Read(QStringLiteral("1001")) == "debug-refresh");

    Debug.Remove(QStringLiteral("1001"));
}

TEST_CASE("Reading an account Credential Manager never stored gives nothing", "[credentials]")
{
    AppWindowsCredentialStore Store(QStringLiteral("IkonikaTests/google/"));

    CHECK(Store.Read(QStringLiteral("never-stored")).isEmpty());
}

#endif
