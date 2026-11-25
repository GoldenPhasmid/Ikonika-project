import bpy
import json
import math
import sys
from mathutils import Vector
from pathlib import Path
from bpy_extras.object_utils import world_to_camera_view

Root = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(Root))
from animation_paths import Work, Start, End, FrameCount, CameraName, Profile
from foreleg_geometry import FramePaths
Data = json.loads((Work / 'prepared.json').read_text())
StrokeCaps = json.loads((Work / 'stroke-caps.json').read_text())
Foreleg = json.loads((Work / 'foreleg-strokes.json').read_text())
Manifest = [{'name': Layer['nm'].split(' | ')[0], 'layer': Layer, 'transforms': None} for Layer in Data['layers']]
Scene = bpy.context.scene
Scene.camera = bpy.data.objects[CameraName]
Maximum = {}
for Frame in range(1, FrameCount + 1):
    Scene.frame_set(Start + Frame - 1)
    Graph = bpy.context.evaluated_depsgraph_get()
    for Entry in Manifest:
        if Entry['name'] not in Profile['landmarks']:
            continue
        Layer = Entry['layer']
        Name, LayerName = Layer['nm'].split(' | ')
        LayerName = LayerName.split(' depth ')[0]
        Object = bpy.data.objects[Name].evaluated_get(Graph)
        Drawing = Object.data.layers[LayerName].current_frame().drawing
        if Name == Profile['foreleg']:
            assert all([Stroke.start_cap, Stroke.end_cap] == StrokeCaps[LayerName][Frame - 1] for Stroke in Drawing.strokes)
            assert not any(Item.get('ty') in ('el', 'st') for Group in Layer['shapes'] for Item in Group['it'])
            for Point, Stored in zip(Drawing.strokes[0].points, Foreleg[LayerName][Frame - 1]['points']):
                World = Object.matrix_world @ Point.position
                Center = world_to_camera_view(Scene, Scene.camera, World)
                Edge = world_to_camera_view(Scene, Scene.camera, Object.matrix_world @ (Point.position + Vector((Point.radius, 0, 0))))
                Radius = math.hypot((Edge.x - Center.x) * Scene.render.resolution_x, (Edge.y - Center.y) * Scene.render.resolution_y)
                assert abs(Stored[0] - Center.x * Scene.render.resolution_x) < 0.001
                assert abs(Stored[1] - (1 - Center.y) * Scene.render.resolution_y) < 0.001
                assert abs(Stored[2] - Radius) < 0.001
        def Check(Value):
            if isinstance(Value, dict):
                if Value.get('ty') == 'sh':
                    ForelegPath = Name == Profile['foreleg']
                    Index = int(Value['nm'].rsplit('-', 1)[1]) if ForelegPath else int(Value['nm'].split('_')[-2])
                    Property = Value['ks']
                    if Property['a'] == 0:
                        Points = Property['k']['v']
                    else:
                        Keys = [Key for Key in Property['k'] if 's' in Key]
                        Before = max((Key for Key in Keys if Key['t'] <= Frame), key=lambda Key: Key['t'], default=Keys[0])
                        After = min((Key for Key in Keys if Key['t'] > Frame), key=lambda Key: Key['t'], default=Before)
                        Fraction = (Frame - Before['t']) / (After['t'] - Before['t']) if After is not Before else 0
                        Points = [[A + Fraction * (B - A) for A, B in zip(P, Q)] for P, Q in zip(Before['s'][0]['v'], After['s'][0]['v'])]
                    if ForelegPath:
                        Expected = FramePaths(Foreleg[LayerName][Frame - 1])[Index]['v']
                        Error = max(math.hypot(A[0] - B[0], A[1] - B[1]) for A, B in zip(Points, Expected))
                        Maximum[Name] = max(Maximum.get(Name, 0), Error)
                        assert Error < 0.25, (Frame, LayerName, Index, Error)
                        return
                    Protected = Name in Profile['protected']
                    if Protected:
                        assert len(Points) == len(Drawing.strokes[Index].points), (Frame, Name, Index)
                    for VertexIndex, Point in enumerate(Points if Protected else Points[:1]):
                        World = Object.matrix_world @ Object.data.layers[LayerName].matrix_local @ Drawing.strokes[Index].points[VertexIndex].position
                        Projected = world_to_camera_view(Scene, Scene.camera, World)
                        Error = math.hypot(Point[0] - Projected.x * Scene.render.resolution_x, Point[1] - (1 - Projected.y) * Scene.render.resolution_y)
                        Maximum[Name] = max(Maximum.get(Name, 0), Error)
                        assert Error < (0.02 if Protected else 1.5), (Frame, Name, Index, VertexIndex, Error)
                    return
                for Item in Value.values():
                    Check(Item)
            elif isinstance(Value, list):
                for Item in Value:
                    Check(Item)
        Check(Layer['shapes'])
print('LANDMARK MAXIMUM PIXEL ERROR', Maximum)
assert Maximum and max(Maximum.values()) < 1.5, Maximum
Depths = json.loads((Work / 'depths.json').read_text())
for Frame in range(FrameCount):
    Active = []
    for Layer in Data['layers']:
        Property = Layer['ks']['o']
        Opacity = Property['k'] if Property['a'] == 0 else next(Key['s'][0] for Key in Property['k'] if Key['t'] == Frame + 1)
        if Opacity:
            Active.append(Layer['nm'].split(' | ')[0])
    for Index, Name in enumerate(Active):
        for Other in Active[Index + 1:]:
            assert Depths[Name][Frame] + 0.001 >= Depths[Other][Frame], (Frame + 1, Name, Other)
print('ALL FRAME DEPTH ORDER PASS')
