#pragma once

#include "AppCards.h"
#include "AppDatabase.h"

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTCloudBookRepository.h"
#include "core/FTReaderRepository.h"
#include "core/FTSchema.h"
#include "core/FTUserDeckRepository.h"
#include "core/FTSettings.h"
#include "core/FTYoutubeRepository.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{
    class FTestAppDatabase : public AppDatabase
    {
    public:
        FTestAppDatabase()
        {
            std::optional<FTDatabase> Opened = FTDatabase::OpenInMemory();
            Connection = std::make_unique<FTDatabase>(std::move(*Opened));
            MigrateSchema(*Connection);
            CardRepository = std::make_unique<FTCardRepository>(*Connection);
            AppCards::AnnotateLoadedCards(*CardRepository);

            std::optional<FTDatabase> OpenedDevice = FTDatabase::OpenInMemory();
            DeviceConnection = std::make_unique<FTDatabase>(std::move(*OpenedDevice));
            MigrateDeviceSchema(*DeviceConnection);

            ReaderLibrary = std::make_unique<FTReaderRepository>(*DeviceConnection);
            CloudBooks = std::make_unique<FTCloudBookRepository>(*Connection);
            BookShelf = std::make_unique<FTBookShelfRepository>(*Connection);
            UserDecks = std::make_unique<FTUserDeckRepository>(*Connection);
            YoutubeLibrary = std::make_unique<FTYoutubeRepository>(*DeviceConnection);
        }

        bool Initialise() override
        {
            return true;
        }


        bool IsReady() override
        {
            return Connection != nullptr && CardRepository != nullptr;
        }

        FTCardRepository& GetCardRepository() override
        {
            return *CardRepository;
        }

        FTCardRepository& GetCardRepository(const std::string& TargetLanguage,
                                            const std::string& NativeLanguage) override
        {
            const FTLanguageRules& Languages{FTSettings::Get().GetLanguageRules()};

            if (TargetLanguage.empty() || TargetLanguage == Languages.TargetLanguage)
            {
                return *CardRepository;
            }

            const std::string Native{NativeLanguage.empty() ? Languages.NativeLanguage : NativeLanguage};
            std::unique_ptr<FTCardRepository>& Found{LanguageCardRepositories[{TargetLanguage, Native}]};

            if (Found == nullptr)
            {
                Found = std::make_unique<FTCardRepository>(*Connection);
                Found->SetActiveLanguages(TargetLanguage, Native);
            }

            return *Found;
        }

        FTReaderRepository& GetReaderLibrary() override
        {
            return *ReaderLibrary;
        }

        FTCloudBookRepository& GetCloudBooks() override
        {
            return *CloudBooks;
        }

        FTBookShelfRepository& GetBookShelf() override
        {
            return *BookShelf;
        }

        FTUserDeckRepository& GetUserDecks() override
        {
            return *UserDecks;
        }

        FTYoutubeRepository& GetYoutubeLibrary() override
        {
            return *YoutubeLibrary;
        }

        void ApplyActiveLanguages() override
        {
        }

        FTDatabase& GetConnection() override
        {
            return *Connection;
        }

        QString GetFilePath() const override
        {
            return {};
        }

        QString GetLastError() const override
        {
            return {};
        }

    private:
        std::unique_ptr<FTDatabase> Connection;
        std::unique_ptr<FTDatabase> DeviceConnection;
        std::unique_ptr<FTCardRepository> CardRepository;
        std::map<std::pair<std::string, std::string>, std::unique_ptr<FTCardRepository>> LanguageCardRepositories;
        std::unique_ptr<FTReaderRepository> ReaderLibrary;
        std::unique_ptr<FTCloudBookRepository> CloudBooks;
        std::unique_ptr<FTBookShelfRepository> BookShelf;
        std::unique_ptr<FTUserDeckRepository> UserDecks;
        std::unique_ptr<FTYoutubeRepository> YoutubeLibrary;
    };

    bool AddTestCards(const std::vector<FTCard>& Cards)
    {
        FTCardRepository& Repository = AppDatabase::Get().GetCardRepository();

        for (const FTCard& Card : Cards)
        {
            const std::optional<std::string> CardId = Repository.AddCard(Card);

            if (!CardId.has_value())
            {
                return false;
            }

            if (Card.LeitnerStage == 0 && !Card.DueDate.has_value() && !Card.CompletedDate.has_value())
            {
                continue;
            }

            FTCard Placed = Card;
            Placed.Id = *CardId;

            if (!Repository.UpdateCardSchedule(Placed))
            {
                return false;
            }
        }

        return true;
    }

    struct FAppDatabaseGuard
    {
        FTestAppDatabase TestDatabase;

        FAppDatabaseGuard()
        {
            AppDatabase::SetInstance(&TestDatabase);
        }

        ~FAppDatabaseGuard()
        {
            AppDatabase::SetInstance(nullptr);
        }
    };
}
