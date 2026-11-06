#include <catch2/catch_test_macros.hpp>

#include "core/FTSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QString>

namespace
{
    QString DefaultsIniPath()
    {
        return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("defaults.ini"));
    }

    struct DefaultsIniGuard
    {
        QString Path = DefaultsIniPath();
        bool bExisted = false;
        QByteArray PreviousContent;

        DefaultsIniGuard()
        {
            bExisted = QFile::exists(Path);

            if (bExisted)
            {
                QFile Existing(Path);
                Existing.open(QIODevice::ReadOnly);
                PreviousContent = Existing.readAll();
            }
        }

        void Write(const QString& Content)
        {
            QFile File(Path);
            File.open(QIODevice::WriteOnly | QIODevice::Truncate);
            File.write(Content.toUtf8());
            File.close();
        }

        ~DefaultsIniGuard()
        {
            if (bExisted)
            {
                QFile File(Path);
                File.open(QIODevice::WriteOnly | QIODevice::Truncate);
                File.write(PreviousContent);
            }
            else
            {
                QFile::remove(Path);
            }

            FTSettings::Get().SetTrainingWordCount(FTIntensiveLearningRules{}.TrainingWordCount);
            FTSettings::Get().SetSessionsPerDay(FTIntensiveLearningRules{}.SessionsPerDay);
            FTSettings::Get().ReloadFromDisk();
        }
    };
}

TEST_CASE("A value shipped in defaults.ini overrides the compiled default", "[settings]")
{
    DefaultsIniGuard Guard;

    QSettings UserSettings;
    UserSettings.remove(QStringLiteral("intensiveLearning/trainingWordCount"));

    Guard.Write(QStringLiteral("[intensiveLearning]\ntrainingWordCount=15\n"));

    FTSettings::Get().ReloadFromDisk();

    CHECK(FTSettings::Get().GetIntensiveLearningRules().TrainingWordCount == 15);
}

TEST_CASE("A setting the user already saved is not overridden by defaults.ini", "[settings]")
{
    DefaultsIniGuard Guard;

    FTSettings::Get().SetTrainingWordCount(20);

    Guard.Write(QStringLiteral("[intensiveLearning]\ntrainingWordCount=15\n"));

    FTSettings::Get().ReloadFromDisk();

    CHECK(FTSettings::Get().GetIntensiveLearningRules().TrainingWordCount == 20);
}

TEST_CASE("A missing defaults.ini falls through to the compiled default", "[settings]")
{
    DefaultsIniGuard Guard;

    QSettings UserSettings;
    UserSettings.remove(QStringLiteral("intensiveLearning/trainingWordCount"));

    QFile::remove(Guard.Path);

    FTSettings::Get().ReloadFromDisk();

    CHECK(FTSettings::Get().GetIntensiveLearningRules().TrainingWordCount
          == FTIntensiveLearningRules{}.TrainingWordCount);
}

TEST_CASE("Reloading re-reads every group, not only the first two", "[settings]")
{
    // The reload used to know about the schedule and intensive learning and nothing else, so a
    // shipped default for any other group was read at construction and then never again. Three
    // groups here rather than one, because the bug was per group.
    DefaultsIniGuard Guard;

    QSettings UserSettings;
    UserSettings.remove(QStringLiteral("dictionary/examplesPerSense"));
    UserSettings.remove(QStringLiteral("speech/cachedLineCount"));
    UserSettings.remove(QStringLiteral("answers/requireExactAccents"));
    UserSettings.remove(QStringLiteral("repetition/requireExactAccents"));

    Guard.Write(QStringLiteral("[dictionary]\nexamplesPerSense=5\n"
                               "[speech]\ncachedLineCount=90\n"
                               "[repetition]\nrequireExactAccents=true\n"));

    FTSettings::Get().ReloadFromDisk();

    CHECK(FTSettings::Get().GetDictionaryDisplayRules().ExamplesPerSense == 5);
    CHECK(FTSettings::Get().GetSpeechRules().CachedLineCount == 90);
    CHECK(FTSettings::Get().GetSessionExercises(ReviewSessionGroup).bRequireExactAccents);
}
