import sys
import json
import math
from pathlib import Path
import numpy as np

Root = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(Root))
from animation_paths import Work, Profile
from foreleg_geometry import FramePaths
Foreleg = json.loads((Work / "foreleg-strokes.json").read_text())
Reference = json.loads((Work / 'preserved.json').read_text())
Actual = json.loads((Work / 'shared-assembled.json').read_text())
FrameCount = int(Reference['op'] - Reference['ip'])

def Sample(Property, Frame):
    if Property.get('s') is True:
        return [float(np.asarray(Sample(Property[Name], Frame)).flatten()[0]) for Name in ('x', 'y')]
    if not Property.get('a'):
        return Property['k']
    Keys = [Key for Key in Property['k'] if 's' in Key]
    Before = max((Key for Key in Keys if Key['t'] <= Frame), key=lambda Key: Key['t'], default=Keys[0])
    After = min((Key for Key in Keys if Key['t'] > Frame), key=lambda Key: Key['t'], default=Before)
    A, B = Before['s'], After['s']
    if isinstance(A[0], dict):
        A, B = A[0], B[0]
    if Before is After or Before.get('h'):
        return A
    Fraction = (Frame - Before['t']) / (After['t'] - Before['t'])
    if isinstance(A, dict):
        return {Name: (np.array(A[Name]) + Fraction * (np.array(B[Name]) - A[Name])).tolist() if Name in ('v', 'i', 'o') else Value for Name, Value in A.items()}
    return (np.array(A) + Fraction * (np.array(B) - A)).tolist()

def Transform(Layer, Frame):
    Properties = Layer['ks']
    Angle = float(np.asarray(Sample(Properties['r'], Frame)).flatten()[0]) * math.pi / 180
    Scale = np.asarray(Sample(Properties['s'], Frame)) / 100
    Matrix = np.array([[math.cos(Angle) * Scale[0], -math.sin(Angle) * Scale[1]], [math.sin(Angle) * Scale[0], math.cos(Angle) * Scale[1]]])
    Anchor = np.asarray(Sample(Properties['a'], Frame))
    Offset = np.asarray(Sample(Properties['p'], Frame)) - Matrix @ Anchor
    return Matrix, Offset

def Shapes(Value):
    if isinstance(Value, dict):
        if Value.get('ty') == 'sh':
            yield Value
        for Item in Value.values():
            yield from Shapes(Item)
    elif isinstance(Value, list):
        for Item in Value:
            yield from Shapes(Item)

Expected = {Layer['nm']: Layer for Layer in Reference['layers']}
Maximum = {}
for Layer in Actual['layers']:
    Name = Layer['nm'].split(' depth ')[0]
    Original = Expected[Name]
    if Name.startswith(Profile['foreleg']):
        assert all(Item['ty'] not in ('el', 'st') for Group in Layer['shapes'] for Item in Group['it'])
        assert len(list(Shapes(Layer['shapes']))) == len(FramePaths(Foreleg[Name.split(' | ')[1]][0]))
    OriginalShapes = {Shape['nm']: Shape for Shape in Shapes(Original['shapes'])}
    for Shape in Shapes(Layer['shapes']):
        Source = OriginalShapes[Shape['nm']]
        for Frame in range(1, FrameCount + 1):
            A, B = Sample(Source['ks'], Frame), Sample(Shape['ks'], Frame)
            Matrix, Offset = Transform(Layer, Frame)
            for Key in ('v', 'i', 'o'):
                assert np.array(A[Key]).shape == np.array(B[Key]).shape, (Name, Shape['nm'], Key)
                Result = np.array(B[Key]) @ Matrix.T
                if Key == 'v':
                    Result += Offset
                Error = float(np.max(np.abs(Result - A[Key])))
                Maximum[Name] = max(Maximum.get(Name, 0), Error)
                assert Error < 0.04, (Name, Shape['nm'], Frame, Key, Error)
print('ALL VERTICES AND TANGENTS,', FrameCount, 'FRAMES PASS', round(max(Maximum.values()), 6), 'pixels')
Depths = json.loads((Work / 'depths.json').read_text())
Visibility = json.loads((Work / 'visibility.json').read_text())
for Frame in range(1, FrameCount + 1):
    Active = [Layer['nm'].split(' | ')[0] for Layer in Actual['layers'] if np.asarray(Sample(Layer['ks']['o'], Frame)).flatten()[0] > 0]
    for Index, Name in enumerate(Active):
        for Other in Active[Index + 1:]:
            assert Depths[Name][Frame - 1] + 0.001 >= Depths[Other][Frame - 1], (Frame, Name, Other)
    for Name in Visibility:
        ExpectedCount = sum(Layer['nm'].split(' | ')[0] == Name for Layer in Reference['layers']) if Visibility[Name][Frame - 1] else 0
        assert Active.count(Name) == ExpectedCount, (Frame, Name, Active.count(Name), ExpectedCount)
    for Name in Visibility:
        if Name.startswith('Eyes '):
            assert (Name in Active) == Visibility[Name][Frame - 1], (Frame, Name)
print('DEPTH AND AUTHORED EYE TRACKS PASS')
