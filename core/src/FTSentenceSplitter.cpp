#include "core/FTSentenceSplitter.h"

#include <QSet>
#include <QTextBoundaryFinder>

static const QSet<QString>& FindAbbreviations(const std::string& LanguageCode)
{
    static const QSet<QString> French{
        "M", "MM", "Mme", "Mmes", "Mlle", "Mlles", "Mgr", "Dr", "Pr", "Me", "St", "Ste", "Sts", "Cie", "Vve",
        "av", "Av", "bd", "Bd", "boul", "cf", "Cf", "chap", "éd", "env", "ex", "fig", "hab", "no", "No",
        "p", "pp", "vol", "art", "c.-à-d",
    };
    static const QSet<QString> Spanish{
        "Sr", "Sra", "Srta", "Sres", "Srs", "Dr", "Dra", "Dña", "Ud", "Uds", "Vd", "Vds", "Excmo", "Excma",
        "Ilmo", "Ilma", "Sto", "Sta", "Fr", "Prof", "Lic", "Ing", "Gral", "Cnel", "Mons", "Cía", "Hno", "Hnos",
        "pág", "págs", "núm", "art", "cap", "vol", "ej", "p", "pp", "aprox", "av", "Av", "avda", "Avda",
        "dcha", "izq", "fig", "cf",
    };
    static const QSet<QString> English{
        "Mr", "Mrs", "Ms", "Messrs", "Dr", "Prof", "St", "Jr", "Sr", "Mt", "Capt", "Col", "Gen", "Lt", "Sgt",
        "Rev", "Hon", "Gov", "Ave", "Fig", "fig", "No", "Nos", "vol", "Vol", "pp", "cf", "e.g", "i.e", "vs",
        "viz", "approx",
    };
    static const QSet<QString> Unknown;

    if (LanguageCode == "fr")
    {
        return French;
    }

    if (LanguageCode == "es")
    {
        return Spanish;
    }

    if (LanguageCode == "en")
    {
        return English;
    }

    return Unknown;
}

static bool ContinuesInLowercase(const QString& Text, qsizetype Position)
{
    for (qsizetype Index{Position}; Index < Text.size(); ++Index)
    {
        const QChar Character{Text.at(Index)};

        if (Character.isSpace() || Character.isPunct())
        {
            continue;
        }

        return Character.isLower();
    }

    return false;
}

static QString FindWordBeforePeriod(const QString& Text, qsizetype Boundary)
{
    qsizetype Period{Boundary - 1};

    while (Period >= 0 && Text.at(Period).isSpace())
    {
        --Period;
    }

    if (Period < 0 || Text.at(Period) != u'.')
    {
        return {};
    }

    qsizetype First{Period};

    while (First > 0 && !Text.at(First - 1).isSpace())
    {
        --First;
    }

    while (First < Period && Text.at(First).isPunct())
    {
        ++First;
    }

    return Text.mid(First, Period - First);
}

static bool IsFalseBoundary(const QString& Text, qsizetype Boundary, const QSet<QString>& Abbreviations)
{
    if (ContinuesInLowercase(Text, Boundary))
    {
        return true;
    }

    const QString Word{FindWordBeforePeriod(Text, Boundary)};

    if (Word.size() == 1 && Word.at(0).isUpper())
    {
        return true;
    }

    return Abbreviations.contains(Word);
}

static bool IsOpeningMark(QChar Character)
{
    const QChar::Category Category{Character.category()};

    return Category == QChar::Punctuation_Dash || Category == QChar::Punctuation_Open
        || Category == QChar::Punctuation_InitialQuote || Character == u'¿' || Character == u'¡';
}

static qsizetype MoveBeforeOpeningMarks(const QString& Text, qsizetype Start, qsizetype End)
{
    qsizetype Boundary{End};

    for (qsizetype Index{End - 1}; Index > Start; --Index)
    {
        const QChar Character{Text.at(Index)};

        if (Character.isSpace())
        {
            continue;
        }

        if (!IsOpeningMark(Character))
        {
            break;
        }

        Boundary = Index;
    }

    return Boundary;
}

std::vector<FTSentenceRange> SplitSentences(const QString& Text, const std::string& LanguageCode)
{
    std::vector<FTSentenceRange> Sentences;
    QString Normalized{Text};
    Normalized.replace(u'\r', u' ');
    Normalized.replace(u'\n', u' ');

    const QSet<QString>& Abbreviations{FindAbbreviations(LanguageCode)};
    QTextBoundaryFinder Finder{QTextBoundaryFinder::Sentence, Normalized};
    qsizetype Start{0};

    for (qsizetype End{Finder.toNextBoundary()}; End >= 0; End = Finder.toNextBoundary())
    {
        if (End < Normalized.size() && IsFalseBoundary(Normalized, End, Abbreviations))
        {
            continue;
        }

        const qsizetype Boundary{MoveBeforeOpeningMarks(Normalized, Start, End)};
        Sentences.push_back({Start, Boundary});
        Start = Boundary;
    }

    if (Start < Normalized.size())
    {
        Sentences.push_back({Start, Normalized.size()});
    }

    return Sentences;
}

QString FindSentenceAt(const QString& Text, qsizetype Position, const std::string& LanguageCode)
{
    if (Position < 0 || Position >= Text.size())
    {
        return {};
    }

    for (const FTSentenceRange& Sentence : SplitSentences(Text, LanguageCode))
    {
        if (Position < Sentence.End)
        {
            return Text.mid(Sentence.Start, Sentence.End - Sentence.Start).simplified();
        }
    }

    return {};
}
