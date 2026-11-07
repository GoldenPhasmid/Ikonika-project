find_package(Qt6 REQUIRED COMPONENTS Core Sql)

# The trainer schema's single source of truth is the readable resources/schema.sql file. It is
# turned into a C++ constant at configure time rather than shipped beside the binary: the
# database is created at first launch on the user's machine, so the DDL has to travel inside the
# executable. CONFIGURE_DEPENDS re-runs CMake automatically whenever the .sql file changes, so a
# schema edit is picked up without a manual reconfigure.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/schema.sql" SCHEMA_SQL)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/schema.sql")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/schema_sql.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/schema_sql.h"
    @ONLY
)

add_library(core STATIC
    src/FCardSelectorBase.cpp
    src/FTAnswerComparison.cpp
    src/FTAnkiImporter.cpp
    src/FTAudioPlayer.cpp
    src/FTArticleRules.cpp
    src/FTCard.cpp
    src/FTCardMeaning.cpp
    src/FTDeckProgress.cpp
    src/FTCardStore.cpp
    src/FTWordForms.cpp
    src/FTBookCatalog.cpp
    src/FTCatalog.cpp
    src/FTWebSearch.cpp
    src/FTZipExtract.cpp
    src/FTCssStyle.cpp
    src/FTCsvExporter.cpp
    src/FTCsvImporter.cpp
    src/FTDatabase.cpp
    src/FTCardRepository.cpp
    src/FTDictionaryPackage.cpp
    src/FTDictionaryRepository.cpp
    src/FTDocumentExtractor.cpp
    src/FTLanguagePairScope.cpp
    src/FTLog.cpp
    src/FTPlacementTest.cpp
    src/FTReaderDocument.cpp
    src/FTReaderRepository.cpp
    src/FTCloudBookRepository.cpp
    src/FTRepetitionBatch.cpp
    src/FTErasure.cpp
    src/FTEnglishForms.cpp
    src/FTJournal.cpp
    src/FTNarration.cpp
    src/FTSchedule.cpp
    src/FTScheduleManager.cpp
    src/FTScoring.cpp
    src/FTSchema.cpp
    src/FTSentencePieceTokenizer.cpp
    src/FTSentenceCards.cpp
    src/FTSentenceSplitter.cpp
    src/FTSentenceTree.cpp
    src/FTSettings.cpp
    src/FTTextNormalization.cpp
    src/FTTypes.cpp
    src/FTUuid.cpp
    src/FTVocabularyMarks.cpp
    src/FTYoutubeCaptions.cpp
    src/FTYoutubeRepository.cpp
    src/FTYoutubeVideo.cpp
	include/core/FCardSelectorBase.h
    include/core/FTAnswerComparison.h
    include/core/FTAnkiImporter.h
    include/core/FTAudioPlayer.h
    include/core/FTArticleRules.h
    include/core/FTCard.h
    include/core/FTCardMeaning.h
    include/core/FTDeckProgress.h
    include/core/FTWordForms.h
    include/core/sessions/FCardState.h
    include/core/FTCssStyle.h
    include/core/FTCsvExporter.h
    include/core/FTCsvImporter.h
    include/core/FTDatabase.h
    include/core/FTCardRepository.h
    include/core/FTDictionaryPackage.h
    include/core/FTDictionaryRepository.h
    include/core/FTDocumentExtractor.h
    include/core/sessions/FTExerciseSession.h
    include/core/FTInContextTranslator.h
    include/core/FTLog.h
    include/core/FTReaderDocument.h
    include/core/FTReaderRepository.h
    include/core/FTCloudBookRepository.h
    include/core/FTPlacementTest.h
    include/core/FTRepetitionBatch.h
    include/core/FTErasure.h
    include/core/FTEnglishForms.h
    include/core/FTJournal.h
    include/core/FTNarration.h
    include/core/FTSchedule.h
    include/core/FTScheduleManager.h
    include/core/FTScoring.h
    include/core/FTSchema.h
    include/core/FTSentencePieceTokenizer.h
    include/core/FTSentenceCards.h
    include/core/FTSentenceSplitter.h
    include/core/FTSentenceTree.h
    include/core/FTSettings.h
    include/core/FTSpeech.h
    include/core/FTTypes.h
    include/core/FTUuid.h
    include/core/FTVocabularyMarks.h
    include/core/FTYoutubeCaptions.h
    include/core/FTYoutubeRepository.h
    include/core/FTYoutubeVideo.h
)

# FTInContextTranslator drives onnxruntime through Windows' own LoadLibrary/GetProcAddress, so it
# only builds where that API exists. In-context translation is not part of either phone release.
if (IKONIKA_DESKTOP)
    target_sources(core PRIVATE src/FTInContextTranslator.cpp src/FTSpeech.cpp)
    target_link_libraries(core PRIVATE onnxruntime-c-api sherpa-onnx)
endif()

target_include_directories(core PUBLIC
    include
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(core
    PUBLIC
        Qt6::Core
        Qt6::Sql
    PRIVATE
        miniaudio
        miniz
)

if (IOS)
    target_include_directories(core PRIVATE "${CMAKE_SOURCE_DIR}/third_party/sqlite")
else()
    target_link_libraries(core PRIVATE sqlite3)
endif()
