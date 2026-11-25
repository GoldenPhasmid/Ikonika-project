#include "FTLufiAnimation.h"

#include "AnimationSource.h"
#include "AppMetrics.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"
#include <QPainter>
#include <QtConcurrentRun>

#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
#include <QtGui/private/qguiapplication_p.h>
#include <future>
#endif

static constexpr auto LufiDayKey = "lufi/day";
static constexpr auto LearningDayKey = "learningDay";
static constexpr auto WakeSeenKey = "wakeSeen";
static constexpr auto GoalReachedKey = "goalReached";
static constexpr auto SessionsKey = "sessions";
static constexpr auto ReactionPendingKey = "reactionPending";

static QVariantMap LoadLufiDay(const QSettings& Settings)
{
    const auto LearningDay = QVariant::fromValue(GetTodayDayNumber());
    auto Progress = Settings.value(LufiDayKey).toMap();
    if (Progress.value(LearningDayKey) != LearningDay)
    {
        return {{LearningDayKey, LearningDay}};
    }
    return Progress;
}

FTLufiAnimation::FTLufiAnimation(QQuickItem* Parent)
    : FTLufiAnimationItem{Parent}
{
    PlaybackClock.start();

    connect(&Watcher, &QFutureWatcher<FTPreparedAnimation>::finished, this, [this]
    {
        auto Prepared = Watcher.result();
        SetRenderer(std::move(Prepared.Renderer));
        Clips = std::move(Prepared.Clips);
        emit ClipsChanged();
    });

#if defined(IKONIKA_DESKTOP)
    const QString BundleFolder{IsMobileLayoutEnabled() ? QStringLiteral(":/lufi/mobile") : QStringLiteral(":/lufi/desktop")};
#else
    const QString BundleFolder{QStringLiteral(":/lufi/mobile")};
#endif
    Watcher.setFuture(QtConcurrent::run([BundleFolder]
    {
        AnimationSource Source{BundleFolder};
        if (!Source.Reload())
        {
            return FTPreparedAnimation{};
        }

        QVariantMap Available;
        for (const auto& Entry : Source.GetClips())
        {
            auto Clip = Entry.toMap();
            const auto ClipId = Clip.value(QStringLiteral("id")).toString();
            const auto AuthoredBlinks = Source.GetClipData().value(ClipId).value("visibility").toObject().value("Eyes Closed").toArray();
            QVariantList BlinkFrames;
            for (qsizetype FrameIndex{0}; FrameIndex < AuthoredBlinks.size(); ++FrameIndex)
            {
                if (AuthoredBlinks.at(FrameIndex).toBool())
                {
                    BlinkFrames.append(FrameIndex + 1);
                }
            }
            Clip.insert(QStringLiteral("blinkFrames"), std::move(BlinkFrames));
            Available.insert(ClipId, std::move(Clip));
        }

        for (const auto ClipId : {"sleep-to-awake", "idle", "interested", "paw-lick", "active-to-sleep", "idle-sleep"})
        {
            if (!Available.contains(QLatin1String(ClipId)))
            {
                qCWarning(LogApp) << "Missing Lufi clip" << ClipId;
                return FTPreparedAnimation{};
            }
        }

        return FTPreparedAnimation{PrepareRenderer(Source), std::move(Available)};
    }));
}

void FTLufiAnimation::SetViewBox(const QRectF& Value)
{
    if (ViewBox == Value)
    {
        return;
    }
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    if (ViewBox.isValid() != Value.isValid() && QGuiApplicationPrivate::qtGuiThreadPool())
    {
        setFillColor(Value.isValid() ? QColor{} : QColor{Qt::transparent});
    }
#endif
    ViewBox = Value;
    update();
    emit ViewBoxChanged();
}

void FTLufiAnimation::paint(QPainter* Painter)
{
    if (!ViewBox.isValid())
    {
        FTLufiAnimationItem::paint(Painter);
        return;
    }

    const auto PaintCanvas = [this](QPainter& Target, const QSizeF& Size)
    {
        Target.scale(Size.width() / ViewBox.width(), Size.height() / ViewBox.height());
        Target.translate(-ViewBox.topLeft());
        FTLufiAnimationItem::paint(&Target);
    };
#if defined(Q_OS_ANDROID) || defined(Q_OS_WIN)
    if (auto* RasterPool = QGuiApplicationPrivate::qtGuiThreadPool(); RasterPool && Painter->device()->devType() == QInternal::Image)
    {
        const QSize PixelSize{Painter->device()->width(), Painter->device()->height()};
        std::promise<QImage> RasterResult;
        auto ImageReady = RasterResult.get_future();
        RasterPool->start([PixelSize, PaintCanvas, RasterResult = std::move(RasterResult)]() mutable
        {
            QImage Image{PixelSize, QImage::Format_ARGB32_Premultiplied};
            Image.fill(Qt::transparent);
            QPainter Raster{&Image};
            Raster.setRenderHint(QPainter::Antialiasing);
            PaintCanvas(Raster, PixelSize);
            Raster.end();
            RasterResult.set_value(std::move(Image));
        });
        Painter->save();
        Painter->resetTransform();
        Painter->setCompositionMode(QPainter::CompositionMode_Source);
        Painter->drawImage(QPoint{}, ImageReady.get());
        Painter->restore();
        return;
    }
#endif
    PaintCanvas(*Painter, QSizeF{width(), height()});
}

QVariantMap FTLufiAnimation::syncDay(int WakeLearningDay, bool bGoalComplete)
{
    auto Settings = FTSettings::Get().OpenStore();
    auto Progress = LoadLufiDay(Settings);
    const bool bWasAsleep{Progress.value(GoalReachedKey).toBool()};
    const bool bCanWake{!bWasAsleep && !bGoalComplete && !Progress.value(WakeSeenKey).toBool()};
    if (bCanWake && Progress.value(LearningDayKey).toInt() == WakeLearningDay)
    {
        Progress.insert(WakeSeenKey, true);
        Settings.setValue(LufiDayKey, Progress);
    }
    else if (bGoalComplete && !bWasAsleep)
    {
        Progress.insert(GoalReachedKey, true);
        Progress.insert(ReactionPendingKey, false);
        Settings.setValue(LufiDayKey, Progress);
        qCInfo(LogApp) << "Lufi's daily goal reached on learning day" << Progress.value(LearningDayKey);
    }

    Progress.insert(QStringLiteral("wake"), bCanWake);
    return Progress;
}

void FTLufiAnimation::recordSessionCompleted()
{
    auto Settings = FTSettings::Get().OpenStore();
    auto Progress = LoadLufiDay(Settings);
    const int Sessions{Progress.value(SessionsKey).toInt() + 1};
    Progress.insert(SessionsKey, Sessions);
    Progress.insert(ReactionPendingKey, !Progress.value(GoalReachedKey).toBool());
    Settings.setValue(LufiDayKey, Progress);
    qCInfo(LogApp) << "Lufi recorded a completed session. Eye progression" << Sessions;
}

void FTLufiAnimation::consumeSessionReaction()
{
    auto Settings = FTSettings::Get().OpenStore();
    auto Progress = LoadLufiDay(Settings);
    Progress.insert(ReactionPendingKey, false);
    Settings.setValue(LufiDayKey, Progress);
}
