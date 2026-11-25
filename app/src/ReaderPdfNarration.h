#pragma once

#include "core/FTPdfNarration.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

class QPdfDocument;

class ReaderPdfNarration : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Handed to QML through ReaderController.pdfNarration")

    Q_PROPERTY(bool available READ IsAvailable NOTIFY changed)

public:
    explicit ReaderPdfNarration(QObject* Parent = nullptr);
    ~ReaderPdfNarration() override;

    Q_INVOKABLE void prepare(int Page);
    Q_INVOKABLE QVariantList pageRanges(int Unit, int Start, int End) const;
    Q_INVOKABLE QVariantMap unitPlace(int Page, int Character) const;
    Q_INVOKABLE int pageOf(int Unit, int Character) const;

    void Load(const QString& FilePath);
    void Clear();
    void ExtendAfter(int Unit);

    bool IsAvailable() const;
    const std::vector<std::string>* GetUnits() const;
    std::optional<FTPdfPagePlace> ToPagePlace(int Unit, int Character) const;
    std::optional<FTPdfUnitPlace> ToUnitPlace(int Page, int Character) const;

signals:
    void changed();
    void windowChanged();

private:
    void OnLoaded(quint64 LoadedGeneration, QPdfDocument* Loaded, bool bHasText, double Width);
    void AppendPage();

    QString Source;
    quint64 Generation = 0;
    std::unique_ptr<QPdfDocument> Document;
    bool bAvailable = false;
    double TextWidth = 0.0;
    int FirstPage = -1;
    int LastPage = -1;
    std::vector<FTPdfTextPage> Pages;
    FTPdfNarration Narration;
};
