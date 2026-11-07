#include "AppMetrics.h"

#include "core/FTCard.h"
#include "core/FTCardStore.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QCoreApplication>
#include <QEasingCurve>
#include <QSettings>
#include <QtCore/qglobal.h>

#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>
#endif

namespace
{
    const QString TransitionsKey = QStringLiteral("motion/transitions");

    bool ReadMotionReduced()
    {
#if defined(Q_OS_ANDROID)
        QJniObject Context = QNativeInterface::QAndroidApplication::context();
        QJniObject Resolver = Context.callObjectMethod(
            "getContentResolver", "()Landroid/content/ContentResolver;");
        QJniObject Name = QJniObject::fromString(QStringLiteral("animator_duration_scale"));

        const jfloat Scale = QJniObject::callStaticMethod<jfloat>(
            "android/provider/Settings$Global", "getFloat",
            "(Landroid/content/ContentResolver;Ljava/lang/String;F)F",
            Resolver.object<jobject>(), Name.object<jstring>(), jfloat{1.0f});

        qCInfo(LogApp) << "system animator duration scale:" << Scale;

        return Scale <= 0.0f;
#else
        return false;
#endif
    }

    bool IsMotionReduced()
    {
        static const bool bReduced = ReadMotionReduced();

        return bReduced;
    }
}

// Every measurement below answers 0 on a phone until a ported screen has pinned it against the
// mockup it came from (tools/mobile-preview). A zero is loud: the screen visibly collapses, which
// is the point. A desktop number silently inherited by a phone screen is the mistake this exists
// to prevent, and it looks plausible enough to ship. Only borderWidth, contentPadding, the two
// radii and fontFamily carry a phone value already, because the mockups use one each throughout.

AppMetrics::AppMetrics(QObject* Parent)
    : QObject(Parent)
{
}

bool IsMobileLayoutEnabled()
{
#if !defined(IKONIKA_DESKTOP)
    return true;
#else
    return QCoreApplication::arguments().contains(QStringLiteral("-mobile"));
#endif
}

bool AppMetrics::IsMobileLayout() const
{
    return IsMobileLayoutEnabled();
}

int AppMetrics::GetRailWidth() const
{
    return 232;
}

int AppMetrics::GetRailWidthCollapsed() const
{
    return 56;
}

int AppMetrics::GetTopBarHeight() const
{
    return IsMobileLayout() ? 56 : 64;
}

int AppMetrics::GetHeaderIconTapWidth() const
{
    return IsMobileLayout() ? 44 : 36;
}

int AppMetrics::GetHeaderChevronSize() const
{
    return 22;
}

int AppMetrics::GetHeaderActionIconSize() const
{
    return 18;
}

int AppMetrics::GetExerciseMuteIconSize() const
{
    return 16;
}

int AppMetrics::GetExerciseActionIconTapWidth() const
{
    return IsMobileLayout() ? 40 : 36;
}

int AppMetrics::GetContentPadding() const
{
    return IsMobileLayout() ? 16 : 32;
}

int AppMetrics::GetGapTight() const
{
    return 4;
}

int AppMetrics::GetGapSmall() const
{
    return 8;
}

int AppMetrics::GetGapCompact() const
{
    return 12;
}

int AppMetrics::GetGapWide() const
{
    return 20;
}

int AppMetrics::GetGapMedium() const
{
    return 16;
}

int AppMetrics::GetGapLarge() const
{
    return 24;
}

int AppMetrics::GetButtonHeight() const
{
    return IsMobileLayout() ? 0 : 36;
}

int AppMetrics::GetButtonHeightLarge() const
{
    return IsMobileLayout() ? 48 : 44;
}

int AppMetrics::GetSearchWidth() const
{
    return IsMobileLayout() ? 0 : 280;
}

int AppMetrics::GetDictionarySearchWidth() const
{
    return IsMobileLayout() ? 0 : 560;
}

int AppMetrics::GetMaximumCardFieldLength() const
{
    return MaximumCardFieldLength;
}


int AppMetrics::GetDictionarySearchHeight() const
{
    return IsMobileLayout() ? 0 : 40;
}

int AppMetrics::GetReaderTextWidth() const
{
    // The dictionary's reading column, which is the width the wireframes give to a single column
    // of running text.
    return 560;
}

int AppMetrics::GetDictionaryFontSmall() const
{
    return 13;
}

int AppMetrics::GetDictionaryFontBody() const
{
    return IsMobileLayout() ? 14 : 16;
}

int AppMetrics::GetDictionaryFontMeta() const
{
    return 12;
}

int AppMetrics::GetDictionaryFontSubtitle() const
{
    return IsMobileLayout() ? 0 : 20;
}

int AppMetrics::GetDictionaryFontHeadline() const
{
    return IsMobileLayout() ? 32 : 36;
}

int AppMetrics::GetDictionaryFontTitle() const
{
    return 24;
}

int AppMetrics::GetBorderWidth() const
{
    return 1;
}

int AppMetrics::GetCardRadius() const
{
    // Square on desktop by wireframe, and unpinned on a phone: the mockups round by role rather
    // than by "card", so a ported screen asks for radiusSmall or radiusMedium instead.
    return 0;
}

int AppMetrics::GetCardPadding() const
{
    return IsMobileLayout() ? 20 : 24;
}

int AppMetrics::GetRadiusSmall() const
{
    return 6;
}

int AppMetrics::GetRadiusLarge() const
{
    return IsMobileLayout() ? 20 : 12;
}

int AppMetrics::GetRadiusMedium() const
{
    return 8;
}

QString AppMetrics::GetFontFamily() const
{
    return QStringLiteral("Inter");
}

int AppMetrics::GetFontMeta() const
{
    return 12;
}

int AppMetrics::GetFontMicro() const
{
    return 11;
}

int AppMetrics::GetFontSmall() const
{
    return 13;
}

int AppMetrics::GetFontBody() const
{
    return 14;
}

int AppMetrics::GetFontLabel() const
{
    return 15;
}

int AppMetrics::GetFontLead() const
{
    return 20;
}

int AppMetrics::GetFontSubtitle() const
{
    return 18;
}

int AppMetrics::GetFontTitle() const
{
    return IsMobileLayout() ? 17 : 22;
}

int AppMetrics::GetFontHeadline() const
{
    return 28;
}

qreal AppMetrics::GetTrackingWide() const
{
    return 1.0;
}

int AppMetrics::GetSessionCardWidth() const
{
    return IsMobileLayout() ? 0 : 792;
}

int AppMetrics::GetSessionCardHeight() const
{
    return IsMobileLayout() ? 0 : 550;
}

int AppMetrics::GetSessionWordSize() const
{
    return IsMobileLayout() ? 48 : 52;
}

int AppMetrics::GetSessionTranslationSize() const
{
    return 24;
}

int AppMetrics::GetStatValueSize() const
{
    return IsMobileLayout() ? 24 : 28;
}

int AppMetrics::GetSessionAnswerSize() const
{
    return 40;
}

int AppMetrics::GetSessionDefinitionSize() const
{
    return 15;
}

int AppMetrics::GetSessionOptionSize() const
{
    return 17;
}

int AppMetrics::GetSessionInputSize() const
{
    return 20;
}

int AppMetrics::GetMasteryDotSize() const
{
    return 10;
}

int AppMetrics::GetProgressBarHeight() const
{
    return 4;
}

int AppMetrics::GetStatBoxHeight() const
{
    return 81;
}

int AppMetrics::GetPreviewFieldHeight() const
{
    return 37;
}

int AppMetrics::GetChipListSpacing() const
{
    return 6;
}

int AppMetrics::GetOptionCellMinimumHeight() const
{
    return 90;
}

int AppMetrics::GetExerciseAdvanceDelayCorrect() const
{
    return 800;
}

int AppMetrics::GetMotion(int Milliseconds) const
{
    if (IsMotionReduced() || (!IsMobileLayoutEnabled() && !AreTransitionsEnabled()))
    {
        return 0;
    }

    return Milliseconds;
}

int AppMetrics::GetMotionFast() const
{
    return GetMotion(90);
}

int AppMetrics::GetMotionBase() const
{
    return GetMotion(160);
}

int AppMetrics::GetMotionSlow() const
{
    return GetMotion(240);
}

int AppMetrics::GetMotionTransition() const
{
    return AreTransitionsEnabled() ? GetMotionSlow() : 0;
}

int AppMetrics::GetMotionReveal() const
{
    return GetMotion(720);
}

int AppMetrics::GetMotionMorph() const
{
    return GetMotion(420);
}

int AppMetrics::GetMotionPage() const
{
    return GetMotion(420);
}

bool AppMetrics::AreTransitionsEnabled() const
{
    return FTSettings::Get().OpenStore().value(TransitionsKey, true).toBool();
}

void AppMetrics::SetTransitionsEnabled(bool bValue)
{
    if (AreTransitionsEnabled() == bValue)
    {
        return;
    }

    FTSettings::Get().OpenStore().setValue(TransitionsKey, bValue);

    qCInfo(LogApp) << "animated transitions" << (bValue ? "enabled" : "disabled");

    emit transitionsEnabledChanged();
}

int AppMetrics::GetMotionEasing() const
{
    return QEasingCurve::OutCubic;
}

int AppMetrics::GetMotionTravel() const
{
    return 30;
}

int AppMetrics::GetMotionShake() const
{
    return 8;
}

qreal AppMetrics::GetMotionWiggle() const
{
    return 2.5;
}

int AppMetrics::GetPlayButtonSize() const
{
    return 80;
}

int AppMetrics::GetPlayButtonGlyphSize() const
{
    return 22;
}


int AppMetrics::GetWindowWidth() const
{
    return IsMobileLayout() ? 0 : 1280;
}

int AppMetrics::GetWindowHeight() const
{
    return IsMobileLayout() ? 0 : 800;
}

int AppMetrics::GetWindowMinimumWidth() const
{
    return IsMobileLayout() ? 0 : 960;
}

int AppMetrics::GetWindowMinimumHeight() const
{
    return IsMobileLayout() ? 0 : 640;
}

int AppMetrics::GetContentWidthWide() const
{
    return IsMobileLayout() ? 0 : 1120;
}

int AppMetrics::GetContentWidthNarrow() const
{
    return IsMobileLayout() ? 0 : 720;
}

int AppMetrics::GetListPaneWidth() const
{
    return IsMobileLayout() ? 0 : 400;
}

int AppMetrics::GetNavItemHeight() const
{
    return IsMobileLayout() ? 0 : 40;
}

int AppMetrics::GetIconSize() const
{
    return IsMobileLayout() ? 0 : 18;
}

int AppMetrics::GetIconSizeSmall() const
{
    return IsMobileLayout() ? 0 : 14;
}

int AppMetrics::GetListRowHeight() const
{
    return IsMobileLayout() ? 0 : 56;
}

int AppMetrics::GetChipHeight() const
{
    return IsMobileLayout() ? 36 : 32;
}

int AppMetrics::GetBadgeHeight() const
{
    return IsMobileLayout() ? 0 : 22;
}

int AppMetrics::GetProgressRingSize() const
{
    return IsMobileLayout() ? 0 : 26;
}

int AppMetrics::GetProgressRingStroke() const
{
    return IsMobileLayout() ? 0 : 3;
}

int AppMetrics::GetDialogWidth() const
{
    return IsMobileLayout() ? 0 : 520;
}

int AppMetrics::GetPopoverWidth() const
{
    return IsMobileLayout() ? 0 : 300;
}

int AppMetrics::GetTextAreaHeight() const
{
    return IsMobileLayout() ? 0 : 96;
}

int AppMetrics::GetHeroFigureSize() const
{
    return IsMobileLayout() ? 32 : 44;
}

int AppMetrics::GetSessionTileHeight() const
{
    return IsMobileLayout() ? 72 : 120;
}

int AppMetrics::GetSessionTileHeightCompact() const
{
    return IsMobileLayout() ? 56 : 64;
}

int AppMetrics::GetDayTileHeight() const
{
    return IsMobileLayout() ? 0 : 62;
}

int AppMetrics::GetGoalRingSize() const
{
    return IsMobileLayout() ? 104 : 152;
}

int AppMetrics::GetGoalRingStroke() const
{
    return IsMobileLayout() ? 8 : 12;
}

int AppMetrics::GetGoalRingGap() const
{
    return IsMobileLayout() ? 2 : 3;
}

int AppMetrics::GetWelcomeRingSize() const
{
    return IsMobileLayout() ? 0 : 304;
}

int AppMetrics::GetWelcomeGreetingSize() const
{
    return IsMobileLayout() ? 0 : 40;
}

int AppMetrics::GetPathNodeSize() const
{
    return IsMobileLayout() ? 0 : 16;
}

int AppMetrics::GetPathLineWidth() const
{
    return IsMobileLayout() ? 0 : 2;
}

int AppMetrics::GetTodayRingColumnWidth() const
{
    return IsMobileLayout() ? 0 : 168;
}

int AppMetrics::GetTodayRingOverhang() const
{
    return IsMobileLayout() ? 0 : 12;
}

int AppMetrics::GetTodayBracketOffset() const
{
    return IsMobileLayout() ? 0 : 16;
}

int AppMetrics::GetTodayPracticeGap() const
{
    return IsMobileLayout() ? 0 : 32;
}

int AppMetrics::GetWeekDayCellSize() const
{
    return IsMobileLayout() ? 0 : 38;
}

int AppMetrics::GetWeekRingSize() const
{
    return IsMobileLayout() ? 0 : 28;
}

int AppMetrics::GetWeekRingStroke() const
{
    return IsMobileLayout() ? 0 : 3;
}

int AppMetrics::GetWeekRingGap() const
{
    return IsMobileLayout() ? 0 : 2;
}

int AppMetrics::GetWeekFutureDotSize() const
{
    return IsMobileLayout() ? 0 : 6;
}

int AppMetrics::GetFocusRingWidth() const
{
    return IsMobileLayout() ? 0 : 2;
}

int AppMetrics::GetScrollBarWidth() const
{
    return IsMobileLayout() ? 0 : 8;
}

int AppMetrics::GetKeyHintSize() const
{
    return IsMobileLayout() ? 0 : 22;
}

int AppMetrics::GetSliderTickSpacing() const
{
    return IsMobileLayout() ? 0 : 16;
}

int AppMetrics::GetToastWidth() const
{
    return IsMobileLayout() ? 0 : 360;
}

int AppMetrics::GetSettingsControlWidth() const
{
    return IsMobileLayout() ? 0 : 220;
}

int AppMetrics::GetRowProgressWidth() const
{
    return IsMobileLayout() ? 0 : 64;
}

int AppMetrics::GetCategoryPaneWidth() const
{
    return IsMobileLayout() ? 0 : 260;
}

int AppMetrics::GetCoverTileMinimumWidth() const
{
    return IsMobileLayout() ? 0 : 260;
}

qreal AppMetrics::GetReaderTileAspect() const
{
    return 1.7;
}

qreal AppMetrics::GetReaderPageAspect() const
{
    return 0.707;
}

int AppMetrics::GetSwitchGlyphSize() const
{
    return IsMobileLayout() ? 40 : 48;
}

int AppMetrics::GetReaderOpenIconSize() const
{
    return IsMobileLayout() ? 26 : 32;
}

int AppMetrics::GetReaderOpenPlusSize() const
{
    return IsMobileLayout() ? 39 : 48;
}

int AppMetrics::GetCoverRadius() const
{
    return IsMobileLayout() ? 10 : 12;
}

int AppMetrics::GetCoverProgressHeight() const
{
    return IsMobileLayout() ? 3 : 4;
}

qreal AppMetrics::GetCoverHoverScale() const
{
    return 1.03;
}

int AppMetrics::GetBookRadius() const
{
    return 4;
}

int AppMetrics::GetBookTitleSize() const
{
    return 36;
}

qreal AppMetrics::GetShelfRecedeScale() const
{
    return 0.98;
}

qreal AppMetrics::GetCoverStripeBottomStart() const
{
    return 0.14;
}

qreal AppMetrics::GetCoverStripeBottomEnd() const
{
    return 0.47;
}

qreal AppMetrics::GetCoverStripeRightStart() const
{
    return 0.22;
}

qreal AppMetrics::GetCoverStripeRightEnd() const
{
    return 0.53;
}

qreal AppMetrics::GetSessionTileBandWidth() const
{
    return 0.4;
}

qreal AppMetrics::GetSessionTileBandTop() const
{
    return 0.14;
}

qreal AppMetrics::GetTodayTopFill() const
{
    return 0.41;
}

qreal AppMetrics::GetTodaySideShare() const
{
    return 0.39;
}

int AppMetrics::GetReaderRowHeight() const
{
    return 68;
}

int AppMetrics::GetReaderRowCoverWidth() const
{
    return 68;
}

int AppMetrics::GetReaderContentsWidth() const
{
    return 260;
}

int AppMetrics::GetCoverGridWidth() const
{
    return 1600;
}

int AppMetrics::GetDeckIconSize() const
{
    return IsMobileLayout() ? 22 : 26;
}

int AppMetrics::GetCategoryDotSize() const
{
    return 8;
}

int AppMetrics::GetAvatarSize() const
{
    return IsMobileLayout() ? 0 : 48;
}

int AppMetrics::GetToggleChipHeight() const
{
    return IsMobileLayout() ? 0 : 64;
}

int AppMetrics::GetTooltipDelay() const
{
    return IsMobileLayout() ? 0 : 400;
}

int AppMetrics::GetToastDuration() const
{
    return IsMobileLayout() ? 0 : 4800;
}

int AppMetrics::GetMenuWidth() const
{
    return IsMobileLayout() ? 0 : 200;
}

int AppMetrics::GetPopoverListHeight() const
{
    return IsMobileLayout() ? 0 : 420;
}

int AppMetrics::GetDialogListHeight() const
{
    return IsMobileLayout() ? 0 : 280;
}

int AppMetrics::GetTwoColumnMinimumWidth() const
{
    return IsMobileLayout() ? 0 : 520;
}

int AppMetrics::GetListMarkerWidth() const
{
    return IsMobileLayout() ? 0 : 22;
}

int AppMetrics::GetCompleteBadgeSize() const
{
    return IsMobileLayout() ? 0 : 80;
}

int AppMetrics::GetCompleteGlyphSize() const
{
    return IsMobileLayout() ? 0 : 44;
}

qreal AppMetrics::GetIconStrokeBold() const
{
    return IsMobileLayout() ? 0 : 2.0;
}
qreal AppMetrics::GetDashStrokeWidth() const
{
    return IsMobileLayout() ? 0 : 1.5;
}

qreal AppMetrics::GetDashLength() const
{
    return IsMobileLayout() ? 0 : 2.0;
}

int AppMetrics::GetSelectionBarWidth() const
{
    return IsMobileLayout() ? 0 : 3;
}

int AppMetrics::GetSelectionBarHeight() const
{
    return IsMobileLayout() ? 0 : 24;
}

int AppMetrics::GetTabHeight() const
{
    return IsMobileLayout() ? 44 : 36;
}

int AppMetrics::GetFontTab() const
{
    return 16;
}

int AppMetrics::GetTabIndicatorHeight() const
{
    return 2;
}


int AppMetrics::GetExplainPaneWidth() const
{
    return IsMobileLayout() ? 0 : 280;
}

int AppMetrics::GetReaderSettingsWidth() const
{
    return IsMobileLayout() ? 0 : 360;
}

int AppMetrics::GetReaderPageMargin() const
{
    return IsMobileLayout() ? 0 : 24;
}

int AppMetrics::GetReaderPageWidth() const
{
    return IsMobileLayout() ? 0 : 880;
}

int AppMetrics::GetReaderWheelStep() const
{
    return IsMobileLayout() ? 0 : 120;
}

int AppMetrics::GetReaderAutoscrollDeadZone() const
{
    return 12;
}

qreal AppMetrics::GetReaderAutoscrollSpeed() const
{
    return 6.0;
}

int AppMetrics::GetReaderLookupWordLimit() const
{
    return 3;
}

qreal AppMetrics::GetReaderTranslationWidthShare() const
{
    return 0.5;
}

int AppMetrics::GetReaderAutoscrollAnchorSize() const
{
    return 28;
}

qreal AppMetrics::GetReaderImageHeightShare() const
{
    return 0.8;
}

qreal AppMetrics::GetReaderEndSpaceShare() const
{
    return 0.25;
}

int AppMetrics::GetReaderCropPadding() const
{
    return IsMobileLayout() ? 0 : 8;
}

int AppMetrics::GetReaderPositionWidth() const
{
    return IsMobileLayout() ? 0 : 88;
}

int AppMetrics::GetReaderBarHeight() const
{
    return IsMobileLayout() ? 0 : 52;
}

int AppMetrics::GetReaderSaveDelay() const
{
    return 800;
}

qreal AppMetrics::GetReaderZoomStep() const
{
    return 1.2;
}

qreal AppMetrics::GetReaderMinimumZoom() const
{
    return 0.25;
}

qreal AppMetrics::GetReaderMaximumZoom() const
{
    return 8.0;
}

int AppMetrics::GetBrandTitleSize() const
{
    return IsMobileLayout() ? 0 : 56;
}

int AppMetrics::GetSessionProgressHeight() const
{
    return IsMobileLayout() ? 0 : 56;
}

int AppMetrics::GetStageSegmentWidth() const
{
    return IsMobileLayout() ? 0 : 32;
}

qreal AppMetrics::GetTriagePaneShare() const
{
    return IsMobileLayout() ? 0.0 : 0.16;
}

int AppMetrics::GetActionButtonWidth() const
{
    return IsMobileLayout() ? 0 : 200;
}

int AppMetrics::GetTriageWordSize() const
{
    return IsMobileLayout() ? 0 : 44;
}

int AppMetrics::GetRankTrackWidth() const
{
    return IsMobileLayout() ? 0 : 40;
}

int AppMetrics::GetRankHandleSize() const
{
    return IsMobileLayout() ? 0 : 6;
}

int AppMetrics::GetWaveformButtonWidth() const
{
    return IsMobileLayout() ? 0 : 320;
}

int AppMetrics::GetWaveformButtonHeight() const
{
    return IsMobileLayout() ? 0 : 64;
}

int AppMetrics::GetWaveformBarWidth() const
{
    return IsMobileLayout() ? 0 : 3;
}

int AppMetrics::GetWaveformBarMinimumHeight() const
{
    return IsMobileLayout() ? 0 : 4;
}

int AppMetrics::GetWaveformCycle() const
{
    return IsMobileLayout() ? 0 : 700;
}

int AppMetrics::GetExerciseAnswerHeight() const
{
    return IsMobileLayout() ? 0 : 160;
}

int AppMetrics::GetExercisePromptHeight() const
{
    return IsMobileLayout() ? 0 : 168;
}

int AppMetrics::GetSentenceMinimumSize() const
{
    return 18;
}

int AppMetrics::GetSentenceNativeSize() const
{
    return 20;
}

int AppMetrics::GetSentenceNativeMinimumSize() const
{
    return 16;
}

int AppMetrics::GetTileRowLimit() const
{
    return 3;
}

int AppMetrics::GetExercisePromptHeightCompact() const
{
    return IsMobileLayout() ? 0 : 52;
}

int AppMetrics::GetAnswerLineCount() const
{
    return 2;
}

int AppMetrics::GetRevealLineCount() const
{
    return 3;
}

int AppMetrics::GetDefinitionLineCount() const
{
    return 3;
}

int AppMetrics::GetAnswerSlotHeight() const
{
    return IsMobileLayout() ? 0 : 64;
}

int AppMetrics::GetOptionCellHeight() const
{
    return IsMobileLayout() ? 0 : 72;
}

int AppMetrics::GetMatchCellHeight() const
{
    return IsMobileLayout() ? 0 : 60;
}

int AppMetrics::GetMatchCellHeightCompact() const
{
    return IsMobileLayout() ? 0 : 46;
}

int AppMetrics::GetFormCellHeight() const
{
    return IsMobileLayout() ? 0 : 48;
}
