#pragma once

#include <QObject>
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

signals:
    void enabledChanged();
    void lookupRequested(const QString& Text);

private:
    struct FKeyboardCapture;
    void ApplyEnabled(bool bEnable);
    void ReadSelection();

    std::unique_ptr<FKeyboardCapture> Capture;
    QTimer ClipboardTimer;
    int ClipboardAttempts = 0;
    bool bEnabled = false;
};
