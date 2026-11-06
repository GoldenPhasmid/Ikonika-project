#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

#include <utility>
#include <vector>

using FTCssDeclarations = std::vector<std::pair<QString, QString>>;

struct FTCssLength
{
    float Em = 0.0f;
    float Width = 0.0f;
};

struct FTCssElement
{
    QString Tag;
    QString Id;
    QStringList Classes;
};

struct FTCssComputedStyle
{
    float FontScale = 1.0f;
    FTCssLength TextIndent;
    FTCssLength MarginTop;
    FTCssLength MarginBottom;
    FTCssLength PaddingTop;
    FTCssLength PaddingBottom;
    FTCssLength MarginLeft;
    FTCssLength MarginRight;
    FTCssLength PaddingLeft;
    FTCssLength PaddingRight;
    QString TextAlign;
    QString TextTransform;
    QString VerticalAlign;
    bool bBold = false;
    bool bItalic = false;
    bool bSmallCaps = false;
    bool bHidden = false;

    FTCssComputedStyle Inherit() const;
    void Apply(const FTCssDeclarations& Declarations, QHash<QString, int>* Unhandled = nullptr);
    QString InlineDifference(const FTCssComputedStyle& Parent) const;
};

class FTCssStyleSheet
{
public:
    static FTCssStyleSheet ForXhtml();
    static FTCssStyleSheet ForFb2();

    void Append(const QString& Css);

    FTCssDeclarations Match(const std::vector<FTCssElement>& Path, const FTCssElement* PreviousSibling) const;
    const QStringList& GetSkippedRules() const;

    static FTCssDeclarations ParseDeclarations(const QString& Block);

private:
    struct FTCssCompound
    {
        QString Tag;
        QString Id;
        QStringList Classes;
    };

    struct FTCssSelector
    {
        std::vector<FTCssCompound> Compounds;
        std::vector<QChar> Combinators;
        int Specificity = 0;
    };

    struct FTCssRule
    {
        FTCssSelector Selector;
        FTCssDeclarations Declarations;
        int Order = 0;
    };

    static bool ParseSelector(const QString& Text, FTCssSelector& Selector);
    static bool MatchesCompound(const FTCssCompound& Compound, const FTCssElement& Element);
    static bool MatchesFrom(const FTCssSelector& Selector, int CompoundIndex, const std::vector<FTCssElement>& Path,
                            int PathIndex, const FTCssElement* PreviousSibling);

    void AppendRules(const QString& Css);

    std::vector<FTCssRule> Rules;
    QStringList SkippedRules;
    int NextOrder = 0;
};
