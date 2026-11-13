add_executable(AppTests
    ReaderIntakeTests.cpp
    TextIntakeTests.cpp
    "${CMAKE_SOURCE_DIR}/app/src/AppTextIntake.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppReaderIntake.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppSingleInstance.cpp"
    ExerciseControllerBaseTests.cpp
    FlashcardControllerTests.cpp
    FlipRecallControllerTests.cpp
    MultipleChoiceControllerTests.cpp
    CorrectIncorrectControllerTests.cpp
    WrittenProductionControllerTests.cpp
    AudioTranscriptionControllerTests.cpp
    SpeedMatchControllerTests.cpp
    RepetitionControllerTests.cpp
    IntensiveLearningControllerTests.cpp
    ExerciseFlowStagesTests.cpp
    CardEditDuringSessionTests.cpp
    VocabularyControllerTests.cpp
    ExerciseLauncherControllerTests.cpp
    WordListControllerTests.cpp
    WordRowGroupingTests.cpp
    AppCardsTests.cpp
    TriageControllerTests.cpp
    DeckTests.cpp
    ReaderShelfTests.cpp
    DictionaryControllerTests.cpp
    DailySessionControllerTests.cpp
    SentenceTilesControllerTests.cpp
    SessionSentenceStageTests.cpp
    SentenceTrainingControllerTests.cpp
    FormsSessionTests.cpp
    DictionaryUpdateTests.cpp
    LanguageNameTests.cpp
    AppSpeechTests.cpp
    AccountRegistryTests.cpp
    WorkspaceTests.cpp
    WorkspaceHostTests.cpp
    DriveStoreTests.cpp
    GoogleTokenSourceTests.cpp
    CredentialStoreTests.cpp
    WorkspaceSyncTests.cpp
    WorkspaceMergeTests.cpp
    TestMain.cpp
    "${CMAKE_SOURCE_DIR}/app/src/controllers/FlashcardController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/FlipRecallController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/MultipleChoiceController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/CorrectIncorrectController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/WrittenProductionController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/GapProductionController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/AudioTranscriptionController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/SpeedMatchController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/SentenceTilesController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/SentenceSelection.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/SentenceTrainingController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/ExerciseControllerBase.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/SessionSettings.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/StageSessionControllerBase.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/ExerciseFlowStages.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/FormsStages.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/SessionSentenceStage.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/RepetitionController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/IntensiveLearningController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/VocabularyController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/DailySessionController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/ExerciseLauncherController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/WordListController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/WordRowGrouping.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/TriageController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/DiscoveryController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/DecksController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/controllers/DictionaryController.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppGoogleAccount.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppSpeech.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppTranslator.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppSoundEffects.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppDatabase.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppDatabaseService.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppSessionCheckpoints.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppState.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppPaths.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppCards.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppCoreCards.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppDictionary.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppReadSources.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppCatalog.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppWebSearch.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppPackageDownloader.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppFileTransfer.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppLocalization.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppAccountRegistry.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppWorkspace.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppWorkspaceHost.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppWorkspaceSync.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppCloudStore.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppFiles.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppWorkspaceMerger.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppCredentialStore.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/AppGoogleTokenSource.cpp"
)

target_link_libraries(AppTests PRIVATE core Catch2::Catch2 Qt6::Core Qt6::Gui Qt6::Qml
    Qt6::Network Qt6::NetworkAuth miniz)

if(WIN32)
    target_link_libraries(AppTests PRIVATE Advapi32)
endif()

target_include_directories(AppTests PRIVATE
    "${CMAKE_SOURCE_DIR}/app/src"
    "${CMAKE_SOURCE_DIR}/app/src/controllers"
)

target_compile_definitions(AppTests PRIVATE
    PROJECT_ROOT_DIRECTORY="${CMAKE_SOURCE_DIR}"
)

if (IKONIKA_ENABLE_MACHINE_TRANSLATION)
    target_compile_definitions(AppTests PRIVATE IKONIKA_ENABLE_MACHINE_TRANSLATION)
endif()

set_target_properties(AppTests PROPERTIES AUTOMOC ON)

find_package(Qt6QSQLiteDriverPlugin REQUIRED PATHS "${Qt6Sql_DIR}" NO_DEFAULT_PATH)

add_custom_command(TARGET AppTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:AppTests>/sqldrivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QSQLiteDriverPlugin>"
            "$<TARGET_FILE_DIR:AppTests>/sqldrivers"
    COMMENT "Copying the SQLite driver plugin next to AppTests"
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}/_deps/catch2-src/extras")
include(Catch)
catch_discover_tests(AppTests)
