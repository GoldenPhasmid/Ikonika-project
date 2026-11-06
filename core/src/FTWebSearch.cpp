#include "core/FTWebSearch.h"

#include <QString>
#include <QUrl>

#include <algorithm>
#include <iterator>

bool FTWebSearchSource::ServesPair(const std::string& TargetLanguage,
                                   const std::string& NativeLanguage) const
{
    return LanguageNames.count(TargetLanguage) > 0 && LanguageNames.count(NativeLanguage) > 0;
}

std::string FTWebSearchSource::BuildUrl(const std::string& TargetLanguage,
                                        const std::string& NativeLanguage,
                                        const std::string& Word) const
{
    if (!ServesPair(TargetLanguage, NativeLanguage) || Word.empty())
    {
        return {};
    }

    QString Address{QString::fromStdString(UrlTemplate)};
    Address.replace(QStringLiteral("{target}"), QString::fromStdString(LanguageNames.at(TargetLanguage)));
    Address.replace(QStringLiteral("{native}"), QString::fromStdString(LanguageNames.at(NativeLanguage)));
    Address.replace(QStringLiteral("{word}"),
                    QString::fromUtf8(QUrl::toPercentEncoding(QString::fromStdString(Word))));

    return Address.toStdString();
}

std::vector<FTWebSearchSource> GetBuiltInWebSearchSources()
{
    return {
        FTWebSearchSource{
            "reverso",
            "Reverso",
            "https://context.reverso.net/translation/{target}-{native}/{word}",
            {
                {"de", "german"}, {"en", "english"}, {"es", "spanish"}, {"fr", "french"},
                {"it", "italian"}, {"pt", "portuguese"}, {"ru", "russian"}, {"uk", "ukrainian"}
            }
        },
        FTWebSearchSource{
            "google",
            "Google Translate",
            "https://translate.google.com/?sl={target}&tl={native}&text={word}&op=translate",
            {
                {"de", "de"}, {"en", "en"}, {"es", "es"}, {"fr", "fr"},
                {"it", "it"}, {"pt", "pt"}, {"ru", "ru"}, {"uk", "uk"}
            }
        }
    };
}

std::vector<FTWebSearchSource> SelectWebSearchSourcesForPair(
    const std::vector<FTWebSearchSource>& Sources, const std::string& TargetLanguage,
    const std::string& NativeLanguage)
{
    std::vector<FTWebSearchSource> Result;

    std::copy_if(Sources.begin(), Sources.end(), std::back_inserter(Result),
                 [&TargetLanguage, &NativeLanguage](const FTWebSearchSource& Source)
                 {
                     return Source.ServesPair(TargetLanguage, NativeLanguage);
                 });

    return Result;
}

int FindWebSearchSourceIndex(const std::vector<FTWebSearchSource>& Sources,
                             const std::string& PreferredId)
{
    if (Sources.empty())
    {
        return -1;
    }

    const auto Found = std::find_if(Sources.begin(), Sources.end(),
                                    [&PreferredId](const FTWebSearchSource& Source)
                                    {
                                        return Source.Id == PreferredId;
                                    });

    if (Found == Sources.end())
    {
        return 0;
    }

    return static_cast<int>(std::distance(Sources.begin(), Found));
}
