#include "AppLocalization.h"

#include "AppLog.h"

#include "core/FTLog.h"
#include "core/FTSettings.h"

#include <QCoreApplication>
#include <QQmlEngine>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace
{
    constexpr const char* LanguageKey = "appearance/language";
    constexpr int DefaultLanguage = 0;

    QQmlEngine* GEngine = nullptr;

    const QStringList LanguageFiles
    {
        QStringLiteral(":/i18n/Ikonika_en.qm"),
        QStringLiteral(":/i18n/Ikonika_uk.qm")
    };

    const QStringList LanguageLocales
    {
        QStringLiteral("en_US"),
        QStringLiteral("uk_UA")
    };

    struct FLanguageName
    {
        const char* Code;
        const char* Name;
    };

    constexpr FLanguageName LanguageNames[]
    {
        {"en", QT_TRANSLATE_NOOP("Languages", "English")},
        {"es", QT_TRANSLATE_NOOP("Languages", "Spanish")},
        {"fr", QT_TRANSLATE_NOOP("Languages", "French")},
        {"de", QT_TRANSLATE_NOOP("Languages", "German")},
        {"it", QT_TRANSLATE_NOOP("Languages", "Italian")},
        {"pt", QT_TRANSLATE_NOOP("Languages", "Portuguese")},
        {"uk", QT_TRANSLATE_NOOP("Languages", "Ukrainian")},
        {"ru", QT_TRANSLATE_NOOP("Languages", "Russian")},
        {"pl", QT_TRANSLATE_NOOP("Languages", "Polish")},
        {"nl", QT_TRANSLATE_NOOP("Languages", "Dutch")},
        {"sv", QT_TRANSLATE_NOOP("Languages", "Swedish")},
        {"el", QT_TRANSLATE_NOOP("Languages", "Greek")},
        {"cs", QT_TRANSLATE_NOOP("Languages", "Czech")},
        {"tr", QT_TRANSLATE_NOOP("Languages", "Turkish")},
        {"ar", QT_TRANSLATE_NOOP("Languages", "Arabic")},
        {"hi", QT_TRANSLATE_NOOP("Languages", "Hindi")},
        {"zh", QT_TRANSLATE_NOOP("Languages", "Chinese")},
        {"ja", QT_TRANSLATE_NOOP("Languages", "Japanese")},
        {"ko", QT_TRANSLATE_NOOP("Languages", "Korean")},
        {"vi", QT_TRANSLATE_NOOP("Languages", "Vietnamese")}
    };

    bool IsValidLanguageIndex(int Index)
    {
        return Index >= 0 && Index < LanguageFiles.length();
    }
    
    QTranslator& GetActiveTranslator()
    {
        static QTranslator Instance;
        return Instance;
    }

    void ApplyLanguage(int Language)
    {
        QTranslator& Translator = GetActiveTranslator();

        QCoreApplication::removeTranslator(&Translator);

        if (IsValidLanguageIndex(Language))
        {
            QLocale::setDefault(QLocale(LanguageLocales[Language]));
        }
        
        if (Language == DefaultLanguage || !IsValidLanguageIndex(Language))
        {
            return;
        }
        
        if (Translator.load(LanguageFiles[Language]))
        {
            QCoreApplication::installTranslator(&Translator);
        }
    }
}

QString LanguageDisplayName(const std::string& Code)
{
    for (const FLanguageName& Language : LanguageNames)
    {
        if (Code == Language.Code)
        {
            return QCoreApplication::translate("Languages", Language.Name);
        }
    }

    const QLocale Locale{QString::fromStdString(Code)};

    if (Locale.language() == QLocale::C)
    {
        qCWarning(LogApp) << "No display name for language" << QString::fromStdString(Code);

        return QString::fromStdString(Code);
    }

    qCInfo(LogApp) << "Language" << QString::fromStdString(Code) << "has no translated name, using Qt's English one";

    return QLocale::languageToString(Locale.language());
}


AppLocalization::AppLocalization(QObject* Parent)
    : QObject(Parent)
{

}

void AppLocalization::ApplySavedLanguage()
{
    const QSettings Settings(FTSettings::Get().OpenStore());

    const int SavedLanguage = Settings.value(LanguageKey, DefaultLanguage).toInt();
    SetLanguageIndex(SavedLanguage);
}

AppLocalization& AppLocalization::Get()
{
    static AppLocalization Instance;
    return Instance;
}

void AppLocalization::SetEngine(QQmlEngine* Engine)
{
    GEngine = Engine;
}

int AppLocalization::GetLanguageIndex() const
{
    return CurrentLanguage;
}

QString AppLocalization::GetLocaleName() const
{
    return LanguageLocales[CurrentLanguage];
}

QStringList AppLocalization::GetLanguageNames() const
{
    return {tr("English"), tr("Українська")};
}

void AppLocalization::SetLanguageIndex(int NewLanguage)
{
    if (NewLanguage == CurrentLanguage)
    {
        return;
    }

    if (!IsValidLanguageIndex(NewLanguage))
    {
        NewLanguage = 0;
    }
    
    CurrentLanguage = NewLanguage;

    QSettings Settings(FTSettings::Get().OpenStore());
    Settings.setValue(LanguageKey, CurrentLanguage);

    ApplyLanguage(CurrentLanguage);
    
    if (GEngine != nullptr)
    {
        GEngine->retranslate();
    }
    else
    {
        qCWarning(LogApp) << "Language changed to" << CurrentLanguage
                          << "with no engine to retranslate; the interface will not update until "
                             "the app is restarted";
    }

    emit languageChanged();
}
