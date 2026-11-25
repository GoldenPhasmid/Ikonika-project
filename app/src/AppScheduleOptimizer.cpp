#include "AppScheduleOptimizer.h"

#include "AppDatabase.h"
#include "AppWorkspaceHost.h"

#include "core/FTDatabase.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTScheduleOptimizer.h"

#include <QCoreApplication>
#include <QPointer>
#include <QThreadPool>

namespace
{
    AppScheduleOptimizer* Instance = nullptr;
}

AppScheduleOptimizer* AppScheduleOptimizer::GetInstance()
{
    return Instance;
}

AppScheduleOptimizer::AppScheduleOptimizer(AppWorkspaceHost& Host, QObject* Parent)
    : QObject(Parent)
{
    Instance = this;

    connect(&Host, &AppWorkspaceHost::workspaceOpened, this, &AppScheduleOptimizer::OnWorkspaceOpened,
            Qt::QueuedConnection);

    Start();
}

AppScheduleOptimizer::~AppScheduleOptimizer()
{
    if (Instance == this)
    {
        Instance = nullptr;
    }
}

void AppScheduleOptimizer::OnWorkspaceOpened()
{
    ++Serial;
    bRunning = false;
    CheckedDay.reset();

    Start();
}

void AppScheduleOptimizer::OnApplicationStateChanged(Qt::ApplicationState State)
{
    if (State == Qt::ApplicationActive)
    {
        Start();
    }
}

void AppScheduleOptimizer::Start()
{
    const FTDayNumber Today = GetTodayDayNumber();

    if (bRunning || CheckedDay == Today || !AppDatabase::Get().IsReady())
    {
        return;
    }

    const QString FilePath = AppDatabase::Get().GetFilePath();

    if (FilePath.isEmpty() || FilePath.contains(QStringLiteral(":memory:")))
    {
        return;
    }

    CheckedDay = Today;
    bRunning = true;

    const int FittedReviewCount = FTSettings::Get().GetScheduleRules().FittedReviewCount;
    const quint64 RunSerial = Serial;
    const QPointer<AppScheduleOptimizer> Self{this};

    QThreadPool::globalInstance()->start([Path = FilePath.toStdString(), FittedReviewCount, RunSerial, Self]()
    {
        std::optional<FTScheduleWeights> Weights;
        int ReviewCount = 0;

        if (std::optional<FTDatabase> Connection = FTDatabase::OpenReadOnly(Path))
        {
            const std::vector<FTReviewHistory> Histories = LoadReviewHistories(*Connection);
            ReviewCount = CountTrainingReviews(Histories);

            if (ShouldOptimizeSchedule(ReviewCount, FittedReviewCount))
            {
                Weights = OptimizeScheduleWeights(Histories).Weights;
            }
        }
        else
        {
            qCWarning(LogSchedule) << "Could not open the database to fit the schedule";
        }

        QMetaObject::invokeMethod(QCoreApplication::instance(),
                                  [Self, Weights, ReviewCount, RunSerial]()
                                  {
                                      if (Self != nullptr)
                                      {
                                          Self->Finish(Weights, ReviewCount, RunSerial);
                                      }
                                  },
                                  Qt::QueuedConnection);
    });
}

void AppScheduleOptimizer::Finish(const std::optional<FTScheduleWeights>& Weights, int ReviewCount, quint64 RunSerial)
{
    if (RunSerial != Serial)
    {
        return;
    }

    bRunning = false;

    if (!Weights.has_value())
    {
        qCDebug(LogSchedule) << "The schedule stays as it is with" << ReviewCount << "reviews";

        return;
    }

    FTSettings::Get().SetScheduleWeights(*Weights, ReviewCount);

    qCInfo(LogSchedule) << "The schedule now uses weights fitted to" << ReviewCount << "reviews";

    emit weightsChanged();
}
