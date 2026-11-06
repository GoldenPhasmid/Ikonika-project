#include "AppSessionCheckpoints.h"

#include <QtGlobal>

#if defined(Q_OS_ANDROID)
#include <QtCore/qcoreapplication_platform.h>
#include <QtCore/qjniobject.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#endif

#if defined(Q_OS_ANDROID)
namespace
{
    constexpr const char* AndroidSessionCheckpointsClass =
        "com/goldenphasmid/ikonika/SessionCheckpoints";
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
#else
    Q_UNUSED(Kind);
#endif

    return Result;
}
