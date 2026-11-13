#pragma once

#include <QObject>
#include <QSize>
#include <QTimer>
#include <memory>

class QWindow;

class AppQuickLookup : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ IsEnabled WRITE SetEnabled NOTIFY enabledChanged)

public:
    explicit AppQuickLookup(QObject* Parent = nullptr);
    ~AppQuickLookup() override;

    bool IsEnabled() const;
    void SetEnabled(bool bEnabled);
    void Reset();

    Q_INVOKABLE void present(QWindow* Window);
    Q_INVOKABLE void dismiss(QWindow* Window, bool bRestoreFocus);
    Q_INVOKABLE void keepOnScreen(QWindow* Window);
    Q_INVOKABLE int availableHeight(QWindow* Window) const;
    Q_INVOKABLE QSize savedSize() const;
    Q_INVOKABLE void saveSize(int Width, int Height);

signals:
    void enabledChanged();
    void lookupRequested(const QString& Text);
    void typedLookupRequested();

private:
    struct FKeyboardCapture;
    void ApplyEnabled(bool bEnable);
    void ReadSelection();

    std::unique_ptr<FKeyboardCapture> Capture;
    QTimer ClipboardTimer;
    int ClipboardAttempts = 0;
    bool bEnabled = false;
};
