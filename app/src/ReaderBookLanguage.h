#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

class ReaderBookLanguage : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Handed to QML through ReaderController.bookLanguage")

    Q_PROPERTY(QString code READ GetLanguage NOTIFY changed)

public:
    QString GetLanguage() const;

    void Update(const QString& DocumentId);

signals:
    void changed();

private:
    QString Language;
};
