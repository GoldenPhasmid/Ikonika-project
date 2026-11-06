#include "FTParagraphModel.h"

#include <QString>

#include <utility>

FTParagraphModel::FTParagraphModel(QObject* Parent)
    : QAbstractListModel(Parent)
{
}

int FTParagraphModel::rowCount(const QModelIndex& Parent) const
{
    return Parent.isValid() ? 0 : static_cast<int>(Paragraphs.size());
}

QVariant FTParagraphModel::data(const QModelIndex& Index, int Role) const
{
    if (!Index.isValid()
        || Index.row() < 0
        || Index.row() >= static_cast<int>(Paragraphs.size()))
    {
        return {};
    }

    const size_t Row = static_cast<size_t>(Index.row());

    if (Role == Qt::DisplayRole)
    {
        return QString::fromStdString(Paragraphs[Row]);
    }

    if (Role == HeadingRole)
    {
        return HeadingAt(Row);
    }

    if (Role == MarkupRole)
    {
        return Row < Markup.size() ? QString::fromStdString(Markup[Row]) : QString();
    }

    const FTParagraphImage Image{Row < Images.size() ? Images[Row] : FTParagraphImage{}};

    switch (Role)
    {
    case ImageRole:
        return QString::fromStdString(Image.Path);
    case ImageWidthRole:
        return Image.Width;
    case ImageHeightRole:
        return Image.Height;
    default:
        break;
    }

    const FTParagraphLayout Layout{LayoutAt(Row)};
    const FTParagraphLayout Previous{Row > 0 ? LayoutAt(Row - 1) : FTParagraphLayout{}};

    switch (Role)
    {
    case MarginTopRole:
        return Layout.MarginTop;
    case MarginTopWidthRole:
        return Layout.MarginTopWidth;
    case PreviousMarginBottomRole:
        return Previous.MarginBottom;
    case PreviousMarginBottomWidthRole:
        return Previous.MarginBottomWidth;
    case MarginLeftRole:
        return Layout.MarginLeft;
    case MarginLeftWidthRole:
        return Layout.MarginLeftWidth;
    case MarginRightRole:
        return Layout.MarginRight;
    case MarginRightWidthRole:
        return Layout.MarginRightWidth;
    case TextIndentRole:
        return Layout.TextIndent;
    case TextIndentWidthRole:
        return Layout.TextIndentWidth;
    case TextAlignRole:
        return QString::fromStdString(Layout.TextAlign);
    case FontScaleRole:
        return Layout.FontScale;
    case BoldRole:
        return Layout.bBold;
    case ItalicRole:
        return Layout.bItalic;
    case SmallCapsRole:
        return Layout.bSmallCaps;
    case TextTransformRole:
        return QString::fromStdString(Layout.TextTransform);
    case RuleRole:
        return Layout.bRule;
    default:
        return {};
    }
}

int FTParagraphModel::HeadingAt(size_t Row) const
{
    return Row < HeadingLevels.size() ? HeadingLevels[Row] : 0;
}

FTParagraphLayout FTParagraphModel::LayoutAt(size_t Row) const
{
    return Row < Layouts.size() ? Layouts[Row] : FTParagraphLayout{};
}

bool FTParagraphModel::IsStyled() const
{
    return !Layouts.empty();
}

QHash<int, QByteArray> FTParagraphModel::roleNames() const
{
    return {
        {Qt::DisplayRole, QByteArrayLiteral("display")},
        {HeadingRole, QByteArrayLiteral("heading")},
        {MarkupRole, QByteArrayLiteral("markup")},
        {MarginTopRole, QByteArrayLiteral("marginTop")},
        {MarginTopWidthRole, QByteArrayLiteral("marginTopWidth")},
        {PreviousMarginBottomRole, QByteArrayLiteral("previousMarginBottom")},
        {PreviousMarginBottomWidthRole, QByteArrayLiteral("previousMarginBottomWidth")},
        {ImageRole, QByteArrayLiteral("image")},
        {ImageWidthRole, QByteArrayLiteral("imageWidth")},
        {ImageHeightRole, QByteArrayLiteral("imageHeight")},
        {MarginLeftRole, QByteArrayLiteral("marginLeft")},
        {MarginLeftWidthRole, QByteArrayLiteral("marginLeftWidth")},
        {MarginRightRole, QByteArrayLiteral("marginRight")},
        {MarginRightWidthRole, QByteArrayLiteral("marginRightWidth")},
        {TextIndentRole, QByteArrayLiteral("textIndent")},
        {TextIndentWidthRole, QByteArrayLiteral("textIndentWidth")},
        {TextAlignRole, QByteArrayLiteral("textAlign")},
        {FontScaleRole, QByteArrayLiteral("fontScale")},
        {BoldRole, QByteArrayLiteral("bold")},
        {ItalicRole, QByteArrayLiteral("italic")},
        {SmallCapsRole, QByteArrayLiteral("smallCaps")},
        {TextTransformRole, QByteArrayLiteral("textTransform")},
        {RuleRole, QByteArrayLiteral("rule")}
    };
}

void FTParagraphModel::SetParagraphs(std::vector<std::string> NewParagraphs,
                                     std::vector<int> NewHeadingLevels,
                                     std::vector<std::string> NewMarkup,
                                     std::vector<FTParagraphLayout> NewLayouts,
                                     std::vector<FTParagraphImage> NewImages)
{
    beginResetModel();
    Paragraphs = std::move(NewParagraphs);
    HeadingLevels = std::move(NewHeadingLevels);
    Markup = std::move(NewMarkup);
    Layouts = std::move(NewLayouts);
    Images = std::move(NewImages);
    endResetModel();

    emit styledChanged();
}

void FTParagraphModel::Clear()
{
    SetParagraphs({});
}
