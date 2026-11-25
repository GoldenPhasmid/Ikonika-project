#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <optional>
#include <string>

class FTImportSource;

class UserDecksController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList categories READ GetCategories NOTIFY changed)
    Q_PROPERTY(QVariantList decks READ GetDecks NOTIFY changed)

public:
    explicit UserDecksController(QObject* Parent = nullptr);

    Q_INVOKABLE void refresh();

    Q_INVOKABLE QString createCategory(const QString& Name, int Colour);
    Q_INVOKABLE bool renameCategory(const QString& Id, const QString& Name);
    Q_INVOKABLE bool deleteCategory(const QString& Id);

    Q_INVOKABLE QString createDeck(const QString& Name, const QString& CategoryId, const QString& Description = QString());
    Q_INVOKABLE bool renameDeck(const QString& Id, const QString& Name);
    Q_INVOKABLE bool editDeck(const QString& Id, const QString& Name, const QString& Description);
    Q_INVOKABLE bool setDailyLearning(const QString& Id, bool bDailyLearning);
    Q_INVOKABLE bool moveDeck(const QString& Id, const QString& CategoryId);
    Q_INVOKABLE bool deleteDeck(const QString& Id);

    Q_INVOKABLE QVariantMap importFromFile(const QUrl& File, const QString& CategoryId);
    Q_INVOKABLE QVariantMap importIntoDeck(const QUrl& File, const QString& DeckId);

    Q_INVOKABLE QString sessionKey(const QString& DeckId) const;
    Q_INVOKABLE QStringList collectSessionCards(const QString& DeckId) const;

    QVariantList GetCategories() const;
    QVariantList GetDecks() const;

signals:
    void changed();

private:
    QVariantMap ImportWords(const FTImportSource& Source, const QString& Name,
                            const std::function<std::optional<std::string>()>& ChooseDeck);

    QVariantList Categories;
    QVariantList Decks;
};
