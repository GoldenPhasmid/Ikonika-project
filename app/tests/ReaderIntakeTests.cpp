#include "AppReaderIntake.h"
#include "AppSingleInstance.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <future>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Reader intake retains a desktop book until consumed", "[reader-intake]")
{
    AppReaderIntake Intake;
    int Arrivals{0};
    QObject::connect(&Intake, &AppReaderIntake::bookArrived, [&Arrivals]() { ++Arrivals; });

    Intake.OpenPath(QStringLiteral("C:/Books/Le petit prince.epub"));

    REQUIRE(Arrivals == 1);
    const QVariantMap Book{Intake.takePending()};
    REQUIRE(Book.value(QStringLiteral("path")).toString() == QStringLiteral("C:/Books/Le petit prince.epub"));
    REQUIRE(Book.value(QStringLiteral("name")).toString() == QStringLiteral("Le petit prince.epub"));
    REQUIRE(Intake.takePending().isEmpty());
}

TEST_CASE("Second launch forwards a Unicode book path to the running instance", "[reader-intake]")
{
    AppSingleInstance Primary;
    REQUIRE(Primary.TryAcquire());
    QString Received;
    QObject::connect(&Primary, &AppSingleInstance::bookRequested,
                     [&Received](const QString& Path) { Received = Path; });

    const QString Path{QString::fromUtf8("C:/Books/Émile et les enfants.fb2")};
    auto Launch = std::async(std::launch::async, [Path]()
    {
        AppSingleInstance Secondary;
        return Secondary.TryAcquire(Path);
    });
    QElapsedTimer Timer;
    Timer.start();
    while (Received.isEmpty() && Timer.elapsed() < 1000)
    {
        QCoreApplication::processEvents();
    }
    REQUIRE(Received == Path);
    REQUIRE_FALSE(Launch.get());
}
