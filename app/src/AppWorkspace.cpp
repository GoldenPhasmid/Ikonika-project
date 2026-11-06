#include "AppWorkspace.h"

#include "AppCards.h"

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTCloudBookRepository.h"
#include "core/FTReaderRepository.h"
#include "core/FTLog.h"
#include "core/FTSchema.h"
#include "core/FTSettings.h"
#include "core/FTYoutubeRepository.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
    void RecordSetting(FTDatabase& Connection, const QString& Key, const QVariant& Value, int64_t At,
                       bool bReplace)
    {
        FTStatement Write{Connection.Prepare(
            bReplace ? "INSERT INTO SyncedSettings (Key, Value, LastUpdatedDate) VALUES (?1, ?2, ?3) "
                       "ON CONFLICT (Key) DO UPDATE SET Value = excluded.Value, LastUpdatedDate = excluded.LastUpdatedDate"
                     : "INSERT OR IGNORE INTO SyncedSettings (Key, Value, LastUpdatedDate) VALUES (?1, ?2, ?3)")};

        if (!Write.IsValid())
        {
            qCWarning(LogDatabase) << "Could not record the setting" << Key;

            return;
        }

        Write.BindText(1, Key.toStdString());
        Write.BindText(2, SerializeSettingValue(Value));
        Write.BindInt64(3, At);

        if (!Write.Run())
        {
            qCWarning(LogDatabase) << "Could not record the setting" << Key;
        }
    }

    QJsonObject ReadCheckpointFile(const QString& Path)
    {
        QFile File(Path);

        if (!File.open(QIODevice::ReadOnly))
        {
            return {};
        }

        return QJsonDocument::fromJson(File.readAll()).object();
    }

    void WriteCheckpointFile(const QString& Path, const QJsonObject& Checkpoints)
    {
        if (!QDir().mkpath(QFileInfo(Path).absolutePath()))
        {
            qCWarning(LogSession) << "Could not create the checkpoint folder for" << Path;

            return;
        }

        QSaveFile File(Path);

        if (!File.open(QIODevice::WriteOnly))
        {
            qCWarning(LogSession) << "Could not open" << Path << "to save a checkpoint";

            return;
        }

        File.write(QJsonDocument(Checkpoints).toJson(QJsonDocument::Compact));

        if (!File.commit())
        {
            qCWarning(LogSession) << "Could not save the checkpoint file" << Path;
        }
    }
}

AppWorkspaceCheckpoints::AppWorkspaceCheckpoints(QString Directory)
    : Directory{std::move(Directory)}
{
}

void AppWorkspaceCheckpoints::Save(const std::string& Kind, const std::string& Key,
                                   const std::string& StateJson)
{
    const QString Path{GetFilePath(Kind)};

    QJsonObject Checkpoints{ReadCheckpointFile(Path)};
    Checkpoints.insert(QString::fromStdString(Key), QString::fromStdString(StateJson));

    WriteCheckpointFile(Path, Checkpoints);
}

void AppWorkspaceCheckpoints::Clear(const std::string& Kind, const std::string& Key)
{
    const QString Path{GetFilePath(Kind)};

    QJsonObject Checkpoints{ReadCheckpointFile(Path)};

    if (!Checkpoints.contains(QString::fromStdString(Key)))
    {
        return;
    }

    Checkpoints.remove(QString::fromStdString(Key));

    WriteCheckpointFile(Path, Checkpoints);
}

std::vector<std::pair<std::string, std::string>> AppWorkspaceCheckpoints::LoadAll(
    const std::string& Kind) const
{
    std::vector<std::pair<std::string, std::string>> Result;

    const QJsonObject Checkpoints{ReadCheckpointFile(GetFilePath(Kind))};

    for (auto It = Checkpoints.constBegin(); It != Checkpoints.constEnd(); ++It)
    {
        Result.emplace_back(It.key().toStdString(), It.value().toString().toStdString());
    }

    return Result;
}

QString AppWorkspaceCheckpoints::GetFilePath(const std::string& Kind) const
{
    return QDir(Directory).filePath(QString::fromStdString(Kind) + QStringLiteral(".json"));
}

std::unique_ptr<AppWorkspace> AppWorkspace::Open(const AppWorkspaceFolder& Folder)
{
    if (!QDir().mkpath(Folder.Directory))
    {
        qCCritical(LogDatabase) << "Could not create the workspace folder" << Folder.Directory;

        return nullptr;
    }

    std::optional<FTDatabase> Opened{FTDatabase::Open(Folder.GetDatabaseFilePath().toStdString())};

    if (!Opened.has_value())
    {
        qCCritical(LogDatabase) << "Could not open the workspace database"
                                << Folder.GetDatabaseFilePath();

        return nullptr;
    }

    std::unique_ptr<AppWorkspace> Workspace{new AppWorkspace(Folder)};
    Workspace->Connection = std::make_unique<FTDatabase>(std::move(*Opened));

    if (!MigrateSchema(*Workspace->Connection))
    {
        qCCritical(LogDatabase) << "Could not bring the workspace database to the current schema:"
                                << QString::fromStdString(Workspace->Connection->GetLastError());

        return nullptr;
    }

    Workspace->Settings = std::make_unique<FTSettings>(Folder.GetSettingsFilePath());

    for (const auto& [Key, Value] : Workspace->Settings->ReadSyncedValues())
    {
        RecordSetting(*Workspace->Connection, Key, Value, 0, false);
    }

    Workspace->Settings->SetChangeListener(
        [Connection = Workspace->Connection.get()](const QString& Key, const QVariant& Value)
        {
            RecordSetting(*Connection, Key, Value, GetSystemTimeMilliseconds(), true);
        });
    Workspace->CardRepository = std::make_unique<FTCardRepository>(*Workspace->Connection);
    AdoptLastStudiedLanguage(*Workspace->CardRepository, *Workspace->Settings);
    AppCards::AnnotateLoadedCards(*Workspace->CardRepository);

    std::optional<FTDatabase> OpenedDevice{FTDatabase::Open(Folder.GetDeviceDatabaseFilePath().toStdString())};

    if (!OpenedDevice.has_value() || !MigrateDeviceSchema(*OpenedDevice))
    {
        qCCritical(LogDatabase) << "Could not open the device database" << Folder.GetDeviceDatabaseFilePath();

        return nullptr;
    }

    Workspace->DeviceConnection = std::make_unique<FTDatabase>(std::move(*OpenedDevice));
    Workspace->ReaderLibrary = std::make_unique<FTReaderRepository>(*Workspace->DeviceConnection);
    Workspace->CloudBooks = std::make_unique<FTCloudBookRepository>(*Workspace->Connection);
    Workspace->YoutubeLibrary = std::make_unique<FTYoutubeRepository>(*Workspace->DeviceConnection);

    Workspace->ApplyActiveLanguages();

    qCInfo(LogApp) << "Opened the workspace" << Folder.Directory;

    return Workspace;
}

AppWorkspace::AppWorkspace(const AppWorkspaceFolder& Folder)
    : Folder{Folder}
    , Checkpoints{Folder.GetCheckpointsDirectory()}
{
}

AppWorkspace::~AppWorkspace()
{
    qCInfo(LogApp) << "Closed the workspace" << Folder.Directory;
}

const AppWorkspaceFolder& AppWorkspace::GetFolder() const
{
    return Folder;
}

FTDatabase& AppWorkspace::GetConnection()
{
    return *Connection;
}

FTCardRepository& AppWorkspace::GetCardRepository()
{
    return *CardRepository;
}

FTReaderRepository& AppWorkspace::GetReaderLibrary()
{
    return *ReaderLibrary;
}

FTCloudBookRepository& AppWorkspace::GetCloudBooks()
{
    return *CloudBooks;
}

FTYoutubeRepository& AppWorkspace::GetYoutubeLibrary()
{
    return *YoutubeLibrary;
}

FTSettings& AppWorkspace::GetSettings()
{
    return *Settings;
}

AppWorkspaceCheckpoints& AppWorkspace::GetCheckpoints()
{
    return Checkpoints;
}

void AppWorkspace::ApplyActiveLanguages()
{
    const FTLanguageRules& Languages{Settings->GetLanguageRules()};

    CardRepository->SetActiveLanguages(Languages.TargetLanguage, Languages.NativeLanguage);

    qCInfo(LogDatabase) << "Cards scoped to" << QString::fromStdString(Languages.TargetLanguage)
                        << "->" << QString::fromStdString(Languages.NativeLanguage);
}
