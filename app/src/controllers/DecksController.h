#pragma once

#include "core/FTDictionaryEntry.h"
#include "core/FTDictionaryRepository.h"

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

class DecksController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool available READ IsAvailable NOTIFY decksChanged)
    Q_PROPERTY(QVariantList decks READ GetDecks NOTIFY decksChanged)
    Q_PROPERTY(bool categorised READ HasCategories NOTIFY decksChanged)
    Q_PROPERTY(QString discoverBlockedReason READ GetDiscoverBlockedReason NOTIFY decksChanged)

public:
    explicit DecksController(QObject* Parent = nullptr);

    Q_INVOKABLE void refresh();

    bool IsAvailable() const;
    bool HasCategories() const;
    QString GetDiscoverBlockedReason() const;
    QVariantList GetDecks() const;

signals:
    void decksChanged();

private:
    void LoadDeckData();

    QVariantList Decks;

    bool bDeckDataLoaded = false;
    std::vector<FTDeck> DeckList;
    std::map<int64_t, std::vector<FTDeckWord>> Readings;
};
