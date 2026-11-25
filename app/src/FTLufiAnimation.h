#pragma once

#include "LufiAnimationItem.h"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QRectF>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class FTLufiAnimation : public FTLufiAnimationItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(LufiAnimation)
    Q_PROPERTY(QVariantMap clips READ GetClips NOTIFY ClipsChanged)
    Q_PROPERTY(QRectF viewBox READ GetViewBox WRITE SetViewBox NOTIFY ViewBoxChanged)

public:
    explicit FTLufiAnimation(QQuickItem* Parent = nullptr);

    const QVariantMap& GetClips() const { return Clips; }
    QRectF GetViewBox() const { return ViewBox; }
    void SetViewBox(const QRectF& Value);
    void paint(QPainter* Painter) override;
    Q_INVOKABLE qint64 playbackTime() const { return PlaybackClock.elapsed(); }
    Q_INVOKABLE QVariantMap syncDay(int WakeLearningDay, bool bGoalComplete);
    Q_INVOKABLE void recordSessionCompleted();
    Q_INVOKABLE void consumeSessionReaction();

signals:
    void ClipsChanged();
    void ViewBoxChanged();

private:
    struct FTPreparedAnimation
    {
        std::shared_ptr<const FTLufiTrackRenderer> Renderer;
        QVariantMap Clips;
    };

    QFutureWatcher<FTPreparedAnimation> Watcher;
    QElapsedTimer PlaybackClock;
    QVariantMap Clips;
    QRectF ViewBox;
};
