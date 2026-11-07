#pragma once

#include <QObject>

class QLocalServer;

/**
 * Prevents two Ikonika processes running at the same time. Tray instead of Quit.
 */
class AppSingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit AppSingleInstance(QObject* Parent = nullptr);
    ~AppSingleInstance() override;

    AppSingleInstance(const AppSingleInstance&) = delete;
    AppSingleInstance& operator=(const AppSingleInstance&) = delete;

    /** Claims the single-instance server */
    bool TryAcquire(const QString& BookPath = {});

signals:
    /** A second launch asked to be shown. */
    void activationRequested();
    void bookRequested(const QString& Path);

private:
    void OnNewConnection();

    QLocalServer* Server = nullptr;
};
