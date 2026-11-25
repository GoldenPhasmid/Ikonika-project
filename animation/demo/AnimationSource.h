#pragma once
#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QUrl>
#include <QDir>
#include <QVariantList>
#include <QMap>
#include <optional>
#include <utility>
#include <QtCore/private/qzipreader_p.h>
#include "SharedAnimation.h"

class AnimationSource : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList Clips READ GetClips NOTIFY Changed)
    Q_PROPERTY(QStringList Eyes READ GetEyes NOTIFY Changed)
    Q_PROPERTY(QString Error READ GetError NOTIFY Changed)
    QString AssetFolder;
    std::optional<QTemporaryDir> ExportDirectory;
    QJsonObject Character;
    QMap<QString, QJsonObject> ClipData;
    QVariantList Clips;
    QStringList Eyes;
    QString Error;

    int ScalarCount(const QJsonValue& Value) const
    {
        if (Value.isDouble()) return 1;
        int Count{0};
        if (Value.isArray())
        {
            for (const auto& Child : Value.toArray()) Count += ScalarCount(Child);
        }
        if (Value.isObject())
        {
            for (const auto& Key : {"v", "i", "o"}) Count += ScalarCount(Value.toObject()[Key]);
        }
        return Count;
    }

    bool Fail(const QString& Message)
    {
        Error = Message;
        qWarning().noquote() << Error;
        emit Changed();
        return false;
    }

public:
    explicit AnimationSource(QString Folder) : AssetFolder{std::move(Folder)} {}
    QVariantList GetClips() const { return Clips; }
    QStringList GetEyes() const { return Eyes; }
    QString GetError() const { return Error; }
    const QJsonObject& GetCharacter() const { return Character; }
    const QMap<QString, QJsonObject>& GetClipData() const { return ClipData; }

    Q_INVOKABLE bool Reload()
    {
        ExportDirectory.reset();
        Character = {};
        ClipData.clear();
        Clips.clear();
        Eyes = {"Eyes Authored"};
        Error.clear();
        QFile Input{QDir{AssetFolder}.filePath("lufi-shared.zip")};
        if (!Input.open(QIODevice::ReadOnly)) return Fail("Cannot open " + Input.fileName());
        QZipReader Bundle{&Input};
        Character = QJsonDocument::fromJson(Bundle.fileData("character.json")).object();
        if (Character["version"].toInt() != 1 || Character["layers"].toArray().isEmpty() || Character["composition"].toObject()["w"].toInt() < 1 || Character["composition"].toObject()["h"].toInt() < 1) return Fail("Invalid character.json");
        for (const auto& Entry : Bundle.fileInfoList())
        {
            if (!Entry.isFile || !Entry.filePath.endsWith(".json") || Entry.filePath == "character.json") continue;
            auto Clip{QJsonDocument::fromJson(Bundle.fileData(Entry.filePath)).object()};
            auto Identifier{QFileInfo{Entry.filePath}.completeBaseName()};
            if (Clip["version"].toInt() != 1 || Clip["frames"].toInt() < 1 || Clip["fps"].toDouble() <= 0) return Fail("Invalid clip: " + Identifier);
            ClipData.insert(Identifier, Clip);
        }
        for (auto Iterator{ClipData.constBegin()}; Iterator != ClipData.constEnd(); ++Iterator)
        {
            auto Clip{Iterator.value()};
            auto Resolved{Clip};
            if (Clip.contains("reference"))
            {
                Resolved = ClipData.value(Clip["reference"].toString());
                if (Resolved.isEmpty() || Resolved.contains("reference") || Clip["hold"].toInt() < 1 || Clip["hold"].toInt() > Resolved["frames"].toInt()) return Fail("Invalid pose reference: " + Iterator.key());
            }
            int Frames{Resolved["frames"].toInt()};
            auto Visibility{Resolved["visibility"].toObject()};
            auto Depths{Resolved["depths"].toObject()};
            for (const auto& Item : Character["layers"].toArray())
            {
                auto Name{Item.toObject()["nm"].toString().section(" | ", 0, 0)};
                if (Visibility[Name].toArray().size() != Frames || Depths[Name].toArray().size() != Frames) return Fail("Invalid visibility/depth: " + Iterator.key() + "/" + Name);
                if (Name.startsWith("Eyes ") && !Eyes.contains(Name)) Eyes.append(Name);
            }
            auto Tracks{Resolved["tracks"].toObject()};
            auto Properties{Resolved["properties"].toObject()};
            auto Definitions{Character["properties"].toObject()};
            for (auto Property{Properties.constBegin()}; Property != Properties.constEnd(); ++Property)
            {
                if (!Definitions.contains(Property.key())) return Fail("Unknown property: " + Property.key());
                const auto Track = Tracks[Property.value().toString()].toArray();
                if (Track.size() != Frames) return Fail("Invalid track: " + Property.key());
                auto Definition{Definitions[Property.key()].toObject()};
                int Count{ScalarCount(Definition["base"])};
                if (Count < 1) return Fail("Invalid property base: " + Property.key());
                const auto Basis = Definition["basis"].toArray();
                for (const auto& Direction : Basis)
                {
                    if (!Direction.isArray() || Direction.toArray().size() != Count || ScalarCount(Direction) != Count) return Fail("Invalid basis: " + Property.key());
                }
                for (const auto& Row : Track)
                {
                    if (!Row.isArray() || ScalarCount(Row) != Row.toArray().size() || (Basis.isEmpty() ? Row.toArray().size() != Count : Row.toArray().size() > Basis.size())) return Fail("Invalid controls: " + Property.key());
                }
            }
            auto Composition{Character["composition"].toObject()};
            Clips.append(QVariantMap{{"id", Iterator.key()}, {"name", Clip["name"].toString(Iterator.key())}, {"frames", Clip.contains("hold") ? 1 : Frames}, {"fps", Resolved["fps"].toDouble()}, {"width", Composition["w"].toInt()}, {"height", Composition["h"].toInt()}});
        }
        if (Clips.isEmpty()) return Fail("No clips in lufi-shared.zip");
        emit Changed();
        return true;
    }

    Q_INVOKABLE QString CaptureFile(const QString& Name) const
    {
        QDir Folder{QDir{AssetFolder}.filePath("local/captures")};
        Folder.mkpath(".");
        return Folder.absoluteFilePath(Name + ".png");
    }

    Q_INVOKABLE QUrl Select(const QString& EyesName, const QString& ClipName)
    {
        if (!Error.isEmpty() || !ClipData.contains(ClipName)) return {};
        if (!ExportDirectory) ExportDirectory.emplace();
        if (!ExportDirectory->isValid()) return {};
        const auto Path{ExportDirectory->filePath(ClipName + "-" + EyesName + ".json")};
        if (QFile::exists(Path)) return QUrl::fromLocalFile(Path);
        auto Clip{ClipData.value(ClipName)};
        int HoldFrame{Clip["hold"].toInt()};
        if (Clip.contains("reference")) Clip = ClipData.value(Clip["reference"].toString());
        FTSharedAnimation Animation{Character, Clip, HoldFrame};
        auto Root{Animation.Compose(EyesName)};
        if (Root.isEmpty()) return {};
        QFile Output{Path};
        if (!Output.open(QIODevice::WriteOnly)) return {};
        Output.write(QJsonDocument{Root}.toJson(QJsonDocument::Compact));
        Output.close();
        return QUrl::fromLocalFile(Path);
    }

signals:
    void Changed();
};
