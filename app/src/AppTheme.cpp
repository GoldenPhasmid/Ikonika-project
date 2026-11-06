#include "AppTheme.h"
#include "AppLocalization.h"
#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QSettings>

#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>
#elif defined(Q_OS_IOS)
#include <QGuiApplication>
#include <QStyleHints>
#endif

namespace
{
    constexpr const char* ThemeKey = "appearance/theme";
    constexpr const char* ColourByGenderKey = "appearance/colourByGender";

    struct FThemePalette
    {
        QColor Text;
        QColor Masculine;
        QColor Feminine;
        QColor Muted;
        QColor Accent;
        QColor AccentText;
        QColor Panel;
        QColor Faint;
        QColor Sunken;
        QColor Subtle;
        QColor Outline;
        QColor Border;
        QColor Correct;
        QColor Warn;
        QColor Wrong;
        QColor Scrim;
        QColor Page;
        QColor Card;
        QColor Well;
        QVariantList CoverColours;
        QColor CoverInk;
        QColor CoverMuted;
        QColor CoverStripe;
    };

    const FThemePalette LightPalette{
        .Text = QColor("#2e2a25"),
        .Masculine = QColor("#2a6593"),
        .Feminine = QColor("#ad4860"),
        .Muted = QColor("#75716a"),
        .Accent = QColor("#3d63b0"),
        .AccentText = QColor("#ffffff"),
        .Panel = QColor("#efede5"),
        .Faint = QColor("#e4e1d7"),
        .Sunken = QColor("#e9e6dd"),
        .Subtle = QColor("#57534b"),
        .Outline = QColor("#857f72"),
        .Border = QColor("#d6d2c5"),
        .Correct = QColor("#3b7a53"),
        .Warn = QColor("#9e6c2c"),
        .Wrong = QColor("#a23a37"),
        .Scrim = QColor(0, 0, 0, 82),
        .Page = QColor("#e4e1d7"),
        .Card = QColor("#efede5"),
        .Well = QColor("#e9e6dd"),
        .CoverColours = {QColor("#8f4a48"), QColor("#8a6838"), QColor("#5d7a46"), QColor("#3a6d72"),
                         QColor("#4a5f8c"), QColor("#6b4c7c"), QColor("#7c5a6f"), QColor("#5a5652")},
        .CoverInk = QColor("#f2f0e9"),
        .CoverMuted = QColor("#6e6a64"),
        .CoverStripe = QColor(0, 0, 0, 26)
    };

    const FThemePalette DarkPalette{
        .Text = QColor("#f2f0e9"),
        .Masculine = QColor("#74afdc"),
        .Feminine = QColor("#e8919f"),
        .Muted = QColor("#9c9a8f"),
        .Accent = QColor("#5b9cf8"),
        .AccentText = QColor("#000000"),
        .Panel = QColor("#262624"),
        .Faint = QColor("#30302e"),
        .Sunken = QColor("#2b2b29"),
        .Subtle = QColor("#c2c0b6"),
        .Outline = QColor("#8a887e"),
        .Border = QColor("#3a3937"),
        .Correct = QColor("#4caf7d"),
        .Warn = QColor("#d8a657"),
        .Wrong = QColor("#d9695f"),
        .Scrim = QColor(0, 0, 0, 128),
        .Page = QColor("#262624"),
        .Card = QColor("#30302e"),
        .Well = QColor("#2b2b29"),
        .CoverColours = {QColor("#7a3b3b"), QColor("#7a5a2f"), QColor("#4f6b3a"), QColor("#2f5d62"),
                         QColor("#3b4f7a"), QColor("#5b3e6b"), QColor("#6b4a5e"), QColor("#4a4744")},
        .CoverInk = QColor("#f2f0e9"),
        .CoverMuted = QColor("#45403d"),
        .CoverStripe = QColor(0, 0, 0, 38)
    };

    const QVariantMap FrequencyColours{
        {QStringLiteral("TOP 100"), QColor("#4caf7d")},
        {QStringLiteral("TOP 500"), QColor("#7fb356")},
        {QStringLiteral("TOP 1000"), QColor("#a9a94a")},
        {QStringLiteral("TOP 2000"), QColor("#c98f45")},
        {QStringLiteral("TOP 5000"), QColor("#c9703f")},
        {QStringLiteral("TOP 10000"), QColor("#c25a5a")},
        {QStringLiteral("TOP 20000"), QColor("#a85a63")},
        {QStringLiteral("TOP 50000"), QColor("#8c5a6b")}
    };

    const QVariantMap CefrColours{
        {QStringLiteral("A1"), QColor("#4caf7d")},
        {QStringLiteral("A2"), QColor("#7fb356")},
        {QStringLiteral("B1"), QColor("#c9a53f")},
        {QStringLiteral("B2"), QColor("#c98f45")},
        {QStringLiteral("C1"), QColor("#c9703f")},
        {QStringLiteral("C2"), QColor("#c25a5a")}
    };

    const FThemePalette& GetPalette(AppTheme::ETheme Theme)
    {
        return Theme == AppTheme::ETheme::Light ? LightPalette : DarkPalette;
    }

    AppTheme::ETheme ClampTheme(int Index)
    {
        return Index == static_cast<int>(AppTheme::ETheme::Light)
            ? AppTheme::ETheme::Light
            : AppTheme::ETheme::Dark;
    }

#if defined(Q_OS_ANDROID)
    constexpr const char* AndroidStatusBarControllerClass =
        "com/goldenphasmid/ikonika/StatusBarController";

    void AndroidApplyStatusBarAppearance(AppTheme::ETheme Theme)
    {
        QJniObject Activity = QNativeInterface::QAndroidApplication::context();

        QJniObject::callStaticMethod<void>(AndroidStatusBarControllerClass, "applyAppearance",
            "(Landroid/content/Context;Z)V", Activity.object<jobject>(),
            static_cast<jboolean>(Theme == AppTheme::ETheme::Light));
    }
#elif defined(Q_OS_IOS)
    void IosApplyStatusBarAppearance(AppTheme::ETheme Theme)
    {
        const Qt::ColorScheme Scheme = Theme == AppTheme::ETheme::Light
            ? Qt::ColorScheme::Light
            : Qt::ColorScheme::Dark;

        QGuiApplication::styleHints()->setColorScheme(Scheme);

        qCInfo(LogApp) << "Status bar follows the app theme, light =" << (Theme == AppTheme::ETheme::Light);
    }
#endif
}

AppTheme::AppTheme(QObject* Parent)
    : QObject(Parent)
{
    const QSettings Settings(FTSettings::Get().OpenStore());
    const int Stored = Settings.value(ThemeKey, static_cast<int>(ETheme::Dark)).toInt();

    Theme = ClampTheme(Stored);
    bColourByGender = Settings.value(ColourByGenderKey, true).toBool();
    bColourByVerbGroup = Settings.value("appearance/colourByVerbGroup", false).toBool();
    connect(this, &AppTheme::themeChanged, this, &AppTheme::verbColoursChanged);

    connect(&AppLocalization::Get(), &AppLocalization::languageChanged,
            this, &AppTheme::themeNamesChanged);
    connect(this, &AppTheme::themeChanged, this, &AppTheme::genderColoursChanged);
    connect(this, &AppTheme::colourByGenderChanged, this, &AppTheme::genderColoursChanged);

#if defined(Q_OS_ANDROID)
    AndroidApplyStatusBarAppearance(Theme);
#elif defined(Q_OS_IOS)
    IosApplyStatusBarAppearance(Theme);
#endif
}

int AppTheme::GetThemeIndex() const
{
    return static_cast<int>(Theme);
}

void AppTheme::SetThemeIndex(int Index)
{
    const ETheme NewTheme = ClampTheme(Index);

    if (NewTheme == Theme)
    {
        return;
    }

    Theme = NewTheme;

    QSettings Settings(FTSettings::Get().OpenStore());
    Settings.setValue(ThemeKey, static_cast<int>(Theme));

#if defined(Q_OS_ANDROID)
    AndroidApplyStatusBarAppearance(Theme);
#elif defined(Q_OS_IOS)
    IosApplyStatusBarAppearance(Theme);
#endif

    emit themeChanged();
}

QStringList AppTheme::GetThemeNames() const
{
    return {tr("Light"), tr("Dark")};
}

QColor AppTheme::GetColourText() const
{
    return GetPalette(Theme).Text;
}

QColor AppTheme::GetColourMuted() const
{
    return GetPalette(Theme).Muted;
}

QColor AppTheme::GetColourAccent() const
{
    return GetPalette(Theme).Accent;
}

QColor AppTheme::GetColourAccentText() const
{
    return GetPalette(Theme).AccentText;
}

QColor AppTheme::GetColourPanel() const
{
    return GetPalette(Theme).Panel;
}

QColor AppTheme::GetColourFaint() const
{
    return GetPalette(Theme).Faint;
}

QColor AppTheme::GetColourSunken() const
{
    return GetPalette(Theme).Sunken;
}

QColor AppTheme::GetColourSubtle() const
{
    return GetPalette(Theme).Subtle;
}

QColor AppTheme::GetColourOutline() const
{
    return GetPalette(Theme).Outline;
}

QColor AppTheme::GetColourBorder() const
{
    return GetPalette(Theme).Border;
}

QColor AppTheme::GetColourCorrect() const
{
    return GetPalette(Theme).Correct;
}

QColor AppTheme::GetColourWarn() const
{
    return GetPalette(Theme).Warn;
}

QColor AppTheme::GetColourWrong() const
{
    return GetPalette(Theme).Wrong;
}

QColor AppTheme::GetColourScrim() const
{
    return GetPalette(Theme).Scrim;
}

QColor AppTheme::GetColourPage() const
{
    return GetPalette(Theme).Page;
}

QColor AppTheme::GetColourCard() const
{
    return GetPalette(Theme).Card;
}

QColor AppTheme::GetColourWell() const
{
    return GetPalette(Theme).Well;
}

QColor AppTheme::GetColourHover() const
{
    return faded(GetColourText(), GetAlphaFaint());
}

QColor AppTheme::GetColourPressed() const
{
    return faded(GetColourText(), GetAlphaSoft());
}

bool AppTheme::IsColourByGender() const
{
    return bColourByGender;
}

void AppTheme::SetColourByGender(bool bEnabled)
{
    if (bEnabled == bColourByGender)
    {
        return;
    }

    bColourByGender = bEnabled;

    QSettings Settings(FTSettings::Get().OpenStore());
    Settings.setValue(ColourByGenderKey, bColourByGender);

    qCInfo(LogApp) << "Setting changed: colour by gender ->" << bColourByGender;

    emit colourByGenderChanged();
}

QVariantList AppTheme::GetGenderColours() const
{
    const FThemePalette& Palette = GetPalette(Theme);

    if (!bColourByGender)
    {
        return {Palette.Text, Palette.Text, Palette.Text};
    }

    return {Palette.Masculine, Palette.Feminine, Palette.Text};
}

QVariantList AppTheme::GetCoverColours() const
{
    return GetPalette(Theme).CoverColours;
}

QColor AppTheme::GetColourCoverInk() const
{
    return GetPalette(Theme).CoverInk;
}

QColor AppTheme::GetColourCoverMuted() const
{
    return GetPalette(Theme).CoverMuted;
}

QColor AppTheme::GetColourCoverStripe() const
{
    return GetPalette(Theme).CoverStripe;
}

QVariantMap AppTheme::GetFrequencyColours() const
{
    return FrequencyColours;
}

QVariantMap AppTheme::GetCefrColours() const
{
    return CefrColours;
}

qreal AppTheme::GetAlphaFaint() const
{
    return 0.08;
}

qreal AppTheme::GetAlphaSoft() const
{
    return 0.12;
}

qreal AppTheme::GetAlphaMedium() const
{
    return 0.18;
}

qreal AppTheme::GetAlphaStrong() const
{
    return 0.24;
}

qreal AppTheme::GetAlphaHalf() const
{
    return 0.5;
}

qreal AppTheme::GetOpacityDisabled() const
{
    return 0.4;
}

qreal AppTheme::GetOpacitySecondary() const
{
    return 0.7;
}

qreal AppTheme::GetOpacityHover() const
{
    return 0.85;
}

QColor AppTheme::faded(const QColor& Colour, qreal Alpha) const
{
    QColor Result{Colour};
    Result.setAlphaF(static_cast<float>(Alpha));

    return Result;
}

bool AppTheme::IsColourByVerbGroup() const
{
    return bColourByVerbGroup;
}

void AppTheme::SetColourByVerbGroup(bool bEnabled)
{
    if (bEnabled == bColourByVerbGroup)
    {
        return;
    }
    bColourByVerbGroup = bEnabled;
    QSettings Settings(FTSettings::Get().OpenStore());
    Settings.setValue("appearance/colourByVerbGroup", bEnabled);
    qCInfo(LogApp) << "Setting changed: colour by verb group ->" << bEnabled;
    emit verbColoursChanged();
}

QVariantMap AppTheme::GetVerbGroupColours() const
{
    if (!bColourByVerbGroup)
    {
        return {};
    }
    const bool bDark = GetThemeIndex() == 1;
    return {{"first", QColor(bDark ? "#79C8B8" : "#246B5D")},
            {"second", QColor(bDark ? "#E5BB76" : "#805818")},
            {"third", QColor(bDark ? "#B9A5E8" : "#70529B")},
            {"irregular", QColor(bDark ? "#EAA09A" : "#A04741")}};
}

QVariantMap AppTheme::GetVerbColours() const
{
    const bool bDark = GetThemeIndex() == 1;
    return {{"-er", QColor(bDark ? "#72c6b5" : "#226c5d")},
            {"-ar", QColor(bDark ? "#72c6b5" : "#226c5d")},
            {"-ir", QColor(bDark ? "#8db8ef" : "#305f9c")},
            {"-re", QColor(bDark ? "#c2a4e5" : "#785399")},
            {"irregular", QColor(bDark ? "#e89a96" : "#a34543")}};
}
