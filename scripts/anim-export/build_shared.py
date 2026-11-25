import sys
import copy
import hashlib
import json
import math
import zipfile
import argparse
from pathlib import Path
import numpy as np

Root = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(Root))
from animation_paths import Work, Assets, ClipId
Parser = argparse.ArgumentParser()
Parser.add_argument('--input', type=Path, default=Work / 'preserved.json')
Parser.add_argument('--metadata', type=Path, default=Work)
Parser.add_argument('--clip', default=ClipId)
Parser.add_argument('--reuse', action='store_true')
Parser.add_argument('--character', type=Path, default=Assets / 'shared' / 'character.json')
Parser.add_argument('--rigid-layout', type=Path)
Parser.add_argument('--output', type=Path, default=Assets / 'shared')
Parser.add_argument('--bundle', type=Path, default=Assets / 'lufi-shared.zip')
Arguments = Parser.parse_args()
Data = json.loads(Arguments.input.read_text())
Visibility = json.loads((Arguments.metadata / 'visibility.json').read_text())
Depths = json.loads((Arguments.metadata / 'depths.json').read_text())
FrameCount = int(Data['op'] - Data['ip'])
Previous = json.loads(Arguments.character.read_text()) if Arguments.reuse else None
CompatibleRigidParts = set(json.loads(Arguments.rigid_layout.read_text())['rigidParts']) if Arguments.rigid_layout else None
PreviousLayers = {Layer['nm']: Layer for Layer in Previous['layers']} if Previous else {}
if Previous and any(Previous['composition'][Key] != Data[Key] for Key in ('w', 'h')):
    raise ValueError('Additional clips must use the shared character camera dimensions')
Character = copy.deepcopy(Previous) if Previous else {'version': 1, 'composition': {Key: Value for Key, Value in Data.items() if Key != 'layers'}, 'layers': [], 'properties': {}, 'rigidParts': []}
Character['layers'] = []
Clip = {'version': 1, 'character': 'lufi', 'name': Arguments.clip.replace('-', ' ').capitalize(), 'frames': FrameCount, 'fps': Data['fr'], 'tracks': {}, 'depths': Depths, 'visibility': Visibility}
Center = np.array([Data['w'] / 2, Data['h'] / 2])
Maximum = 0

def PreviousProperty(Identifier):
    Name, *Path = Identifier.split('/')
    if Name not in PreviousLayers:
        return None
    Value = PreviousLayers[Name]
    for Component in Path:
        Value = Value[int(Component)] if isinstance(Value, list) else Value[Component]
    return Value

def Sample(Property, Frame):
    if not Property.get('a'):
        return Property['k']
    Keys = [Key for Key in Property['k'] if 's' in Key]
    Before = max((Key for Key in Keys if Key['t'] <= Frame), key=lambda Key: Key['t'], default=Keys[0])
    After = min((Key for Key in Keys if Key['t'] > Frame), key=lambda Key: Key['t'], default=Before)
    Start = Before['s']
    if isinstance(Start[0], dict):
        Start = Start[0]
    if Before is After or Before.get('h'):
        return copy.deepcopy(Start)
    End = After['s']
    if isinstance(End[0], dict):
        End = End[0]
    Fraction = (Frame - Before['t']) / (After['t'] - Before['t'])
    def Blend(A, B):
        if isinstance(A, dict):
            return {Key: Blend(Value, B[Key]) if Key in ('v', 'i', 'o') else Value for Key, Value in A.items()}
        if isinstance(A, list):
            return [Blend(X, Y) for X, Y in zip(A, B)]
        return A + Fraction * (B - A)
    return Blend(Start, End)

def Flatten(Value):
    if isinstance(Value, dict):
        return Flatten([Value[Key] for Key in ('v', 'i', 'o')])
    if isinstance(Value, list):
        return [Number for Item in Value for Number in Flatten(Item)]
    return [float(Value)]

def AddTrack(Values):
    Key = hashlib.sha256(json.dumps(Values, separators=(',', ':')).encode()).hexdigest()[:16]
    Clip['tracks'][Key] = Values
    return Key

def Property(Values, Identifier):
    global Maximum
    Existing = Character['properties'].get(Identifier)
    Prior = PreviousProperty(Identifier) if Previous and not Existing and '/transform/' not in Identifier else None
    Base = Existing['base'] if Existing else Prior['k'] if Prior and 'k' in Prior else Values[0]
    Matrix = np.array([Flatten(Value) for Value in Values], dtype=float)
    if Matrix.shape[1] != len(Flatten(Base)):
        raise ValueError(('Character topology changed', Identifier))
    Difference = Matrix - np.array(Flatten(Base))
    if not Existing and np.max(np.abs(Difference)) < 0.00001:
        return {'a': 0, 'k': Base[0] if isinstance(Base, list) and len(Base) == 1 else Base}
    Definition = copy.deepcopy(Existing) if Existing else {'base': Base}
    if Matrix.shape[1] > 8:
        OldBasis = np.array(Definition.get('basis', []), dtype=float)
        OldWeights = np.linalg.lstsq(OldBasis.T, Difference.T, rcond=None)[0].T if len(OldBasis) else np.empty((FrameCount, 0))
        Residual = Difference - OldWeights @ OldBasis if len(OldBasis) else Difference
        Weights = np.round(OldWeights, 5)
        Basis = OldBasis
        Error = float(np.max(np.abs(Residual)))
        if not np.isfinite(Residual).all():
            raise ValueError(('Non-finite deformation', Identifier))
        Left, Singular, Right = np.linalg.svd(Residual, full_matrices=False) if Error >= 0.02 else (None, [], None)
        for Rank in range(1, min(len(Singular), 24) + 1):
            if Error < 0.02:
                break
            AddedWeights = np.round(Left[:, :Rank] * Singular[:Rank], 5)
            AddedBasis = np.round(Right[:Rank], 8)
            Weights = np.column_stack((OldWeights, AddedWeights))
            Basis = np.vstack((OldBasis, AddedBasis)) if len(OldBasis) else AddedBasis
            Error = float(np.max(np.abs(Weights @ Basis - Difference)))
            if Error < 0.02:
                break
        if Error >= 0.02:
            raise ValueError(('Deformation needs more controls', Identifier, Error))
        Maximum = max(Maximum, Error)
        Definition['basis'] = Basis.tolist()
        Clip.setdefault('properties', {})[Identifier] = AddTrack(Weights.tolist())
    else:
        Clip.setdefault('properties', {})[Identifier] = AddTrack(np.round(Matrix, 5).tolist())
    Character['properties'][Identifier] = Definition
    return {'$property': Identifier}

def WalkProperties(Value, Identifier):
    if isinstance(Value, list):
        return [WalkProperties(Item, Identifier + '/' + str(Index)) for Index, Item in enumerate(Value)]
    if not isinstance(Value, dict):
        return Value
    if 'k' in Value and Value.get('a') in (0, 1):
        return Property([Sample(Value, Frame) for Frame in range(1, FrameCount + 1)], Identifier)
    return {Key: WalkProperties(Item, Identifier + '/' + Key) for Key, Item in Value.items()}

def Paths(Value):
    if isinstance(Value, dict):
        if Value.get('ty') == 'sh':
            yield Value['ks']
        for Item in Value.values():
            yield from Paths(Item)
    elif isinstance(Value, list):
        for Item in Value:
            yield from Paths(Item)

Seen = set()
for Source in Data['layers']:
    Name = Source['nm'].split(' depth ')[0]
    if Name in Seen:
        continue
    Seen.add(Name)
    Layer = copy.deepcopy(Source)
    Layer['nm'] = Name
    Layer['ks']['o'] = {'a': 0, 'k': 100}
    Properties = list(Paths(Layer['shapes']))
    PriorLayer = PreviousLayers.get(Name)
    PriorPaths = list(Paths(PriorLayer['shapes'])) if PriorLayer else []
    Reference = [copy.deepcopy(Item['k']) for Item in PriorPaths] if Previous and Name in Character['rigidParts'] else [Sample(Item, 1) for Item in Properties]
    if Previous and Name in Character['rigidParts']:
        for Shape in Reference:
            Shape['v'] = (np.array(Shape['v']) + Center).tolist()
    Points = np.array([Point for Shape in Reference for Point in Shape['v']])
    Basis = np.column_stack((Points - Center, np.ones(len(Points))))
    Transforms = []
    Rigid = True
    for Frame in range(1, FrameCount + 1):
        Target = np.array([Point for Item in Properties for Point in Sample(Item, Frame)['v']])
        Transform = np.linalg.lstsq(Basis, Target, rcond=None)[0]
        X, Y, Translation = Transform
        ScaleX = float(np.linalg.norm(X))
        ScaleY = float(np.linalg.det(Transform[:2])) / max(ScaleX, 1e-12)
        Angle = math.atan2(X[1], X[0])
        Rebuilt = np.array([[math.cos(Angle) * ScaleX, math.sin(Angle) * ScaleX], [-math.sin(Angle) * ScaleY, math.cos(Angle) * ScaleY], Translation])
        Error = float(np.max(np.abs(Basis @ Rebuilt - Target)))
        if Error > 0.025:
            Rigid = False
            break
        Transforms.append({'p': Translation.tolist(), 'r': [math.degrees(Angle)], 's': [ScaleX * 100, ScaleY * 100]})
    CompatibleRigidPart = Name in Character['rigidParts'] if Previous else CompatibleRigidParts is None or Name in CompatibleRigidParts
    if Rigid and CompatibleRigidPart and not any(Item.get('ty') == 'el' for Group in Layer['shapes'] for Item in Group['it']):
        for Item, Shape in zip(Properties, Reference):
            Shape['v'] = np.round(np.array(Shape['v']) - Center, 5).tolist()
            Item.update(a=0, k=Shape)
        Layer['ks']['a'] = {'a': 0, 'k': [0, 0]}
        for Key in ('p', 'r', 's'):
            Layer['ks'][Key] = Property([Transform[Key] for Transform in Transforms], Name + '/transform/' + Key)
        if Name not in Character['rigidParts']:
            Character['rigidParts'].append(Name)
        print('Shared rigid paths:', Name)
    elif Previous and Name in Character['rigidParts']:
        raise ValueError(('Rigid character part became deformable', Name))
    Layer = WalkProperties(Layer, Name)
    Character['layers'].append(Layer)

if Previous and Seen != set(PreviousLayers):
    raise ValueError('Additional clips must contain the same character parts')

Rest = {'version': 1, 'character': 'lufi', 'name': 'Rest pose', 'frames': 1, 'fps': Data['fr'], 'reference': Arguments.clip, 'hold': 1}
Output = Arguments.output
Output.mkdir(exist_ok=True)
Results = [('character.json', Character), (Arguments.clip + '.json', Clip)]
if not (Output / 'rest.json').exists():
    Results.append(('rest.json', Rest))
for File, Value in Results:
    (Output / File).write_text(json.dumps(Value, separators=(',', ':')))
with zipfile.ZipFile(Arguments.bundle, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as Bundle:
    for File in sorted(Output.glob('*.json')):
        Information = zipfile.ZipInfo(File.name, date_time=(1980, 1, 1, 0, 0, 0))
        Information.compress_type = zipfile.ZIP_DEFLATED
        Information.create_system = 3
        Information.external_attr = 0o100644 << 16
        Bundle.writestr(Information, File.read_bytes(), compresslevel=9)
    print('SHARED PACKAGE', [(Info.filename, Info.file_size, Info.compress_size) for Info in Bundle.infolist()])
print('Maximum deformation reconstruction error:', Maximum)
