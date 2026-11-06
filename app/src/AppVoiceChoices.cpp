#include "AppVoiceChoices.h"

#include <QString>
#include <QVariantMap>

#include <algorithm>
#include <filesystem>
#include <vector>

namespace
{
    struct FVoiceChoice
    {
        QString Title;
        bool bInstalled = false;
        bool bSelected = false;
        int Index = -1;
        QString PackageId;
        qlonglong Bytes = 0;
    };

    QString EngineTitle(const std::string& PackageId)
    {
        QString Title = QString::fromStdString(PackageId.substr(0, PackageId.find('-')));

        if (!Title.isEmpty())
        {
            Title[0] = Title[0].toUpper();
        }

        return Title;
    }
}

std::string AppVoiceChoices::DirectoryOf(const FTCatalogPackage& Package)
{
    return std::filesystem::path(Package.UnpackTo).filename().string();
}

QVariantList AppVoiceChoices::Build(const std::string& TargetLanguage,
                                    const AppSpeech& Speech,
                                    const FTCatalog* Catalog)
{
    std::vector<FVoiceChoice> Choices;
    std::vector<std::string> InstalledDirectories;

    for (int Index = 0; Index < Speech.GetVoiceCount(); ++Index)
    {
        const FTVoiceInfo Info = Speech.GetVoiceInfo(Index);

        InstalledDirectories.push_back(Info.Name);

        if (!IsLanguageSupported(Info, TargetLanguage))
        {
            continue;
        }

        FVoiceChoice Choice;
        Choice.Title = QString::fromStdString(Info.Engine);
        Choice.bInstalled = true;
        Choice.bSelected = Index == Speech.GetActiveVoice();
        Choice.Index = Index;

        Choices.push_back(Choice);
    }

    if (Catalog != nullptr)
    {
        for (const FTCatalogPackage& Package : Catalog->Packages)
        {
            if (Package.Kind != CatalogKindVoice
                || !Package.IsCompatible()
                || !Package.ServesTarget(TargetLanguage))
            {
                continue;
            }

            const std::string Directory = DirectoryOf(Package);

            if (std::find(InstalledDirectories.begin(), InstalledDirectories.end(), Directory)
                != InstalledDirectories.end())
            {
                continue;
            }

            FVoiceChoice Choice;
            Choice.Title = EngineTitle(Package.Id);
            Choice.PackageId = QString::fromStdString(Package.Id);
            Choice.Bytes = static_cast<qlonglong>(Package.Bytes);

            Choices.push_back(Choice);
        }
    }

    QVariantList Result;

    for (const FVoiceChoice& Choice : Choices)
    {
        QVariantMap Entry;
        Entry[QStringLiteral("title")] = Choice.Title;
        Entry[QStringLiteral("installed")] = Choice.bInstalled;
        Entry[QStringLiteral("selected")] = Choice.bSelected;
        Entry[QStringLiteral("index")] = Choice.Index;
        Entry[QStringLiteral("packageId")] = Choice.PackageId;
        Entry[QStringLiteral("bytes")] = Choice.Bytes;

        Result.append(Entry);
    }

    return Result;
}
