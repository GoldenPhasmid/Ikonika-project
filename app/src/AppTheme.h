#pragma once

#include <QColor>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

/** App-wide colour palette */
class AppTheme : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int themeIndex READ GetThemeIndex WRITE SetThemeIndex NOTIFY themeChanged)
    Q_PROPERTY(QStringList themeNames READ GetThemeNames NOTIFY themeNamesChanged)

    Q_PROPERTY(QColor colourText READ GetColourText NOTIFY themeChanged)
    Q_PROPERTY(QColor colourMuted READ GetColourMuted NOTIFY themeChanged)
    Q_PROPERTY(QColor colourAccent READ GetColourAccent NOTIFY themeChanged)
    
    Q_PROPERTY(QColor colourAccentText READ GetColourAccentText NOTIFY themeChanged)

    Q_PROPERTY(QColor colourPanel READ GetColourPanel NOTIFY themeChanged)

    /** Fill for a surface that sits on a panel without being a panel of its own: a thumbnail
        well, an inactive chip, a progress track. */
    Q_PROPERTY(QColor colourFaint READ GetColourFaint NOTIFY themeChanged)

    /** A shade below faint, for a surface the eye should read as recessed rather than merely
        tinted: an option card waiting to be chosen, a read-only field. */
    Q_PROPERTY(QColor colourSunken READ GetColourSunken NOTIFY themeChanged)

    /** Text that is quieter than colourText but still meant to be read, unlike colourMuted: a
        secondary link, a definition beside its headword. */
    Q_PROPERTY(QColor colourSubtle READ GetColourSubtle NOTIFY themeChanged)
    
    Q_PROPERTY(QColor colourOutline READ GetColourOutline NOTIFY themeChanged)

    Q_PROPERTY(QColor colourBorder READ GetColourBorder NOTIFY themeChanged)
    Q_PROPERTY(QColor colourCorrect READ GetColourCorrect NOTIFY themeChanged)
    Q_PROPERTY(QColor colourWarn READ GetColourWarn NOTIFY themeChanged)
    Q_PROPERTY(QColor colourWrong READ GetColourWrong NOTIFY themeChanged)

    /** The dim laid over a window behind a dialog that has to be answered before anything else. */
    Q_PROPERTY(QColor colourScrim READ GetColourScrim NOTIFY themeChanged)
    Q_PROPERTY(QColor colourPage READ GetColourPage NOTIFY themeChanged)
    Q_PROPERTY(QColor colourCard READ GetColourCard NOTIFY themeChanged)
    Q_PROPERTY(QColor colourWell READ GetColourWell NOTIFY themeChanged)
    Q_PROPERTY(QColor colourHover READ GetColourHover NOTIFY themeChanged)
    Q_PROPERTY(QColor colourPressed READ GetColourPressed NOTIFY themeChanged)

    Q_PROPERTY(bool colourByGender READ IsColourByGender WRITE SetColourByGender NOTIFY colourByGenderChanged)
    Q_PROPERTY(QVariantList genderColours READ GetGenderColours NOTIFY genderColoursChanged)

    Q_PROPERTY(bool colourByVerbGroup READ IsColourByVerbGroup WRITE SetColourByVerbGroup NOTIFY verbColoursChanged)
    Q_PROPERTY(QVariantMap verbGroupColours READ GetVerbGroupColours NOTIFY verbColoursChanged)
    Q_PROPERTY(QVariantMap verbColours READ GetVerbColours NOTIFY themeChanged)

    Q_PROPERTY(QVariantList coverColours READ GetCoverColours NOTIFY themeChanged)
    Q_PROPERTY(QColor colourCoverInk READ GetColourCoverInk NOTIFY themeChanged)
    Q_PROPERTY(QColor colourCoverMuted READ GetColourCoverMuted NOTIFY themeChanged)
    Q_PROPERTY(QColor colourCoverStripe READ GetColourCoverStripe NOTIFY themeChanged)

    
    Q_PROPERTY(QVariantMap frequencyColours READ GetFrequencyColours CONSTANT)
    
    Q_PROPERTY(QVariantMap cefrColours READ GetCefrColours CONSTANT)

    Q_PROPERTY(QColor colourReaderMarkDark READ GetColourReaderMarkDark CONSTANT)
    Q_PROPERTY(qreal alphaFaint READ GetAlphaFaint CONSTANT)
    Q_PROPERTY(qreal alphaSoft READ GetAlphaSoft CONSTANT)
    Q_PROPERTY(qreal alphaMedium READ GetAlphaMedium CONSTANT)
    Q_PROPERTY(qreal alphaStrong READ GetAlphaStrong CONSTANT)
    Q_PROPERTY(qreal alphaHalf READ GetAlphaHalf CONSTANT)

    Q_PROPERTY(qreal opacityDisabled READ GetOpacityDisabled CONSTANT)
    Q_PROPERTY(qreal opacitySecondary READ GetOpacitySecondary CONSTANT)
    Q_PROPERTY(qreal opacityHover READ GetOpacityHover CONSTANT)

public:
    explicit AppTheme(QObject* Parent = nullptr);
    
    enum class ETheme : int
    {
        Light = 0,
        Dark = 1
    };

    QVariantMap GetVerbColours() const;
    QVariantMap GetVerbGroupColours() const;
    bool IsColourByVerbGroup() const;
    void SetColourByVerbGroup(bool bEnabled);
    int GetThemeIndex() const;
    void SetThemeIndex(int Index);

    QStringList GetThemeNames() const;

    QColor GetColourText() const;
    QColor GetColourMuted() const;
    QColor GetColourAccent() const;
    QColor GetColourAccentText() const;
    QColor GetColourPanel() const;
    QColor GetColourFaint() const;
    QColor GetColourSunken() const;
    QColor GetColourSubtle() const;
    QColor GetColourOutline() const;
    QColor GetColourBorder() const;
    QColor GetColourCorrect() const;
    QColor GetColourWarn() const;
    QColor GetColourWrong() const;
    QColor GetColourScrim() const;
    QColor GetColourPage() const;
    QColor GetColourCard() const;
    QColor GetColourWell() const;
    QColor GetColourHover() const;
    QColor GetColourPressed() const;

    bool IsColourByGender() const;
    void SetColourByGender(bool bEnabled);
    QVariantList GetGenderColours() const;

    QVariantList GetCoverColours() const;
    QColor GetColourCoverInk() const;
    QColor GetColourCoverMuted() const;
    QColor GetColourCoverStripe() const;

    QVariantMap GetFrequencyColours() const;
    QVariantMap GetCefrColours() const;

    QColor GetColourReaderMarkDark() const;
    qreal GetAlphaFaint() const;
    qreal GetAlphaSoft() const;
    qreal GetAlphaMedium() const;
    qreal GetAlphaStrong() const;
    qreal GetAlphaHalf() const;

    qreal GetOpacityDisabled() const;
    qreal GetOpacitySecondary() const;
    qreal GetOpacityHover() const;

    Q_INVOKABLE QColor faded(const QColor& Colour, qreal Alpha) const;

signals:
    void themeChanged();
    void themeNamesChanged();
    void colourByGenderChanged();
    void genderColoursChanged();
    void verbColoursChanged();

private:
    ETheme Theme = ETheme::Dark;
    bool bColourByGender = true;
    bool bColourByVerbGroup = false;
};
