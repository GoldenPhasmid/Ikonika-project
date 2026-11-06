#pragma once

#include "AppCards.h"
#include "AppDatabase.h"

#include "core/FTDatabase.h"
#include "core/FTCardRepository.h"
#include "core/FTCloudBookRepository.h"
#include "core/FTReaderRepository.h"
#include "core/FTSchema.h"
#include "core/FTYoutubeRepository.h"

#include <memory>
#include <optional>
#include <string>
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

        FTReaderRepository& GetReaderLibrary() override
        {
            return *ReaderLibrary;
        }

        FTCloudBookRepository& GetCloudBooks() override
        {
            return *CloudBooks;
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
        std::unique_ptr<FTReaderRepository> ReaderLibrary;
        std::unique_ptr<FTCloudBookRepository> CloudBooks;
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
