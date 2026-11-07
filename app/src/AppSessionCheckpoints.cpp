#include "AppSessionCheckpoints.h"

#include <QtGlobal>

#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#elif defined(Q_OS_IOS)
#include "AppPaths.h"

#include "core/FTLog.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QString>
#endif

#if defined(Q_OS_ANDROID)
namespace
{
    constexpr const char* AndroidSessionCheckpointsClass =
        "com/goldenphasmid/ikonika/SessionCheckpoints";
}
#elif defined(Q_OS_IOS)
namespace
{
    QString StoreFilePath()
    {
        return AppPaths::DataRoot() + QStringLiteral("/session-checkpoints.json");
    }

    QString KeyFor(const std::string& Kind, const std::string& SessionKey)
    {
        return QString::fromStdString(Kind) + QLatin1Char('|') + QString::fromStdString(SessionKey);
    }

    QJsonObject ReadStore()
    {
        QFile File(StoreFilePath());

        if (!File.open(QIODevice::ReadOnly))
        {
            return {};
        }

        return QJsonDocument::fromJson(File.readAll()).object();
    }

    void WriteStore(const QJsonObject& Store)
    {
        QSaveFile File(StoreFilePath());

        if (!File.open(QIODevice::WriteOnly))
        {
            qCWarning(LogApp) << "Session checkpoints: could not open" << StoreFilePath();

            return;
        }

        File.write(QJsonDocument(Store).toJson(QJsonDocument::Compact));

        if (!File.commit())
        {
            qCWarning(LogApp) << "Session checkpoints: could not write" << StoreFilePath();
        }
    }
}
#endif

void AppSessionCheckpoints::Save(const std::string& Kind, const std::string& SessionKey,
                                 const std::string& StateJson)
{
#if defined(Q_OS_ANDROID)
    QJniObject Activity = QNativeInterface::QAndroidApplication::context();
    QJniObject JniKind = QJniObject::fromString(QString::fromStdString(Kind));
    QJniObject JniSessionKey = QJniObject::fromString(QString::fromStdString(SessionKey));
    QJniObject JniJson = QJniObject::fromString(QString::fromStdString(StateJson));

    QJniObject::callStaticMethod<void>(AndroidSessionCheckpointsClass, "save",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        Activity.object<jobject>(), JniKind.object<jstring>(), JniSessionKey.object<jstring>(),
        JniJson.object<jstring>());
#elif defined(Q_OS_IOS)
    QJsonObject Store{ReadStore()};
    Store.insert(KeyFor(Kind, SessionKey), QString::fromStdString(StateJson));

    WriteStore(Store);
#else
    Q_UNUSED(Kind);
    Q_UNUSED(SessionKey);
    Q_UNUSED(StateJson);
#endif
}

void AppSessionCheckpoints::Clear(const std::string& Kind, const std::string& SessionKey)
{
#if defined(Q_OS_ANDROID)
    QJniObject Activity = QNativeInterface::QAndroidApplication::context();
    QJniObject JniKind = QJniObject::fromString(QString::fromStdString(Kind));
    QJniObject JniSessionKey = QJniObject::fromString(QString::fromStdString(SessionKey));

    QJniObject::callStaticMethod<void>(AndroidSessionCheckpointsClass, "clear",
        "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)V",
        Activity.object<jobject>(), JniKind.object<jstring>(), JniSessionKey.object<jstring>());
#elif defined(Q_OS_IOS)
    QJsonObject Store{ReadStore()};

    if (Store.contains(KeyFor(Kind, SessionKey)))
    {
        Store.remove(KeyFor(Kind, SessionKey));

        WriteStore(Store);
    }
#else
    Q_UNUSED(Kind);
    Q_UNUSED(SessionKey);
#endif
}

std::vector<std::pair<std::string, std::string>> AppSessionCheckpoints::LoadAll(
    const std::string& Kind)
{
    std::vector<std::pair<std::string, std::string>> Result;

#if defined(Q_OS_ANDROID)
    QJniObject Activity = QNativeInterface::QAndroidApplication::context();
    QJniObject JniKind = QJniObject::fromString(QString::fromStdString(Kind));

    QJniObject Reply = QJniObject::callStaticObjectMethod(AndroidSessionCheckpointsClass,
        "loadAll", "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;",
        Activity.object<jobject>(), JniKind.object<jstring>());

    const QJsonDocument Document = QJsonDocument::fromJson(Reply.toString().toUtf8());

    if (Document.isObject())
    {
        const QJsonObject Root = Document.object();

        for (auto It = Root.constBegin(); It != Root.constEnd(); ++It)
        {
            Result.emplace_back(It.key().toStdString(), It.value().toString().toStdString());
        }
    }
#elif defined(Q_OS_IOS)
    const QJsonObject Store{ReadStore()};
    const QString Prefix{QString::fromStdString(Kind) + QLatin1Char('|')};

    for (auto It = Store.constBegin(); It != Store.constEnd(); ++It)
    {
        if (It.key().startsWith(Prefix))
        {
            Result.emplace_back(It.key().mid(Prefix.size()).toStdString(), It.value().toString().toStdString());
        }
    }
#else
    Q_UNUSED(Kind);
#endif

    return Result;
}
