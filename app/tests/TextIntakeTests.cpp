#include "AppTextIntake.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Selected text survives until lookup consumes it once", "[text-intake]")
{
    AppTextIntake Intake;
    int Arrivals{0};
    QObject::connect(&Intake, &AppTextIntake::textArrived, [&Arrivals]() { ++Arrivals; });

    Intake.ReceiveText(QString::fromUtf8("  élève  "));

    REQUIRE(Arrivals == 1);
    REQUIRE(Intake.takePending() == QString::fromUtf8("élève"));
    REQUIRE(Intake.takePending().isEmpty());
}

TEST_CASE("Empty selections do not erase a pending lookup", "[text-intake]")
{
    AppTextIntake Intake;
    int Arrivals{0};
    QObject::connect(&Intake, &AppTextIntake::textArrived, [&Arrivals]() { ++Arrivals; });

    Intake.ReceiveText(QStringLiteral("bonjour"));
    Intake.ReceiveText(QStringLiteral(" \n\t "));

    REQUIRE(Arrivals == 1);
    REQUIRE(Intake.takePending() == QStringLiteral("bonjour"));
}

TEST_CASE("A new selection replaces an unconsumed lookup", "[text-intake]")
{
    AppTextIntake Intake;
    Intake.ReceiveText(QStringLiteral("bonjour"));
    Intake.ReceiveText(QStringLiteral("au revoir"));

    REQUIRE(Intake.takePending() == QStringLiteral("au revoir"));
}
