#include <catch2/catch_test_macros.hpp>

#include "DailySessionController.h"
#include "TestAppDatabase.h"

#include "core/FTSchema.h"

#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QVariantMap>

TEST_CASE("Today's learned count keeps words beyond the daily goal", "[daily]")
{
    FAppDatabaseGuard DatabaseGuard;
    FTCardRepository& Cards = AppDatabase::Get().GetCardRepository();
    Cards.SetActiveLanguages("fr", "en");

    DailySessionController Controller;
    REQUIRE(Controller.property("learnedToday").toInt() == 0);
    const int LearnedCount{Controller.GetDailyGoal() + 15};
    REQUIRE(Cards.RecordDailyProgress(GetTodayDayNumber(), LearnedCount, 0, Controller.GetDailyGoal()));
    Controller.refresh();

    CHECK(Controller.property("learnedToday").toInt() == LearnedCount);
    CHECK(Controller.GetWordsLeftForGoal() == 0);
}

#ifdef QT_DEBUG
TEST_CASE("The weekly timeline and daily progress share the two AM learning-day boundary", "[daily][learningday]")
{
    FAppDatabaseGuard DatabaseGuard;
    struct FDayOffsetGuard
    {
        int Saved{GetDebugDayOffset()};

        ~FDayOffsetGuard()
        {
            SetDebugDayOffset(Saved);
        }
    } DayOffsetGuard;

    const QDate Monday{2026, 11, 9};
    const FTDayNumber Sunday = DayNumberForMilliseconds(QDateTime{Monday.addDays(-1), QTime{12, 0}}.toMSecsSinceEpoch());
    FTCardRepository& Cards = AppDatabase::Get().GetCardRepository();
    Cards.SetActiveLanguages("fr", "en");
    REQUIRE(Cards.RecordDailyProgress(Sunday, 15, 3, 15));

    DailySessionController Controller;

    for (const QTime& Time : {QTime{0, 0}, QTime{0, 30}, QTime{1, 59}, QTime{2, 0}, QTime{2, 1}})
    {
        CAPTURE(Time.toString().toStdString());
        const FTDayNumber Day = DayNumberForMilliseconds(QDateTime{Monday, Time}.toMSecsSinceEpoch());
        SetDebugDayOffset(Day - DayNumberForMilliseconds(GetRawSystemTimeMilliseconds()));
        Controller.refresh();

        const bool bBeforeReset = Time < QTime{2, 0};
        CHECK(Controller.property("weekday").toInt() == (bBeforeReset ? Qt::Sunday : Qt::Monday));
        const QVariantList Progress = Controller.GetRecentProgress();
        REQUIRE(Progress.size() == 7);
        CHECK(Progress.back().toMap().value("learnedCount").toInt() == (bBeforeReset ? 15 : 0));
        CHECK(Progress.back().toMap().value("recorded").toBool() == bBeforeReset);
    }
}
#endif
