#include "DictionaryController.h"
#include "core/FTSentenceSplitter.h"

#include "AppCards.h"
#include "AppDatabase.h"
#include "AppDictionary.h"
#include "AppSpeech.h"
#include "AppTranslator.h"
#include "AppWebSearch.h"
#include "CefrFormatting.h"
#include "WordRowGrouping.h"

#include "core/FTAnswerComparison.h"
#include "core/FTCard.h"
#include "core/FTCardMeaning.h"
#include "core/FTDatabase.h"
#include "core/FTCardStore.h"
#include "core/FTCardRepository.h"
#include "core/FTDictionaryPackage.h"
#include "core/FTDictionaryRepository.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"
#include "core/FTWordForms.h"

#include <QClipboard>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QHash>
#include <QMap>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QThreadPool>
#include <QUrl>

#include <algorithm>
#include <array>
#include <iterator>
#include <map>
#include <optional>

namespace
{
    const QChar Apostrophe{QLatin1Char('\'')};
    const QChar Hyphen{QLatin1Char('-')};

    struct FElisionRules
    {
        QString RestoredVowel;
        QHash<QString, QStringList> RestoredWords;
        QSet<QString> EuphonicParts;
    };

    const FElisionRules& GetElisionRules(const QString& Language)
    {
        static const QHash<QString, FElisionRules> Rules = {
            {QStringLiteral("fr"),
             FElisionRules{
                 QStringLiteral("e"),
                 {{QStringLiteral("l"), {QStringLiteral("le"), QStringLiteral("la")}},
                  {QStringLiteral("s"), {QStringLiteral("se"), QStringLiteral("si")}}},
                 {QStringLiteral("t")}}}
        };

        static const FElisionRules None;

        const auto Found = Rules.constFind(Language);

        return Found != Rules.constEnd() ? *Found : None;
    }

    QString NormaliseApostrophes(const QString& Text)
    {
        QString Normalised = Text;
        Normalised.replace(QChar(0x2019), Apostrophe);
        Normalised.replace(QChar(0x02BC), Apostrophe);

        return Normalised;
    }

    QString TrimPunctuation(const QString& Text)
    {
        const QString Normalised = NormaliseApostrophes(Text);

        qsizetype First = 0;
        qsizetype Last = Text.size() - 1;

        while (First <= Last && !Text[First].isLetterOrNumber())
        {
            ++First;
        }

        while (Last >= First && !Text[Last].isLetterOrNumber() && Normalised[Last] != Apostrophe)
        {
            --Last;
        }

        return Text.mid(First, Last - First + 1);
    }

    struct FLookupPart
    {
        QStringList Alternatives;
        bool bElided = false;
    };

    std::vector<FLookupPart> SplitLookupParts(const QString& Word, const FElisionRules& Rules)
    {
        std::vector<FLookupPart> Parts;

        for (const QString& Piece : NormaliseApostrophes(Word).split(Hyphen, Qt::SkipEmptyParts))
        {
            if (Rules.EuphonicParts.contains(Piece.toLower()))
            {
                continue;
            }

            QStringList Segments = Piece.split(Apostrophe, Qt::SkipEmptyParts);

            for (qsizetype Index = 0; Index < Segments.size(); ++Index)
            {
                const QString Segment = Segments[Index].toLower();
                const bool bElided = Index + 1 < Segments.size() || Piece.endsWith(Apostrophe);

                if (!bElided || Rules.RestoredVowel.isEmpty())
                {
                    Parts.push_back(FLookupPart{{Segments[Index]}, false});

                    continue;
                }

                const auto Restored = Rules.RestoredWords.constFind(Segment);

                Parts.push_back(FLookupPart{Restored != Rules.RestoredWords.constEnd()
                                                ? *Restored
                                                : QStringList{Segment + Rules.RestoredVowel},
                                            true});
            }
        }

        std::stable_partition(Parts.begin(), Parts.end(),
                              [](const FLookupPart& Part) { return !Part.bElided; });

        return Parts;
    }

    bool HasExactMatch(const std::vector<FTDictionarySearchMatch>& Matches)
    {
        return std::any_of(Matches.begin(), Matches.end(),
            [](const FTDictionarySearchMatch& Match) { return Match.bExactMatch; });
    }

    QString FoldSpanishAccents(QString Word)
    {
        const QString Accented = QString::fromUtf8("áéíóú");
        const QString Plain = QStringLiteral("aeiou");

        for (qsizetype Index = 0; Index < Accented.size(); ++Index)
        {
            Word.replace(Accented[Index], Plain[Index]);
        }

        return Word;
    }

    void AppendLookupMatches(const FTDictionaryRepository& Repository, const QString& Word,
                             std::vector<FTDictionarySearchMatch>& Matches, bool bVerbOnly = false)
    {
        for (const FTDictionaryEntry& Entry : Repository.LookUp(Word.toStdString()))
        {
            if (bVerbOnly && Entry.PartOfSpeech != "verb")
            {
                continue;
            }

            if (bVerbOnly && FoldSpanishAccents(QString::fromStdString(Entry.Headword)) != Word
                && std::none_of(Entry.Forms.begin(), Entry.Forms.end(), [&Word](const FTDictionaryForm& Form)
                {
                    return (Form.Mood == 4 || Form.Tense == 5 || Form.Tense == 6)
                        && FoldSpanishAccents(QString::fromStdString(Form.Text)) == Word;
                }))
            {
                continue;
            }

            const bool bDuplicate = std::any_of(Matches.begin(), Matches.end(), [&Entry](const auto& Match)
            {
                return Match.Headword == Entry.Headword && Match.PartOfSpeech == Entry.PartOfSpeech;
            });

            if (bDuplicate)
            {
                continue;
            }

            std::optional<FTDictionarySearchMatch> Match = Repository.LookUpExact(Entry.Headword, Entry.PartOfSpeech);

            if (Match.has_value())
            {
                Match->bExactMatch = true;
                Matches.push_back(std::move(*Match));
            }
        }
    }

    std::vector<FTDictionarySearchMatch> SearchSpanishPronouns(const FTDictionaryRepository& Repository,
                                                              const QString& Word)
    {
        static const QStringList Pronouns{"me", "te", "se", "nos", "os", "lo", "la", "los", "las", "le", "les"};
        static const QStringList FirstPronouns{"me", "te", "se", "nos", "os"};
        static const QStringList LastPronouns{"lo", "la", "los", "las"};
        const QString Lower = Word.toLower();
        QStringList Suffixes;

        for (const QString& First : FirstPronouns)
        {
            for (const QString& Last : LastPronouns)
            {
                Suffixes.append(First + Last);
            }
        }

        Suffixes.append(Pronouns);
        std::vector<FTDictionarySearchMatch> Matches;

        for (const QString& Suffix : Suffixes)
        {
            if (!Lower.endsWith(Suffix) || Lower.size() <= Suffix.size() + 1)
            {
                continue;
            }

            const QString Stem = FoldSpanishAccents(Lower.chopped(Suffix.size()));
            AppendLookupMatches(Repository, Stem, Matches, true);
        }

        return Matches;
    }

    std::vector<FTDictionarySearchMatch> SearchWord(const FTDictionaryRepository& Repository, const QString& Word,
                                                    const QString& TargetLanguage, int Limit, bool bNativeQuery)
    {
        std::vector<FTDictionarySearchMatch> Matches = Repository.Search(Word.toStdString(), Limit, bNativeQuery);

        if (bNativeQuery || HasExactMatch(Matches))
        {
            return Matches;
        }

        const QString Normalised = NormaliseApostrophes(Word);
        const bool bCompound = Normalised.contains(Apostrophe) || Normalised.contains(Hyphen);
        const bool bPhrase = Word.trimmed().contains(QLatin1Char(' '));

        if (bPhrase)
        {
            return Matches;
        }

        std::vector<FTDictionarySearchMatch> PartMatches = TargetLanguage == QStringLiteral("es")
            ? SearchSpanishPronouns(Repository, Word) : std::vector<FTDictionarySearchMatch>{};

        if (bCompound)
        {
            for (const FLookupPart& Part : SplitLookupParts(Word, GetElisionRules(TargetLanguage)))
            {
                for (const QString& Alternative : Part.Alternatives)
                {
                    AppendLookupMatches(Repository, Alternative.toLower(), PartMatches);
                }
            }
        }

        if (PartMatches.empty())
        {
            return Matches;
        }

        qCInfo(LogApp) << "Dictionary search resolved" << Word << "through its parts -" << PartMatches.size()
                       << "matches";

        PartMatches.insert(PartMatches.end(), std::make_move_iterator(Matches.begin()),
                           std::make_move_iterator(Matches.end()));

        if (PartMatches.size() > static_cast<size_t>(Limit))
        {
            PartMatches.resize(static_cast<size_t>(Limit));
        }

        return PartMatches;
    }

    QString FrequencyBucket(const std::optional<int>& Rank)
    {
        if (!Rank.has_value())
        {
            return {};
        }

        static const std::array<std::pair<int, const char*>, 8> Buckets{{
            {100, "TOP 100"}, {500, "TOP 500"}, {1000, "TOP 1000"}, {2000, "TOP 2000"},
            {5000, "TOP 5000"}, {10000, "TOP 10000"}, {20000, "TOP 20000"}, {50000, "TOP 50000"}
        }};

        for (const auto& [Ceiling, Label] : Buckets)
        {
            if (*Rank <= Ceiling)
            {
                return QString::fromUtf8(Label);
            }
        }

        return {};
    }

    int VerbColumnIndex(int Person, int Number)
    {
        return (Number - 1) * 3 + (Person - 1);
    }

    bool IsVerbShaped(const std::vector<FTDictionaryForm>& Forms)
    {
        return std::any_of(Forms.begin(), Forms.end(),
            [](const FTDictionaryForm& Form)
            {
                return Form.Mood != 0 || Form.Tense >= 5;
            });
    }

    void AppendUniqueText(std::vector<std::string>& Values, const std::string& Value)
    {
        if (std::find(Values.begin(), Values.end(), Value) == Values.end())
        {
            Values.push_back(Value);
        }
    }

    QString JoinTexts(const std::vector<std::string>& Values)
    {
        QStringList Parts;

        for (const std::string& Value : Values)
        {
            Parts << QString::fromStdString(Value);
        }

        return Parts.join(QStringLiteral(" / "));
    }

    QString MarkEndings(const std::vector<std::string>& Values, const std::string& Headword, int Mood, int Tense,
                        int CellIndex)
    {
        const std::string& TargetLanguage = FTSettings::Get().GetLanguageRules().TargetLanguage;

        QStringList Parts;

        for (const std::string& Value : Values)
        {
            const std::string Ending = FindFormEnding(TargetLanguage, Headword, Mood, Tense, CellIndex, Value);
            const QString Stem = QString::fromStdString(Value.substr(0, Value.size() - Ending.size())).toHtmlEscaped();

            Parts << (Ending.empty() ? Stem
                                     : Stem + QStringLiteral("<a href=\"ending\">")
                                           + QString::fromStdString(Ending).toHtmlEscaped() + QStringLiteral("</a>"));
        }

        return Parts.join(QStringLiteral(" / "));
    }

    QStringList ListCues(const std::string& Kind)
    {
        if (Kind == FormsKindVerb && FTSettings::Get().GetLanguageRules().TargetLanguage == "en")
        {
            return {"I", "you", "he/she/it", "we", "you", "they"};
        }

        QStringList Cues;

        for (const std::string& Cue : ListFormsCues(Kind, FTSettings::Get().GetLanguageRules().TargetLanguage))
        {
            Cues << QString::fromStdString(Cue);
        }

        return Cues;
    }

    QVariantMap BuildVerbForms(const std::vector<FTDictionaryForm>& Forms, const std::string& Headword, const QString& SearchText)
    {
        std::map<int, std::vector<std::string>> NonfiniteByTense;
        std::map<int, std::map<int, std::array<std::vector<std::string>, 6>>> CellsByMoodTense;

        for (const FTDictionaryForm& Form : Forms)
        {
            if (Form.Mood == 0)
            {
                if (Form.Tense >= 5)
                {
                    AppendUniqueText(NonfiniteByTense[Form.Tense], Form.Text);
                }

                continue;
            }

            if (Form.Person < 1 || Form.Person > 3 || Form.Number < 1 || Form.Number > 2)
            {
                continue;
            }

            std::array<std::vector<std::string>, 6>& Cells =
                CellsByMoodTense[Form.Mood][Form.Tense];

            AppendUniqueText(Cells[VerbColumnIndex(Form.Person, Form.Number)], Form.Text);
        }

        const auto Matches = [&SearchText](const std::vector<std::string>& Values)
        {
            return !SearchText.isEmpty() && std::any_of(Values.begin(), Values.end(), [&SearchText](const std::string& Value)
            {
                return QString::fromStdString(Value).compare(SearchText, Qt::CaseInsensitive) == 0;
            });
        };
        QStringList MatchedReadings;
        const QStringList Cues = ListCues(FormsKindVerb);
        const QStringList TenseNames{QObject::tr("Present"), QObject::tr("Present"), QObject::tr("Imperfect"),
                                     QObject::tr("Simple past"), QObject::tr("Future")};
        const QStringList NonfiniteNames{"", "", "", "", "", QObject::tr("Infinitive"), QObject::tr("Gerund"),
                                         QObject::tr("Present participle"), QObject::tr("Past participle")};
        QString MatchedKey;
        bool bMatched = false;
        QVariantList Nonfinite;

        for (const auto& [Tense, Texts] : NonfiniteByTense)
        {
            QVariantMap Row;
            Row[QStringLiteral("tense")] = Tense;
            Row[QStringLiteral("text")] = JoinTexts(Texts);
            const bool bRowMatched = Matches(Texts);
            Row[QStringLiteral("matched")] = bRowMatched;
            if (bRowMatched && Tense >= 0 && Tense < NonfiniteNames.size())
            {
                MatchedReadings << NonfiniteNames[Tense];
            }
            bMatched = bMatched || bRowMatched;
            Nonfinite.append(Row);
        }

        QVariantList Moods;

        for (const auto& [Mood, TenseCells] : CellsByMoodTense)
        {
            QVariantList Tenses;

            for (const auto& [Tense, Cells] : TenseCells)
            {
                QVariantList CellTexts;
                QVariantList MarkedTexts;
                QVariantList MatchesByCell;
                bool bTenseMatched = false;

                for (int CellIndex = 0; CellIndex < static_cast<int>(Cells.size()); ++CellIndex)
                {
                    const std::vector<std::string>& CellValues = Cells[static_cast<size_t>(CellIndex)];

                    CellTexts.append(JoinTexts(CellValues));
                    const bool bCellMatched = Matches(CellValues);
                    MatchesByCell.append(bCellMatched);
                    bTenseMatched = bTenseMatched || bCellMatched;
                    MarkedTexts.append(MarkEndings(CellValues, Headword, Mood, Tense, CellIndex));
                }

                if (bTenseMatched)
                {
                    QStringList Subjects;
                    for (int Index = 0; Index < MatchesByCell.size(); ++Index)
                    {
                        if (MatchesByCell[Index].toBool() && Index < Cues.size())
                        {
                            Subjects << Cues[Index];
                        }
                    }
                    const QString TenseName = Tense >= 0 && Tense < TenseNames.size() ? TenseNames[Tense] : QString{};
                    const QString Description = Mood == 3 ? QObject::tr("Present conditional")
                        : Mood == 4 ? QObject::tr("Present imperative")
                        : Mood == 2 ? TenseName + " " + QObject::tr("subjunctive") : TenseName;
                    MatchedReadings << Description + (Subjects.isEmpty() ? QString{} : " · " + Subjects.join(" / "));
                }
                if (bTenseMatched && MatchedKey.isEmpty())
                {
                    MatchedKey = QString::number(Mood) + "-" + QString::number(Tense);
                }
                bMatched = bMatched || bTenseMatched;
                QVariantMap TenseRow;
                TenseRow[QStringLiteral("matches")] = MatchesByCell;
                TenseRow[QStringLiteral("matched")] = bTenseMatched;
                TenseRow[QStringLiteral("tense")] = Tense;
                TenseRow[QStringLiteral("cells")] = CellTexts;
                TenseRow[QStringLiteral("marked")] = MarkedTexts;
                Tenses.append(TenseRow);
            }

            QVariantMap MoodRow;
            MoodRow[QStringLiteral("mood")] = Mood;
            MoodRow[QStringLiteral("tenses")] = Tenses;
            Moods.append(MoodRow);
        }

        QVariantMap Result;
        Result[QStringLiteral("kind")] = QStringLiteral("verb");
        Result[QStringLiteral("matchedReadings")] = MatchedReadings;
        Result[QStringLiteral("matchedKey")] = MatchedKey;
        Result[QStringLiteral("matchedText")] = bMatched ? SearchText : QString{};
        Result[QStringLiteral("cues")] = ListCues(FormsKindVerb);
        Result[QStringLiteral("language")] = QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage);
        Result[QStringLiteral("nonfinite")] = Nonfinite;
        Result[QStringLiteral("moods")] = Moods;
        return Result;
    }

    QVariantMap BuildAgreementForms(const FTDictionaryEntry& Entry)
    {
        const std::string Kind = Entry.PartOfSpeech == FormsKindNoun ? FormsKindNoun : FormsKindAdjective;

        if (!MakeFormsCard(Kind, Entry, FTSettings::Get().GetLanguageRules().TargetLanguage, {}).has_value())
        {
            QVariantMap Result;
            Result[QStringLiteral("kind")] = QStringLiteral("none");
            return Result;
        }

        std::array<std::array<std::vector<std::string>, 2>, 2> Cells;

        for (const FTDictionaryForm& Form : Entry.Forms)
        {
            if (Form.Gender < 1 || Form.Gender > 2 || Form.Number < 1 || Form.Number > 2)
            {
                continue;
            }

            AppendUniqueText(Cells[Form.Gender - 1][Form.Number - 1], Form.Text);
        }

        const bool bFeminineHeadword = !Entry.Senses.empty() && GetGenderCode(Entry.Senses.front().Gender) == "f";
        std::vector<std::string>& HeadwordCell = Cells[bFeminineHeadword ? 1 : 0][0];

        if (HeadwordCell.empty())
        {
            HeadwordCell.push_back(Entry.Headword);
        }

        QVariantList Rows;
        QVariantList MarkedRows;

        for (const std::array<std::vector<std::string>, 2>& Row : Cells)
        {
            QVariantList RowTexts;
            QVariantList MarkedTexts;

            for (const std::vector<std::string>& CellValues : Row)
            {
                RowTexts.append(JoinTexts(CellValues));
                MarkedTexts.append(MarkEndings(CellValues, Entry.Headword, 0, 0, -1));
            }

            Rows.append(QVariant::fromValue(RowTexts));
            MarkedRows.append(QVariant::fromValue(MarkedTexts));
        }

        QVariantMap Result;
        Result[QStringLiteral("kind")] = QStringLiteral("agreement");
        Result[QStringLiteral("cues")] = ListCues(Entry.PartOfSpeech == FormsKindNoun ? FormsKindNoun : FormsKindAdjective);
        Result[QStringLiteral("cells")] = Rows;
        Result[QStringLiteral("marked")] = MarkedRows;
        return Result;
    }

    QVariantMap BuildForms(const FTDictionaryEntry& Entry, const QString& SearchText)
    {
        if (Entry.Forms.empty())
        {
            QVariantMap Result;
            Result[QStringLiteral("kind")] = QStringLiteral("none");
            return Result;
        }

        return IsVerbShaped(Entry.Forms) ? BuildVerbForms(Entry.Forms, Entry.Headword, SearchText) : BuildAgreementForms(Entry);
    }

    QVariantMap BuildSense(const FTDictionarySense& Sense)
    {
        QVariantMap Result;
        Result[QStringLiteral("gender")] = QString::fromStdString(GetGenderCode(Sense.Gender));
        Result[QStringLiteral("text")] = QString::fromStdString(
            Sense.Definition.empty() ? Sense.Gloss : Sense.Definition);

        QStringList Translations;

        for (const FTDictionaryTranslation& Translation : Sense.Translations)
        {
            Translations << QString::fromStdString(Translation.Text);
        }

        Result[QStringLiteral("translations")] = Translations;

        return Result;
    }

    std::optional<int> ResolveEntryGender(const FTDictionaryEntry& Entry)
    {
        if (Entry.PartOfSpeech != "noun")
        {
            return std::nullopt;
        }

        bool bMasculine{false};
        bool bFeminine{false};

        for (const FTDictionarySense& Sense : Entry.Senses)
        {
            if (!Sense.Gender.has_value())
            {
                continue;
            }

            bMasculine = bMasculine || *Sense.Gender == GenderMasculine || *Sense.Gender == GenderCommon;
            bFeminine = bFeminine || *Sense.Gender == GenderFeminine || *Sense.Gender == GenderCommon;
        }

        if (bMasculine && bFeminine)
        {
            return GenderCommon;
        }

        if (bMasculine)
        {
            return GenderMasculine;
        }

        if (bFeminine)
        {
            return GenderFeminine;
        }

        return std::nullopt;
    }

    QString BuildEntryGender(const FTDictionaryEntry& Entry)
    {
        if (Entry.PartOfSpeech != "noun")
        {
            return {};
        }

        const std::optional<int> Gender = ResolveEntryGender(Entry);

        if (!Gender.has_value())
        {
            return QCoreApplication::translate("DictionaryController", "none");
        }

        if (*Gender == GenderCommon)
        {
            return QCoreApplication::translate("DictionaryController", "mixed");
        }

        return *Gender == GenderMasculine ? QCoreApplication::translate("DictionaryController", "masculine")
                                          : QCoreApplication::translate("DictionaryController", "feminine");
    }

    QVariantList BuildExamples(const std::vector<FTDictionaryExample>& Examples)
    {
        std::vector<const FTDictionaryExample*> Shortest;
        Shortest.reserve(Examples.size());

        for (const FTDictionaryExample& Example : Examples)
        {
            Shortest.push_back(&Example);
        }

        std::stable_sort(Shortest.begin(), Shortest.end(),
            [](const FTDictionaryExample* Left, const FTDictionaryExample* Right)
            {
                return QString::fromStdString(Left->Text).size()
                    < QString::fromStdString(Right->Text).size();
            });

        const int Wanted = FTSettings::Get().GetDictionaryDisplayRules().ExamplesPerSense;

        if (Wanted >= 0 && Shortest.size() > static_cast<size_t>(Wanted))
        {
            Shortest.resize(static_cast<size_t>(Wanted));
        }

        QVariantList Result;

        for (const FTDictionaryExample* Example : Shortest)
        {
            QVariantMap Row;
            Row[QStringLiteral("text")] = QString::fromStdString(Example->Text);
            Row[QStringLiteral("translation")] = QString::fromStdString(Example->Translation);
            Result.append(Row);
        }

        return Result;
    }

    QVariantMap BuildEntry(const FTDictionaryEntry& Entry, const QString& SearchText)
    {
        QVariantMap Result;
        Result[QStringLiteral("id")] = static_cast<qint64>(Entry.Id);
        Result[QStringLiteral("headword")] = QString::fromStdString(Entry.Headword);
        Result[QStringLiteral("displayHeadword")] = QString::fromStdString(GetDisplayHeadword(
            Entry.Headword, Entry.PartOfSpeech, FTSettings::Get().GetLanguageRules().TargetLanguage));
        Result[QStringLiteral("partOfSpeech")] = QString::fromStdString(Entry.PartOfSpeech);
        Result[QStringLiteral("gender")] = BuildEntryGender(Entry);
        Result[QStringLiteral("headwordGender")] = ResolveEntryGender(Entry).value_or(-1);
        Result[QStringLiteral("phonetic")] = QString::fromStdString(Entry.Phonetic);
        Result[QStringLiteral("frequencyBucket")] = FrequencyBucket(Entry.FrequencyRank);
        Result[QStringLiteral("cefrLevel")] = CefrLetter(Entry.CefrLevel);
        Result[QStringLiteral("inDictionary")] = Entry.Id != 0;

        QVariantList Senses;

        for (const FTDictionarySense& Sense : Entry.Senses)
        {
            if (Sense.Definition.empty() && Sense.Gloss.empty())
            {
                continue;
            }

            Senses.append(BuildSense(Sense));
        }

        Result[QStringLiteral("senses")] = Senses;
        Result[QStringLiteral("examples")] = BuildExamples(Entry.Examples);
        Result[QStringLiteral("forms")] = BuildForms(Entry, SearchText);

        if (Entry.PartOfSpeech == FormsKindVerb)
        {
            const std::string& Language = FTSettings::Get().GetLanguageRules().TargetLanguage;
            const QString Headword = QString::fromStdString(Entry.Headword);
            const QString Ending = Headword.right(2).toLower();
            Result[QStringLiteral("verbEnding")] = QStringList{"er", "ir", "re", "ar"}.contains(Ending) ? "-" + Ending : QString{};
            Result[QStringLiteral("irregular")] = IsIrregularVerb(Language, Entry);
            const QString Group = QString::fromStdString(GetVerbGroup(Language, Entry));
            Result[QStringLiteral("verbGroup")] = Group;
            const QMap<QString, QString> GroupNames{{"first", QObject::tr("1st group")},
                {"second", QObject::tr("2nd group")}, {"third", QObject::tr("3rd group")},
                {"irregular", QObject::tr("Irregular")}};
            const QString VerbEnding = Language == "it" ? Headword.right(3).toLower() : Ending;
            Result[QStringLiteral("verbGroupLabel")] = GroupNames.value(Group)
                + (Group.isEmpty() || Group == "irregular" || (Language == "fr" && Group == "third")
                    ? QString{} : " · -" + VerbEnding);
            const QMap<QString, QString> Languages{{"fr", "french"}, {"es", "spanish"}, {"pt", "portuguese"},
                                                  {"en", "english"}, {"de", "german"}, {"it", "italian"}};
            const QString LanguageName = Languages.value(QString::fromStdString(Language));
            if (!LanguageName.isEmpty() && Entry.Id != 0)
            {
                Result[QStringLiteral("conjugationUrl")] = "https://conjugator.reverso.net/conjugation-"
                    + LanguageName + "-verb-" + QString::fromLatin1(QUrl::toPercentEncoding(Headword)) + ".html";
            }
        }

        return Result;
    }

    constexpr int MaximumChipTranslations = 50;

    QVariantList BuildAllTranslations(const FTDictionaryEntry& Entry, const QString& Headword)
    {
        QVariantList Merged;
        QHash<QString, int> SeenAt;

        std::vector<std::optional<int>> Genders;

        for (const FTDictionarySense& Sense : Entry.Senses)
        {
            if (std::find(Genders.begin(), Genders.end(), Sense.Gender) == Genders.end())
            {
                Genders.push_back(Sense.Gender);
            }
        }

        if (Genders.empty())
        {
            Genders.push_back(std::nullopt);
        }

        FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();

        for (const std::optional<int>& Gender : Genders)
        {
            const std::vector<FTDictionaryTranslation> Readings = Dictionary.LoadTranslationsForReading(
                Headword.toStdString(), Entry.PartOfSpeech, Gender, MaximumChipTranslations);

            for (const FTDictionaryTranslation& Translation : Readings)
            {
                const QString Text = QString::fromStdString(Translation.Text);
                const QString Key = Text.toLower();

                const bool bHasProbability = Translation.Probability.has_value();
                const double Probability = Translation.Probability.value_or(0.0);

                const auto Existing = SeenAt.constFind(Key);

                if (Existing != SeenAt.constEnd())
                {
                    QVariantMap Kept = Merged[*Existing].toMap();

                    if (bHasProbability
                        && Probability > Kept.value(QStringLiteral("probability")).toDouble())
                    {
                        Kept[QStringLiteral("probability")] = Probability;
                        Kept[QStringLiteral("hasProbability")] = true;
                    }

                    Merged[*Existing] = Kept;

                    continue;
                }

                QVariantMap Row;
                Row[QStringLiteral("text")] = Text;
                Row[QStringLiteral("hasProbability")] = bHasProbability;
                Row[QStringLiteral("probability")] = Probability;

                const std::optional<FTWordTranslationMatch> Match = AppDatabase::Get().IsReady()
                    ? AppDatabase::Get().GetCardRepository().FindWordTranslationMatch(
                          Headword.toStdString(), Entry.PartOfSpeech, Translation.Text)
                    : std::nullopt;

                Row[QStringLiteral("learnedStage")] = Match.has_value() ? Match->Stage : -1;
                Row[QStringLiteral("cardId")] = Match.has_value()
                    ? QString::fromStdString(Match->CardId) : QString();
                Row[QStringLiteral("cardPartOfSpeech")] = Match.has_value()
                    ? QString::fromStdString(Match->PartOfSpeech) : QString();
                Row[QStringLiteral("cardGender")] = Match.has_value() && Match->Gender.has_value()
                    ? *Match->Gender : -1;

                SeenAt.insert(Key, static_cast<int>(Merged.size()));
                Merged.append(Row);
            }

            if (AppDatabase::Get().IsReady())
            {
                const std::vector<std::string> CustomTranslations =
                    AppDatabase::Get().GetCardRepository().FindCardTranslationsForReading(
                        Headword.toStdString(), Entry.PartOfSpeech, Gender);

                for (const std::string& CustomText : CustomTranslations)
                {
                    const QString Text = QString::fromStdString(CustomText);
                    const QString Key = Text.toLower();

                    if (SeenAt.contains(Key))
                    {
                        continue;
                    }

                    QVariantMap Row;
                    Row[QStringLiteral("text")] = Text;
                    Row[QStringLiteral("hasProbability")] = false;
                    Row[QStringLiteral("probability")] = 0.0;

                    const std::optional<FTWordTranslationMatch> Match =
                        AppDatabase::Get().GetCardRepository().FindWordTranslationMatch(
                            Headword.toStdString(), Entry.PartOfSpeech, CustomText);

                    Row[QStringLiteral("learnedStage")] = Match.has_value() ? Match->Stage : -1;
                    Row[QStringLiteral("cardId")] = Match.has_value()
                        ? QString::fromStdString(Match->CardId) : QString();
                    Row[QStringLiteral("cardPartOfSpeech")] = Match.has_value()
                        ? QString::fromStdString(Match->PartOfSpeech) : QString();
                    Row[QStringLiteral("cardGender")] = Match.has_value() && Match->Gender.has_value()
                        ? *Match->Gender : -1;

                    SeenAt.insert(Key, static_cast<int>(Merged.size()));
                    Merged.append(Row);
                }
            }
        }

        double Best = 0.0;

        for (const QVariant& Value : Merged)
        {
            Best = std::max(Best, Value.toMap().value(QStringLiteral("probability")).toDouble());
        }

        for (QVariant& Value : Merged)
        {
            QVariantMap Row = Value.toMap();
            const double Probability = Row.value(QStringLiteral("probability")).toDouble();
            Row[QStringLiteral("relative")] = Best > 0.0 ? Probability / Best : 0.0;
            Value = Row;
        }

        std::stable_sort(Merged.begin(), Merged.end(),
            [](const QVariant& Left, const QVariant& Right)
            {
                const QVariantMap LeftRow = Left.toMap();
                const QVariantMap RightRow = Right.toMap();

                const bool LeftLearned = LeftRow.value(QStringLiteral("learnedStage")).toInt() >= 0;
                const bool RightLearned = RightRow.value(QStringLiteral("learnedStage")).toInt() >= 0;

                if (LeftLearned != RightLearned)
                {
                    return LeftLearned;
                }

                const bool LeftHasProbability = LeftRow.value(QStringLiteral("hasProbability")).toBool();
                const bool RightHasProbability = RightRow.value(QStringLiteral("hasProbability")).toBool();

                if (LeftHasProbability != RightHasProbability)
                {
                    return LeftHasProbability;
                }

                return LeftRow.value(QStringLiteral("probability")).toDouble()
                    > RightRow.value(QStringLiteral("probability")).toDouble();
            });

        return Merged;
    }

    constexpr std::array<std::pair<const char*, const char*>, 13> PartOfSpeechNames{{
        {"noun", QT_TRANSLATE_NOOP("DictionaryController", "Noun")},
        {"verb", QT_TRANSLATE_NOOP("DictionaryController", "Verb")},
        {"adj", QT_TRANSLATE_NOOP("DictionaryController", "Adjective")},
        {"adv", QT_TRANSLATE_NOOP("DictionaryController", "Adverb")},
        {"name", QT_TRANSLATE_NOOP("DictionaryController", "Proper noun")},
        {"intj", QT_TRANSLATE_NOOP("DictionaryController", "Interjection")},
        {"prep", QT_TRANSLATE_NOOP("DictionaryController", "Preposition")},
        {"phrase", QT_TRANSLATE_NOOP("DictionaryController", "Phrase")},
        {"prep_phrase", QT_TRANSLATE_NOOP("DictionaryController", "Prepositional phrase")},
        {"num", QT_TRANSLATE_NOOP("DictionaryController", "Numeral")},
        {"pron", QT_TRANSLATE_NOOP("DictionaryController", "Pronoun")},
        {"conj", QT_TRANSLATE_NOOP("DictionaryController", "Conjunction")},
        {"det", QT_TRANSLATE_NOOP("DictionaryController", "Determiner")}
    }};

    QString PartOfSpeechName(const QString& Code)
    {
        const auto Found = std::find_if(PartOfSpeechNames.begin(), PartOfSpeechNames.end(),
            [&Code](const auto& Name) { return Code == QLatin1String(Name.first); });

        return Found == PartOfSpeechNames.end() ? Code : QCoreApplication::translate("DictionaryController", Found->second);
    }

    QVariantMap BuildPartOfSpeechChoice(const QString& Code)
    {
        QVariantMap Choice;
        Choice[QStringLiteral("value")] = Code;
        Choice[QStringLiteral("label")] = PartOfSpeechName(Code);

        return Choice;
    }

    QVariantList BuildPartsOfSpeech(const QVariantList& Readings, bool bInDictionary, const QString& Requested)
    {
        QVariantList Result;
        QStringList Codes;

        if (bInDictionary)
        {
            for (const QVariant& Reading : Readings)
            {
                Codes << Reading.toMap().value(QStringLiteral("partOfSpeech")).toString();
            }
        }
        else
        {
            for (const auto& Name : PartOfSpeechNames)
            {
                Codes << QString::fromLatin1(Name.first);
            }
        }

        if (!Requested.isEmpty() && !Codes.contains(Requested))
        {
            Codes << Requested;
        }

        for (const QString& Code : Codes)
        {
            Result.append(BuildPartOfSpeechChoice(Code));
        }

        return Result;
    }

    constexpr int CardEntryLimit = 50;

    std::vector<FTCard> LoadHeadwordCards(const QString& Headword)
    {
        std::vector<FTCard> Result;

        if (!AppDatabase::Get().IsReady())
        {
            return Result;
        }

        const QString Wanted = Headword.trimmed().toLower();

        for (FTCard& Card : AppDatabase::Get().GetCardRepository().FindCardsByHeadword(Wanted.toStdString(), CardEntryLimit))
        {
            if (QString::fromStdString(Card.TargetWord).toLower() == Wanted)
            {
                Result.push_back(std::move(Card));
            }
        }

        return Result;
    }

    QVariantList BuildCardRows(const std::vector<FTCard>& Cards, const std::string& PartOfSpeech)
    {
        QVariantList Result;

        for (const FTCard& Card : Cards)
        {
            if (Card.PartOfSpeech != PartOfSpeech && !Card.PartOfSpeech.empty())
            {
                continue;
            }

            QVariantMap Row;
            Row[QStringLiteral("cardId")] = QString::fromStdString(Card.Id);
            Row[QStringLiteral("word")] = QString::fromStdString(Card.TargetWord);
            Row[QStringLiteral("translation")] = QString::fromStdString(Card.Translation);
            Row[QStringLiteral("gender")] = Card.Gender.has_value() ? *Card.Gender : -1;
            Row[QStringLiteral("partOfSpeech")] = QString::fromStdString(Card.PartOfSpeech);
            Row[QStringLiteral("stage")] = Card.LeitnerStage;
            Row[QStringLiteral("completed")] = IsCompleted(Card);
            Result.append(Row);
        }

        return Result;
    }

    std::vector<FTDictionaryEntry> BuildCardEntries(const std::vector<FTCard>& Cards)
    {
        std::vector<FTDictionaryEntry> Result;

        for (const FTCard& Card : Cards)
        {
            auto Existing = std::find_if(Result.begin(), Result.end(),
                [&Card](const FTDictionaryEntry& Entry)
                {
                    return Entry.PartOfSpeech == Card.PartOfSpeech;
                });

            if (Existing == Result.end())
            {
                FTDictionaryEntry Entry;
                Entry.Headword = Result.empty() ? Card.TargetWord : Result.front().Headword;
                Entry.PartOfSpeech = Card.PartOfSpeech;
                Result.push_back(std::move(Entry));
                Existing = std::prev(Result.end());
            }

            FTDictionarySense Sense;
            Sense.Definition = Card.Definition;
            Sense.Gender = Card.Gender;
            Existing->Senses.push_back(std::move(Sense));
        }

        if (!Result.empty())
        {
            qCInfo(LogApp) << "Dictionary entry built from cards:" << QString::fromStdString(Result.front().Headword)
                           << "readings" << Result.size();
        }

        return Result;
    }

    QVariantList BuildMeanings(const FTDictionaryEntry& Entry, const std::vector<FTCard>& Cards)
    {
        QVariantList Result;

        std::vector<FTDictionarySense> Meanings;

        for (const FTDictionarySense& Sense : Entry.Senses)
        {
            if (!Sense.Gist.empty())
            {
                Meanings.push_back(Sense);
            }
        }

        SortMeaningsByUse(Meanings);

        for (const FTDictionarySense& Sense : Meanings)
        {
            QVariantMap Row = AppCards::MakeMeaningEntry(Sense);

            const auto Card = std::find_if(Cards.begin(), Cards.end(),
                [&Entry, &Sense](const FTCard& Candidate)
                {
                    return Candidate.MeaningGist == Sense.Gist && Candidate.PartOfSpeech == Entry.PartOfSpeech
                        && MeaningFitsGender(Sense, Candidate.Gender);
                });

            const bool bLearned = Card != Cards.end();

            if (bLearned)
            {
                QStringList Translations = Row.value(QStringLiteral("translations")).toStringList();

                for (const std::string& Part : SplitTranslations(Card->Translation))
                {
                    const QString CardTranslation = QString::fromStdString(Part);

                    if (!CardTranslation.isEmpty() && !Translations.contains(CardTranslation, Qt::CaseInsensitive))
                    {
                        Translations.append(CardTranslation);
                    }
                }

                Row[QStringLiteral("translations")] = Translations;
            }

            Row[QStringLiteral("learnedStage")] = bLearned ? Card->LeitnerStage : -1;
            Row[QStringLiteral("completed")] = bLearned && IsCompleted(*Card);
            Row[QStringLiteral("cardId")] = bLearned ? QString::fromStdString(Card->Id) : QString();
            Row[QStringLiteral("cardTranslation")] = bLearned ? QString::fromStdString(Card->Translation) : QString();
            Row[QStringLiteral("cardGender")] = bLearned && Card->Gender.has_value() ? *Card->Gender : -1;

            Result.append(Row);
        }

        return Result;
    }

    QVariantList BuildReadings(const std::vector<FTDictionaryEntry>& Found, const std::string& Headword)
    {
        QVariantList Result;
        QStringList SeenPartsOfSpeech;

        const bool bDatabaseReady = AppDatabase::Get().IsReady();

        for (const FTDictionaryEntry& Candidate : Found)
        {
            const QString PartOfSpeech = QString::fromStdString(Candidate.PartOfSpeech);

            if (Candidate.Headword != Headword || PartOfSpeech.isEmpty() || SeenPartsOfSpeech.contains(PartOfSpeech))
            {
                continue;
            }

            SeenPartsOfSpeech << PartOfSpeech;

            QVariantMap Row;
            Row[QStringLiteral("partOfSpeech")] = PartOfSpeech;
            Row[QStringLiteral("label")] = PartOfSpeechName(PartOfSpeech);
            Row[QStringLiteral("stage")] = bDatabaseReady
                ? AppDatabase::Get().GetCardRepository().FindWordStage(Headword, Candidate.PartOfSpeech).value_or(-1)
                : -1;
            Row[QStringLiteral("completed")] = bDatabaseReady
                && AppDatabase::Get().GetCardRepository().IsWordCompleted(Headword, Candidate.PartOfSpeech);

            Result.append(Row);
        }

        return Result;
    }

    QVariantMap BuildResultRow(const FTDictionarySearchMatch& Match)
    {
        QVariantMap Row;
        Row[QStringLiteral("word")] = QString::fromStdString(Match.Headword);
        Row[QStringLiteral("displayWord")] = QString::fromStdString(GetDisplayHeadword(
            Match.Headword, Match.PartOfSpeech, FTSettings::Get().GetLanguageRules().TargetLanguage));
        Row[QStringLiteral("verbGroup")] = AppCards::GetVerbGroup(Match.Headword, Match.PartOfSpeech);
        Row[QStringLiteral("partOfSpeech")] = QString::fromStdString(Match.PartOfSpeech);
        Row[QStringLiteral("translation")] = QString::fromStdString(Match.Translation);
        Row[QStringLiteral("cefrLevel")] = CefrLetter(Match.CefrLevel);
        Row[QStringLiteral("exact")] = Match.bExactMatch;

        if (Match.FrequencyRank.has_value())
        {
            Row[QStringLiteral("frequencyRank")] = *Match.FrequencyRank;
        }

        Row[QStringLiteral("stage")] = AppDatabase::Get().IsReady()
            ? AppDatabase::Get().GetCardRepository().FindWordStage(Match.Headword, Match.PartOfSpeech).value_or(-1)
            : -1;
        Row[QStringLiteral("completed")] = AppDatabase::Get().IsReady()
            && AppDatabase::Get().GetCardRepository().IsWordCompleted(Match.Headword, Match.PartOfSpeech);

        return Row;
    }

    QVariantMap BuildCardResultRow(const FTCard& Card, const std::string& SearchText)
    {
        QVariantMap Row;
        Row[QStringLiteral("word")] = QString::fromStdString(Card.TargetWord);
        Row[QStringLiteral("displayWord")] = QString::fromStdString(GetDisplayHeadword(
            Card.TargetWord, Card.PartOfSpeech, FTSettings::Get().GetLanguageRules().TargetLanguage));
        Row[QStringLiteral("partOfSpeech")] = QString::fromStdString(Card.PartOfSpeech);
        Row[QStringLiteral("translation")] = QString::fromStdString(Card.Translation);
        Row[QStringLiteral("cefrLevel")] = QString();
        Row[QStringLiteral("stage")] = Card.LeitnerStage;
        Row[QStringLiteral("completed")] = IsCompleted(Card);
        Row[QStringLiteral("exact")] = QString::fromStdString(Card.TargetWord).toLower() == QString::fromStdString(SearchText);

        return Row;
    }

    QVariantList AppendCustomCardResults(QVariantList Rows, const std::vector<FTDictionarySearchMatch>& Matches,
                                         const std::string& SearchText, int Limit)
    {
        if (!AppDatabase::Get().IsReady())
        {
            return Rows;
        }

        QHash<QString, bool> SeenWords;

        for (const FTDictionarySearchMatch& Match : Matches)
        {
            SeenWords.insert(QString::fromStdString(Match.Headword).toLower(), true);
        }

        for (const FTCard& Card : AppDatabase::Get().GetCardRepository().FindCardsByHeadword(SearchText, Limit))
        {
            const QString FoldedWord = QString::fromStdString(Card.TargetWord).toLower();

            if (!SeenWords.contains(FoldedWord))
            {
                SeenWords.insert(FoldedWord, true);
                Rows.append(BuildCardResultRow(Card, SearchText));
            }
        }

        return Rows;
    }

    QVariantList LoadRecentSearchResults(int Limit)
    {
        QVariantList Rows;

        if (!AppDatabase::Get().IsReady() || !AppDictionary::Get().IsReady())
        {
            return Rows;
        }

        FTDictionaryRepository& Dictionary = AppDictionary::Get().GetRepository();

        for (const FTSearchHistoryEntry& Recent :
             AppDatabase::Get().GetCardRepository().LoadRecentSearches(Limit))
        {
            std::string Headword = Recent.Term;
            std::string PartOfSpeech = Recent.PartOfSpeech;

            if (Recent.WordId != 0)
            {
                const std::optional<FTDictionaryEntry> Resolved =
                    Dictionary.LookUpById(Recent.WordId);

                if (Resolved.has_value())
                {
                    Headword = Resolved->Headword;
                    PartOfSpeech = Resolved->PartOfSpeech;
                }
            }

            const std::optional<FTDictionarySearchMatch> Match =
                Dictionary.LookUpExact(Headword, PartOfSpeech);

            if (Match.has_value())
            {
                Rows.append(BuildResultRow(*Match));
            }
        }

        return Rows;
    }
}

DictionaryController::DictionaryController(QObject* Parent)
    : QObject(Parent)
{
    AppDictionary::Get().Initialise();

    connect(&AppDictionary::Notifier(), &AppDictionaryNotifier::languagesChanged, this,
            &DictionaryController::languagesChanged);

    connect(&AppTranslator::Notifier(), &AppTranslatorNotifier::translated, this,
            [this](const QString& Source, const QString& Text)
            {
                if (Source != TranslationQuery)
                {
                    return;
                }

                MachineTranslation = Text;
                bTranslating = false;

                emit translationChanged();
            });

    connect(&AppTranslator::Notifier(), &AppTranslatorNotifier::modelsChanged, this,
            [this]()
            {
                if (Results.isEmpty() && MachineTranslation.isEmpty())
                {
                    RequestTranslation();
                }
            });
}

void DictionaryController::ResetTranslation(const QString& Text)
{
    TranslationQuery = Text;

    if (MachineTranslation.isEmpty() && !bTranslating)
    {
        return;
    }

    MachineTranslation.clear();
    bTranslating = false;

    emit translationChanged();
}

void DictionaryController::RequestTranslation()
{
    if (!AppTranslator::IsEnabled() || TranslationQuery.isEmpty() || !AppTranslator::IsSupportedPair())
    {
        return;
    }

    qCInfo(LogTranslate) << "Translating" << TranslationQuery;

    bTranslating = true;

    emit translationChanged();

    AppTranslator::Translate(TranslationQuery);
}

void DictionaryController::translate(const QString& Text)
{
    ResetTranslation(Text.trimmed());
    RequestTranslation();
}

QString DictionaryController::GetMachineTranslation() const
{
    return MachineTranslation;
}

bool DictionaryController::IsTranslating() const
{
    return bTranslating;
}

void DictionaryController::search(const QString& Text)
{
    const QString Trimmed = TrimPunctuation(Text.trimmed()).toLower();

    LastSearchText = Text;

    ++PendingGeneration;
    const quint64 Generation = PendingGeneration;

    ResetTranslation(Text.trimmed());

    if (Trimmed.isEmpty())
    {
        Results = GroupRowsByHeadword(LoadRecentSearchResults(ResultLimit));

        emit resultsChanged();

        return;
    }

    const FTDictionaryPackage* ActivePackage =
        AppDictionary::Get().IsReady() ? AppDictionary::Get().GetActivePackage() : nullptr;

    if (ActivePackage == nullptr)
    {
        Results.clear();

        emit resultsChanged();

        return;
    }

    const std::string DictionaryPath = ActivePackage->FilePath;
    const std::string NativeLanguage = AppDictionary::Get().GetRepository().GetNativeLanguage();
    const std::string SearchText = Trimmed.toStdString();
    const QString TargetLanguage = QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage);
    const int Limit = ResultLimit;
    const bool bNativeQuery = bNativeSearch;

    const QPointer<DictionaryController> Guard(this);

    QThreadPool::globalInstance()->start(
        [Guard, Generation, DictionaryPath, NativeLanguage, Trimmed, SearchText, TargetLanguage, Limit, bNativeQuery]()
        {
            QElapsedTimer RepositoryTimer;
            RepositoryTimer.start();

            std::vector<FTDictionarySearchMatch> Matches;

            std::optional<FTDatabase> Connection = FTDatabase::OpenReadOnly(DictionaryPath);

            if (Connection.has_value())
            {
                FTDictionaryRepository Repository(*Connection, NativeLanguage);
                Matches = SearchWord(Repository, Trimmed, TargetLanguage, Limit, bNativeQuery);
            }

            const qint64 RepositoryMicroseconds = RepositoryTimer.nsecsElapsed() / 1000;

            if (!Guard)
            {
                return;
            }

            QMetaObject::invokeMethod(Guard.data(),
                [Guard, Generation, SearchText, Limit, Matches = std::move(Matches),
                 RepositoryMicroseconds]()
                {
                    if (!Guard || Generation != Guard->PendingGeneration)
                    {
                        return;
                    }

                    QElapsedTimer RowTimer;
                    RowTimer.start();

                    Guard->Results.clear();
                    Guard->Results.reserve(static_cast<int>(Matches.size()));

                    for (const FTDictionarySearchMatch& Match : Matches)
                    {
                        Guard->Results.append(BuildResultRow(Match));
                    }

                    Guard->Results = GroupRowsByHeadword(AppendCustomCardResults(
                        std::move(Guard->Results), Matches, SearchText, Limit));

                    const qint64 RowMicroseconds = RowTimer.nsecsElapsed() / 1000;

                    qCDebug(LogApp) << "Dictionary search profile:"
                                    << QString::fromStdString(SearchText)
                                    << "matches" << static_cast<qint64>(Matches.size())
                                    << "| repository" << RepositoryMicroseconds << "us"
                                    << "| stage lookups" << RowMicroseconds << "us"
                                    << "| total" << (RepositoryMicroseconds + RowMicroseconds)
                                    << "us";

                    emit Guard->resultsChanged();

                    if (Guard->Results.isEmpty())
                    {
                        Guard->RequestTranslation();
                    }
                }, Qt::QueuedConnection);
        });
}

bool DictionaryController::IsNativeSearch() const
{
    return bNativeSearch;
}

void DictionaryController::SetNativeSearch(bool bInNativeSearch)
{
    if (bNativeSearch == bInNativeSearch)
    {
        return;
    }

    bNativeSearch = bInNativeSearch;

    qCInfo(LogApp) << "Dictionary search language set to" << (bNativeSearch ? "native" : "foreign");

    emit nativeSearchChanged();

    if (!LastSearchText.trimmed().isEmpty())
    {
        search(LastSearchText);
    }
}

QString DictionaryController::GetTargetLanguageCode() const
{
    return QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage).toUpper();
}

QString DictionaryController::GetNativeLanguageCode() const
{
    if (!AppDictionary::Get().IsReady())
    {
        return {};
    }

    return QString::fromStdString(AppDictionary::Get().GetRepository().GetNativeLanguage()).toUpper();
}

void DictionaryController::recordSearch()
{
    const QString Headword = Entry.value(QStringLiteral("headword")).toString();

    if (Headword.isEmpty() || !AppDatabase::Get().IsReady())
    {
        return;
    }

    const QString PartOfSpeech = Entry.value(QStringLiteral("partOfSpeech")).toString();
    const int64_t WordId = Entry.value(QStringLiteral("id")).toLongLong();

    AppDatabase::Get().GetCardRepository().RecordSearch(Headword.toStdString(), PartOfSpeech.toStdString(),
                                               WordId);
}

void DictionaryController::clear()
{
    ResetTranslation({});

    LastSearchText.clear();
    Results = GroupRowsByHeadword(LoadRecentSearchResults(ResultLimit));

    emit resultsChanged();
}

void DictionaryController::openEntry(const QString& Headword, const QString& PartOfSpeech, const QString& SearchText)
{
    Entry.clear();
    AllTranslations.clear();
    Readings.clear();
    Cards.clear();

    bool bInDictionary = false;

    if (AppDictionary::Get().IsReady())
    {
        std::vector<FTDictionaryEntry> Found =
            AppDictionary::Get().GetRepository().LookUp(Headword.toStdString());

        std::vector<FTCard> HeadwordCards;

        if (Found.empty())
        {
            HeadwordCards = LoadHeadwordCards(Headword);
            Found = BuildCardEntries(HeadwordCards);
        }

        const FTDictionaryEntry* Match = nullptr;

        for (const FTDictionaryEntry& Candidate : Found)
        {
            if (QString::fromStdString(Candidate.PartOfSpeech) == PartOfSpeech)
            {
                Match = &Candidate;
                break;
            }
        }

        if (!Match && !Found.empty())
        {
            Match = &Found.front();

            for (const FTDictionaryEntry& Candidate : Found)
            {
                if (Candidate.Headword == Match->Headword && Candidate.FrequencyRank.has_value()
                    && (!Match->FrequencyRank.has_value() || *Candidate.FrequencyRank < *Match->FrequencyRank))
                {
                    Match = &Candidate;
                }
            }
        }

        if (Match)
        {
            const QString Query = (SearchText.isEmpty() ? Headword : SearchText).trimmed();
            Entry = BuildEntry(*Match, Query.compare(QString::fromStdString(Match->Headword), Qt::CaseInsensitive) == 0 ? QString{} : Query);
            AllTranslations = BuildAllTranslations(*Match, Headword);
            Readings = BuildReadings(Found, Match->Headword);

            if (HeadwordCards.empty())
            {
                HeadwordCards = LoadHeadwordCards(QString::fromStdString(Match->Headword));
            }

            Cards = BuildCardRows(HeadwordCards, Match->PartOfSpeech);
            Entry[QStringLiteral("meanings")] = BuildMeanings(*Match, HeadwordCards);

            bInDictionary = Match->Id != 0
                && QString::fromStdString(Match->Headword).compare(Headword, Qt::CaseInsensitive) == 0;
        }
    }

    PartsOfSpeech = BuildPartsOfSpeech(Readings, bInDictionary, PartOfSpeech);

    emit entryChanged();
}

void DictionaryController::speak(const QString& Word) const
{
    if (Word.isEmpty())
    {
        return;
    }

    qCInfo(LogSpeech) << "DictionaryController speaks:" << Word;

    AppSpeech::Get().Speak(Word,
        QString::fromStdString(FTSettings::Get().GetLanguageRules().TargetLanguage));
}

void DictionaryController::speakAutomatically(const QString& Word) const
{
    if (AppSpeech::Get().AllowsAutomaticSpeech())
    {
        speak(Word);
    }
}

void DictionaryController::openWebSearch(const QString& Headword) const
{
    AppWebSearch::Open(Headword);
}

QString DictionaryController::sentenceAt(const QString& Text, int Position) const
{
    return FindSentenceAt(Text, Position, FTSettings::Get().GetLanguageRules().TargetLanguage);
}

QString DictionaryController::cardHint(const QString& CardId) const
{
    if (!AppDatabase::Get().IsReady())
    {
        return {};
    }

    const auto Card = AppDatabase::Get().GetCardRepository().LoadCard(CardId.toStdString());

    return Card.has_value() ? QString::fromStdString((*Card)->Hint) : QString();
}

QString DictionaryController::cardMeaningGist(const QString& CardId) const
{
    return AppCards::FindCardMeaningGist(CardId.toStdString());
}

void DictionaryController::copyToClipboard(const QString& Text) const
{
    if (Text.isEmpty())
    {
        return;
    }

    QGuiApplication::clipboard()->setText(Text);

    qCInfo(LogApp) << "Copied" << Text.size() << "characters to the clipboard";
}

QVariantList DictionaryController::GetResults() const
{
    return Results;
}

QVariantMap DictionaryController::GetEntry() const
{
    return Entry;
}

QVariantList DictionaryController::GetAllTranslations() const
{
    return AllTranslations;
}

QVariantList DictionaryController::GetReadings() const
{
    return Readings;
}

QVariantList DictionaryController::GetCards() const
{
    return Cards;
}

QVariantList DictionaryController::GetPartsOfSpeech() const
{
    return PartsOfSpeech;
}
