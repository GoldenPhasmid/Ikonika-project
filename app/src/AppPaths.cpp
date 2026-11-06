#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>
#endif

#if defined(Q_OS_WIN)
#include <windows.h>
#include <appmodel.h>
#endif

namespace
{
    bool bHeadlessSandbox = false;

    QString HeadlessRoot()
    {
        return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
            .filePath(QStringLiteral("Ikonika-headless"));
    }

    void WipeHeadlessRoot(bool bKeepDictionaries)
    {
        const QString Root = HeadlessRoot();

        if (!bKeepDictionaries)
        {
            QDir(Root).removeRecursively();

            return;
        }

        const QFileInfoList Entries = QDir(Root).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);

        for (const QFileInfo& Entry : Entries)
        {
            if (Entry.fileName() == QStringLiteral("dictionaries"))
            {
                continue;
            }

            if (Entry.isDir())
            {
                QDir(Entry.absoluteFilePath()).removeRecursively();

                continue;
            }

            QFile::remove(Entry.absoluteFilePath());
        }
    }

#if defined(QT_DEBUG) && defined(IKONIKA_DESKTOP)
    QString DebugRoot()
    {
        return QDir(QStringLiteral(PROJECT_ROOT_DIRECTORY)).filePath(QStringLiteral("local"));
    }
#endif

    QString RoamingRoot()
    {
        if (bHeadlessSandbox)
        {
            return HeadlessRoot();
        }

#if defined(QT_DEBUG) && defined(IKONIKA_DESKTOP)
        return DebugRoot();
#else
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
    }

    QString LocalRoot()
    {
        if (bHeadlessSandbox)
        {
            return HeadlessRoot();
        }

#if defined(QT_DEBUG) && defined(IKONIKA_DESKTOP)
        return DebugRoot();
#else
        return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#endif
    }

#if defined(Q_OS_ANDROID)
    QString ExternalFilesRoot()
    {
        QJniObject Context = QNativeInterface::QAndroidApplication::context();
        QJniObject Directory = Context.callObjectMethod(
            "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;",
            static_cast<jstring>(nullptr));

        if (!Directory.isValid())
        {
            return LocalRoot();
        }

        return Directory.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;").toString();
    }
#endif
}

namespace AppPaths
{
    void EnableHeadlessSandbox(bool bKeepDictionaries)
    {
        WipeHeadlessRoot(bKeepDictionaries);

        bHeadlessSandbox = true;
    }

    bool IsHeadless()
    {
        return bHeadlessSandbox;
    }

    bool IsStorePackage()
    {
#if defined(Q_OS_WIN)
        UINT32 Length = 0;

        return GetCurrentPackageFullName(&Length, nullptr) != APPMODEL_ERROR_NO_PACKAGE;
#else
        return false;
#endif
    }

    void Install()
    {
        QDir().mkpath(RoamingRoot());

        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, RoamingRoot());
    }

    QString DatabaseFilePath()
    {
        return QDir(RoamingRoot()).filePath(QStringLiteral("Ikonika.db"));
    }

    QString DataRoot()
    {
        return LocalRoot();
    }

    QString InstalledDictionariesDirectory()
    {
        return QDir(DownloadRoot()).filePath(QStringLiteral("dictionaries"));
    }

    QString DownloadRoot()
    {
        return LocalRoot();
    }

    QString DownloadedModelsDirectory()
    {
        return QDir(DownloadRoot()).filePath(QStringLiteral("models"));
    }

    QString DownloadScratchDirectory()
    {
        return QDir(DownloadRoot()).filePath(QStringLiteral("downloads"));
    }

    QString PendingUpdateDirectory()
    {
        return QDir(DownloadRoot()).filePath(QStringLiteral("updates"));
    }

    QString LogsDirectory()
    {
#if defined(Q_OS_ANDROID)
        if (!bHeadlessSandbox)
        {
            return QDir(ExternalFilesRoot()).filePath(QStringLiteral("logs"));
        }
#elif defined(Q_OS_IOS)
        if (!bHeadlessSandbox)
        {
            return QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
                .filePath(QStringLiteral("logs"));
        }
#endif
        return QDir(LocalRoot()).filePath(QStringLiteral("logs"));
    }
}
