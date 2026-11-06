# find_package results do not cross into a sibling add_subdirectory scope, so this needs its own
# call even though core/CMakeLists.txt already found Qt6::Core.
find_package(Qt6 REQUIRED COMPONENTS Core Qml Pdf)

# The SQLite plugin is its own optional CMake package, not pulled in by find_package(Qt6 ...
# Sql) automatically, and its config file lives nested under Qt6Sql's own cmake directory rather
# than a findable top-level one. Needed here only to copy it next to CoreTests.exe; app/ gets it
# from the same Qt install without asking, since qt_add_executable's deployment step handles that
# itself.
find_package(Qt6QSQLiteDriverPlugin REQUIRED PATHS "${Qt6Sql_DIR}" NO_DEFAULT_PATH)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
    GIT_SHALLOW ON
)

# Fetched here rather than under third_party/, so the folder the vendored libraries get is set
# around this call too. Catch2 brings three targets of its own.
set(CMAKE_FOLDER "Third party")
FetchContent_MakeAvailable(Catch2)
unset(CMAKE_FOLDER)

add_executable(CoreTests
    AnswerComparisonTests.cpp
    AnkiImporterTests.cpp
    BookCatalogTests.cpp
    CatalogTests.cpp
    CssStyleTests.cpp
    ZipExtractTests.cpp
    CardTimerTests.cpp
    WordFormsTests.cpp
    CsvImportExportTests.cpp
    DatabaseTests.cpp
    DictionaryRepositoryTests.cpp
    DocumentExtractorTests.cpp
    FTExerciseSessionTests.cpp
    CardRepositoryTests.cpp
    ParagraphModelTests.cpp
    PdfDocumentExtractorTests.cpp
    PlacementTestTests.cpp
    RepetitionBatchTests.cpp
    ResultMarksTests.cpp
    ReaderRepositoryTests.cpp
    CloudBookRepositoryTests.cpp
    ScheduleManagerTests.cpp
    ErasureTests.cpp
    JournalTests.cpp
    ScheduleTests.cpp
    SchemaTests.cpp
    SettingsDefaultsTests.cpp
    SettingsSyncTests.cpp
    SentencePieceTokenizerTests.cpp
    SentenceCardsTests.cpp
    EnglishFormsTests.cpp
    CardMeaningTests.cpp
    DeckProgressTests.cpp
    SentenceTreeTests.cpp
    SpeechTests.cpp
    TestMain.cpp
    TranslationTests.cpp
    TypesTests.cpp
    VocabularyMarksTests.cpp
    CardSelectorTests.cpp
    CardStoreTests.cpp
    YoutubeCaptionsTests.cpp
    YoutubeRepositoryTests.cpp
    YoutubeVideoTests.cpp
)

# Plain Catch2, not Catch2WithMain: TestMain.cpp supplies main() itself, since QSqlDatabase needs
# a QCoreApplication constructed before Catch2's session runs.
target_link_libraries(CoreTests PRIVATE core Catch2::Catch2 Qt6::Core Qt6::Qml Qt6::Pdf miniz)

# App-tier files that depend on nothing else in app/, compiled against the same headers the app
# builds them with. Anything reaching AppDatabase or AppSpeech stays out: those are singletons over
# a fixed file path and the speech models, so a test could only run against the developer's real
# database or load 140 MB of voices to check a list model.
target_sources(CoreTests PRIVATE
    "${CMAKE_SOURCE_DIR}/app/src/FTPdfDocumentExtractor.cpp"
    "${CMAKE_SOURCE_DIR}/app/src/FTParagraphModel.cpp"
)

# FTParagraphModel is a QObject, so the test binary needs moc of its own.
set_target_properties(CoreTests PROPERTIES AUTOMOC ON)

target_include_directories(CoreTests PRIVATE
    "${CMAKE_SOURCE_DIR}/app/src"
)

# The tests read the data straight out of the source tree rather than a copy, since they only need to
# find it and not to ship it.
target_compile_definitions(CoreTests PRIVATE
    PIPER_MODEL_DIRECTORY="${SHERPA_ONNX_MODELS_DIR}/piper-fr-siwis-medium"
    ENGLISH_MODEL_DIRECTORY="${SHERPA_ONNX_MODELS_DIR}/piper-en-amy-medium"
    SPANISH_MODEL_DIRECTORY="${SHERPA_ONNX_MODELS_DIR}/piper-es-claude-high"
    SUPERTONIC_MODEL_DIRECTORY="${SHERPA_ONNX_MODELS_DIR}/supertonic-3"
    SAVED_TEST_DATA_DIRECTORY="${CMAKE_SOURCE_DIR}/local/saved"
    DICTIONARY_DATABASE_PATH="${CMAKE_SOURCE_DIR}/data/built/fr/fr-dictionary.db"
    DICTIONARY_PACKAGES_PATH="${CMAKE_SOURCE_DIR}/data/built"
    DICTIONARY_SCHEMA_PATH="${CMAKE_SOURCE_DIR}/scripts/dictionary/schema.sql"
    LEARNABLE_READINGS_SCRIPT_PATH="${CMAKE_SOURCE_DIR}/scripts/dictionary/learnable_readings.sql"
    OPUS_MT_MODEL_DIRECTORY="${CMAKE_SOURCE_DIR}/third_party/models/opus-mt-fr-en"
    OPUS_MT_SPANISH_MODEL_DIRECTORY="${CMAKE_SOURCE_DIR}/third_party/models/opus-mt-es-en"
    OPUS_MT_ENGLISH_MODEL_DIRECTORY="${CMAKE_SOURCE_DIR}/third_party/models/opus-mt-en-uk"
    OPUS_MT_PORTUGUESE_MODEL_DIRECTORY="${CMAKE_SOURCE_DIR}/third_party/models/opus-mt-pt-en"
    OPUS_MT_ITALIAN_MODEL_DIRECTORY="${CMAKE_SOURCE_DIR}/third_party/models/opus-mt-it-en"
)

# The DLLs do have to sit next to the executable, or loading fails before main runs.
add_custom_command(TARGET CoreTests POST_BUILD
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_SOURCE_DIR}/scripts/tools/copy-files-retry.ps1"
            -Destination "$<TARGET_FILE_DIR:CoreTests>" ${SHERPA_ONNX_DLLS}
    COMMENT "Copying the speech libraries next to the tests"
)

# QSqlDatabase loads its SQLite backend as a plugin found relative to the executable's own
# sqldrivers/ folder; nothing here inherits Qt Creator's or an installed app's plugin path.
add_custom_command(TARGET CoreTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:CoreTests>/sqldrivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QSQLiteDriverPlugin>"
            "$<TARGET_FILE_DIR:CoreTests>/sqldrivers"
    COMMENT "Copying the SQLite driver plugin next to the tests"
)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(CoreTests)
