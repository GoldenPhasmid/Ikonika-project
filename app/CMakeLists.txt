# Qml is asked for by name even though Quick already brings it in. Deployment registers its
# helpers per directly requested component, and without it the generated install script calls
# qt6_deploy_qml_imports, which nothing has defined.
#
# WebEngineQuick has no Android build at all, so the YouTube screen it serves stays desktop-only.
# Pdf ships for Android too (unlike WebEngineQuick), so the reader's PDF pipeline builds on both
# platforms; PdfQuick is desktop-only since nothing here uses it - the reader paints its own PDF
# bitmaps rather than using PdfQuick's page-view components. WebEngineQuick is also gated by its
# own option, since it pulls in Chromium and is otherwise only for the YouTube screen.
option(IKONIKA_ENABLE_WEBENGINE "Build the YouTube screen, which needs QtWebEngine" ON)
option(IKONIKA_ENABLE_MACHINE_TRANSLATION "Translate what dictionary search cannot find" ON)

set(IKONIKA_QT_COMPONENTS Qml Quick QuickControls2 Sql Widgets Network LinguistTools Pdf)

if (NOT IOS)
    list(APPEND IKONIKA_QT_COMPONENTS NetworkAuth)
endif()

if (IKONIKA_DESKTOP)
    list(APPEND IKONIKA_QT_COMPONENTS PdfQuick)
endif()

if (IKONIKA_DESKTOP AND IKONIKA_ENABLE_WEBENGINE)
    list(APPEND IKONIKA_QT_COMPONENTS WebEngineQuick)
endif()

find_package(Qt6 REQUIRED COMPONENTS ${IKONIKA_QT_COMPONENTS})

# 6.11 rather than a lower floor so the project opts into current Qt CMake policies instead of
# warning about them. Nothing here needs the older behaviour.
qt_standard_project_setup(REQUIRES 6.11)

set(IKONIKA_READER_CORE_SOURCES
    src/FTParagraphModel.cpp
    src/FTParagraphModel.h
    src/controllers/ReaderController.cpp
    src/controllers/ReaderController.h
)

set(IKONIKA_READER_SOURCES
    src/FTPdfDocumentExtractor.cpp
    src/FTPdfDocumentExtractor.h
    src/FTPdfRenderWorker.cpp
    src/FTPdfRenderWorker.h
    src/ReaderPdfDocument.cpp
    src/ReaderPdfDocument.h
    src/ReaderPdfPage.cpp
    src/ReaderPdfPage.h
    src/ReaderTextHitTest.cpp
    src/ReaderTextHitTest.h
    src/ReaderVocabulary.cpp
    src/ReaderVocabulary.h
    src/ReaderReadAloud.cpp
    src/ReaderReadAloud.h
    src/ReaderTextHighlighter.cpp
    src/ReaderTextHighlighter.h
)

set(IKONIKA_YOUTUBE_SOURCES
    src/AppYoutubeCaptions.cpp
    src/AppYoutubeCaptions.h
    src/controllers/YoutubeController.cpp
    src/controllers/YoutubeController.h
)

set(IKONIKA_SINGLE_INSTANCE_SOURCES
    src/AppSingleInstance.cpp
    src/AppSingleInstance.h
)

set(IKONIKA_BUNDLED_SPEECH_SOURCES
    src/AppSpeechService.cpp
    src/AppSpeechService.h
)

set(IKONIKA_PLATFORM_SPEECH_SOURCES
    src/AppPlatformSpeech.cpp
    src/AppPlatformSpeech.h
)

set(IKONIKA_BUNDLED_TRANSLATION_SOURCES
    src/AppTranslator.cpp
)

set(IKONIKA_PLATFORM_TRANSLATION_SOURCES
    src/AppPlatformTranslator.cpp
)

set(IKONIKA_IOS_SOURCES
    src/AppIosNotifications.h
    src/AppIosNotifications.mm
    src/AppIosPresenter.h
    src/AppIosPresenter.mm
    src/AppIosReader.h
    src/AppIosReader.mm
    src/AppIosShare.h
    src/AppIosShare.mm
    src/AppIosSpeech.h
    src/AppIosSpeech.mm
    src/AppIosStorage.h
    src/AppIosStorage.mm
    src/AppTranslatorUnavailable.cpp
    ios/PrivacyInfo.xcprivacy
)

set(IKONIKA_STARTUP_TASK_SOURCES
    src/AppStartupTask.cpp
    src/AppStartupTask.h
)

qt_add_executable(IkonikaApp
    src/AppCards.cpp
    src/AppCards.h
    src/AppCatalog.cpp
    src/AppCatalog.h
    src/AppWebSearch.cpp
    src/AppWebSearch.h
    src/AppFileTransfer.cpp
    src/AppFileTransfer.h
    src/AppPackageDownloader.cpp
    src/AppPackageDownloader.h
    src/AppUpdater.cpp
    src/AppUpdater.h
    src/AppDatabase.cpp
    src/AppDatabase.h
    src/AppDatabaseService.cpp
    src/AppDatabaseService.h
    src/AppAccountRegistry.cpp
    src/AppAccountRegistry.h
    src/AppCloudStore.cpp
    src/AppFiles.cpp
    src/AppFiles.h
    src/AppCloudStore.h
    src/AppBookCloud.cpp
    src/AppBookCloud.h
    src/AppBookImageProvider.cpp
    src/AppBookImageProvider.h
    src/AppFreeBooks.cpp
    src/AppFreeBooks.h
    src/AppCredentialStore.cpp
    src/AppCredentialStore.h
    src/AppGoogleTokenSource.cpp
    src/AppGoogleTokenSource.h
    src/AppWorkspace.cpp
    src/AppWorkspace.h
    src/AppWorkspaceHost.cpp
    src/AppWorkspaceHost.h
    src/AppSyncMerger.h
    src/AppWorkspaceSync.cpp
    src/AppWorkspaceMerger.cpp
    src/AppWorkspaceMerger.h
    src/AppWorkspaceSync.h
    src/AppDictionary.cpp
    src/AppDictionary.h
    src/AppLocalization.cpp
    src/AppLocalization.h
    src/AppLog.cpp
    src/AppLog.h
    src/AppOnboarding.cpp
    src/AppOnboarding.h
    src/AppGoogleAccount.cpp
    src/AppGoogleAccount.h
    src/AppSpeech.cpp
    src/AppSpeech.h
    src/AppVoiceChoices.cpp
    src/AppVoiceChoices.h
    src/AppStartup.cpp
    src/controllers/AppSyncController.cpp
    src/controllers/AppSyncController.h
    src/AppTheme.cpp
    src/AppTheme.h
    src/AppTranslator.h
    src/AppMachineTranslation.cpp
    src/AppMachineTranslation.h
    src/AppMetrics.cpp
    src/AppMetrics.h
    src/AppPaths.cpp
    src/AppPaths.h
    src/AppHomeWidget.cpp
    src/AppHomeWidget.h
    src/AppModifierKeys.cpp
    src/AppModifierKeys.h
    src/AppReaderIntake.cpp
    src/AppReaderIntake.h
    src/AppPaneWidths.cpp
    src/AppPaneWidths.h
    src/AppViewLayouts.cpp
    src/AppViewLayouts.h
    src/AppReaderSettings.cpp
    src/AppReaderSettings.h
    src/AppReminder.cpp
    src/AppReminder.h
    src/AppHeartbeat.cpp
    src/AppHeartbeat.h
    src/AppSoundEffects.cpp
    src/AppSwitchCover.cpp
    src/AppSoundEffects.h
    src/AppSessionCheckpoints.cpp
    src/AppSessionCheckpoints.h
    src/AppState.cpp
    src/AppState.h
    src/controllers/AudioTranscriptionController.cpp
    src/controllers/AudioTranscriptionController.h
    src/controllers/CorrectIncorrectController.cpp
    src/controllers/CorrectIncorrectController.h
    src/controllers/DailySessionController.cpp
    src/controllers/TriageController.h
    src/controllers/TriageController.cpp
    src/controllers/PlacementTestController.h
    src/controllers/PlacementTestController.cpp
    src/controllers/DailySessionController.h
    src/controllers/DecksController.cpp
    src/controllers/DecksController.h
    src/controllers/VocabularyController.cpp
    src/controllers/VocabularyController.h
    src/controllers/CefrFormatting.h
    src/controllers/DictionaryController.cpp
    src/controllers/DictionaryController.h
    src/controllers/ExerciseControllerBase.cpp
    src/controllers/ExerciseControllerBase.h
    src/controllers/FormsStages.cpp
    src/controllers/FormsStages.h
    src/controllers/SessionSentenceStage.cpp
    src/controllers/SessionSentenceStage.h
    src/controllers/ExerciseFlowStages.cpp
    src/controllers/ExerciseFlowStages.h
    src/controllers/ExerciseLauncherController.cpp
    src/controllers/ExerciseLauncherController.h
    src/controllers/IExerciseFlowStage.h
    src/controllers/FlashcardController.cpp
    src/controllers/FlashcardController.h
    src/controllers/FlipRecallController.cpp
    src/controllers/FlipRecallController.h
    src/controllers/WordListController.cpp
    src/controllers/WordListController.h
    src/controllers/WordRowGrouping.cpp
    src/controllers/WordRowGrouping.h
    src/FTCardTimer.h
    src/SettingsStore.cpp
    src/SettingsStore.h
    src/SessionSettings.cpp
    src/SessionSettings.h
    src/controllers/IntensiveLearningController.cpp
    src/controllers/IntensiveLearningController.h
    src/main.cpp
    src/controllers/MultipleChoiceController.cpp
    src/controllers/MultipleChoiceController.h
    src/ResultMarks.h
    src/controllers/RepetitionController.cpp
    src/controllers/RepetitionController.h
    src/controllers/SentenceTilesController.cpp
    src/controllers/SentenceTilesController.h
    src/controllers/SentenceSelection.cpp
    src/controllers/SentenceSelection.h
    src/controllers/SentenceTrainingController.cpp
    src/controllers/SentenceTrainingController.h
    src/controllers/SpeedMatchController.cpp
    src/controllers/SpeedMatchController.h
    src/controllers/StageSessionControllerBase.cpp
    src/controllers/StageSessionControllerBase.h
    src/UiProbe.cpp
    src/UiProbe.h
    src/controllers/WrittenProductionController.cpp
    src/controllers/WrittenProductionController.h
)

target_sources(IkonikaApp PRIVATE ${IKONIKA_READER_CORE_SOURCES} ${IKONIKA_READER_SOURCES})

if (ANDROID)
    target_sources(IkonikaApp PRIVATE ${IKONIKA_PLATFORM_SPEECH_SOURCES} ${IKONIKA_PLATFORM_TRANSLATION_SOURCES})
elseif (IOS)
    target_sources(IkonikaApp PRIVATE ${IKONIKA_IOS_SOURCES})
    target_link_libraries(IkonikaApp PRIVATE
        "-framework AVFoundation"
        "-framework UniformTypeIdentifiers"
        "-framework UserNotifications"
        "-framework UIKit")
else()
    target_sources(IkonikaApp PRIVATE
        ${IKONIKA_YOUTUBE_SOURCES} ${IKONIKA_SINGLE_INSTANCE_SOURCES} ${IKONIKA_BUNDLED_SPEECH_SOURCES}
        ${IKONIKA_BUNDLED_TRANSLATION_SOURCES})
endif()

if (WIN32)
    target_sources(IkonikaApp PRIVATE ${IKONIKA_STARTUP_TASK_SOURCES})
    target_link_libraries(IkonikaApp PRIVATE runtimeobject Advapi32)
endif()

set(IKONIKA_MOBILE_QML_FILES
    qml/MobileMain.qml
    qml/mobile/MobileGoalRing.qml
    qml/mobile/MobileDoubleRing.qml
    qml/mobile/MobileRingTile.qml
    qml/mobile/MobileVocabularyBar.qml
    qml/mobile/MobileLockedRow.qml
    qml/mobile/MobileTranslationChip.qml
    qml/mobile/MobileTriageMeaning.qml
    qml/mobile/MobileGistTag.qml
    qml/mobile/MobileMeaningDetails.qml
    qml/mobile/MobileMeaningSummary.qml
    qml/mobile/MobileDictionaryMeaning.qml
    qml/mobile/MobileSignalBars.qml
    qml/mobile/MobileBottomTab.qml
    qml/mobile/MobileBottomTabBar.qml
    qml/mobile/MobileHeader.qml
    qml/mobile/MobileShell.qml
    qml/mobile/MobileTodayScreen.qml
    qml/mobile/MobileExerciseButton.qml
    qml/mobile/MobileVocabularyScreen.qml
    qml/mobile/MobileDecksScreen.qml
    qml/mobile/MobileFilterChip.qml
    qml/mobile/MobileDotProgress.qml
    qml/mobile/MobileButtonPair.qml
    qml/mobile/MobileSessionButton.qml
    qml/mobile/MobileGhostLink.qml
    qml/mobile/MobileWideButton.qml
    qml/mobile/MobileActionButton.qml
    qml/mobile/MobileToast.qml
    qml/mobile/MobileTextField.qml
    qml/mobile/MobileTextInput.qml
    qml/mobile/MobileTextEdit.qml
    qml/mobile/MobileSearchField.qml
    qml/mobile/MobileActionIconButton.qml
    qml/mobile/MobileVectorIcon.qml
    qml/mobile/MobileArrowIcon.qml
    qml/mobile/MobileIcons.js
    qml/mobile/MobileBadge.qml
    qml/mobile/MobileCard.qml
    qml/mobile/MobileReconnectBanner.qml
    qml/mobile/MobileCefrBadge.qml
    qml/mobile/MobileDictionaryFormsLabel.qml
    qml/mobile/MobileDictionaryFormsSection.qml
    qml/mobile/MobileFrequencyBadge.qml
    qml/mobile/MobileHeaderSearchField.qml
    qml/mobile/MobileWordCard.qml
    qml/mobile/MobileProgressRing.qml
    qml/mobile/MobileWordCardAction.qml
    qml/mobile/MobileWordCardList.qml
    qml/mobile/MobileBottomSheet.qml
    qml/mobile/MobileSheetLoader.qml
    qml/mobile/MobileShake.qml
    qml/mobile/MobileScreenMotion.qml
    qml/mobile/MobileOptionListPanel.qml
    qml/mobile/MobileCheckbox.qml
    qml/mobile/MobileWideButtonRow.qml
    qml/mobile/MobileSearchFilterBar.qml
    qml/mobile/MobileFilterSheet.qml
    qml/mobile/MobileSortSheet.qml
    qml/mobile/MobileTenseSheet.qml
    qml/mobile/MobileTab.qml
    qml/mobile/MobileCardPickSheet.qml
    qml/mobile/MobileExercisePracticeSheet.qml
    qml/mobile/MobileFilterChipRow.qml
    qml/mobile/MobileSectionLabel.qml
    qml/mobile/MobileSettingsRow.qml
    qml/mobile/MobileSettingsGroup.qml
    qml/mobile/MobileSettingsToggle.qml
    qml/mobile/MobileSettingsSlider.qml
    qml/mobile/MobileSettingsTextField.qml
    qml/mobile/MobileExerciseTypesRow.qml
    qml/mobile/MobileFieldPicker.qml
    qml/mobile/MobileSettingsSelect.qml
    qml/mobile/MobileSettingsValue.qml
    qml/mobile/MobileSettingsChevron.qml
    qml/mobile/MobileSettingsPicker.qml
    qml/mobile/MobileFloatingButton.qml
    qml/mobile/MobileFormField.qml
    qml/mobile/MobilePicker.qml
    qml/mobile/MobileWordRow.qml
    qml/mobile/MobileAnswerField.qml
    qml/mobile/MobilePlayButton.qml
    qml/mobile/MobileWaveform.qml
    qml/mobile/MobileOptionGrid.qml
    qml/mobile/MobileMasteryDots.qml
    qml/mobile/MobileExerciseShell.qml
    qml/mobile/MobileExerciseOptionsSheet.qml
    qml/mobile/MobileExerciseMeta.qml
    qml/mobile/MobileContentArea.qml
    qml/mobile/MobileExerciseCompleteScreen.qml
    qml/mobile/MobileFlashcardScreen.qml
    qml/mobile/MobileFlipScreen.qml
    qml/mobile/MobileMultipleChoiceScreen.qml
    qml/mobile/MobileWrittenProductionScreen.qml
    qml/mobile/MobileAudioTranscriptionScreen.qml
    qml/mobile/MobileSpeedMatchScreen.qml
    qml/mobile/MobileCorrectIncorrectScreen.qml
    qml/mobile/MobileRepetitionScreen.qml
    qml/mobile/MobileIntensiveLearningScreen.qml
    qml/mobile/MobileDictionaryScreen.qml
    qml/mobile/MobileDictionaryWordScreen.qml
    qml/mobile/MobileCardDetailsScreen.qml
    qml/mobile/MobilePracticeScreen.qml
    qml/mobile/MobileSentenceTrainingScreen.qml
    qml/mobile/MobileSentenceStartSheet.qml
    qml/mobile/MobileChipSelectRow.qml
    qml/mobile/MobileToggleChip.qml
    qml/mobile/MobileSentenceSessionScreen.qml
    qml/mobile/MobileSentenceTilesScreen.qml
    qml/mobile/MobileWordTile.qml
    qml/mobile/MobileCenteredFlow.qml
    qml/mobile/MobileSentenceAudioSlot.qml
    qml/mobile/MobileSentenceText.qml
    qml/mobile/MobilePromptText.qml
    qml/mobile/MobileSeparator.qml
    qml/mobile/MobileExampleBlock.qml
    qml/mobile/MobileWordExplainSheet.qml
    qml/mobile/MobileLookupSheet.qml
    qml/mobile/MobileMachineTranslation.qml
    qml/mobile/MobileWordListScreen.qml
    qml/mobile/MobileOnboardingSignInScreen.qml
    qml/mobile/MobileOnboardingSetupScreen.qml
    qml/mobile/MobileOnboardingDownloadScreen.qml
    qml/mobile/MobileOnboardingPlacementScreen.qml
    qml/mobile/MobileSettingsScreen.qml
    qml/mobile/MobileSettingsGeneral.qml
    qml/mobile/MobileSettingsReminders.qml
    qml/mobile/MobileSettingsSessionStages.qml
    qml/mobile/MobileSettingsSound.qml
    qml/mobile/MobileSettingsReader.qml
    qml/mobile/MobileSettingsData.qml
    qml/mobile/MobileSettingsLanguages.qml
    qml/mobile/MobileSettingsLanguage.qml
    qml/mobile/MobileDeleteLanguageSheet.qml
    qml/mobile/MobileSettingsAbout.qml
    qml/mobile/MobileDeleteDataSheet.qml
    qml/mobile/MobileSyncResultSheet.qml
    qml/mobile/MobileSignOutSheet.qml
    qml/mobile/MobileOnboardingRestoreScreen.qml
    qml/mobile/MobileDictionaryUpdateSheet.qml
    qml/mobile/MobileLanguageSheet.qml
    qml/mobile/MobileTriageScreen.qml
    qml/mobile/MobileFormsStudyScreen.qml
    qml/mobile/MobileFormsTable.qml
    qml/mobile/MobileReaderScreen.qml
    qml/mobile/MobileReaderBook.qml
    qml/mobile/MobileDeck.qml
    qml/mobile/MobileReaderOpen.qml
    qml/mobile/MobileReaderBookSheet.qml
    qml/mobile/MobileConfirmSheet.qml
    qml/mobile/MobileReaderPdfView.qml
    qml/mobile/MobileReaderPdfSheet.qml
    qml/mobile/MobileReaderFrame.qml
    qml/mobile/MobileReaderPlayBar.qml
    qml/mobile/MobileReaderPageNumber.qml
)

set(IKONIKA_QML_FILES
    qml/DictionaryMatchedForm.qml
    qml/DictionaryBenchmark.qml
    qml/LearnedMark.qml
    qml/CategoryDot.qml
    qml/CoverStripe.qml
    qml/Decks.js
    qml/FormsMarkup.js
    qml/ReadAloudPlace.js
    qml/ReaderShelf.js
    qml/ReaderLevelBadge.qml
    qml/ReaderTones.js
    qml/Greeting.js
    qml/WelcomeScreen.qml
    qml/Main.qml
    qml/SwitchingScreen.qml
    qml/LoadingScreen.qml
    qml/MetaLabel.qml
    qml/PracticeSelection.qml
    qml/NavigationWalk.qml
    qml/ScreenWalk.qml
    qml/TimePickerDigit.qml
    qml/TimePickerSheet.qml
    qml/WireButton.qml
    qml/desktop/DesktopAccountProfile.qml
    qml/desktop/DesktopSettingsAccount.qml
    qml/desktop/DesktopActionBar.qml
    qml/desktop/DesktopAnswerField.qml
    qml/desktop/DesktopBookCover.qml
    qml/desktop/DesktopBookMorph.qml
    qml/desktop/DesktopButton.qml
    qml/desktop/DesktopCard.qml
    qml/desktop/DesktopCenteredFlow.qml
    qml/desktop/DesktopChip.qml
    qml/desktop/DesktopChipRow.qml
    qml/desktop/DesktopChoiceExercise.qml
    qml/desktop/DesktopChoiceGrid.qml
    qml/desktop/DesktopChoiceTile.qml
    qml/desktop/DesktopConfirmDialog.qml
    qml/desktop/DesktopCountBadge.qml
    qml/desktop/DesktopDayTile.qml
    qml/desktop/DesktopVocabularyScreen.qml
    qml/desktop/DesktopDecksScreen.qml
    qml/desktop/DesktopDeckTile.qml
    qml/desktop/DesktopTileBackground.qml
    qml/desktop/DesktopDeleteDataDialog.qml
    qml/desktop/DesktopDialog.qml
    qml/desktop/DesktopDictionaryEntry.qml
    qml/desktop/DesktopDictionaryForms.qml
    qml/desktop/DesktopPackageUpdateCard.qml
    qml/desktop/DesktopAppUpdateCard.qml
    qml/desktop/DesktopUpdateCard.qml
    qml/desktop/DesktopReconnectBanner.qml
    qml/desktop/DesktopDivider.qml
    qml/desktop/DesktopDropdownButton.qml
    qml/desktop/DesktopExerciseCard.qml
    qml/desktop/DesktopFlashcardExercise.qml
    qml/desktop/DesktopFlipExercise.qml
    qml/desktop/DesktopFocusRing.qml
    qml/desktop/DesktopFormField.qml
    qml/desktop/DesktopFormPage.qml
    qml/desktop/DesktopFormsStudyExercise.qml
    qml/desktop/DesktopFormsGrid.qml
    qml/desktop/DesktopMeaningWell.qml
    qml/desktop/DesktopGoalRing.qml
    qml/desktop/DesktopDoubleRing.qml
    qml/desktop/DesktopDailyRing.qml
    qml/desktop/DesktopWeekDay.qml
    qml/desktop/DesktopWelcomeScreen.qml
    qml/desktop/DesktopIcon.qml
    qml/desktop/DesktopIconButton.qml
    qml/desktop/DesktopKeyHint.qml
    qml/desktop/DesktopLabel.qml
    qml/desktop/DesktopLanguageEntry.qml
    qml/desktop/DesktopLanguageDownloads.qml
    qml/desktop/DesktopLanguageTile.qml
    qml/desktop/DesktopWordRangePanel.qml
    qml/desktop/DesktopLanguagePopover.qml
    qml/desktop/DesktopVoicePopover.qml
    qml/desktop/DesktopLearnedMark.qml
    qml/desktop/DesktopLevelBadge.qml
    qml/desktop/DesktopMasteryDots.qml
    qml/desktop/DesktopMatchExercise.qml
    qml/desktop/DesktopMenu.qml
    qml/desktop/DesktopMenuItem.qml
    qml/desktop/DesktopNavItem.qml
    qml/desktop/DesktopNavigationWalk.qml
    qml/desktop/DesktopOnboardingDownload.qml
    qml/desktop/DesktopOnboardingRestore.qml
    qml/desktop/DesktopOnboardingPlacement.qml
    qml/desktop/DesktopOnboardingScreen.qml
    qml/desktop/DesktopOnboardingSetup.qml
    qml/desktop/DesktopOnboardingSignIn.qml
    qml/desktop/DesktopOptionGrid.qml
    qml/desktop/DesktopPage.qml
    qml/desktop/DesktopPathNode.qml
    qml/desktop/DesktopPlayButton.qml
    qml/desktop/DesktopPopover.qml
    qml/desktop/DesktopPopoverRow.qml
    qml/desktop/DesktopPracticeDialog.qml
    qml/desktop/DesktopProgressBar.qml
    qml/desktop/DesktopProgressRing.qml
    qml/desktop/DesktopPromptLabel.qml
    qml/desktop/DesktopReaderBar.qml
    qml/desktop/DesktopReaderPlayBar.qml
    qml/desktop/DesktopReaderBookTile.qml
    qml/desktop/DesktopReaderOpenTile.qml
    qml/desktop/DesktopCoverBar.qml
    qml/desktop/DesktopCoverButton.qml
    qml/desktop/DesktopReaderAppearanceButton.qml
    qml/desktop/DesktopPaneGrip.qml
    qml/desktop/DesktopReaderContentsPane.qml
    qml/desktop/DesktopLookupPane.qml
    qml/desktop/DesktopMachineTranslation.qml
    qml/desktop/DesktopReaderPdfSheet.qml
    qml/desktop/DesktopReaderPdfView.qml
    qml/desktop/DesktopReaderScreen.qml
    qml/desktop/DesktopReaderSettingsRows.qml
    qml/desktop/DesktopReaderTextView.qml
    qml/desktop/DesktopReaderTranslation.qml
    qml/desktop/DesktopRevealDetails.qml
    qml/desktop/DesktopRevealWell.qml
    qml/desktop/DesktopReviewExercise.qml
    qml/desktop/DesktopRuledLabel.qml
    qml/desktop/DesktopScreenMorph.qml
    qml/desktop/DesktopScrollBar.qml
    qml/desktop/DesktopSearchArea.qml
    qml/desktop/DesktopSearchField.qml
    qml/desktop/DesktopSectionLabel.qml
    qml/desktop/DesktopSelect.qml
    qml/desktop/DesktopSelectionBackground.qml
    qml/desktop/DesktopSentenceDialog.qml
    qml/desktop/DesktopSentenceText.qml
    qml/desktop/DesktopSessionBar.qml
    qml/desktop/DesktopSessionComplete.qml
    qml/desktop/DesktopSessionProgress.qml
    qml/desktop/DesktopSessionScreen.qml
    qml/desktop/DesktopSessionTile.qml
    qml/desktop/DesktopSettingsAbout.qml
    qml/desktop/DesktopSettingsGeneral.qml
    qml/desktop/DesktopSettingsGroup.qml
    qml/desktop/DesktopSettingsReader.qml
    qml/desktop/DesktopSettingsReminders.qml
    qml/desktop/DesktopSettingsRow.qml
    qml/desktop/DesktopSettingsScreen.qml
    qml/desktop/DesktopSettingsSessionStages.qml
    qml/desktop/DesktopSettingsSlider.qml
    qml/desktop/DesktopSettingsSound.qml
    qml/desktop/DesktopShakeAnimation.qml
    qml/desktop/DesktopSidePane.qml
    qml/desktop/DesktopSignalBars.qml
    qml/desktop/DesktopStartDialog.qml
    qml/desktop/DesktopSwitch.qml
    qml/desktop/DesktopTenseDialog.qml
    qml/desktop/DesktopTab.qml
    qml/desktop/DesktopTextArea.qml
    qml/desktop/DesktopTextField.qml
    qml/desktop/DesktopTilesExercise.qml
    qml/desktop/DesktopToast.qml
    qml/desktop/DesktopTodayPath.qml
    qml/desktop/DesktopTodayScreen.qml
    qml/desktop/DesktopToggle.qml
    qml/desktop/DesktopToggleChip.qml
    qml/desktop/DesktopTopBar.qml
    qml/desktop/DesktopTranslationChip.qml
    qml/desktop/DesktopTriageMeaning.qml
    qml/desktop/DesktopGistBadge.qml
    qml/desktop/DesktopEntryHeader.qml
    qml/desktop/DesktopRankSlider.qml
    qml/desktop/DesktopMeaningDetails.qml
    qml/desktop/DesktopMeaningSummary.qml
    qml/desktop/DesktopSpeakButton.qml
    qml/desktop/DesktopDictionaryMeaning.qml
    qml/desktop/DesktopTriageScreen.qml
    qml/desktop/DesktopTypedExercise.qml
    qml/desktop/DesktopWaveform.qml
    qml/desktop/DesktopWiggleAnimation.qml
    qml/desktop/DesktopCardDetailsDialog.qml
    qml/desktop/DesktopCardForm.qml
    qml/desktop/DesktopCardPane.qml
    qml/desktop/DesktopWordList.qml
    qml/desktop/DesktopWordRow.qml
    qml/desktop/DesktopWordTile.qml
    qml/desktop/DesktopWordsScreen.qml
)

list(APPEND IKONIKA_QML_FILES ${IKONIKA_MOBILE_QML_FILES})

# NO_LINT drops the four qmllint projects Qt would otherwise generate per module. qmllint is still
# runnable by hand; nothing here ran it, so the targets were solution clutter.
qt_add_qml_module(IkonikaApp
    URI Ikonika
    VERSION 1.0
    NO_LINT
    QML_FILES ${IKONIKA_QML_FILES}
)

# Ukrainian only; English is the source language every qsTr() call already returns, so it has no
# .ts file of its own. RESOURCE_PREFIX defaults to "/i18n", which is why AppLocalization.cpp loads
# from ":/i18n/Ikonika_uk.qm" — lupdate/lrelease both re-run automatically on build, merging any
# new or changed source strings into the .ts file without touching translations already filled in.
#
# -no-obsolete drops the entries for strings the source no longer has. Keeping them means a
# translation file that grows with every screen ever deleted, and a "how much is left" count that
# includes work nobody can see.
qt_add_translations(IkonikaApp
    TS_FILES i18n/Ikonika_uk.ts
    LUPDATE_OPTIONS -no-obsolete
)

target_include_directories(IkonikaApp PRIVATE src src/controllers)

qt_add_resources(IkonikaApp "placement"
    PREFIX "/placement"
    BASE placement
    FILES
        placement/en-uk.tsv
        placement/es-en.tsv
        placement/fr-en.tsv
        placement/pt-en.tsv
)

qt_add_resources(IkonikaApp "icons"
    PREFIX "/icons"
    BASE icons
    FILES icons/AppIcon.png
)

qt_add_resources(IkonikaApp "fonts"
    PREFIX "/fonts"
    BASE fonts
    FILES
        fonts/Inter-Regular.ttf
        fonts/Inter-Italic.ttf
        fonts/Inter-Medium.ttf
        fonts/Inter-SemiBold.ttf
        fonts/Inter-Bold.ttf
        fonts/Inter-LICENSE.txt
)

if (WIN32)
    target_sources(IkonikaApp PRIVATE icons/AppIcon.rc)
endif()

if (NOT ANDROID)
    qt_add_resources(IkonikaApp "sounds"
        PREFIX "/sounds"
        BASE android/res/raw
        FILES
            android/res/raw/sfx_incorrect.wav
            android/res/raw/sfx_complete.wav
            android/res/raw/sfx_goal.wav
    )
endif()

# A packaged build opens no console window. It is launched from Explorer, where a black window
# sitting behind the app is noise, and everything it would have printed is in the log file anyway.
# A debug build keeps its console: that is where a developer watches a run.
#
# /SUBSYSTEM:WINDOWS with /ENTRY:mainCRTStartup rather than WIN32_EXECUTABLE, which is one value for
# every configuration and so cannot differ between debug and release. The explicit entry point is
# what keeps main() as the entry point under the windows subsystem, instead of the WinMain that
# subsystem otherwise demands.
target_link_options(IkonikaApp PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<CONFIG:Debug>>>:/SUBSYSTEM:WINDOWS>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<NOT:$<CONFIG:Debug>>>:/ENTRY:mainCRTStartup>
)

# Where a debug build keeps everything it writes and reads locally: its log file, its data
# directory, the dictionaries in data/built, the models fetch-native.ps1 leaves in third_party.
# The build output directory differs between msvc-debug and msvc-release and neither is where a
# developer looks for any of it.
#
# Debug only, because this is an absolute path on whoever's machine ran CMake. A release build
# must not carry it: the directory is not there on a user's machine, and on the machine that
# built it the packaged application would read and hold the checkout instead of what it shipped
# with.
target_compile_definitions(IkonikaApp PRIVATE
    IKONIKA_VERSION="${PROJECT_VERSION}"
    $<$<CONFIG:Debug>:PROJECT_ROOT_DIRECTORY="${CMAKE_SOURCE_DIR}">
)

target_link_libraries(IkonikaApp PRIVATE
    core
    miniz
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::Network
    Qt6::Pdf
)

if (NOT IOS)
    target_link_libraries(IkonikaApp PRIVATE Qt6::NetworkAuth)
endif()

if (IKONIKA_DESKTOP)
    target_link_libraries(IkonikaApp PRIVATE Qt6::PdfQuick)
endif()

if (IKONIKA_DESKTOP AND IKONIKA_ENABLE_WEBENGINE)
    target_link_libraries(IkonikaApp PRIVATE Qt6::WebEngineQuick)
    target_compile_definitions(IkonikaApp PRIVATE IKONIKA_ENABLE_WEBENGINE)
endif()

if (IKONIKA_ENABLE_MACHINE_TRANSLATION AND NOT IOS)
    target_compile_definitions(IkonikaApp PRIVATE IKONIKA_ENABLE_MACHINE_TRANSLATION)
endif()

# OpenSSL ships with no Qt Android kit at all, for licensing reasons, so QSslSocket has no TLS
# backend to load without it. Qt Creator's own Android wizard fetches these same prebuilt
# libraries; fetch-native.ps1 does the same fetch here. See third_party/android-openssl.
if (ANDROID)
    set(IKONIKA_OPENSSL_DIR "${CMAKE_SOURCE_DIR}/third_party/android-openssl/${ANDROID_ABI}")

    set_target_properties(IkonikaApp PROPERTIES
        QT_ANDROID_PACKAGE_NAME "com.goldenphasmid.ikonika"
        QT_ANDROID_APP_NAME "Ikonika"
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE "${IKONIKA_ANDROID_VERSION_CODE}"
        QT_ANDROID_EXTRA_LIBS
            "${IKONIKA_OPENSSL_DIR}/libcrypto_3.so;${IKONIKA_OPENSSL_DIR}/libssl_3.so"
    )
endif()

if (IOS)
    set(IKONIKA_IOS_BUILD_NUMBER "1" CACHE STRING "CFBundleVersion of the iOS build, raised on every upload")

    set_target_properties(IkonikaApp PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/ios/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "Ikonika"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${IKONIKA_IOS_BUILD_NUMBER}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.goldenphasmid.ikonika"
        XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1"
    )

    set_property(TARGET IkonikaApp APPEND PROPERTY RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/ios/PrivacyInfo.xcprivacy")

    set(IKONIKA_IOS_TEAM_ID "" CACHE STRING "Apple developer team that signs the iOS build, empty for an unsigned build")
    set(IKONIKA_IOS_PROFILE "" CACHE STRING "Name of the App Store provisioning profile that signs the iOS build")

    if (IKONIKA_IOS_TEAM_ID AND IKONIKA_IOS_PROFILE)
        set_target_properties(IkonikaApp PROPERTIES
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${IKONIKA_IOS_TEAM_ID}"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Manual"
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Distribution"
            XCODE_ATTRIBUTE_PROVISIONING_PROFILE_SPECIFIER "${IKONIKA_IOS_PROFILE}"
        )
    endif()
endif()

# An APK has no directory beside the executable to read, so the shipped defaults travel as a
# resource instead.
if (NOT IKONIKA_DESKTOP)
    qt_add_resources(IkonikaApp "ShippedDefaults"
        PREFIX "/"
        FILES defaults.ini
    )
endif()

# Both speech libraries resolve their data relative to a path given at run time, and the app passes its
# own directory, so the libraries and the data have to land next to the executable. Android never
# reads this path: it speaks through the platform's own TextToSpeech engine instead, so nothing here
# runs for that target.
if (IKONIKA_DESKTOP)
    add_custom_command(TARGET IkonikaApp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${SHERPA_ONNX_DLLS} "$<TARGET_FILE_DIR:IkonikaApp>"
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_SOURCE_DIR}/scripts/tools/copy-directory-retry.ps1"
                -Source "${SHERPA_ONNX_MODELS_DIR}" -Destination "$<TARGET_FILE_DIR:IkonikaApp>/models"
        COMMENT "Copying the speech libraries and models next to the application"
    )
endif()

if (NOT IKONIKA_DESKTOP)
    return()
endif()

# The translation models, resolved the same way at run time and so needing the same treatment. One
# per language pair, whichever have been fetched: the app picks the directory from the pair being
# learned, so copying each of them is what lets a developer switch language and still translate.
# Absent from a checkout that has not run the fetch script, in which case there is nothing to copy
# and translation reports itself unavailable for that pair.
file(GLOB OPUS_MT_MODEL_DIRS "${CMAKE_SOURCE_DIR}/third_party/models/opus-mt-*")

foreach (OPUS_MT_MODEL_DIR ${OPUS_MT_MODEL_DIRS})
    if (EXISTS "${OPUS_MT_MODEL_DIR}/encoder_model.onnx")
        get_filename_component(OPUS_MT_MODEL_NAME "${OPUS_MT_MODEL_DIR}" NAME)

        add_custom_command(TARGET IkonikaApp POST_BUILD
            COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                    -File "${CMAKE_SOURCE_DIR}/scripts/tools/copy-directory-retry.ps1"
                    -Source "${OPUS_MT_MODEL_DIR}"
                    -Destination "$<TARGET_FILE_DIR:IkonikaApp>/models/${OPUS_MT_MODEL_NAME}"
            COMMENT "Copying the ${OPUS_MT_MODEL_NAME} translation model next to the application"
        )
    endif()
endforeach()

# QSqlDatabase loads its SQLite backend as a plugin found relative to the executable's own
# sqldrivers/ folder. In a developer build nothing supplies it, since a run from Qt Creator
# inherits Qt's own plugin path; running the built exe directly does not. The install below gets
# this from windeployqt, so this only covers the build tree.
add_custom_command(TARGET IkonikaApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:IkonikaApp>/sqldrivers"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Qt6::QSQLiteDriverPlugin>"
            "$<TARGET_FILE_DIR:IkonikaApp>/sqldrivers"
    COMMENT "Copying the SQLite driver plugin next to the application"
)

# Next to the executable in a developer build too, so the file is exercised rather than only
# existing in packages.
add_custom_command(TARGET IkonikaApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/defaults.ini" "$<TARGET_FILE_DIR:IkonikaApp>"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/defaults.ini"
    COMMENT "Copying the default settings next to the application"
)

# ---------------------------------------------------------------------------------------------
# Packaging
#
# Everything the app opens at run time ships beside it, so the result runs from any folder on a
# machine with no Qt, no compiler and no downloads: the Qt runtime and its plugins, the two speech
# libraries with their models, and whichever dictionaries were built.
#
# Deliberately flat. The app resolves the speech models and the bundled dictionaries relative to
# its own directory, so an install layout that moved them would need code to match it.
# ---------------------------------------------------------------------------------------------

install(TARGETS IkonikaApp
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
    LIBRARY DESTINATION .
)

install(FILES ${SHERPA_ONNX_DLLS} DESTINATION .)

# Read by FTSettings for any value the user has not chosen themselves.
install(FILES defaults.ini DESTINATION .)

# The OAuth client for Google sign-in, renamed to the name the app looks for. An installed-app
# client id and secret cannot be kept confidential inside a distributed binary, and Google's own
# desktop flow does not treat them as confidential - the loopback redirect is what secures it. So
# the pair ships, and sync works from the unpacked archive instead of only on the machine that
# built it. data/ stays git-ignored: not committing it is a separate concern from not shipping it.
file(GLOB IKONIKA_OAUTH_CLIENT_FILES
    "${CMAKE_SOURCE_DIR}/data/google-oauth.json"
    "${CMAKE_SOURCE_DIR}/data/client_secret_*.json")

if (IKONIKA_OAUTH_CLIENT_FILES)
    list(GET IKONIKA_OAUTH_CLIENT_FILES 0 IKONIKA_OAUTH_CLIENT_FILE)

    install(FILES "${IKONIKA_OAUTH_CLIENT_FILE}" DESTINATION . RENAME google-oauth.json)
else()
    message(WARNING
        "No Google OAuth client file under data/. The package will build, but sign-in and cloud "
        "sync in it will refuse to start. See docs/google-oauth-setup.md.")
endif()

# Voice models, translation model and dictionaries are downloaded at run time, not packaged.
# See docs/releases.md.

# The Qt runtime, its QML modules and its plugins, including the SQLite driver the database needs
# and the text-to-speech backends.
#
# windeployqt directly rather than qt_generate_deploy_qml_app_script. The generated script calls
# qt6_deploy_qml_imports, which is only defined once Qt's QML deployment helpers have registered
# themselves on the imported Qt6::Core target -- and an imported target belongs to the directory
# that found it, so registering from here never reaches the top-level scope the script runs in.
# windeployqt does the same work and needs nothing to be in scope.
#
# --qmldir points at the sources so the QML imports are scanned rather than guessed. Without it,
# only the modules linked into the executable are deployed and the app starts to a blank window.
find_program(IKONIKA_WINDEPLOYQT windeployqt
    HINTS "${Qt6_DIR}/../../../bin"
    DOC "Qt's Windows deployment tool, used to fill the package with the Qt runtime")

if (WIN32 AND NOT IKONIKA_WINDEPLOYQT)
    message(WARNING
        "windeployqt not found beside Qt6_DIR. The package will contain the app but none of the "
        "Qt runtime, and will not start on a machine without Qt.")
endif()

set(IKONIKA_WINDEPLOYQT_QMLDIR "${CMAKE_CURRENT_SOURCE_DIR}/qml")
set(IKONIKA_WINDEPLOYQT_STAGE_CODE "")

if (NOT IKONIKA_ENABLE_WEBENGINE)
    set(IKONIKA_WINDEPLOYQT_QMLDIR "${CMAKE_CURRENT_BINARY_DIR}/qmldir-scan")
    set(IKONIKA_WINDEPLOYQT_STAGE_CODE "
        file(REMOVE_RECURSE \"${IKONIKA_WINDEPLOYQT_QMLDIR}\")
        file(COPY \"${CMAKE_CURRENT_SOURCE_DIR}/qml/\" DESTINATION \"${IKONIKA_WINDEPLOYQT_QMLDIR}\")
        file(REMOVE
            \"${IKONIKA_WINDEPLOYQT_QMLDIR}/YoutubeScreen.qml\"
            \"${IKONIKA_WINDEPLOYQT_QMLDIR}/YoutubeVideoTile.qml\")
    ")
endif()

if (WIN32 AND IKONIKA_WINDEPLOYQT)
    install(CODE "
        ${IKONIKA_WINDEPLOYQT_STAGE_CODE}

        message(STATUS \"Deploying the Qt runtime with windeployqt\")

        execute_process(
            COMMAND \"${IKONIKA_WINDEPLOYQT}\"
                    --qmldir \"${IKONIKA_WINDEPLOYQT_QMLDIR}\"
                    --\$<LOWER_CASE:\$<CONFIG>>
                    --no-compiler-runtime
                    --no-translations
                    \"\${CMAKE_INSTALL_PREFIX}/IkonikaApp.exe\"
            RESULT_VARIABLE IkonikaDeployResult)

        if (NOT IkonikaDeployResult EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed: \${IkonikaDeployResult}\")
        endif()
    ")
endif()

add_subdirectory(tests)
