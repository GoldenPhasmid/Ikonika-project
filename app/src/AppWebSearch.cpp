#include "AppWebSearch.h"

#include "AppCatalog.h"

#include "core/FTCatalog.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QDesktopServices>
#include <QUrl>

std::vector<FTWebSearchSource> AppWebSearch::GetSources()
{
    const FTCatalog* Catalog{AppCatalog::Get().GetCatalog()};
    const bool bCatalogHasSources = Catalog != nullptr && !Catalog->WebSearchSources.empty();

    const FTLanguageRules& Rules{FTSettings::Get().GetLanguageRules()};

    return SelectWebSearchSourcesForPair(
        bCatalogHasSources ? Catalog->WebSearchSources : GetBuiltInWebSearchSources(),
        Rules.TargetLanguage, Rules.NativeLanguage);
}

int AppWebSearch::GetActiveSourceIndex()
{
    return FindWebSearchSourceIndex(GetSources(), FTSettings::Get().GetWebSearchSource());
}

void AppWebSearch::Open(const QString& Word)
{
    if (Word.isEmpty())
    {
        return;
    }

    const FTLanguageRules& Rules{FTSettings::Get().GetLanguageRules()};
    const std::vector<FTWebSearchSource> Sources{GetSources()};
    const int Index{FindWebSearchSourceIndex(Sources, FTSettings::Get().GetWebSearchSource())};

    if (Index < 0)
    {
        qCWarning(LogApp) << "No web search source serves"
                          << QString::fromStdString(Rules.TargetLanguage) << "->"
                          << QString::fromStdString(Rules.NativeLanguage);

        return;
    }

    const FTWebSearchSource& Source{Sources[static_cast<std::size_t>(Index)]};
    const QUrl Address{QString::fromStdString(
        Source.BuildUrl(Rules.TargetLanguage, Rules.NativeLanguage, Word.toStdString()))};

    qCInfo(LogApp) << "Searching the web for" << Word << "on" << QString::fromStdString(Source.Name);

    if (!QDesktopServices::openUrl(Address))
    {
        qCWarning(LogApp) << "The system refused to open" << Address.toString();
    }
}
