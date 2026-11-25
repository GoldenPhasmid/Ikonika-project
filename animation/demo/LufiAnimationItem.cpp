#include "LufiAnimationItem.h"

#include "AnimationSource.h"
#include <QBitArray>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>
#include <numeric>
#include <optional>

struct FTLufiProperty
{
    QList<qreal> Base;
    QList<QList<qreal>> Directions;
    bool bClosed{false};
};

struct FTLufiValue
{
    qsizetype Property{-1};
    QList<qreal> Constant;
};

struct FTLufiTransform
{
    FTLufiValue Anchor;
    FTLufiValue Position;
    FTLufiValue Rotation;
    FTLufiValue Scale;
    FTLufiValue Opacity;
    FTLufiValue Skew;
    FTLufiValue SkewAxis;
};

struct FTLufiShape
{
    QPainterPath StaticPath;
    qsizetype PathProperty{-1};
    FTLufiValue EllipsePosition;
    FTLufiValue EllipseSize;
    bool bEllipse{false};
    bool bReversed{false};
};

struct FTLufiStyle
{
    FTLufiValue Colour;
    FTLufiValue Opacity;
    FTLufiValue Width;
    Qt::FillRule FillRule{Qt::WindingFill};
    Qt::PenCapStyle Cap{Qt::FlatCap};
    Qt::PenJoinStyle Join{Qt::MiterJoin};
    qreal MiterLimit{4};
};

struct FTLufiGroup
{
    FTLufiTransform Transform;
    QList<FTLufiShape> Shapes;
    std::optional<FTLufiStyle> Fill;
    std::optional<FTLufiStyle> Stroke;
};

struct FTLufiLayer
{
    QString Name;
    FTLufiTransform Transform;
    QList<FTLufiGroup> Groups;
    QList<qsizetype> DayEyes;
    bool bHasNightVariant{false};
    bool bEye{false};
};

struct FTLufiClip
{
    QList<QList<QList<qreal>>> PropertyTracks;
    QList<QBitArray> Visibility;
    QList<QList<qsizetype>> DrawOrder;
    int HeldFrame{0};
};

static void AppendLufiNumbers(const QJsonValue& Value, QList<qreal>& Numbers)
{
    if (Value.isDouble())
    {
        Numbers.append(Value.toDouble());
        return;
    }
    if (Value.isArray())
    {
        const auto Values = Value.toArray();
        for (const auto& Child : Values)
        {
            AppendLufiNumbers(Child, Numbers);
        }
        return;
    }
    if (Value.isObject())
    {
        const auto Path = Value.toObject();
        AppendLufiNumbers(Path.value("v"), Numbers);
        AppendLufiNumbers(Path.value("i"), Numbers);
        AppendLufiNumbers(Path.value("o"), Numbers);
    }
}

static QList<qreal> ReadLufiNumbers(const QJsonValue& Value)
{
    QList<qreal> Numbers;
    AppendLufiNumbers(Value, Numbers);
    return Numbers;
}

static QPainterPath MakeLufiPath(const QList<qreal>& Numbers, bool bClosed, bool bReversed)
{
    if (Numbers.size() < 12 || Numbers.size() % 6 != 0)
    {
        return {};
    }

    const qsizetype Points{Numbers.size() / 6};
    const auto Point = [&Numbers](qsizetype Index)
    {
        return QPointF{Numbers[Index * 2], Numbers[Index * 2 + 1]};
    };
    QPainterPath Path;
    Path.moveTo(Point(0));
    const qsizetype Segments{bClosed ? Points : Points - 1};
    for (qsizetype Index{0}; Index < Segments; ++Index)
    {
        const qsizetype Next{(Index + 1) % Points};
        const QPointF Start{Point(Index)};
        const QPointF End{Point(Next)};
        const QPointF Outgoing{Point(2 * Points + Index)};
        const QPointF Incoming{Point(Points + Next)};
        if (Outgoing.isNull() && Incoming.isNull())
        {
            Path.lineTo(End);
        }
        else
        {
            Path.cubicTo(Start + Outgoing, End + Incoming, End);
        }
    }
    return bReversed ? Path.toReversed() : Path;
}

struct FTLufiFrame
{
    const QList<FTLufiProperty>& Properties;
    const FTLufiClip& Clip;
    qsizetype Index;

    QList<qreal> Evaluate(const FTLufiValue& Value) const
    {
        if (Value.Property < 0)
        {
            return Value.Constant;
        }

        const auto& Property = Properties[Value.Property];
        const auto& Track = Clip.PropertyTracks[Value.Property];
        if (Track.isEmpty())
        {
            return Property.Base;
        }

        const auto& Controls = Track[Index];
        QList<qreal> Result = Property.Directions.isEmpty() ? Controls : Property.Base;
        for (qsizetype Control{0}; Control < Controls.size() && Control < Property.Directions.size(); ++Control)
        {
            const qreal Weight{Controls[Control]};
            if (Weight == 0)
            {
                continue;
            }
            const auto& Direction = Property.Directions[Control];
            for (qsizetype Coordinate{0}; Coordinate < Result.size(); ++Coordinate)
            {
                Result[Coordinate] += Weight * Direction[Coordinate];
            }
        }
        for (qreal& Number : Result)
        {
            Number = std::round(Number * 1000.0) / 1000.0;
        }
        return Result;
    }

    QPointF Point(const FTLufiValue& Value, const QPointF& Default = {}) const
    {
        const auto Numbers = Evaluate(Value);
        return {Numbers.value(0, Default.x()), Numbers.value(1, Default.y())};
    }

    qreal Scalar(const FTLufiValue& Value, qreal Default = 0) const
    {
        return Evaluate(Value).value(0, Default);
    }

    QColor Colour(const FTLufiStyle& Style) const
    {
        const auto Channels = Evaluate(Style.Colour);
        const qreal Alpha = Channels.value(3, 1) * Scalar(Style.Opacity, 100) / 100;
        return QColor::fromRgbF(Channels.value(0), Channels.value(1), Channels.value(2), qBound(0.0, Alpha, 1.0));
    }

    void ApplyTransform(QPainter& Painter, const FTLufiTransform& Value) const
    {
        const QPointF Position{Point(Value.Position)};
        const QPointF Anchor{Point(Value.Anchor)};
        const QPointF Scale{Point(Value.Scale, {100, 100})};
        const qreal Skew{Scalar(Value.Skew)};
        Painter.translate(Position);
        Painter.rotate(Scalar(Value.Rotation));
        if (!qFuzzyIsNull(Skew))
        {
            const qreal Axis{Scalar(Value.SkewAxis)};
            Painter.rotate(Axis);
            Painter.shear(std::tan(qDegreesToRadians(-Skew)), 0);
            Painter.rotate(-Axis);
        }
        Painter.scale(Scale.x() / 100, Scale.y() / 100);
        Painter.translate(-Anchor);
        Painter.setOpacity(Painter.opacity() * qBound(0.0, Scalar(Value.Opacity, 100) / 100, 1.0));
    }

    QPainterPath Path(const FTLufiShape& Shape) const
    {
        if (Shape.bEllipse)
        {
            const QPointF Center{Point(Shape.EllipsePosition)};
            const QPointF Size{Point(Shape.EllipseSize)};
            QPainterPath Ellipse;
            Ellipse.addEllipse(Center, Size.x() / 2, Size.y() / 2);
            return Shape.bReversed ? Ellipse.toReversed() : Ellipse;
        }
        if (Shape.PathProperty < 0)
        {
            return Shape.StaticPath;
        }
        const bool bClosed{Properties[Shape.PathProperty].bClosed};
        return MakeLufiPath(Evaluate({Shape.PathProperty, {}}), bClosed, Shape.bReversed);
    }

    void PaintGroup(QPainter& Painter, const FTLufiGroup& Group) const
    {
        Painter.save();
        ApplyTransform(Painter, Group.Transform);
        QPainterPath Combined;
        for (auto Shape = Group.Shapes.crbegin(); Shape != Group.Shapes.crend(); ++Shape)
        {
            Combined.addPath(Path(*Shape));
        }
        if (Group.Fill)
        {
            Combined.setFillRule(Group.Fill->FillRule);
            Painter.setBrush(Colour(*Group.Fill));
        }
        else
        {
            Painter.setBrush(Qt::NoBrush);
        }
        if (Group.Stroke)
        {
            QPen Stroke{Colour(*Group.Stroke)};
            Stroke.setWidthF(Scalar(Group.Stroke->Width));
            Stroke.setCapStyle(Group.Stroke->Cap);
            Stroke.setJoinStyle(Group.Stroke->Join);
            Stroke.setMiterLimit(Group.Stroke->MiterLimit);
            Painter.setPen(Stroke);
        }
        else
        {
            Painter.setPen(Qt::NoPen);
        }
        Painter.drawPath(Combined);
        Painter.restore();
    }
};

class FTLufiTrackRenderer
{
public:
    explicit FTLufiTrackRenderer(const AnimationSource& Source);
    void Paint(QPainter& Painter, const QString& ClipName, const QString& Eyes, bool bNightEyes, int Frame) const;

private:
    FTLufiValue ReadValue(const QJsonValue& Value) const;
    FTLufiTransform ReadTransform(const QJsonObject& Value, bool bGroup) const;
    FTLufiGroup ReadGroup(const QJsonObject& Value) const;
    FTLufiClip ReadClip(const QJsonObject& Value) const;

    QList<FTLufiProperty> Properties;
    QHash<QString, qsizetype> PropertyIndices;
    QList<FTLufiLayer> Layers;
    QHash<QString, FTLufiClip> Clips;
    QHash<QString, QList<qsizetype>> EyeLayers;
};

FTLufiTrackRenderer::FTLufiTrackRenderer(const AnimationSource& Source)
{
    const auto Definitions = Source.GetCharacter().value("properties").toObject();
    for (auto Entry = Definitions.constBegin(); Entry != Definitions.constEnd(); ++Entry)
    {
        const auto Definition = Entry.value().toObject();
        const auto Base = Definition.value("base");
        FTLufiProperty Property;
        Property.Base = ReadLufiNumbers(Base);
        Property.bClosed = Base.toObject().value("c").toBool();
        const auto Directions = Definition.value("basis").toArray();
        for (const auto& Direction : Directions)
        {
            Property.Directions.append(ReadLufiNumbers(Direction));
        }
        PropertyIndices.insert(Entry.key(), Properties.size());
        Properties.append(std::move(Property));
    }

    const auto SourceLayers = Source.GetCharacter().value("layers").toArray();
    for (const auto& Entry : SourceLayers)
    {
        const auto Definition = Entry.toObject();
        FTLufiLayer Layer;
        Layer.Name = Definition.value("nm").toString().section(" | ", 0, 0);
        Layer.bEye = Layer.Name.startsWith("Eyes ");
        if (Layer.bEye)
        {
            EyeLayers[Layer.Name].append(Layers.size());
        }
        Layer.Transform = ReadTransform(Definition.value("ks").toObject(), false);
        Layer.Transform.Opacity = {};

        const auto Groups = Definition.value("shapes").toArray();
        for (const auto& Group : Groups)
        {
            Layer.Groups.append(ReadGroup(Group.toObject()));
        }
        Layers.append(std::move(Layer));
    }

    const QString NightSuffix{QStringLiteral(" Night")};
    for (auto Eye = EyeLayers.constBegin(); Eye != EyeLayers.constEnd(); ++Eye)
    {
        if (!Eye.key().endsWith(NightSuffix))
        {
            continue;
        }
        const auto DayEyes = EyeLayers.value(Eye.key().chopped(NightSuffix.size()));
        for (const qsizetype Index : DayEyes)
        {
            Layers[Index].bHasNightVariant = true;
        }
        for (const qsizetype Index : Eye.value())
        {
            Layers[Index].DayEyes = DayEyes;
        }
    }

    const auto& SourceClips = Source.GetClipData();
    for (auto Entry = SourceClips.constBegin(); Entry != SourceClips.constEnd(); ++Entry)
    {
        if (!Entry.value().contains("reference"))
        {
            Clips.insert(Entry.key(), ReadClip(Entry.value()));
        }
    }
    for (auto Entry = SourceClips.constBegin(); Entry != SourceClips.constEnd(); ++Entry)
    {
        if (Entry.value().contains("reference"))
        {
            auto Clip = Clips.value(Entry.value().value("reference").toString());
            Clip.HeldFrame = Entry.value().value("hold").toInt();
            Clips.insert(Entry.key(), std::move(Clip));
        }
    }
}

FTLufiValue FTLufiTrackRenderer::ReadValue(const QJsonValue& Value) const
{
    const auto Object = Value.toObject();
    const qsizetype Property{PropertyIndices.value(Object.value("$property").toString(), -1)};
    return {Property, ReadLufiNumbers(Object.value("k"))};
}

FTLufiTransform FTLufiTrackRenderer::ReadTransform(const QJsonObject& Value, bool bGroup) const
{
    return {
        ReadValue(Value.value("a")), ReadValue(Value.value("p")), ReadValue(Value.value("r")),
        ReadValue(Value.value("s")), ReadValue(Value.value("o")),
        bGroup ? ReadValue(Value.value("sk")) : FTLufiValue{},
        bGroup ? ReadValue(Value.value("sa")) : FTLufiValue{}};
}

FTLufiGroup FTLufiTrackRenderer::ReadGroup(const QJsonObject& Value) const
{
    FTLufiGroup Group;
    const auto Items = Value.value("it").toArray();
    for (const auto& Entry : Items)
    {
        const auto Item = Entry.toObject();
        const QString Kind{Item.value("ty").toString()};
        if (Item.value("hd").toBool())
        {
            continue;
        }
        if (Kind == "tr")
        {
            Group.Transform = ReadTransform(Item, true);
        }
        else if (Kind == "sh" || Kind == "el")
        {
            FTLufiShape Shape;
            Shape.bReversed = Item.value("d").toInt() == 3;
            Shape.bEllipse = Kind == "el";
            if (Shape.bEllipse)
            {
                Shape.EllipsePosition = ReadValue(Item.value("p"));
                Shape.EllipseSize = ReadValue(Item.value("s"));
            }
            else
            {
                const auto Data = Item.value("ks");
                const auto Coordinates = ReadValue(Data);
                Shape.PathProperty = Coordinates.Property;
                if (Shape.PathProperty < 0)
                {
                    const bool bClosed = Data.toObject().value("k").toObject().value("c").toBool();
                    Shape.StaticPath = MakeLufiPath(Coordinates.Constant, bClosed, Shape.bReversed);
                }
            }
            Group.Shapes.append(std::move(Shape));
        }
        else if (Kind == "fl" || Kind == "st")
        {
            FTLufiStyle Style;
            Style.Colour = ReadValue(Item.value("c"));
            Style.Opacity = ReadValue(Item.value("o"));
            if (Kind == "fl")
            {
                Style.FillRule = Item.value("r").toInt() == 2 ? Qt::OddEvenFill : Qt::WindingFill;
                Group.Fill = std::move(Style);
            }
            else
            {
                constexpr Qt::PenCapStyle Caps[]{Qt::FlatCap, Qt::RoundCap, Qt::SquareCap};
                constexpr Qt::PenJoinStyle Joins[]{Qt::MiterJoin, Qt::RoundJoin, Qt::BevelJoin};
                Style.Width = ReadValue(Item.value("w"));
                Style.Cap = Caps[qBound(1, Item.value("lc").toInt(1), 3) - 1];
                Style.Join = Joins[qBound(1, Item.value("lj").toInt(1), 3) - 1];
                Style.MiterLimit = Item.value("ml").toDouble(4);
                Group.Stroke = std::move(Style);
            }
        }
        else
        {
            qWarning() << "Unsupported Lufi shape" << Kind << Item.value("nm").toString();
        }
    }
    return Group;
}

FTLufiClip FTLufiTrackRenderer::ReadClip(const QJsonObject& Value) const
{
    FTLufiClip Clip;
    Clip.PropertyTracks.resize(Properties.size());
    QHash<QString, QList<QList<qreal>>> Tracks;
    const auto SourceTracks = Value.value("tracks").toObject();
    for (auto Entry = SourceTracks.constBegin(); Entry != SourceTracks.constEnd(); ++Entry)
    {
        QList<QList<qreal>> Rows;
        const auto SourceRows = Entry.value().toArray();
        Rows.reserve(SourceRows.size());
        for (const auto& Row : SourceRows)
        {
            Rows.append(ReadLufiNumbers(Row));
        }
        Tracks.insert(Entry.key(), std::move(Rows));
    }
    const auto Bindings = Value.value("properties").toObject();
    for (auto Entry = Bindings.constBegin(); Entry != Bindings.constEnd(); ++Entry)
    {
        const qsizetype Property{PropertyIndices.value(Entry.key(), -1)};
        if (Property >= 0)
        {
            Clip.PropertyTracks[Property] = Tracks.value(Entry.value().toString());
        }
    }

    const int Frames{Value.value("frames").toInt()};
    const auto SourceDepths = Value.value("depths").toObject();
    const auto SourceVisibility = Value.value("visibility").toObject();
    QList<QList<qreal>> Depths;
    for (const auto& Layer : Layers)
    {
        Depths.append(ReadLufiNumbers(SourceDepths.value(Layer.Name)));
        const auto Authored = SourceVisibility.value(Layer.Name).toArray();
        QBitArray Visibility{Frames};
        for (int Frame{0}; Frame < Frames; ++Frame)
        {
            Visibility.setBit(Frame, Authored[Frame].toBool());
        }
        Clip.Visibility.append(std::move(Visibility));
    }

    QList<qsizetype> Indices(Layers.size());
    std::iota(Indices.rbegin(), Indices.rend(), qsizetype{0});
    for (int Frame{0}; Frame < Frames; ++Frame)
    {
        auto Order = Indices;
        std::stable_sort(Order.begin(), Order.end(), [&Depths, Frame](qsizetype Left, qsizetype Right)
        {
            return Depths.at(Left).at(Frame) < Depths.at(Right).at(Frame);
        });
        Clip.DrawOrder.append(std::move(Order));
    }
    return Clip;
}

void FTLufiTrackRenderer::Paint(QPainter& Painter, const QString& ClipName, const QString& Eyes, bool bNightEyes, int Frame) const
{
    const auto Found = Clips.constFind(ClipName);
    if (Found == Clips.constEnd() || Found->DrawOrder.isEmpty())
    {
        return;
    }

    const auto& Clip = Found.value();
    const qsizetype RequestedFrame{Clip.HeldFrame ? Clip.HeldFrame : Frame};
    const qsizetype Sample{std::clamp(RequestedFrame, qsizetype{1}, Clip.DrawOrder.size()) - 1};
    const FTLufiFrame State{Properties, Clip, Sample};
    const bool bOverrideEyes{Eyes != "Eyes Authored"};
    const QString NightVariant{bNightEyes && bOverrideEyes ? Eyes + QStringLiteral(" Night") : QString{}};
    const QString& SelectedEyes = EyeLayers.contains(NightVariant) ? NightVariant : Eyes;
    for (qsizetype Index : Clip.DrawOrder[Sample])
    {
        const auto& Layer = Layers[Index];
        bool bVisible{Clip.Visibility[Index].testBit(Sample)};
        if (Layer.bEye && bOverrideEyes)
        {
            bVisible = Layer.Name == SelectedEyes;
        }
        else if (Layer.bEye && bNightEyes)
        {
            bVisible = !Layer.bHasNightVariant && (bVisible || std::any_of(
                Layer.DayEyes.cbegin(), Layer.DayEyes.cend(), [&Clip, Sample](qsizetype DayIndex)
                {
                    return Clip.Visibility[DayIndex].testBit(Sample);
                }));
        }
        if (!bVisible)
        {
            continue;
        }
        Painter.save();
        State.ApplyTransform(Painter, Layer.Transform);
        for (auto Group = Layer.Groups.crbegin(); Group != Layer.Groups.crend(); ++Group)
        {
            State.PaintGroup(Painter, *Group);
        }
        Painter.restore();
    }
}

FTLufiAnimationItem::FTLufiAnimationItem(QQuickItem* Parent)
    : QQuickPaintedItem{Parent}
{
    setAntialiasing(true);
}

FTLufiAnimationItem::~FTLufiAnimationItem() = default;

QObject* FTLufiAnimationItem::GetAnimationSource() const
{
    return Source;
}

void FTLufiAnimationItem::SetAnimationSource(QObject* Value)
{
    auto* NewSource = qobject_cast<AnimationSource*>(Value);
    if (Source == NewSource)
    {
        return;
    }
    if (Source)
    {
        disconnect(Source, nullptr, this, nullptr);
    }
    Source = NewSource;
    if (Source)
    {
        connect(Source, &AnimationSource::Changed, this, &FTLufiAnimationItem::Reload);
    }
    Reload();
    emit AnimationSourceChanged();
}

void FTLufiAnimationItem::SetClipName(const QString& Value)
{
    if (ClipName != Value)
    {
        ClipName = Value;
        update();
        emit ClipNameChanged();
    }
}

void FTLufiAnimationItem::SetSelectedEyes(const QString& Value)
{
    if (SelectedEyes != Value)
    {
        SelectedEyes = Value;
        update();
        emit SelectedEyesChanged();
    }
}

void FTLufiAnimationItem::SetFrame(int Value)
{
    if (Frame != Value)
    {
        Frame = Value;
        update();
        emit FrameChanged();
    }
}

void FTLufiAnimationItem::SetNightEyes(bool bValue)
{
    if (bNightEyes == bValue)
    {
        return;
    }

    bNightEyes = bValue;
    update();
    emit NightEyesChanged();
}

void FTLufiAnimationItem::paint(QPainter* Painter)
{
    if (Renderer)
    {
        Renderer->Paint(*Painter, ClipName, SelectedEyes, bNightEyes, Frame);
    }
}

void FTLufiAnimationItem::Reload()
{
    SetRenderer(Source ? PrepareRenderer(*Source) : nullptr);
}

std::shared_ptr<const FTLufiTrackRenderer> FTLufiAnimationItem::PrepareRenderer(const AnimationSource& Value)
{
    if (!Value.GetError().isEmpty())
    {
        return {};
    }
    return std::make_shared<FTLufiTrackRenderer>(Value);
}

void FTLufiAnimationItem::SetRenderer(std::shared_ptr<const FTLufiTrackRenderer> Value)
{
    Renderer = std::move(Value);
    update();
}
