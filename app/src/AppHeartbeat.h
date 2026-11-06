#pragma once

#include <QObject>
#include <QQmlParserStatus>
#include <QtQml/qqmlregistration.h>

class AppHeartbeat : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    QML_ELEMENT
    Q_INTERFACES(QQmlParserStatus)

public:
    explicit AppHeartbeat(QObject* Parent = nullptr);

    static void SetActive(bool bValue);
    static void Beat();

    void classBegin() override;
    void componentComplete() override;
};
