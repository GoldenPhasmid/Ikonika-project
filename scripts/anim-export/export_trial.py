import bpy
import json
import sys
import runpy
from pathlib import Path

Root = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
sys.path.insert(0, str(Root))
from animation_paths import Work, Start, End, FrameCount, CameraName, Profile
sys.path.insert(0, str(Root / 'vendor'))
from io_lottie_export.exporter import export_gp_vector_lottie
from io_lottie_export import exporter as Exporter

OriginalFill = Exporter._fill_color_from_stroke
OriginalStroke = Exporter._stroke_color_from_stroke

def FillColor(Data, Stroke, Drawing, Index):
    if Drawing.attributes.get('fill_color'):
        Color = Stroke.fill_color
        return Exporter._gp_scene_linear_rgba_to_lottie(Color[0], Color[1], Color[2], Color[3] * Stroke.fill_opacity)
    Material = Exporter.gp_material_for_stroke(Data, Stroke)
    if Material and not Material.grease_pencil.show_fill:
        return [0, 0, 0, 0]
    return OriginalFill(Data, Stroke, Drawing, Index)

def StrokeColor(Data, Stroke):
    if getattr(Stroke, 'hide_stroke', False):
        return [0, 0, 0, 0]
    return OriginalStroke(Data, Stroke)

Exporter._fill_color_from_stroke = FillColor
Exporter._stroke_color_from_stroke = StrokeColor
Exporter._dedupe_layer_shape_keys_in_place = lambda *Arguments, **Options: 0

runpy.run_path(str(Root / 'export_foreleg.py'))
Scene = bpy.context.scene
Scene.camera = bpy.data.objects[CameraName]
Objects = [Object for Object in Scene.objects if Object.type == 'GREASEPENCIL']
for Object in Objects:
    Object.driver_remove('hide_viewport')
    Object.hide_viewport = False
    Object.hide_set(False)
Samples = {}
Scene.frame_set(Start)
ReferenceMatrices = {Object.name: Object.evaluated_get(bpy.context.evaluated_depsgraph_get()).matrix_world.copy() for Object in Objects}
Visibility = {}
Depths = {}
StrokeCaps = {}
for Frame in range(Start, End + 1):
    Scene.frame_set(Frame)
    Graph = bpy.context.evaluated_depsgraph_get()
    for Object in Objects:
        Evaluated = Object.evaluated_get(Graph)
        VisibleLayer = any(not Layer.hide and Layer.opacity > 0 for Layer in Evaluated.data.layers)
        Visibility.setdefault(Object.name, []).append(not Object.hide_render and VisibleLayer)
        if Object.name == Profile['foreleg']:
            for CapLayer in Evaluated.data.layers:
                CapStroke = CapLayer.current_frame().drawing.strokes[0]
                StrokeCaps.setdefault(CapLayer.name, []).append([CapStroke.start_cap, CapStroke.end_cap])
        Positions = [(Evaluated.matrix_world @ Point.position).y for Layer in Evaluated.data.layers if Layer.current_frame() for Stroke in Layer.current_frame().drawing.strokes for Point in Stroke.points]
        Depths.setdefault(Object.name, []).append(round(-min(Positions) * 1000, 3) if Positions else 0)
        Evaluated = Object.evaluated_get(Graph)
        Relative = ReferenceMatrices[Object.name].inverted_safe() @ Evaluated.matrix_world
        for Layer in Evaluated.data.layers:
            Source = Layer.current_frame()
            if not Source:
                continue
            Drawing = Source.drawing
            Attributes = []
            for Attribute in Drawing.attributes:
                Field = {'FLOAT_VECTOR': 'vector', 'FLOAT_COLOR': 'color', 'BYTE_COLOR': 'color', 'FLOAT2': 'vector'}.get(Attribute.data_type, 'value')
                Values = [getattr(Item, Field) for Item in Attribute.data]
                if Attribute.name in ('position', 'handle_left', 'handle_right'):
                    Values = [Relative @ Value for Value in Values]
                Values = [tuple(Value) if hasattr(Value, '__len__') else Value for Value in Values]
                Attributes.append((Attribute.name, Attribute.data_type, Attribute.domain, Field, Values))
            Samples.setdefault((Object.name, Layer.name), []).append((Frame, [len(Stroke.points) for Stroke in Drawing.strokes], Attributes))

for (ObjectName, LayerName), Frames in Samples.items():
    Layer = bpy.data.objects[ObjectName].data.layers[LayerName]
    for Frame in list(Layer.frames):
        Layer.frames.remove(Frame.frame_number)
    for Number, Sizes, Attributes in Frames:
        Drawing = Layer.frames.new(Number).drawing
        Drawing.add_strokes(Sizes)
        for Name, Type, Domain, Field, Values in Attributes:
            Attribute = Drawing.attributes.get(Name)
            if Attribute is None:
                Attribute = Drawing.attributes.new(Name, Type, Domain)
            for Item, Value in zip(Attribute.data, Values):
                setattr(Item, Field, Value)
        Drawing.tag_positions_changed()

for Object in Objects:
    Object.modifiers.clear()
    Object.constraints.clear()
    Object.animation_data_clear()
    Object.parent = None
    Object.matrix_world = ReferenceMatrices[Object.name]
    for Layer in Object.data.layers:
        Layer.name = Object.name + ' | ' + Layer.name
Scene.frame_set(Start)
export_gp_vector_lottie(str(Work / 'baked.json'), Objects, Scene, frame_start=Start, frame_end=End, float_precision=2, depsgraph=bpy.context.evaluated_depsgraph_get(), bake_animation=True, optimize_stroke_keys=False)
Data = json.loads((Work / 'baked.json').read_text())
def Normalize(Value):
    if isinstance(Value, dict):
        if Value.get('a') == 1 and isinstance(Value.get('k'), list):
            Keys = Value['k']
            if Keys and 's' in Keys[-1]:
                Keys.append({'t': End + 1, 's': Keys[-1]['s']})
            for Index, Key in enumerate(Keys[:-1]):
                if 's' in Key and 's' in Keys[Index + 1]:
                    Key['e'] = Keys[Index + 1]['s']
                    Key['o'] = {'x': 0, 'y': 0}
                    Key['i'] = {'x': 1, 'y': 1}
                    Key.pop('h', None)
            Keys[-1] = {'t': Keys[-1]['t']}
        for Child in Value.values():
            Normalize(Child)
    elif isinstance(Value, list):
        for Child in Value:
            Normalize(Child)
Normalize(Data)
def ShiftTimes(Value):
    if isinstance(Value, dict):
        if 't' in Value and isinstance(Value['t'], (int, float)):
            Value['t'] -= Start - 1
        for Child in Value.values():
            ShiftTimes(Child)
    elif isinstance(Value, list):
        for Child in Value:
            ShiftTimes(Child)
ShiftTimes(Data)
Data['ip'] = 1
Data['op'] = FrameCount + 1
for Layer in Data['layers']:
    Layer['ip'] = 1
    Layer['op'] = FrameCount + 1
(Work / 'baked.json').write_text(json.dumps(Data, separators=(',', ':')))
(Work / 'visibility.json').write_text(json.dumps(Visibility))
(Work / 'depths.json').write_text(json.dumps(Depths))
(Work / 'stroke-caps.json').write_text(json.dumps(StrokeCaps))
print('BAKED', len(Samples), 'layers;', len(Data['layers']), 'exported layers', flush=True)
