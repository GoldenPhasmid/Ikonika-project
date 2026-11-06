#pragma once

#include "core/FTReaderDocument.h"

#include <QtQml/qqmlregistration.h>

#include <QAbstractListModel>

#include <string>
#include <vector>

class FTParagraphModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Handed to QML through ReaderController.paragraphModel")

    Q_PROPERTY(bool styled READ IsStyled NOTIFY styledChanged)

public:
    explicit FTParagraphModel(QObject* Parent = nullptr);

    static constexpr int HeadingRole = Qt::UserRole + 1;
    static constexpr int MarkupRole = Qt::UserRole + 2;
    static constexpr int MarginTopRole = Qt::UserRole + 3;
    static constexpr int TextIndentRole = Qt::UserRole + 4;
    static constexpr int TextAlignRole = Qt::UserRole + 5;
    static constexpr int FontScaleRole = Qt::UserRole + 6;
    static constexpr int BoldRole = Qt::UserRole + 7;
    static constexpr int ItalicRole = Qt::UserRole + 8;
    static constexpr int SmallCapsRole = Qt::UserRole + 9;
    static constexpr int TextTransformRole = Qt::UserRole + 10;
    static constexpr int TextIndentWidthRole = Qt::UserRole + 11;
    static constexpr int MarginTopWidthRole = Qt::UserRole + 12;
    static constexpr int PreviousMarginBottomRole = Qt::UserRole + 13;
    static constexpr int PreviousMarginBottomWidthRole = Qt::UserRole + 14;
    static constexpr int MarginLeftRole = Qt::UserRole + 15;
    static constexpr int MarginLeftWidthRole = Qt::UserRole + 16;
    static constexpr int MarginRightRole = Qt::UserRole + 17;
    static constexpr int MarginRightWidthRole = Qt::UserRole + 18;
    static constexpr int ImageRole = Qt::UserRole + 19;
    static constexpr int ImageWidthRole = Qt::UserRole + 20;
    static constexpr int ImageHeightRole = Qt::UserRole + 21;
    static constexpr int RuleRole = Qt::UserRole + 22;

    int rowCount(const QModelIndex& Parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& Index, int Role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    /** Replaces the paragraphs wholesale. Empty for a document with no text. */
    void SetParagraphs(std::vector<std::string> Paragraphs, std::vector<int> HeadingLevels = {},
                       std::vector<std::string> Markup = {}, std::vector<FTParagraphLayout> Layouts = {},
                       std::vector<FTParagraphImage> Images = {});

    /** Drops the paragraphs; the view shows nothing. */
    void Clear();

    bool IsStyled() const;

signals:
    void styledChanged();

private:
    int HeadingAt(size_t Row) const;
    FTParagraphLayout LayoutAt(size_t Row) const;

    std::vector<std::string> Paragraphs;
    std::vector<int> HeadingLevels;
    std::vector<std::string> Markup;
    std::vector<FTParagraphLayout> Layouts;
    std::vector<FTParagraphImage> Images;
};
