#pragma once

#include <QJSEngine>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QtQml/qqmlregistration.h>

class AppReaderSettings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int textSize READ GetTextSize WRITE SetTextSize NOTIFY changed)
    Q_PROPERTY(int defaultTextSize READ GetDefaultTextSize CONSTANT)
    Q_PROPERTY(int minimumTextSize READ GetMinimumTextSize CONSTANT)
    Q_PROPERTY(int maximumTextSize READ GetMaximumTextSize CONSTANT)
    Q_PROPERTY(int lineSpacing READ GetLineSpacing WRITE SetLineSpacing NOTIFY changed)
    Q_PROPERTY(int minimumLineSpacing READ GetMinimumLineSpacing CONSTANT)
    Q_PROPERTY(int maximumLineSpacing READ GetMaximumLineSpacing CONSTANT)
    Q_PROPERTY(int textMargin READ GetTextMargin WRITE SetTextMargin NOTIFY changed)
    Q_PROPERTY(int minimumTextMargin READ GetMinimumTextMargin CONSTANT)
    Q_PROPERTY(int maximumTextMargin READ GetMaximumTextMargin CONSTANT)
    Q_PROPERTY(int pageWidth READ GetPageWidth WRITE SetPageWidth NOTIFY changed)
    Q_PROPERTY(int minimumPageWidth READ GetMinimumPageWidth CONSTANT)
    Q_PROPERTY(int maximumPageWidth READ GetMaximumPageWidth CONSTANT)
    Q_PROPERTY(QString pageTone READ GetPageTone WRITE SetPageTone NOTIFY changed)
    Q_PROPERTY(QString textTone READ GetTextTone WRITE SetTextTone NOTIFY changed)
    Q_PROPERTY(QString textFont READ GetTextFont WRITE SetTextFont NOTIFY changed)
    Q_PROPERTY(QStringList textFonts READ GetTextFonts CONSTANT)
    Q_PROPERTY(bool cropMargins READ IsCropMargins WRITE SetCropMargins NOTIFY changed)
    Q_PROPERTY(bool volumeKeysTurnPages READ AreVolumeKeysTurningPages WRITE SetVolumeKeysTurnPages NOTIFY changed)
    Q_PROPERTY(bool highlightVocabulary READ IsHighlightingVocabulary WRITE SetHighlightVocabulary NOTIFY changed)
    Q_PROPERTY(QString librarySortOrder READ GetLibrarySortOrder WRITE SetLibrarySortOrder NOTIFY changed)
    Q_PROPERTY(bool librarySortReversed READ IsLibrarySortReversed WRITE SetLibrarySortReversed NOTIFY changed)

public:
    static AppReaderSettings& Get();

    explicit AppReaderSettings(QObject* Parent = nullptr);

    int GetTextSize() const;
    void SetTextSize(int Value);
    int GetDefaultTextSize() const;
    int GetMinimumTextSize() const;
    int GetMaximumTextSize() const;

    int GetLineSpacing() const;
    void SetLineSpacing(int Value);
    int GetMinimumLineSpacing() const;
    int GetMaximumLineSpacing() const;

    int GetTextMargin() const;
    void SetTextMargin(int Value);
    int GetMinimumTextMargin() const;
    int GetMaximumTextMargin() const;

    int GetPageWidth() const;
    void SetPageWidth(int Value);
    int GetMinimumPageWidth() const;
    int GetMaximumPageWidth() const;

    QString GetPageTone() const;
    void SetPageTone(const QString& Value);

    QString GetTextTone() const;
    void SetTextTone(const QString& Value);

    QString GetTextFont() const;
    void SetTextFont(const QString& Value);
    QStringList GetTextFonts() const;

    bool IsCropMargins() const;
    void SetCropMargins(bool bValue);

    bool AreVolumeKeysTurningPages() const;
    void SetVolumeKeysTurnPages(bool bValue);

    bool IsHighlightingVocabulary() const;
    void SetHighlightVocabulary(bool bValue);

    QString GetLibrarySortOrder() const;
    void SetLibrarySortOrder(const QString& Value);

    bool IsLibrarySortReversed() const;
    void SetLibrarySortReversed(bool bValue);

signals:
    void changed();

private:
    void Write(const QString& Key, const QVariant& Value);
};

struct AppReaderSettingsSingleton
{
    Q_GADGET
    QML_FOREIGN(AppReaderSettings)
    QML_SINGLETON
    QML_NAMED_ELEMENT(AppReaderSettings)

public:
    static AppReaderSettings* create(QQmlEngine* Engine, QJSEngine*)
    {
        Q_UNUSED(Engine);

        AppReaderSettings& Instance = AppReaderSettings::Get();

        QJSEngine::setObjectOwnership(&Instance, QJSEngine::CppOwnership);

        return &Instance;
    }
};
