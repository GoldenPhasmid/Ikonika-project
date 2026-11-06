#include "WordRowGrouping.h"

#include <QHash>
#include <QString>
#include <QVariantMap>

#include <algorithm>

static QString GetRowHeadword(const QVariantMap& Row)
{
    const QVariant TargetWord = Row.value(QStringLiteral("targetWord"));

    return TargetWord.isValid() ? TargetWord.toString() : Row.value(QStringLiteral("word")).toString();
}

static QString PickEasierCefrLevel(const QString& Kept, const QString& Candidate)
{
    if (Kept.isEmpty())
    {
        return Candidate;
    }

    if (Candidate.isEmpty())
    {
        return Kept;
    }

    return std::min(Kept, Candidate);
}

static void MergeRow(QVariantMap& Kept, QVariantMap Row)
{
    const QVariant CandidateRank = Row.value(QStringLiteral("frequencyRank"));
    const QVariant KeptRank = Kept.value(QStringLiteral("frequencyRank"));

    if (CandidateRank.isValid() && (!KeptRank.isValid() || CandidateRank.toInt() < KeptRank.toInt()))
    {
        Kept.swap(Row);
    }

    const QString Stage = QStringLiteral("stage");
    const QString Completed = QStringLiteral("completed");
    const QString CefrLevel = QStringLiteral("cefrLevel");
    const QString Translation = QStringLiteral("translation");
    const QString Exact = QStringLiteral("exact");

    Kept[Stage] = std::max(Kept.value(Stage, -1).toInt(), Row.value(Stage, -1).toInt());
    Kept[Completed] = Kept.value(Completed).toBool() || Row.value(Completed).toBool();
    Kept[Exact] = Kept.value(Exact).toBool() || Row.value(Exact).toBool();
    Kept[CefrLevel] = PickEasierCefrLevel(Kept.value(CefrLevel).toString(), Row.value(CefrLevel).toString());

    if (Kept.value(Translation).toString().isEmpty())
    {
        Kept[Translation] = Row.value(Translation);
    }
}

QVariantList GroupRowsByHeadword(const QVariantList& Rows)
{
    QVariantList Result;
    QHash<QString, qsizetype> IndexByHeadword;

    for (const QVariant& Value : Rows)
    {
        const QVariantMap Row = Value.toMap();
        const QString Headword = GetRowHeadword(Row);

        const auto Existing = IndexByHeadword.constFind(Headword);

        if (Existing == IndexByHeadword.constEnd())
        {
            IndexByHeadword.insert(Headword, Result.size());
            Result.append(Row);

            continue;
        }

        QVariantMap Kept = Result[*Existing].toMap();
        MergeRow(Kept, Row);
        Result[*Existing] = Kept;
    }

    return Result;
}
