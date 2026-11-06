#include "core/FTAnkiImporter.h"

#include "core/FTLog.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <utility>

namespace
{
    /** Splits one line on Separator, honouring double-quoted fields and "" escapes the way Anki's
        text files use them. */
    QStringList SplitFields(const QString& Line, QChar Separator)
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
            else if (Character == Separator && !bInQuotes)
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

bool FTAnkiImporter::Parse(const std::string& Source)
{
    Rows.clear();
    LastError.clear();

    // Anki packages are zip archives; the database inside them is not a file a user passes us
    // directly. Let them point at the exported text file or an extracted .anki2 instead.
    if (Source.size() >= 5)
    {
        const std::string Lower = [&Source]() -> std::string {
            std::string Result = Source;
            for (char& Character : Result)
            {
                Character = static_cast<char>(std::tolower(static_cast<unsigned char>(Character)));
            }
            return Result;
        }();

        if (Lower.ends_with(".apkg") || Lower.ends_with(".colpkg"))
        {
            LastError = "Anki package files are zip archives. Point at the plain-text export or "
                        "an extracted collection.anki2 instead.";
            return false;
        }

        if (Lower.ends_with(".anki2"))
        {
            return ParseAnkiDatabase(Source);
        }
    }

    return ParseTextFile(Source);
}

void FTAnkiImporter::AddRow(const std::string& TargetWord, const std::string& Translation)
{
    std::string Word = TargetWord;
    std::string Meaning = Translation;

    // Trim leading and trailing whitespace on both sides; a blank either side is not a row.
    while (!Word.empty() && std::isspace(static_cast<unsigned char>(Word.back())))
    {
        Word.pop_back();
    }
    while (!Word.empty() && std::isspace(static_cast<unsigned char>(Word.front())))
    {
        Word.erase(Word.begin());
    }
    while (!Meaning.empty() && std::isspace(static_cast<unsigned char>(Meaning.back())))
    {
        Meaning.pop_back();
    }
    while (!Meaning.empty() && std::isspace(static_cast<unsigned char>(Meaning.front())))
    {
        Meaning.erase(Meaning.begin());
    }

    if (Word.empty() || Meaning.empty())
    {
        return;
    }

    FTImportRow Row;
    Row.TargetWord = std::move(Word);
    Row.Translation = std::move(Meaning);
    Rows.push_back(std::move(Row));
}

bool FTAnkiImporter::ParseTextFile(const std::string& FilePath)
{
    QFile File(QString::fromStdString(FilePath));

    if (!File.open(QIODevice::ReadOnly))
    {
        LastError = File.errorString().toStdString();
        return false;
    }

    const QString Content = QString::fromUtf8(File.readAll());
    const QStringList Lines = Content.split(QLatin1Char('\n'));

    // Anki separates fields with tabs by default and only switches on a #separator: header, so a
    // tab is the safe default for files that carry none.
    QChar Separator = QLatin1Char('\t');

    for (const QString& Line : Lines)
    {
        const QString Trimmed = Line.trimmed();

        if (Trimmed.startsWith(QLatin1String("#separator:")))
        {
            const QString Name = Trimmed.mid(11).trimmed();

            if (Name.compare(QLatin1String("Comma"), Qt::CaseInsensitive) == 0)
            {
                Separator = QLatin1Char(',');
            }
            else if (Name.compare(QLatin1String("Semicolon"), Qt::CaseInsensitive) == 0)
            {
                Separator = QLatin1Char(';');
            }
            else if (Name.compare(QLatin1String("Tab"), Qt::CaseInsensitive) == 0)
            {
                Separator = QLatin1Char('\t');
            }
            else if (Name.compare(QLatin1String("Space"), Qt::CaseInsensitive) == 0)
            {
                Separator = QLatin1Char(' ');
            }
            else if (Name.compare(QLatin1String("Pipe"), Qt::CaseInsensitive) == 0)
            {
                Separator = QLatin1Char('|');
            }
            else if (Name.compare(QLatin1String("Colon"), Qt::CaseInsensitive) == 0)
            {
                Separator = QLatin1Char(':');
            }
        }
    }

    for (const QString& Line : Lines)
    {
        const QString Trimmed = Line.trimmed();

        if (Trimmed.isEmpty() || Trimmed.startsWith(QLatin1Char('#')))
        {
            continue;
        }

        const QStringList Fields = SplitFields(Trimmed, Separator);

        if (Fields.size() < 2)
        {
            continue;
        }

        AddRow(Fields.at(0).toStdString(), Fields.at(1).toStdString());
    }

    if (Rows.empty())
    {
        LastError = "no word-translation pairs found";
        return false;
    }

    return true;
}

bool FTAnkiImporter::ParseAnkiDatabase(const std::string& FilePath)
{
    // Deliberately not FTDatabase::Open: that call switches the file to WAL journal mode, which
    // writes to a collection that belongs to the user's Anki install. This opens strictly
    // read-only so importing can never modify the source.
    const QString ConnectionName = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QSqlDatabase Connection = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), ConnectionName);
    Connection.setDatabaseName(QString::fromStdString(FilePath));
    Connection.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));

    if (!Connection.open())
    {
        LastError = Connection.lastError().text().toStdString();

        QSqlDatabase::removeDatabase(ConnectionName);

        return false;
    }

    bool bSucceeded = false;

    {
        QSqlQuery Query(Connection);

        if (!Query.exec(QStringLiteral("SELECT flds FROM notes")))
        {
            LastError = Query.lastError().text().toStdString();
        }
        else
        {
            while (Query.next())
            {
                // Fields come back joined on the unit separator, front first, translation second.
                const QStringList Fields = Query.value(0).toString().split(QChar(0x1f));

                if (Fields.size() >= 2)
                {
                    AddRow(Fields.at(0).toStdString(), Fields.at(1).toStdString());
                }
            }

            bSucceeded = true;
        }
    }

    Connection.close();
    QSqlDatabase::removeDatabase(ConnectionName);

    if (bSucceeded && Rows.empty())
    {
        LastError = "no word-translation pairs found";
        return false;
    }

    return bSucceeded;
}
