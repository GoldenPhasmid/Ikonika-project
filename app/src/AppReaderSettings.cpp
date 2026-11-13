#include "AppReaderSettings.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QFontDatabase>
#include <QSettings>

namespace
{
    const QString TextSizeKey = QStringLiteral("reader/textSize");
    const QString LineSpacingKey = QStringLiteral("reader/lineSpacing");
    const QString TextMarginKey = QStringLiteral("reader/textMargin");
    const QString PageWidthKey = QStringLiteral("reader/pageWidthPercent");
    const QString PageToneKey = QStringLiteral("reader/pageTone");
    const QString TextToneKey = QStringLiteral("reader/textTone");
    const QString TextFontKey = QStringLiteral("reader/textFont");
    const QString CropMarginsKey = QStringLiteral("reader/cropMargins");
    const QString VolumeKeysKey = QStringLiteral("reader/volumeKeysTurnPages");
    const QString HighlightVocabularyKey = QStringLiteral("reader/highlightVocabulary");
    const QString LibrarySortOrderKey = QStringLiteral("reader/librarySortOrder");
    const QString LibrarySortReversedKey = QStringLiteral("reader/librarySortReversed");

    constexpr int DefaultTextSize = 16;
    constexpr int MinimumTextSize = 12;
    constexpr int MaximumTextSize = 28;

    constexpr int DefaultLineSpacing = 140;
    constexpr int MinimumLineSpacing = 100;
    constexpr int MaximumLineSpacing = 200;

    constexpr int DefaultTextMargin = 16;
    constexpr int MinimumTextMargin = 0;
    constexpr int MaximumTextMargin = 48;

    constexpr int DefaultPageWidth = 100;
    constexpr int MinimumPageWidth = 50;
    constexpr int MaximumPageWidth = 100;

    const QStringList BookFonts{QStringLiteral("Georgia"), QStringLiteral("Cambria"), QStringLiteral("Palatino Linotype"),
                                QStringLiteral("Book Antiqua"), QStringLiteral("Constantia"), QStringLiteral("Times New Roman"),
                                QStringLiteral("Noto Serif"), QStringLiteral("Segoe UI"), QStringLiteral("Calibri"),
                                QStringLiteral("Verdana"), QStringLiteral("Roboto")};
}

AppReaderSettings& AppReaderSettings::Get()
{
    static AppReaderSettings Instance;

    return Instance;
}

AppReaderSettings::AppReaderSettings(QObject* Parent)
    : QObject(Parent)
{
}

void AppReaderSettings::Write(const QString& Key, const QVariant& Value)
{
    QSettings Settings(FTSettings::Get().OpenStore());

    if (Settings.value(Key) == Value)
    {
        return;
    }

    Settings.setValue(Key, Value);

    qCInfo(LogApp) << "Reader: setting" << Key << "is now" << Value;

    emit changed();
}

int AppReaderSettings::GetTextSize() const
{
    return FTSettings::Get().OpenStore().value(TextSizeKey, DefaultTextSize).toInt();
}

void AppReaderSettings::SetTextSize(int Value)
{
    Write(TextSizeKey, qBound(MinimumTextSize, Value, MaximumTextSize));
}

int AppReaderSettings::GetMinimumTextSize() const
{
    return MinimumTextSize;
}

int AppReaderSettings::GetDefaultTextSize() const
{
    return DefaultTextSize;
}

int AppReaderSettings::GetMaximumTextSize() const
{
    return MaximumTextSize;
}

int AppReaderSettings::GetLineSpacing() const
{
    return FTSettings::Get().OpenStore().value(LineSpacingKey, DefaultLineSpacing).toInt();
}

void AppReaderSettings::SetLineSpacing(int Value)
{
    Write(LineSpacingKey, qBound(MinimumLineSpacing, Value, MaximumLineSpacing));
}

int AppReaderSettings::GetMinimumLineSpacing() const
{
    return MinimumLineSpacing;
}

int AppReaderSettings::GetMaximumLineSpacing() const
{
    return MaximumLineSpacing;
}

int AppReaderSettings::GetTextMargin() const
{
    return FTSettings::Get().OpenStore().value(TextMarginKey, DefaultTextMargin).toInt();
}

void AppReaderSettings::SetTextMargin(int Value)
{
    Write(TextMarginKey, qBound(MinimumTextMargin, Value, MaximumTextMargin));
}

int AppReaderSettings::GetMinimumTextMargin() const
{
    return MinimumTextMargin;
}

int AppReaderSettings::GetMaximumTextMargin() const
{
    return MaximumTextMargin;
}

int AppReaderSettings::GetPageWidth() const
{
    return FTSettings::Get().OpenStore().value(PageWidthKey, DefaultPageWidth).toInt();
}

void AppReaderSettings::SetPageWidth(int Value)
{
    Write(PageWidthKey, qBound(MinimumPageWidth, Value, MaximumPageWidth));
}

int AppReaderSettings::GetMinimumPageWidth() const
{
    return MinimumPageWidth;
}

int AppReaderSettings::GetMaximumPageWidth() const
{
    return MaximumPageWidth;
}

QString AppReaderSettings::GetPageTone() const
{
    return FTSettings::Get().OpenStore().value(PageToneKey, QStringLiteral("paper")).toString();
}

void AppReaderSettings::SetPageTone(const QString& Value)
{
    Write(PageToneKey, Value);
}

QString AppReaderSettings::GetTextTone() const
{
    return FTSettings::Get().OpenStore().value(TextToneKey, QStringLiteral("theme")).toString();
}

void AppReaderSettings::SetTextTone(const QString& Value)
{
    Write(TextToneKey, Value);
}

QString AppReaderSettings::GetTextFont() const
{
    const QString Font{FTSettings::Get().OpenStore().value(TextFontKey).toString()};

    return GetTextFonts().contains(Font) ? Font : QString();
}

void AppReaderSettings::SetTextFont(const QString& Value)
{
    if (!Value.isEmpty() && !GetTextFonts().contains(Value))
    {
        qCWarning(LogApp) << "Reader: font" << Value << "is not installed";

        return;
    }

    Write(TextFontKey, Value);
}

QStringList AppReaderSettings::GetTextFonts() const
{
    QStringList Fonts;

    for (const QString& Font : BookFonts)
    {
        if (QFontDatabase::hasFamily(Font))
        {
            Fonts.append(Font);
        }
    }

    return Fonts;
}

bool AppReaderSettings::IsCropMargins() const
{
    return FTSettings::Get().OpenStore().value(CropMarginsKey, false).toBool();
}

void AppReaderSettings::SetCropMargins(bool bValue)
{
    Write(CropMarginsKey, bValue);
}

bool AppReaderSettings::AreVolumeKeysTurningPages() const
{
    return FTSettings::Get().OpenStore().value(VolumeKeysKey,
        FTSettings::GetShippedDefault("reader/volumeKeysTurnPages", false)).toBool();
}

void AppReaderSettings::SetVolumeKeysTurnPages(bool bValue)
{
    Write(VolumeKeysKey, bValue);
}

bool AppReaderSettings::IsHighlightingUnknownWords() const
{
    return FTSettings::Get().OpenStore().value(QStringLiteral("reader/highlightUnknownWords"), false).toBool();
}

void AppReaderSettings::SetHighlightUnknownWords(bool bValue)
{
    Write(QStringLiteral("reader/highlightUnknownWords"), bValue);
}

bool AppReaderSettings::IsHighlightingVocabulary() const
{
    return FTSettings::Get().OpenStore().value(HighlightVocabularyKey,
        FTSettings::GetShippedDefault("reader/highlightVocabulary", false)).toBool();
}

void AppReaderSettings::SetHighlightVocabulary(bool bValue)
{
    Write(HighlightVocabularyKey, bValue);
}

QString AppReaderSettings::GetLibrarySortOrder() const
{
    return FTSettings::Get().OpenStore().value(LibrarySortOrderKey, QStringLiteral("recent")).toString();
}

void AppReaderSettings::SetLibrarySortOrder(const QString& Value)
{
    Write(LibrarySortOrderKey, Value);
}

bool AppReaderSettings::IsLibrarySortReversed() const
{
    return FTSettings::Get().OpenStore().value(LibrarySortReversedKey, false).toBool();
}

void AppReaderSettings::SetLibrarySortReversed(bool bValue)
{
    Write(LibrarySortReversedKey, bValue);
}
