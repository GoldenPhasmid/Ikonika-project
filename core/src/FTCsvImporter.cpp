#include "core/FTCsvImporter.h"

#include <QFile>
#include <QString>
#include <QStringList>

#include <utility>

namespace
{
    /** Splits one line into fields, honouring double-quoted fields and "" escapes. Handles both
        bare commas and quoted commas, which is what keeps "dog, hound" in one field. */
    QStringList SplitCsvLine(const QString& Line)
    {
        QStringList Fields;
        QString Current;
        bool bInQuotes = false;

        for (int Index = 0; Index < Line.size(); ++Index)
        {
            const QChar Character = Line.at(Index);

            if (Character == QLatin1Char('"'))
            {
                if (bInQuotes && Index + 1 < Line.size() && Line.at(Index + 1) == QLatin1Char('"'))
                {
                    Current += QLatin1Char('"');
                    ++Index;
                }
                else
                {
                    bInQuotes = !bInQuotes;
                }
            }
            else if (Character == QLatin1Char(',') && !bInQuotes)
            {
                Fields.append(Current);
                Current.clear();
            }
            else
            {
                Current += Character;
            }
        }

        Fields.append(Current);
        return Fields;
    }
}

bool FTCsvImporter::Parse(const std::string& Source)
{
    Rows.clear();
    LastError.clear();

    QFile File(QString::fromStdString(Source));

    if (!File.open(QIODevice::ReadOnly))
    {
        LastError = File.errorString().toStdString();
        return false;
    }

    const QString Content = QString::fromUtf8(File.readAll());
    const QStringList Lines = Content.split(QLatin1Char('\n'));

    bool bSawHeader = false;

    for (const QString& Line : Lines)
    {
        const QString Trimmed = Line.trimmed();

        if (Trimmed.isEmpty())
        {
            continue;
        }

        const QStringList Fields = SplitCsvLine(Trimmed);

        if (Fields.size() < 2)
        {
            continue;
        }

        // A first "Word,Translation" line is the header the exporter writes, not a pair.
        if (!bSawHeader)
        {
            bSawHeader = true;

            if (Fields.at(0).compare(QLatin1String("Word"), Qt::CaseInsensitive) == 0
                && Fields.at(1).compare(QLatin1String("Translation"), Qt::CaseInsensitive) == 0)
            {
                continue;
            }
        }

        std::string TargetWord = Fields.at(0).toStdString();
        std::string Translation = Fields.at(1).toStdString();

        while (!TargetWord.empty() && std::isspace(static_cast<unsigned char>(TargetWord.back())))
        {
            TargetWord.pop_back();
        }
        while (!TargetWord.empty() && std::isspace(static_cast<unsigned char>(TargetWord.front())))
        {
            TargetWord.erase(TargetWord.begin());
        }
        while (!Translation.empty() && std::isspace(static_cast<unsigned char>(Translation.back())))
        {
            Translation.pop_back();
        }
        while (!Translation.empty() && std::isspace(static_cast<unsigned char>(Translation.front())))
        {
            Translation.erase(Translation.begin());
        }

        if (TargetWord.empty() || Translation.empty())
        {
            continue;
        }

        FTImportRow Row;
        Row.TargetWord = std::move(TargetWord);
        Row.Translation = std::move(Translation);
        Rows.push_back(std::move(Row));
    }

    if (Rows.empty())
    {
        LastError = "no word-translation pairs found";
        return false;
    }

    return true;
}
