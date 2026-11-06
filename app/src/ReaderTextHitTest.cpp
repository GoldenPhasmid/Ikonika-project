#include "ReaderTextHitTest.h"

#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>

#include <algorithm>
#include <vector>

ReaderTextHitTest::ReaderTextHitTest(QObject* Parent)
    : QObject(Parent)
{
}

QQuickTextDocument* ReaderTextHitTest::GetTextDocument() const
{
    return TextDocument;
}

void ReaderTextHitTest::SetTextDocument(QQuickTextDocument* Value)
{
    if (TextDocument == Value)
    {
        return;
    }

    TextDocument = Value;

    emit textDocumentChanged();
}

std::optional<ReaderTextHitTest::FLineHit> ReaderTextHitTest::FindLine(qreal Y, bool bClamp) const
{
    const QTextDocument* Document = TextDocument != nullptr ? TextDocument->textDocument() : nullptr;

    if (Document == nullptr)
    {
        return std::nullopt;
    }

    struct FLineBox
    {
        FLineHit Hit;
        qreal Top = 0;
        qreal Bottom = 0;
    };

    std::vector<FLineBox> Boxes;

    for (QTextBlock Block = Document->begin(); Block.isValid(); Block = Block.next())
    {
        const QTextLayout* Layout = Block.layout();

        if (Layout == nullptr)
        {
            continue;
        }

        const qreal LayoutTop{Layout->position().y()};

        for (int LineIndex = 0; LineIndex < Layout->lineCount(); ++LineIndex)
        {
            const QRectF Natural{Layout->lineAt(LineIndex).naturalTextRect()};

            Boxes.push_back(FLineBox{FLineHit{Block.position(), Layout, LineIndex},
                                     LayoutTop + Natural.top(), LayoutTop + Natural.bottom()});
        }
    }

    if (Boxes.empty())
    {
        return std::nullopt;
    }

    const qreal DocumentBottom{std::max(Document->size().height(), Boxes.back().Bottom)};

    for (size_t Index = 0; Index < Boxes.size(); ++Index)
    {
        const qreal NextTop{Index + 1 < Boxes.size() ? Boxes[Index + 1].Top : DocumentBottom};
        const qreal Upper{Index == 0 ? std::min<qreal>(0, Boxes[Index].Top)
                                     : (Boxes[Index - 1].Bottom + Boxes[Index].Top) / 2};
        const qreal Lower{(Boxes[Index].Bottom + NextTop) / 2};

        if (Y >= Upper && Y < Lower)
        {
            return Boxes[Index].Hit;
        }
    }

    if (!bClamp)
    {
        return std::nullopt;
    }

    return Y < Boxes.front().Bottom ? Boxes.front().Hit : Boxes.back().Hit;
}

int ReaderTextHitTest::characterAt(qreal X, qreal Y) const
{
    const std::optional<FLineHit> Hit{FindLine(Y, false)};

    if (!Hit.has_value())
    {
        return -1;
    }

    const QTextLine Line{Hit->Layout->lineAt(Hit->LineIndex)};
    const qreal LocalX{X - Hit->Layout->position().x()};
    const QRectF Natural{Line.naturalTextRect()};

    if (LocalX < Natural.left() || LocalX >= Natural.right())
    {
        return -1;
    }

    const int Character{Line.xToCursor(LocalX, QTextLine::CursorOnCharacter)};

    if (Character < Line.textStart() || Character >= Line.textStart() + Line.textLength())
    {
        return -1;
    }

    return Hit->BlockPosition + Character;
}

int ReaderTextHitTest::positionNear(qreal X, qreal Y) const
{
    const std::optional<FLineHit> Hit{FindLine(Y, true)};

    if (!Hit.has_value())
    {
        return -1;
    }

    const QTextLine Line{Hit->Layout->lineAt(Hit->LineIndex)};

    return Hit->BlockPosition + Line.xToCursor(X - Hit->Layout->position().x(), QTextLine::CursorBetweenCharacters);
}
