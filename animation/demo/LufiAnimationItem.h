#pragma once

#include <QPointer>
#include <QQuickPaintedItem>
#include <memory>

class AnimationSource;
class FTLufiTrackRenderer;

class FTLufiAnimationItem : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QObject* animationSource READ GetAnimationSource WRITE SetAnimationSource NOTIFY AnimationSourceChanged)
    Q_PROPERTY(QString clipName READ GetClipName WRITE SetClipName NOTIFY ClipNameChanged)
    Q_PROPERTY(QString selectedEyes READ GetSelectedEyes WRITE SetSelectedEyes NOTIFY SelectedEyesChanged)
    Q_PROPERTY(bool nightEyes READ HasNightEyes WRITE SetNightEyes NOTIFY NightEyesChanged)
    Q_PROPERTY(int frame READ GetFrame WRITE SetFrame NOTIFY FrameChanged)

public:
    explicit FTLufiAnimationItem(QQuickItem* Parent = nullptr);
    ~FTLufiAnimationItem() override;

    QObject* GetAnimationSource() const;
    const QString& GetClipName() const { return ClipName; }
    const QString& GetSelectedEyes() const { return SelectedEyes; }
    bool HasNightEyes() const { return bNightEyes; }
    int GetFrame() const { return Frame; }

    void SetAnimationSource(QObject* Value);
    void SetClipName(const QString& Value);
    void SetSelectedEyes(const QString& Value);
    void SetNightEyes(bool bValue);
    void SetFrame(int Value);
    void paint(QPainter* Painter) override;

signals:
    void AnimationSourceChanged();
    void ClipNameChanged();
    void SelectedEyesChanged();
    void NightEyesChanged();
    void FrameChanged();

protected:
    static std::shared_ptr<const FTLufiTrackRenderer> PrepareRenderer(const AnimationSource& Value);
    void SetRenderer(std::shared_ptr<const FTLufiTrackRenderer> Value);

private:
    void Reload();

    QPointer<AnimationSource> Source;
    std::shared_ptr<const FTLufiTrackRenderer> Renderer;
    QString ClipName;
    QString SelectedEyes{QStringLiteral("Eyes Authored")};
    int Frame{1};
    bool bNightEyes{false};
};
