#pragma once

#include "AppAccountRegistry.h"

#include <QString>

#include <memory>
#include <string>
#include <utility>
#include <vector>

class FTDatabase;
class FTCardRepository;
class FTCloudBookRepository;
class FTReaderRepository;
class FTSettings;
class FTYoutubeRepository;

class AppWorkspaceCheckpoints
{
public:
    explicit AppWorkspaceCheckpoints(QString Directory);

    void Save(const std::string& Kind, const std::string& Key, const std::string& StateJson);
    void Clear(const std::string& Kind, const std::string& Key);
    std::vector<std::pair<std::string, std::string>> LoadAll(const std::string& Kind) const;

private:
    QString GetFilePath(const std::string& Kind) const;

    QString Directory;
};

class AppWorkspace
{
public:
    static std::unique_ptr<AppWorkspace> Open(const AppWorkspaceFolder& Folder);

    ~AppWorkspace();

    AppWorkspace(const AppWorkspace&) = delete;
    AppWorkspace& operator=(const AppWorkspace&) = delete;

    const AppWorkspaceFolder& GetFolder() const;
    FTDatabase& GetConnection();
    FTCardRepository& GetCardRepository();
    FTReaderRepository& GetReaderLibrary();
    FTCloudBookRepository& GetCloudBooks();
    FTYoutubeRepository& GetYoutubeLibrary();
    FTSettings& GetSettings();
    AppWorkspaceCheckpoints& GetCheckpoints();

    void ApplyActiveLanguages();

private:
    explicit AppWorkspace(const AppWorkspaceFolder& Folder);

    AppWorkspaceFolder Folder;
    std::unique_ptr<FTDatabase> Connection;
    std::unique_ptr<FTDatabase> DeviceConnection;
    std::unique_ptr<FTCardRepository> CardRepository;
    std::unique_ptr<FTReaderRepository> ReaderLibrary;
    std::unique_ptr<FTCloudBookRepository> CloudBooks;
    std::unique_ptr<FTYoutubeRepository> YoutubeLibrary;
    std::unique_ptr<FTSettings> Settings;
    AppWorkspaceCheckpoints Checkpoints;
};
