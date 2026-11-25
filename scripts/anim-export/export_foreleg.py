import sys
import bpy
import json
from pathlib import Path
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view

Root = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(Root))
from animation_paths import Work, Start, End, CameraName, Profile
Scene = bpy.context.scene
Scene.camera = bpy.data.objects[CameraName]
Output = {}
def Project(Position):
    Point = world_to_camera_view(Scene, Scene.camera, Position)
    return Vector((Point.x * Scene.render.resolution_x, (1 - Point.y) * Scene.render.resolution_y))

for Frame in range(Start, End + 1):
    Scene.frame_set(Frame)
    Object = bpy.data.objects[Profile['foreleg']].evaluated_get(bpy.context.evaluated_depsgraph_get())
    for Layer in Object.data.layers:
        Stroke = Layer.current_frame().drawing.strokes[0]
        Points = []
        for Point in Stroke.points:
            World = Object.matrix_world @ Point.position
            Position = Project(World)
            Radius = (Project(Object.matrix_world @ (Point.position + Vector((Point.radius, 0, 0)))) - Position).length
            Points.append([round(Position.x, 5), round(Position.y, 5), round(Radius, 5)])
        Output.setdefault(Layer.name, []).append({'points': Points, 'start': Stroke.start_cap, 'end': Stroke.end_cap})
(Work / 'foreleg-strokes.json').write_text(json.dumps(Output, separators=(',', ':')))
print('Exported variable-width foreleg', len(Output), 'layers,', End - Start + 1, 'frames')
