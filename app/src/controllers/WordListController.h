#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class WordListController : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList entries READ GetEntries NOTIFY entriesChanged)

public:
    explicit WordListController(QObject* Parent = nullptr);

    static constexpr int RecentLimit = 50;

    Q_INVOKABLE void load(const QString& Source);
    Q_INVOKABLE int count(const QString& Source) const;
    Q_INVOKABLE void speakWord(const QString& Word) const;

    QVariantList GetEntries() const;

signals:
    void entriesChanged();

private:
    QVariantList Entries;
};
