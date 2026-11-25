#pragma once

#include "core/FTVocabularyMarks.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <string>
#include <vector>

class ReaderVocabulary : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Handed to QML through ReaderController.vocabulary")

    Q_PROPERTY(bool enabled READ IsEnabled NOTIFY changed)

public:
    explicit ReaderVocabulary(QObject* Parent = nullptr);

    Q_INVOKABLE void refresh();

    /** The foreign language of the text being marked. Empty for the language being learned. */
    void SetLanguage(const std::string& Value);

    bool IsEnabled() const;

    std::vector<FTVocabularyMark> Find(const QString& Text) const;

signals:
    void changed();

private:
    void Rebuild();

    std::string MakeRevision() const;

    std::string GetTargetLanguage() const;

    FTVocabularyMarks Marks;
    std::string BuiltRevision;
    std::string Language;
    bool bEnabled = false;
    bool bHighlightUnknown = false;
};
