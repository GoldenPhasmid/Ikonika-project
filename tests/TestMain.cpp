// QSqlDatabase needs a QCoreApplication to exist before it can load the sqldrivers plugin, so
// Catch2's own default main() (which never constructs one) is not enough once FTDatabase moved
// onto QSqlDatabase. This replaces it with the minimum needed to make that work.

#include <QCoreApplication>
#include <QDir>
#include <QString>

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>

int main(int argc, char* argv[])
{
    QCoreApplication App(argc, argv);

    // QSettings' default constructor needs both names to resolve a real backing store; with
    // neither set, fileName() comes back empty and every FTSettings Set...() call silently writes
    // nowhere. A separate name from the real app's "Ikonika", so a test run and a real one never
    // share a registry key.
    QCoreApplication::setOrganizationName(QStringLiteral("IkonikaTests"));
    QCoreApplication::setApplicationName(QStringLiteral("IkonikaTests"));

    // One home under local/ for the real files a test has to write (a persistence test cannot use
    // an in-memory database). Created here rather than at configure time, so a run started from a
    // stale build directory still finds it, and emptied afterwards so nothing survives the run.
    const QString TestDataDirectory = QStringLiteral(SAVED_TEST_DATA_DIRECTORY);
    QDir().mkpath(TestDataDirectory);

    const int Result = Catch::Session().run(argc, argv);

    QDir(TestDataDirectory).removeRecursively();

    return Result;
}
