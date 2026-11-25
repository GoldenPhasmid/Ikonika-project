#pragma once
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <QDebug>

class FTSharedAnimation
{
    QJsonObject Character;
    QJsonObject Clip;
    int HoldFrame{0};

    void Flatten(const QJsonValue& Value, QVector<double>& Numbers) const
    {
        if (Value.isObject())
        {
            auto Object{Value.toObject()};
            for (const auto& Name : {"v", "i", "o"}) Flatten(Object[Name], Numbers);
            return;
        }
        if (Value.isArray())
        {
            for (const auto& Item : Value.toArray()) Flatten(Item, Numbers);
            return;
        }
        Numbers.append(Value.toDouble());
    }

    QJsonValue Restore(const QJsonValue& Template, const QVector<double>& Numbers, int& Index) const
    {
        if (Template.isObject())
        {
            auto Result{Template.toObject()};
            for (const auto& Name : {"v", "i", "o"}) Result[Name] = Restore(Result[Name], Numbers, Index);
            return Result;
        }
        if (Template.isArray())
        {
            QJsonArray Result;
            for (const auto& Item : Template.toArray()) Result.append(Restore(Item, Numbers, Index));
            return Result;
        }
        return std::round(Numbers[Index++] * 1000.0) / 1000.0;
    }

    QJsonObject Animate(const QJsonArray& Values) const
    {
        bool Constant{true};
        for (const auto& Value : Values) Constant = Constant && Value == Values.first();
        if (Constant)
        {
            auto Value{Values.first()};
            if (Value.isArray() && Value.toArray().size() == 1) Value = Value.toArray().first();
            return {{"a", 0}, {"k", Value}};
        }
        QJsonArray Keys;
        if (!Values.first().isObject())
        {
            for (int Frame{0}; Frame < Values.size(); ++Frame)
            {
                auto Value{Values[Frame]};
                Keys.append(QJsonObject{{"t", Frame + 1}, {"s", Value}, {"e", Value}, {"o", QJsonObject{{"x", 0}, {"y", 0}}}, {"i", QJsonObject{{"x", 1}, {"y", 1}}}});
            }
            Keys.append(QJsonObject{{"t", Values.size() + 1}});
            return {{"a", 1}, {"k", Keys}};
        }
        for (int Frame{0}; Frame < Values.size(); ++Frame)
        {
            Keys.append(QJsonObject{{"t", Frame + 1}, {"s", QJsonArray{Values[Frame]}}, {"h", 1}});
        }
        Keys.append(QJsonObject{{"t", Values.size() + 1}, {"s", QJsonArray{Values.last()}}, {"h", 1}});
        return {{"a", 1}, {"k", Keys}};
    }

    QJsonObject ExpandProperty(const QString& Identifier) const
    {
        auto Definition{Character["properties"].toObject()[Identifier].toObject()};
        auto Base{Definition["base"]};
        auto TrackName{Clip["properties"].toObject()[Identifier].toString()};
        if (TrackName.isEmpty())
        {
            auto Value{Base};
            if (Value.isArray() && Value.toArray().size() == 1) Value = Value.toArray().first();
            return {{"a", 0}, {"k", Value}};
        }
        const auto Track = Clip["tracks"].toObject()[TrackName].toArray();
        const auto Basis = Definition["basis"].toArray();
        QVector<double> Reference;
        Flatten(Base, Reference);
        QJsonArray Values;
        int FrameCount{Clip["frames"].toInt()};
        for (int Frame{0}; Frame < FrameCount; ++Frame)
        {
            const auto Controls = Track[HoldFrame ? HoldFrame - 1 : Frame].toArray();
            QVector<double> Numbers{Reference};
            if (Basis.isEmpty())
            {
                for (int Index{0}; Index < Controls.size(); ++Index) Numbers[Index] = Controls[Index].toDouble();
            }
            else
            {
                for (int Control{0}; Control < Controls.size(); ++Control)
                {
                    double Weight{Controls[Control].toDouble()};
                    const auto Direction = Basis[Control].toArray();
                    for (int Index{0}; Index < Numbers.size(); ++Index) Numbers[Index] += Weight * Direction[Index].toDouble();
                }
            }
            int Index{0};
            Values.append(Restore(Base, Numbers, Index));
        }
        return Animate(Values);
    }

    QJsonValue Expand(const QJsonValue& Value) const
    {
        if (Value.isArray())
        {
            QJsonArray Result;
            for (const auto& Item : Value.toArray()) Result.append(Expand(Item));
            return Result;
        }
        if (!Value.isObject()) return Value;
        auto Object{Value.toObject()};
        if (Object.contains("$property")) return ExpandProperty(Object["$property"].toString());
        const auto Original{Object};
        Object = {};
        for (auto Iterator{Original.constBegin()}; Iterator != Original.constEnd(); ++Iterator) Object.insert(Iterator.key(), Expand(Iterator.value()));
        if (Object.contains("a") && Object.contains("p") && Object.contains("s") && Object.contains("r"))
        {
            auto Position{Object["p"].toObject()};
            if (Position["a"].toInt() == 1)
            {
                QJsonArray Horizontal;
                QJsonArray Vertical;
                for (const auto& Item : Position["k"].toArray())
                {
                    auto Key{Item.toObject()};
                    if (!Key.contains("s")) continue;
                    const auto Point = Key["s"].toArray();
                    Horizontal.append(QJsonArray{Point[0]});
                    Vertical.append(QJsonArray{Point[1]});
                }
                Object["p"] = QJsonObject{{"s", true}, {"x", Animate(Horizontal)}, {"y", Animate(Vertical)}};
            }
        }
        if (Object.value("ty").toString() == "gr")
        {
            auto Items = Object["it"].toArray();
            for (int Index{0}; Index < Items.size(); ++Index)
            {
                auto Ellipse{Items[Index].toObject()};
                if (Ellipse.value("ty").toString() != "el" || Ellipse["p"].toObject()["a"].toInt() != 1) continue;
                const QJsonValue Position{Ellipse.value("p")};
                Ellipse["p"] = QJsonObject{{"a", 0}, {"k", QJsonArray{0, 0}}};
                Items[Index] = Ellipse;
                for (int TransformIndex{0}; TransformIndex < Items.size(); ++TransformIndex)
                {
                    auto Transform{Items[TransformIndex].toObject()};
                    if (Transform["ty"].toString() != "tr") continue;
                    Transform["p"] = Position;
                    Items[TransformIndex] = Expand(Transform);
                }
            }
            Object["it"] = Items;
        }
        return Object;
    }

public:
    FTSharedAnimation(QJsonObject CharacterData, QJsonObject ClipData, int RestFrame = 0)
        : Character{CharacterData}, Clip{ClipData}, HoldFrame{RestFrame}
    {
    }

    QJsonObject Compose(const QString& Eyes) const
    {
        auto Result{Character["composition"].toObject()};
        auto Depths{Clip["depths"].toObject()};
        auto Visibility{Clip["visibility"].toObject()};
        int FrameCount{Clip["frames"].toInt()};
        auto FrameIndex = [this](int Frame) { return HoldFrame ? HoldFrame - 1 : Frame; };
        auto Name = [](const QJsonObject& Layer) { return Layer["nm"].toString().section(" | ", 0, 0); };
        auto Depth = [&](const QString& LayerName, int Frame) { return Depths.value(LayerName).toArray().at(FrameIndex(Frame)).toDouble(); };
        QJsonArray Selected;
        for (const auto& Item : Character["layers"].toArray())
        {
            auto LayerName{Name(Item.toObject())};
            if (LayerName.startsWith("Eyes "))
            {
                bool Used{LayerName == Eyes};
                if (Eyes == "Eyes Authored")
                {
                    const auto Track = Visibility.value(LayerName).toArray();
                    for (int Frame{0}; Frame < FrameCount; ++Frame) Used = Used || Track.at(FrameIndex(Frame)).toBool();
                }
                if (!Used) continue;
            }
            Selected.append(Item);
        }
        const auto Source = Expand(Selected).toArray();
        QVector<QVector<int>> Orders;
        for (int Frame{0}; Frame < FrameCount; ++Frame)
        {
            QVector<int> Order;
            for (int Index{0}; Index < Source.size(); ++Index) Order.append(Index);
            std::stable_sort(Order.begin(), Order.end(), [&](int A, int B)
            {
                return Depth(Name(Source[A].toObject()), Frame) > Depth(Name(Source[B].toObject()), Frame);
            });
            Orders.append(Order);
        }
        QJsonArray Layers;
        for (int Position{0}; Position < Source.size(); ++Position)
        {
            for (int Index{0}; Index < Source.size(); ++Index)
            {
                auto Layer{Source[Index].toObject()};
                auto LayerName{Name(Layer)};
                QJsonArray Opacity;
                bool Used{false};
                for (int Frame{0}; Frame < FrameCount; ++Frame)
                {
                    bool Visible{Visibility.value(LayerName).toArray().at(FrameIndex(Frame)).toBool()};
                    if (LayerName.startsWith("Eyes ") && Eyes != "Eyes Authored") Visible = LayerName == Eyes;
                    Visible = Visible && Orders[Frame][Position] == Index;
                    Used = Used || Visible;
                    Opacity.append(QJsonArray{Visible ? 100 : 0});
                }
                if (!Used) continue;
                auto Transform{Layer["ks"].toObject()};
                Transform["o"] = Animate(Opacity);
                Layer["ks"] = Transform;
                Layer["nm"] = Layer["nm"].toString() + " depth " + QString::number(Position);
                Layer["ind"] = Layers.size() + 1;
                Layer["ip"] = 1;
                Layer["op"] = FrameCount + 1;
                Layers.append(Layer);
            }
        }
        Result["layers"] = Layers;
        Result["ip"] = 1;
        Result["op"] = FrameCount + 1;
        Result["fr"] = Clip["fps"];
        return Result;
    }
};
