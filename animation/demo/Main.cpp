#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QTextStream>
#include <QtQml/qqml.h>
#include "AnimationSource.h"
#include "LufiAnimationItem.h"

int main(int ArgumentCount, char* Arguments[])
{
    QGuiApplication Application{ArgumentCount, Arguments};
    QCommandLineParser Parser;
    Parser.addHelpOption();
    Parser.addOption({"assets", "Folder containing lufi-shared.zip", "folder"});
    Parser.addOption({"clip", "Clip identifier for export", "id"});
    Parser.addOption({"eyes", "Eye layer for export", "name", "Eyes Authored"});
    Parser.addOption({"export-shared", "Write assembled Lottie JSON and exit", "file"});
    Parser.addOption({"inspect", "Print available clips and exit"});
    Parser.process(Application);
    QDir Folder{QCoreApplication::applicationDirPath()};
    if (Parser.isSet("assets")) Folder = QDir{Parser.value("assets")};
    else
    {
        while (!Folder.exists("lufi-shared.zip") && Folder.cdUp()) {}
    }
    AnimationSource Source{Folder.absolutePath()};
    bool Loaded{Source.Reload()};
    if (Parser.isSet("inspect"))
    {
        QTextStream{stdout} << QJsonDocument{QJsonArray::fromVariantList(Source.GetClips())}.toJson(QJsonDocument::Compact) << Qt::endl;
        return Loaded ? 0 : 1;
    }
    if (Parser.isSet("export-shared"))
    {
        if (!Loaded) return 1;
        auto Identifier{Parser.value("clip")};
        if (Identifier.isEmpty()) Identifier = Source.GetClips().first().toMap()["id"].toString();
        auto Url{Source.Select(Parser.value("eyes"), Identifier)};
        QFile::remove(Parser.value("export-shared"));
        return !Url.isEmpty() && QFile::copy(Url.toLocalFile(), Parser.value("export-shared")) ? 0 : 1;
    }
    qmlRegisterType<FTLufiAnimationItem>("Ikonika.Animation", 1, 0, "LufiAnimation");
    QQmlApplicationEngine Engine;
    Engine.rootContext()->setContextProperty("AnimationSource", &Source);
    Engine.load(QUrl{QStringLiteral("qrc:/main.qml")});
    if (Engine.rootObjects().isEmpty()) return 1;
    return Application.exec();
}
