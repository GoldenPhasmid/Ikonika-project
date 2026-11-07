#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

bool IsMobileLayoutEnabled();

/**
 * The non-colour half of the design tokens: sizes, spacing and font sizes taken from the v3
 * wireframes (design/v3/wireframe.css). AppTheme owns colour; this owns everything else, so a
 * screen never hard-codes a pixel value that the wireframe already named.
 *
 * All CONSTANT: these are the design's fixed measurements, not user preferences. Anything the
 * user can change belongs in FTSettings, not here.
 */
class AppMetrics : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // ---- Shell ----
    Q_PROPERTY(int railWidth READ GetRailWidth CONSTANT)
    Q_PROPERTY(int railWidthCollapsed READ GetRailWidthCollapsed CONSTANT)
    Q_PROPERTY(int topBarHeight READ GetTopBarHeight CONSTANT)
    Q_PROPERTY(int headerIconTapWidth READ GetHeaderIconTapWidth CONSTANT)
    Q_PROPERTY(int headerChevronSize READ GetHeaderChevronSize CONSTANT)
    Q_PROPERTY(int headerActionIconSize READ GetHeaderActionIconSize CONSTANT)
    Q_PROPERTY(int exerciseMuteIconSize READ GetExerciseMuteIconSize CONSTANT)
    Q_PROPERTY(int exerciseActionIconTapWidth READ GetExerciseActionIconTapWidth CONSTANT)

    // ---- Spacing ----
    Q_PROPERTY(int contentPadding READ GetContentPadding CONSTANT)
    Q_PROPERTY(int gapTight READ GetGapTight CONSTANT)
    Q_PROPERTY(int gapSmall READ GetGapSmall CONSTANT)
    /** Two steps either side of gapMedium. */
    Q_PROPERTY(int gapCompact READ GetGapCompact CONSTANT)
    Q_PROPERTY(int gapWide READ GetGapWide CONSTANT)

    Q_PROPERTY(int gapMedium READ GetGapMedium CONSTANT)
    Q_PROPERTY(int gapLarge READ GetGapLarge CONSTANT)

    // ---- Controls ----
    Q_PROPERTY(int buttonHeight READ GetButtonHeight CONSTANT)
    Q_PROPERTY(int buttonHeightLarge READ GetButtonHeightLarge CONSTANT)
    Q_PROPERTY(int searchWidth READ GetSearchWidth CONSTANT)

    /** The dictionary's own search field, which is the whole point of that screen rather than a
        control in a bar. Capped: a search term is one word, and a field stretched across a wide
        window reads as somewhere to write a paragraph. */
    Q_PROPERTY(int dictionarySearchWidth READ GetDictionarySearchWidth CONSTANT)

    /** How long a word or translation may be typed, matching what the database stores. Not a
        preference: it bounds what an exercise can lay out on one line. */
    Q_PROPERTY(int maximumCardFieldLength READ GetMaximumCardFieldLength CONSTANT)
    Q_PROPERTY(int dictionarySearchHeight READ GetDictionarySearchHeight CONSTANT)

    /** A comfortable column for a book's text, so a full-width window does not stretch the reader
        across the screen. Prose read as a full-width wall is hard to follow. */
    Q_PROPERTY(int readerTextWidth READ GetReaderTextWidth CONSTANT)

    /* The dictionary is read rather than operated, so its text runs a fifth larger than the rest of
       the app. Separate tokens rather than raising the shared ones, which would resize every
       screen. */
    Q_PROPERTY(int dictionaryFontSmall READ GetDictionaryFontSmall CONSTANT)
    Q_PROPERTY(int dictionaryFontBody READ GetDictionaryFontBody CONSTANT)
    Q_PROPERTY(int dictionaryFontMeta READ GetDictionaryFontMeta CONSTANT)
    Q_PROPERTY(int dictionaryFontSubtitle READ GetDictionaryFontSubtitle CONSTANT)
    Q_PROPERTY(int dictionaryFontTitle READ GetDictionaryFontTitle CONSTANT)

    /** The headword on a word's own entry, larger than the one a search result shows. */
    Q_PROPERTY(int dictionaryFontHeadline READ GetDictionaryFontHeadline CONSTANT)
    Q_PROPERTY(int borderWidth READ GetBorderWidth CONSTANT)

    Q_PROPERTY(int cardRadius READ GetCardRadius CONSTANT)

    /** The inset a card holds its content at. */
    Q_PROPERTY(int cardPadding READ GetCardPadding CONSTANT)

    /** The two corner radii the phone mockups use: small for badges and chips, medium for
        cards, tiles and buttons. */
    Q_PROPERTY(int radiusSmall READ GetRadiusSmall CONSTANT)
    Q_PROPERTY(int radiusMedium READ GetRadiusMedium CONSTANT)

    /** Rounder than a card: the dictionary's own panels. */
    Q_PROPERTY(int radiusLarge READ GetRadiusLarge CONSTANT)

    /** The face every screen asks for by name. */
    Q_PROPERTY(QString fontFamily READ GetFontFamily CONSTANT)

    // ---- Font sizes ----
    Q_PROPERTY(int fontMeta READ GetFontMeta CONSTANT)
    /** Below meta: a badge, a chip label, the smallest thing a screen still asks to be read. */
    Q_PROPERTY(int fontMicro READ GetFontMicro CONSTANT)

    Q_PROPERTY(int fontSmall READ GetFontSmall CONSTANT)
    Q_PROPERTY(int fontBody READ GetFontBody CONSTANT)
    Q_PROPERTY(int fontLabel READ GetFontLabel CONSTANT)
    /** A sentence that leads a screen instead of labelling part of one: the definition a typed
        answer is given for, an empty-state message. Larger than body, short of a headline. */
    Q_PROPERTY(int fontLead READ GetFontLead CONSTANT)

    Q_PROPERTY(int fontSubtitle READ GetFontSubtitle CONSTANT)
    Q_PROPERTY(int fontTitle READ GetFontTitle CONSTANT)
    Q_PROPERTY(int fontHeadline READ GetFontHeadline CONSTANT)

    /** Letter spacing for the uppercase mono labels (kicker, meta, breadcrumb). */
    Q_PROPERTY(qreal trackingWide READ GetTrackingWide CONSTANT)

    // ---- Exercises ----

    /** The card every exercise shows, and the column its controls line up in. Fixed rather than
        proportional, so a word does not change size between one exercise and the next. */
    Q_PROPERTY(int sessionCardWidth READ GetSessionCardWidth CONSTANT)
    Q_PROPERTY(int sessionCardHeight READ GetSessionCardHeight CONSTANT)

    Q_PROPERTY(int sessionWordSize READ GetSessionWordSize CONSTANT)
    Q_PROPERTY(int sessionTranslationSize READ GetSessionTranslationSize CONSTANT)
    /** The figure a stat box reports, larger than any body text and smaller than a headline. */
    Q_PROPERTY(int statValueSize READ GetStatValueSize CONSTANT)

    Q_PROPERTY(int sessionAnswerSize READ GetSessionAnswerSize CONSTANT)
    Q_PROPERTY(int sessionDefinitionSize READ GetSessionDefinitionSize CONSTANT)
    Q_PROPERTY(int sessionOptionSize READ GetSessionOptionSize CONSTANT)
    Q_PROPERTY(int sessionInputSize READ GetSessionInputSize CONSTANT)
    Q_PROPERTY(int masteryDotSize READ GetMasteryDotSize CONSTANT)

    Q_PROPERTY(int progressBarHeight READ GetProgressBarHeight CONSTANT)
    Q_PROPERTY(int statBoxHeight READ GetStatBoxHeight CONSTANT)
    Q_PROPERTY(int previewFieldHeight READ GetPreviewFieldHeight CONSTANT)
    Q_PROPERTY(int chipListSpacing READ GetChipListSpacing CONSTANT)
    Q_PROPERTY(int optionCellMinimumHeight READ GetOptionCellMinimumHeight CONSTANT)
    Q_PROPERTY(int playButtonSize READ GetPlayButtonSize CONSTANT)
    Q_PROPERTY(int playButtonGlyphSize READ GetPlayButtonGlyphSize CONSTANT)

    Q_PROPERTY(int exerciseAdvanceDelayCorrect READ GetExerciseAdvanceDelayCorrect CONSTANT)

    Q_PROPERTY(int motionFast READ GetMotionFast NOTIFY transitionsEnabledChanged)
    Q_PROPERTY(int motionBase READ GetMotionBase NOTIFY transitionsEnabledChanged)
    Q_PROPERTY(int motionSlow READ GetMotionSlow NOTIFY transitionsEnabledChanged)
    Q_PROPERTY(int motionTransition READ GetMotionTransition NOTIFY transitionsEnabledChanged)
    Q_PROPERTY(int motionReveal READ GetMotionReveal NOTIFY transitionsEnabledChanged)
    Q_PROPERTY(int motionMorph READ GetMotionMorph NOTIFY transitionsEnabledChanged)
    Q_PROPERTY(int motionPage READ GetMotionPage NOTIFY transitionsEnabledChanged)
    Q_PROPERTY(bool transitionsEnabled READ AreTransitionsEnabled WRITE SetTransitionsEnabled NOTIFY transitionsEnabledChanged)
    Q_PROPERTY(int motionEasing READ GetMotionEasing CONSTANT)
    Q_PROPERTY(int motionTravel READ GetMotionTravel CONSTANT)
    Q_PROPERTY(int motionShake READ GetMotionShake CONSTANT)
    Q_PROPERTY(qreal motionWiggle READ GetMotionWiggle CONSTANT)

    // ---- Desktop layout ----

    Q_PROPERTY(int windowWidth READ GetWindowWidth CONSTANT)
    Q_PROPERTY(int windowHeight READ GetWindowHeight CONSTANT)
    Q_PROPERTY(int windowMinimumWidth READ GetWindowMinimumWidth CONSTANT)
    Q_PROPERTY(int windowMinimumHeight READ GetWindowMinimumHeight CONSTANT)

    Q_PROPERTY(int contentWidthWide READ GetContentWidthWide CONSTANT)
    Q_PROPERTY(int contentWidthNarrow READ GetContentWidthNarrow CONSTANT)

    Q_PROPERTY(int listPaneWidth READ GetListPaneWidth CONSTANT)

    Q_PROPERTY(int navItemHeight READ GetNavItemHeight CONSTANT)
    Q_PROPERTY(int iconSize READ GetIconSize CONSTANT)
    Q_PROPERTY(int iconSizeSmall READ GetIconSizeSmall CONSTANT)
    Q_PROPERTY(int listRowHeight READ GetListRowHeight CONSTANT)
    Q_PROPERTY(int chipHeight READ GetChipHeight CONSTANT)
    Q_PROPERTY(int badgeHeight READ GetBadgeHeight CONSTANT)

    Q_PROPERTY(int progressRingSize READ GetProgressRingSize CONSTANT)
    Q_PROPERTY(int progressRingStroke READ GetProgressRingStroke CONSTANT)

    Q_PROPERTY(int dialogWidth READ GetDialogWidth CONSTANT)
    Q_PROPERTY(int popoverWidth READ GetPopoverWidth CONSTANT)
    Q_PROPERTY(int textAreaHeight READ GetTextAreaHeight CONSTANT)

    Q_PROPERTY(int heroFigureSize READ GetHeroFigureSize CONSTANT)

    Q_PROPERTY(int sessionTileHeight READ GetSessionTileHeight CONSTANT)
    Q_PROPERTY(int sessionTileHeightCompact READ GetSessionTileHeightCompact CONSTANT)

    Q_PROPERTY(int dayTileHeight READ GetDayTileHeight CONSTANT)

    Q_PROPERTY(int goalRingSize READ GetGoalRingSize CONSTANT)
    Q_PROPERTY(int goalRingStroke READ GetGoalRingStroke CONSTANT)
    Q_PROPERTY(int goalRingGap READ GetGoalRingGap CONSTANT)
    Q_PROPERTY(int welcomeRingSize READ GetWelcomeRingSize CONSTANT)
    Q_PROPERTY(int welcomeGreetingSize READ GetWelcomeGreetingSize CONSTANT)
    Q_PROPERTY(int pathNodeSize READ GetPathNodeSize CONSTANT)
    Q_PROPERTY(int pathLineWidth READ GetPathLineWidth CONSTANT)
    Q_PROPERTY(int todayRingColumnWidth READ GetTodayRingColumnWidth CONSTANT)
    Q_PROPERTY(int todayRingOverhang READ GetTodayRingOverhang CONSTANT)
    Q_PROPERTY(int todayBracketOffset READ GetTodayBracketOffset CONSTANT)
    Q_PROPERTY(int todayPracticeGap READ GetTodayPracticeGap CONSTANT)
    Q_PROPERTY(int weekDayCellSize READ GetWeekDayCellSize CONSTANT)
    Q_PROPERTY(int weekRingSize READ GetWeekRingSize CONSTANT)
    Q_PROPERTY(int weekRingStroke READ GetWeekRingStroke CONSTANT)
    Q_PROPERTY(int weekRingGap READ GetWeekRingGap CONSTANT)
    Q_PROPERTY(int weekFutureDotSize READ GetWeekFutureDotSize CONSTANT)

    Q_PROPERTY(int focusRingWidth READ GetFocusRingWidth CONSTANT)
    Q_PROPERTY(int scrollBarWidth READ GetScrollBarWidth CONSTANT)

    Q_PROPERTY(int keyHintSize READ GetKeyHintSize CONSTANT)
    Q_PROPERTY(int sliderTickSpacing READ GetSliderTickSpacing CONSTANT)
    Q_PROPERTY(int toastWidth READ GetToastWidth CONSTANT)
    Q_PROPERTY(int settingsControlWidth READ GetSettingsControlWidth CONSTANT)
    Q_PROPERTY(int rowProgressWidth READ GetRowProgressWidth CONSTANT)
    Q_PROPERTY(int categoryPaneWidth READ GetCategoryPaneWidth CONSTANT)
    Q_PROPERTY(int coverTileMinimumWidth READ GetCoverTileMinimumWidth CONSTANT)
    Q_PROPERTY(qreal readerTileAspect READ GetReaderTileAspect CONSTANT)
    Q_PROPERTY(qreal readerPageAspect READ GetReaderPageAspect CONSTANT)
    Q_PROPERTY(int switchGlyphSize READ GetSwitchGlyphSize CONSTANT)
    Q_PROPERTY(int readerOpenIconSize READ GetReaderOpenIconSize CONSTANT)
    Q_PROPERTY(int readerOpenPlusSize READ GetReaderOpenPlusSize CONSTANT)
    Q_PROPERTY(int coverRadius READ GetCoverRadius CONSTANT)
    Q_PROPERTY(int coverProgressHeight READ GetCoverProgressHeight CONSTANT)
    Q_PROPERTY(qreal coverHoverScale READ GetCoverHoverScale CONSTANT)
    Q_PROPERTY(int bookRadius READ GetBookRadius CONSTANT)
    Q_PROPERTY(int bookTitleSize READ GetBookTitleSize CONSTANT)
    Q_PROPERTY(qreal shelfRecedeScale READ GetShelfRecedeScale CONSTANT)
    Q_PROPERTY(qreal coverStripeBottomStart READ GetCoverStripeBottomStart CONSTANT)
    Q_PROPERTY(qreal coverStripeBottomEnd READ GetCoverStripeBottomEnd CONSTANT)
    Q_PROPERTY(qreal coverStripeRightStart READ GetCoverStripeRightStart CONSTANT)
    Q_PROPERTY(qreal coverStripeRightEnd READ GetCoverStripeRightEnd CONSTANT)
    Q_PROPERTY(qreal sessionTileBandWidth READ GetSessionTileBandWidth CONSTANT)
    Q_PROPERTY(qreal sessionTileBandTop READ GetSessionTileBandTop CONSTANT)
    Q_PROPERTY(qreal todayTopFill READ GetTodayTopFill CONSTANT)
    Q_PROPERTY(qreal todaySideShare READ GetTodaySideShare CONSTANT)
    Q_PROPERTY(int readerRowHeight READ GetReaderRowHeight CONSTANT)
    Q_PROPERTY(int readerRowCoverWidth READ GetReaderRowCoverWidth CONSTANT)
    Q_PROPERTY(int readerContentsWidth READ GetReaderContentsWidth CONSTANT)
    Q_PROPERTY(int coverGridWidth READ GetCoverGridWidth CONSTANT)
    Q_PROPERTY(int deckIconSize READ GetDeckIconSize CONSTANT)
    Q_PROPERTY(int categoryDotSize READ GetCategoryDotSize CONSTANT)
    Q_PROPERTY(int avatarSize READ GetAvatarSize CONSTANT)
    Q_PROPERTY(int toggleChipHeight READ GetToggleChipHeight CONSTANT)
    Q_PROPERTY(int tooltipDelay READ GetTooltipDelay CONSTANT)
    Q_PROPERTY(int toastDuration READ GetToastDuration CONSTANT)
    Q_PROPERTY(int menuWidth READ GetMenuWidth CONSTANT)
    Q_PROPERTY(int popoverListHeight READ GetPopoverListHeight CONSTANT)
    Q_PROPERTY(int dialogListHeight READ GetDialogListHeight CONSTANT)

    Q_PROPERTY(int twoColumnMinimumWidth READ GetTwoColumnMinimumWidth CONSTANT)

    Q_PROPERTY(int listMarkerWidth READ GetListMarkerWidth CONSTANT)
    Q_PROPERTY(int completeBadgeSize READ GetCompleteBadgeSize CONSTANT)
    Q_PROPERTY(int completeGlyphSize READ GetCompleteGlyphSize CONSTANT)
    Q_PROPERTY(qreal iconStrokeBold READ GetIconStrokeBold CONSTANT)
    Q_PROPERTY(qreal dashStrokeWidth READ GetDashStrokeWidth CONSTANT)
    Q_PROPERTY(qreal dashLength READ GetDashLength CONSTANT)

    Q_PROPERTY(int selectionBarWidth READ GetSelectionBarWidth CONSTANT)
    Q_PROPERTY(int selectionBarHeight READ GetSelectionBarHeight CONSTANT)
    Q_PROPERTY(int tabHeight READ GetTabHeight CONSTANT)
    Q_PROPERTY(int fontTab READ GetFontTab CONSTANT)
    Q_PROPERTY(int tabIndicatorHeight READ GetTabIndicatorHeight CONSTANT)
    Q_PROPERTY(int sessionProgressHeight READ GetSessionProgressHeight CONSTANT)
    Q_PROPERTY(int exerciseAnswerHeight READ GetExerciseAnswerHeight CONSTANT)
    Q_PROPERTY(int sentenceMinimumSize READ GetSentenceMinimumSize CONSTANT)
    Q_PROPERTY(int sentenceNativeSize READ GetSentenceNativeSize CONSTANT)
    Q_PROPERTY(int sentenceNativeMinimumSize READ GetSentenceNativeMinimumSize CONSTANT)
    Q_PROPERTY(int tileRowLimit READ GetTileRowLimit CONSTANT)
    Q_PROPERTY(int exercisePromptHeight READ GetExercisePromptHeight CONSTANT)
    Q_PROPERTY(int exercisePromptHeightCompact READ GetExercisePromptHeightCompact CONSTANT)
    Q_PROPERTY(int answerLineCount READ GetAnswerLineCount CONSTANT)
    Q_PROPERTY(int revealLineCount READ GetRevealLineCount CONSTANT)
    Q_PROPERTY(int definitionLineCount READ GetDefinitionLineCount CONSTANT)
    Q_PROPERTY(int answerSlotHeight READ GetAnswerSlotHeight CONSTANT)
    Q_PROPERTY(int optionCellHeight READ GetOptionCellHeight CONSTANT)
    Q_PROPERTY(int matchCellHeight READ GetMatchCellHeight CONSTANT)
    Q_PROPERTY(int matchCellHeightCompact READ GetMatchCellHeightCompact CONSTANT)
    Q_PROPERTY(int formCellHeight READ GetFormCellHeight CONSTANT)
    Q_PROPERTY(int stageSegmentWidth READ GetStageSegmentWidth CONSTANT)
    Q_PROPERTY(qreal triagePaneShare READ GetTriagePaneShare CONSTANT)
    Q_PROPERTY(int actionButtonWidth READ GetActionButtonWidth CONSTANT)
    Q_PROPERTY(int triageWordSize READ GetTriageWordSize CONSTANT)
    Q_PROPERTY(int rankTrackWidth READ GetRankTrackWidth CONSTANT)
    Q_PROPERTY(int rankHandleSize READ GetRankHandleSize CONSTANT)
    Q_PROPERTY(int waveformButtonWidth READ GetWaveformButtonWidth CONSTANT)
    Q_PROPERTY(int waveformButtonHeight READ GetWaveformButtonHeight CONSTANT)
    Q_PROPERTY(int waveformBarWidth READ GetWaveformBarWidth CONSTANT)
    Q_PROPERTY(int waveformBarMinimumHeight READ GetWaveformBarMinimumHeight CONSTANT)
    Q_PROPERTY(int waveformCycle READ GetWaveformCycle CONSTANT)
    Q_PROPERTY(int explainPaneWidth READ GetExplainPaneWidth CONSTANT)
    Q_PROPERTY(int readerSettingsWidth READ GetReaderSettingsWidth CONSTANT)

    Q_PROPERTY(int readerPageMargin READ GetReaderPageMargin CONSTANT)
    Q_PROPERTY(int readerPageWidth READ GetReaderPageWidth CONSTANT)
    Q_PROPERTY(int readerWheelStep READ GetReaderWheelStep CONSTANT)
    Q_PROPERTY(int readerAutoscrollDeadZone READ GetReaderAutoscrollDeadZone CONSTANT)
    Q_PROPERTY(qreal readerAutoscrollSpeed READ GetReaderAutoscrollSpeed CONSTANT)
    Q_PROPERTY(int readerAutoscrollAnchorSize READ GetReaderAutoscrollAnchorSize CONSTANT)
    Q_PROPERTY(qreal readerImageHeightShare READ GetReaderImageHeightShare CONSTANT)
    Q_PROPERTY(qreal readerEndSpaceShare READ GetReaderEndSpaceShare CONSTANT)
    Q_PROPERTY(int readerLookupWordLimit READ GetReaderLookupWordLimit CONSTANT)
    Q_PROPERTY(qreal readerTranslationWidthShare READ GetReaderTranslationWidthShare CONSTANT)
    Q_PROPERTY(int readerCropPadding READ GetReaderCropPadding CONSTANT)
    Q_PROPERTY(int readerPositionWidth READ GetReaderPositionWidth CONSTANT)
    Q_PROPERTY(int readerBarHeight READ GetReaderBarHeight CONSTANT)
    Q_PROPERTY(int readerSaveDelay READ GetReaderSaveDelay CONSTANT)
    Q_PROPERTY(qreal readerZoomStep READ GetReaderZoomStep CONSTANT)
    Q_PROPERTY(qreal readerMinimumZoom READ GetReaderMinimumZoom CONSTANT)
    Q_PROPERTY(qreal readerMaximumZoom READ GetReaderMaximumZoom CONSTANT)
    Q_PROPERTY(int brandTitleSize READ GetBrandTitleSize CONSTANT)

    Q_PROPERTY(bool mobileLayout READ IsMobileLayout CONSTANT)

public:
    explicit AppMetrics(QObject* Parent = nullptr);

    int GetRailWidth() const;
    int GetRailWidthCollapsed() const;
    int GetTopBarHeight() const;
    int GetHeaderIconTapWidth() const;
    int GetHeaderChevronSize() const;
    int GetHeaderActionIconSize() const;
    int GetExerciseMuteIconSize() const;
    int GetExerciseActionIconTapWidth() const;

    int GetContentPadding() const;
    int GetGapTight() const;
    int GetGapSmall() const;
    int GetGapCompact() const;
    int GetGapWide() const;
    int GetGapMedium() const;
    int GetGapLarge() const;

    int GetButtonHeight() const;
    int GetButtonHeightLarge() const;
    int GetSearchWidth() const;
    int GetDictionarySearchWidth() const;
    int GetMaximumCardFieldLength() const;
    int GetDictionarySearchHeight() const;
    int GetReaderTextWidth() const;

    int GetDictionaryFontSmall() const;
    int GetDictionaryFontBody() const;
    int GetDictionaryFontMeta() const;
    int GetDictionaryFontSubtitle() const;
    int GetDictionaryFontTitle() const;
    int GetDictionaryFontHeadline() const;
    int GetBorderWidth() const;
    int GetCardRadius() const;
    int GetCardPadding() const;
    int GetRadiusSmall() const;
    int GetRadiusMedium() const;
    int GetRadiusLarge() const;

    QString GetFontFamily() const;

    int GetFontMeta() const;
    int GetFontMicro() const;
    int GetFontSmall() const;
    int GetFontBody() const;
    int GetFontLabel() const;
    int GetFontLead() const;
    int GetFontSubtitle() const;
    int GetFontTitle() const;
    int GetFontHeadline() const;

    qreal GetTrackingWide() const;

    int GetSessionCardWidth() const;
    int GetSessionCardHeight() const;
    int GetSessionWordSize() const;
    int GetSessionTranslationSize() const;
    int GetStatValueSize() const;
    int GetSessionAnswerSize() const;
    int GetSessionDefinitionSize() const;
    int GetSessionOptionSize() const;
    int GetSessionInputSize() const;
    int GetMasteryDotSize() const;
    int GetProgressBarHeight() const;
    int GetStatBoxHeight() const;
    int GetPreviewFieldHeight() const;
    int GetChipListSpacing() const;
    int GetOptionCellMinimumHeight() const;
    int GetPlayButtonSize() const;
    int GetPlayButtonGlyphSize() const;

    int GetExerciseAdvanceDelayCorrect() const;

    int GetMotion(int Milliseconds) const;
    int GetMotionFast() const;
    int GetMotionBase() const;
    int GetMotionSlow() const;
    int GetMotionTransition() const;
    int GetMotionReveal() const;
    int GetMotionMorph() const;
    int GetMotionPage() const;
    bool AreTransitionsEnabled() const;
    void SetTransitionsEnabled(bool bValue);
    int GetMotionEasing() const;
    int GetMotionTravel() const;
    int GetMotionShake() const;
    qreal GetMotionWiggle() const;


    int GetWindowWidth() const;
    int GetWindowHeight() const;
    int GetWindowMinimumWidth() const;
    int GetWindowMinimumHeight() const;
    int GetContentWidthWide() const;
    int GetContentWidthNarrow() const;
    int GetListPaneWidth() const;
    int GetNavItemHeight() const;
    int GetIconSize() const;
    int GetIconSizeSmall() const;
    int GetListRowHeight() const;
    int GetChipHeight() const;
    int GetBadgeHeight() const;
    int GetProgressRingSize() const;
    int GetProgressRingStroke() const;
    int GetDialogWidth() const;
    int GetPopoverWidth() const;
    int GetTextAreaHeight() const;
    int GetHeroFigureSize() const;
    int GetSessionTileHeight() const;
    int GetSessionTileHeightCompact() const;
    int GetDayTileHeight() const;
    int GetGoalRingSize() const;
    int GetGoalRingStroke() const;
    int GetGoalRingGap() const;
    int GetWelcomeRingSize() const;
    int GetWelcomeGreetingSize() const;
    int GetPathNodeSize() const;
    int GetPathLineWidth() const;
    int GetTodayRingColumnWidth() const;
    int GetTodayRingOverhang() const;
    int GetTodayBracketOffset() const;
    int GetTodayPracticeGap() const;
    int GetWeekDayCellSize() const;
    int GetWeekRingSize() const;
    int GetWeekRingStroke() const;
    int GetWeekRingGap() const;
    int GetWeekFutureDotSize() const;
    int GetFocusRingWidth() const;
    int GetScrollBarWidth() const;
    int GetKeyHintSize() const;
    int GetSliderTickSpacing() const;
    int GetToastWidth() const;
    int GetSettingsControlWidth() const;
    int GetRowProgressWidth() const;
    int GetCategoryPaneWidth() const;
    int GetCoverTileMinimumWidth() const;
    qreal GetReaderTileAspect() const;
    qreal GetReaderPageAspect() const;
    int GetSwitchGlyphSize() const;
    int GetReaderOpenIconSize() const;
    int GetReaderOpenPlusSize() const;
    int GetCoverRadius() const;
    int GetCoverProgressHeight() const;
    qreal GetCoverHoverScale() const;
    int GetBookRadius() const;
    int GetBookTitleSize() const;
    qreal GetShelfRecedeScale() const;
    qreal GetCoverStripeBottomStart() const;
    qreal GetCoverStripeBottomEnd() const;
    qreal GetCoverStripeRightStart() const;
    qreal GetCoverStripeRightEnd() const;
    qreal GetSessionTileBandWidth() const;
    qreal GetSessionTileBandTop() const;
    qreal GetTodayTopFill() const;
    qreal GetTodaySideShare() const;
    int GetReaderRowHeight() const;
    int GetReaderRowCoverWidth() const;
    int GetReaderContentsWidth() const;
    int GetCoverGridWidth() const;
    int GetDeckIconSize() const;
    int GetCategoryDotSize() const;
    int GetAvatarSize() const;
    int GetToggleChipHeight() const;
    int GetTooltipDelay() const;
    int GetToastDuration() const;
    int GetMenuWidth() const;
    int GetPopoverListHeight() const;
    int GetDialogListHeight() const;
    int GetTwoColumnMinimumWidth() const;
    int GetListMarkerWidth() const;
    int GetCompleteBadgeSize() const;
    int GetCompleteGlyphSize() const;
    qreal GetIconStrokeBold() const;
    qreal GetDashStrokeWidth() const;
    qreal GetDashLength() const;
    int GetSelectionBarWidth() const;
    int GetSelectionBarHeight() const;
    int GetTabHeight() const;
    int GetFontTab() const;
    int GetTabIndicatorHeight() const;
    int GetSessionProgressHeight() const;
    int GetExerciseAnswerHeight() const;
    int GetSentenceMinimumSize() const;
    int GetSentenceNativeSize() const;
    int GetSentenceNativeMinimumSize() const;
    int GetTileRowLimit() const;
    int GetExercisePromptHeight() const;
    int GetExercisePromptHeightCompact() const;
    int GetAnswerLineCount() const;
    int GetRevealLineCount() const;
    int GetDefinitionLineCount() const;
    int GetAnswerSlotHeight() const;
    int GetOptionCellHeight() const;
    int GetMatchCellHeight() const;
    int GetMatchCellHeightCompact() const;
    int GetFormCellHeight() const;
    int GetStageSegmentWidth() const;
    qreal GetTriagePaneShare() const;
    int GetActionButtonWidth() const;
    int GetTriageWordSize() const;
    int GetRankTrackWidth() const;
    int GetRankHandleSize() const;
    int GetWaveformButtonWidth() const;
    int GetWaveformButtonHeight() const;
    int GetWaveformBarWidth() const;
    int GetWaveformBarMinimumHeight() const;
    int GetWaveformCycle() const;
    int GetExplainPaneWidth() const;
    int GetReaderSettingsWidth() const;
    int GetReaderPageMargin() const;
    int GetReaderPageWidth() const;
    int GetReaderWheelStep() const;
    int GetReaderAutoscrollDeadZone() const;
    int GetReaderLookupWordLimit() const;
    qreal GetReaderTranslationWidthShare() const;
    qreal GetReaderAutoscrollSpeed() const;
    int GetReaderAutoscrollAnchorSize() const;
    qreal GetReaderImageHeightShare() const;
    qreal GetReaderEndSpaceShare() const;
    int GetReaderCropPadding() const;
    int GetReaderPositionWidth() const;
    int GetReaderBarHeight() const;
    int GetReaderSaveDelay() const;
    qreal GetReaderZoomStep() const;
    qreal GetReaderMinimumZoom() const;
    qreal GetReaderMaximumZoom() const;
    int GetBrandTitleSize() const;

    bool IsMobileLayout() const;

signals:
    void transitionsEnabledChanged();
};
