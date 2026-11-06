#include <catch2/catch_test_macros.hpp>

#include "core/FTSettings.h"

#include <QScopeGuard>
#include <QSettings>
#include <QTemporaryDir>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct FChange
    {
        QString Key;
        QVariant Value;
    };

    struct Fixture
    {
        QTemporaryDir Folder;
        FTSettings Settings{Folder.filePath("settings.ini")};
        std::vector<FChange> Changes;

        Fixture()
        {
            Settings.SetChangeListener([this](const QString& Key, const QVariant& Value)
                                       {
                                           Changes.push_back(FChange{Key, Value});
                                       });
        }
    };

    std::optional<QVariant> FindValue(const std::vector<std::pair<QString, QVariant>>& Values, const QString& Key)
    {
        const auto Found = std::find_if(Values.begin(), Values.end(),
                                        [&Key](const std::pair<QString, QVariant>& Entry) { return Entry.first == Key; });

        if (Found == Values.end())
        {
            return std::nullopt;
        }

        return Found->second;
    }
}

TEST_CASE("Changing a synced setting tells the listener its key and value", "[settings][sync]")
{
    Fixture Test;

    Test.Settings.SetTrainingWordCount(15);

    REQUIRE(Test.Changes.size() == 1);
    CHECK(Test.Changes.front().Key == "intensiveLearning/trainingWordCount");
    CHECK(Test.Changes.front().Value.toInt() == 15);
}

TEST_CASE("Changing session exercises tells the listener every key it wrote", "[settings][sync]")
{
    Fixture Test;

    FTSessionExerciseRules Rules = Test.Settings.GetSessionExercises(ReviewSessionGroup);
    Rules.bSentenceStage = !Rules.bSentenceStage;

    Test.Settings.SetSessionExercises(ReviewSessionGroup, Rules);

    REQUIRE(Test.Changes.size() == 1);
    CHECK(Test.Changes.front().Key.startsWith(QString::fromStdString(ReviewSessionGroup) + "/"));
}

TEST_CASE("The choice and match stages turn off for one session kind and stay off after reopening",
          "[settings][sync]")
{
    Fixture Test;

    FTSessionExerciseRules Rules = Test.Settings.GetSessionExercises(TrainingSessionGroup);
    REQUIRE(Rules.bChoiceStage);
    REQUIRE(Rules.bMatchStage);

    Rules.bChoiceStage = false;
    Rules.bMatchStage = false;
    Test.Settings.SetSessionExercises(TrainingSessionGroup, Rules);

    REQUIRE(Test.Changes.size() == 2);
    CHECK(Test.Changes[0].Key == "intensiveLearning/choiceStage");
    CHECK(Test.Changes[1].Key == "intensiveLearning/matchStage");

    const FTSettings Reopened{Test.Folder.filePath("settings.ini")};
    CHECK_FALSE(Reopened.GetSessionExercises(TrainingSessionGroup).bChoiceStage);
    CHECK_FALSE(Reopened.GetSessionExercises(TrainingSessionGroup).bMatchStage);
    CHECK(Reopened.GetSessionExercises(ReviewSessionGroup).bMatchStage);
}

TEST_CASE("A review session leaves out the choice stage and asks for three correct answers by default",
          "[settings]")
{
    const FTSessionExerciseRules Review = MakeDefaultSessionExercises(ReviewSessionGroup);

    CHECK_FALSE(Review.bChoiceStage);
    CHECK_FALSE(Review.bSentenceStage);
    CHECK(Review.RepeatsToMaster == 3);
    CHECK(Review.bAudioExercise);
    CHECK(Review.bTypeWordExercise);
    CHECK(Review.bGapExercise);
}

TEST_CASE("Changing a device setting tells the listener nothing", "[settings][sync]")
{
    Fixture Test;

    Test.Settings.SetLaunchAtStartup(true);
    Test.Settings.SetCachedLineCount(5);
#ifdef QT_DEBUG
    Test.Settings.SetDebugContinueForced(true);
#endif

    CHECK(Test.Changes.empty());
}

TEST_CASE("Synced values hold what the file holds and no device setting", "[settings][sync]")
{
    Fixture Test;

    Test.Settings.SetTrainingWordCount(15);
    Test.Settings.SetIntervalDays({1, 3, 9});
    Test.Settings.SetLaunchAtStartup(true);

    const std::vector<std::pair<QString, QVariant>> Values = Test.Settings.ReadSyncedValues();

    REQUIRE(FindValue(Values, "intensiveLearning/trainingWordCount").has_value());
    CHECK(FindValue(Values, "intensiveLearning/trainingWordCount")->toInt() == 15);
    CHECK(FindValue(Values, "schedule/intervalDays").has_value());
    CHECK_FALSE(FindValue(Values, "startup/launchAtStartup").has_value());
}

TEST_CASE("Applying synced values changes the settings without telling the listener", "[settings][sync]")
{
    Fixture Test;

    Test.Settings.ApplySyncedValues({{"intensiveLearning/trainingWordCount", QVariant(20)}});

    CHECK(Test.Settings.GetIntensiveLearningRules().TrainingWordCount == 20);
    CHECK(Test.Changes.empty());

    const FTSettings Reopened{Test.Folder.filePath("settings.ini")};
    CHECK(Reopened.GetIntensiveLearningRules().TrainingWordCount == 20);
}

TEST_CASE("Applying a device setting from elsewhere is ignored", "[settings][sync]")
{
    Fixture Test;

    Test.Settings.ApplySyncedValues({{"startup/launchAtStartup", QVariant(true)}});

    CHECK_FALSE(Test.Settings.GetLaunchAtStartup());
}

TEST_CASE("Intervals survive a round trip through their stored text", "[settings][sync]")
{
    Fixture Source;
    Fixture Destination;

    Source.Settings.SetIntervalDays({1, 3, 9});

    std::vector<std::pair<QString, QVariant>> Carried;

    for (const auto& [Key, Value] : Source.Settings.ReadSyncedValues())
    {
        Carried.emplace_back(Key, ParseSettingValue(SerializeSettingValue(Value)));
    }

    Destination.Settings.ApplySyncedValues(Carried);

    CHECK(Destination.Settings.GetScheduleRules().IntervalDays == std::vector<int>{1, 3, 9});
}

TEST_CASE("The language pair is a synced setting", "[settings][sync]")
{
    FTSettings::SetAvailableLanguagePairs({FTLanguageRules{"fr", "en"}, FTLanguageRules{"es", "en"}});
    const auto Restore = qScopeGuard([] { FTSettings::SetAvailableLanguagePairs({}); });

    Fixture Test;

    REQUIRE(Test.Settings.SetLanguagePair("es", "en"));

    const auto Target = std::find_if(Test.Changes.begin(), Test.Changes.end(),
                                     [](const FChange& Change) { return Change.Key == "languages/target"; });
    REQUIRE(Target != Test.Changes.end());
    CHECK(Target->Value.toString() == "es");
    CHECK(FindValue(Test.Settings.ReadSyncedValues(), "languages/target").has_value());

    Test.Settings.ApplySyncedValues({{"languages/target", QVariant("fr")}});

    CHECK(Test.Settings.GetLanguageRules().TargetLanguage == "fr");
}

TEST_CASE("A chosen language with no dictionary on this device is kept", "[settings][sync]")
{
    FTSettings::SetAvailableLanguagePairs({FTLanguageRules{"en", "uk"}});
    const auto Restore = qScopeGuard([] { FTSettings::SetAvailableLanguagePairs({}); });

    Fixture Test;

    Test.Settings.ApplySyncedValues({{"languages/target", QVariant("fr")}, {"languages/native", QVariant("en")}});

    CHECK(Test.Settings.GetLanguageRules().TargetLanguage == "fr");

    Test.Settings.RevalidateLanguageRules();

    CHECK(Test.Settings.GetLanguageRules().TargetLanguage == "fr");
    CHECK(Test.Settings.HasChosenLanguage());

    const FTSettings Reopened{Test.Folder.filePath("settings.ini")};
    CHECK(Reopened.GetLanguageRules().TargetLanguage == "fr");
}

TEST_CASE("A language nobody chose falls back to one with a dictionary", "[settings][sync]")
{
    FTSettings::SetAvailableLanguagePairs({FTLanguageRules{"en", "uk"}});
    const auto Restore = qScopeGuard([] { FTSettings::SetAvailableLanguagePairs({}); });

    Fixture Test;
    Test.Settings.RevalidateLanguageRules();

    CHECK_FALSE(Test.Settings.HasChosenLanguage());
    CHECK(Test.Settings.GetLanguageRules().TargetLanguage == "en");
}

TEST_CASE("Adopting a language pair needs no dictionary and tells the listener", "[settings][sync]")
{
    FTSettings::SetAvailableLanguagePairs({FTLanguageRules{"en", "uk"}});
    const auto Restore = qScopeGuard([] { FTSettings::SetAvailableLanguagePairs({}); });

    Fixture Test;

    Test.Settings.AdoptLanguagePair(FTLanguageRules{"es", "en"});

    CHECK(Test.Settings.HasChosenLanguage());
    CHECK(Test.Settings.GetLanguageRules().TargetLanguage == "es");
    CHECK(Test.Settings.GetLanguageRules().NativeLanguage == "en");
    CHECK(std::any_of(Test.Changes.begin(), Test.Changes.end(),
                      [](const FChange& Change)
                      {
                          return Change.Key == "languages/target" && Change.Value.toString() == "es";
                      }));
}

TEST_CASE("Device preferences live in the workspace file and never sync", "[settings][sync]")
{
    Fixture Test;

    {
        QSettings Store(Test.Settings.OpenStore());
        Store.setValue("appearance/theme", 0);
        Store.setValue("reader/textSize", 22);
        Store.setValue("audio/voiceName", "Siwis");
        Store.setValue("triage/rangePaneOpen", true);
    }

    const std::vector<std::pair<QString, QVariant>> Values = Test.Settings.ReadSyncedValues();

    CHECK_FALSE(FindValue(Values, "appearance/theme").has_value());
    CHECK_FALSE(FindValue(Values, "reader/textSize").has_value());
    CHECK_FALSE(FindValue(Values, "audio/voiceName").has_value());
    CHECK_FALSE(FindValue(Values, "triage/rangePaneOpen").has_value());

    Test.Settings.ApplySyncedValues({{"reader/textSize", QVariant(30)}});

    CHECK(QSettings(Test.Folder.filePath("settings.ini"), QSettings::IniFormat).value("reader/textSize").toInt() == 22);
}
