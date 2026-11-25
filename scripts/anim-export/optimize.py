import sys
import json
import math
import numpy as np
import copy
from pathlib import Path

Root = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(Root))
from animation_paths import Work, Profile
Data = json.loads((Work / 'baked.json').read_text())
FrameCount = int(Data['op'] - Data['ip'])
from foreleg_geometry import Shapes
Foreleg = json.loads((Work / 'foreleg-strokes.json').read_text())
for Layer in Data['layers']:
    if not Layer['nm'].startswith(Profile['foreleg']):
        continue
    Stroke = next(Item for Group in Layer['shapes'] for Item in Group['it'] if Item['ty'] == 'st')
    Layer['shapes'] = Shapes(Foreleg[Layer['nm'].split(' | ')[1]], Stroke, Layer['nm'])
for Layer in Data['layers']:
    if not Layer['nm'].startswith(Profile['tail']):
        continue
    for Group in Layer['shapes']:
        Paths = [Item for Item in Group['it'] if Item['ty'] == 'sh']
        if len(Paths) < 2:
            continue
        def Area(Path):
            Property = Path['ks']
            Shape = Property['k'][0]['s'][0] if Property['a'] else Property['k']
            Points = Shape['v']
            return abs(sum(A[0] * B[1] - B[0] * A[1] for A, B in zip(Points, Points[1:] + Points[:1])))
        Outer = max(Paths, key=Area)
        Group['it'] = [Item for Item in Group['it'] if Item['ty'] != 'sh' or Item is Outer]
    Layer['shapes'].reverse()

def Sample(Property, Frame):
    if Property['a'] == 0:
        return Property['k']
    Keys = [Key for Key in Property['k'] if 's' in Key]
    Before = max((Key for Key in Keys if Key['t'] <= Frame), key=lambda Key: Key['t'], default=Keys[0])
    After = min((Key for Key in Keys if Key['t'] > Frame), key=lambda Key: Key['t'], default=Before)
    A, B = Before['s'][0], After['s'][0]
    if Before is After:
        return A
    Fraction = (Frame - Before['t']) / (After['t'] - Before['t'])
    return {Name: [[X + Fraction * (Y - X) for X, Y in zip(P, Q)] for P, Q in zip(A[Name], B[Name])] if Name in ('v', 'i', 'o') else A[Name] for Name in A}

def Flat(Value):
    if isinstance(Value, dict):
        return [Number for Name in ('v', 'i', 'o') for Number in Flat(Value.get(Name, []))]
    if isinstance(Value, list):
        return [Number for Item in Value for Number in Flat(Item)]
    return [float(Value)]

def Reduce(Count, Error, Tolerance):
    Keep = {0, Count - 1}
    Pending = [(0, Count - 1)]
    while Pending:
        Start, End = Pending.pop()
        if End <= Start + 1:
            continue
        Distance, Index = max((Error(Start, End, Index), Index) for Index in range(Start + 1, End))
        if Distance > Tolerance:
            Keep.add(Index)
            Pending.extend([(Start, Index), (Index, End)])
    return sorted(Keep)

def SimplifyShapes(Shapes):
    Count = len(Shapes[0]['v'])
    if Count < 20 or any(any(abs(Number) > 0.001 for Number in Flat([Shape['i'], Shape['o']])) for Shape in Shapes):
        return
    def Error(Start, End, Index):
        Maximum = 0
        for Shape in Shapes:
            A, B, P = [Shape['v'][Position] for Position in (Start, End, Index)]
            X, Y = B[0] - A[0], B[1] - A[1]
            Fraction = max(0, min(1, ((P[0] - A[0]) * X + (P[1] - A[1]) * Y) / max(1e-12, X * X + Y * Y)))
            Maximum = max(Maximum, math.hypot(P[0] - A[0] - Fraction * X, P[1] - A[1] - Fraction * Y))
        return Maximum
    Keep = Reduce(Count, Error, 0.15)
    for Shape in Shapes:
        for Name in ('v', 'i', 'o'):
            Shape[Name] = [Shape[Name][Index] for Index in Keep]

def Optimize(Value, Preserve=False):
    if isinstance(Value, list):
        for Item in Value:
            Optimize(Item, Preserve)
        return
    if not isinstance(Value, dict):
        return
    Preserve = Preserve or Value.get('nm', '').split(' | ')[0] in Profile['protected']
    if not Preserve and Value.get('a') == 0 and isinstance(Value.get('k'), dict) and 'v' in Value['k']:
        SimplifyShapes([Value['k']])
    if Value.get('a') == 1 and isinstance(Value.get('k'), list):
        Keys = [Key for Key in Value['k'] if 's' in Key]
        if not Keys:
            return
        if not Preserve and isinstance(Keys[0]['s'], list) and isinstance(Keys[0]['s'][0], dict):
            SimplifyShapes([Key['s'][0] for Key in Keys])
        Vectors = [Flat(Key['s']) for Key in Keys]
        def Error(Start, End, Index):
            Fraction = (Keys[Index]['t'] - Keys[Start]['t']) / (Keys[End]['t'] - Keys[Start]['t'])
            return max(abs(Current - (A + Fraction * (B - A))) for A, B, Current in zip(Vectors[Start], Vectors[End], Vectors[Index]))
        Keep = list(range(len(Keys))) if Preserve else Reduce(len(Keys), Error, 0.15) if len(Keys) > 1 else [0]
        if all(max(abs(A - B) for A, B in zip(Vectors[0], Vector)) < 0.01 for Vector in Vectors):
            Value.update(a=0, k=Keys[0]['s'][0] if isinstance(Keys[0]['s'][0], dict) or len(Keys[0]['s']) == 1 else Keys[0]['s'])
            return
        Result = []
        for Position, Index in enumerate(Keep):
            Following = Keep[min(Position + 1, len(Keep) - 1)]
            Result.append({'t': Keys[Index]['t'], 's': Keys[Index]['s'], 'e': Keys[Following]['s'], 'o': {'x': 0, 'y': 0}, 'i': {'x': 1, 'y': 1}})
        Result.append({'t': FrameCount + 1})
        Value['k'] = Result
        return
    for Item in Value.values():
        Optimize(Item, Preserve)

(Work / 'preserved.json').write_text(json.dumps(Data, separators=(',', ':')))
Optimize(Data)
def RoundGeometry(Value):
    if isinstance(Value, dict):
        for Name, Item in Value.items():
            if Name in ('v', 'i', 'o') and isinstance(Item, list):
                Value[Name] = [[round(Number, 2) for Number in Point] for Point in Item]
            else:
                RoundGeometry(Item)
    elif isinstance(Value, list):
        for Item in Value:
            RoundGeometry(Item)
RoundGeometry(Data)
Visibility = json.loads((Work / 'visibility.json').read_text())
Depths = json.loads((Work / 'depths.json').read_text())
Layers = Data['layers']
Orders = [sorted(range(len(Layers)), key=lambda Index: Depths[Layers[Index]['nm'].split(' | ')[0]][Frame], reverse=True) for Frame in range(FrameCount)]
Ordered = []
for Position in range(len(Layers)):
    for Index, Layer in enumerate(Layers):
        if not any(Order[Position] == Index for Order in Orders):
            continue
        Duplicate = copy.deepcopy(Layer)
        Name = Layer['nm'].split(' | ')[0]
        Duplicate['nm'] += ' depth ' + str(Position)
        Values = [100 if Order[Position] == Index and Visibility[Name][Frame] else 0 for Frame, Order in enumerate(Orders)]
        Keys = [{'t': Frame + 1, 's': [Value], 'e': [Values[min(FrameCount - 1, Frame + 1)]], 'o': {'x': 0, 'y': 0}, 'i': {'x': 1, 'y': 1}} for Frame, Value in enumerate(Values)]
        Keys.append({'t': FrameCount + 1})
        Duplicate['ks']['o'] = {'a': 1, 'k': Keys}
        Duplicate['ind'] = len(Ordered) + 1
        Ordered.append(Duplicate)
Data['layers'] = Ordered
(Work / 'prepared.json').write_text(json.dumps(Data, separators=(',', ':')))
print('Prepared geometry in', Work)
