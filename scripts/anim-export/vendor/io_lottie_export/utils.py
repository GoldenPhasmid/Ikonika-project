"""
SVG Grease Pencil Lottie — import SVG as Grease Pencil and export animated Grease Pencil to Lottie.

Copyright (C) 2026 multlabs (crantisz@gmail.com, to@multlabs.com)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

Read and analyze Grease Pencil / scene data from Blender for Lottie export.
"""

from __future__ import annotations

from typing import Any

import bpy  # type: ignore
from bpy_extras.anim_utils import action_ensure_channelbag_for_slot  # type: ignore
from mathutils import Matrix, Vector  # type: ignore

GP_CURVE_TYPE_BEZIER = 2

_OBJECT_TRANSFORM_PATHS = (
    "location",
    "rotation_euler",
    "rotation_quaternion",
    "rotation_axis_angle",
    "scale",
    "delta_location",
    "delta_rotation_euler",
    "delta_rotation_quaternion",
    "delta_scale",
)


def gp_objects_for_export(
    view_layer: bpy.types.ViewLayer,
    *,
    only_selected: bool,
    collection: bpy.types.Collection | None = None,
) -> list[bpy.types.Object]:
    """Grease Pencil objects in the view layer, optionally filtered by selection or collection."""
    out: list[bpy.types.Object] = []
    allowed_names: set[str] | None = None
    if collection is not None:
        allowed_names = {ob.name for ob in collection.all_objects}
    for ob in view_layer.objects:
        if ob.type != "GREASEPENCIL" or ob.data is None:
            continue
        if allowed_names is not None and ob.name not in allowed_names:
            continue
        if only_selected and not ob.select_get():
            continue
        out.append(ob)
    return out


def gp_objects_sorted_by_camera_z_dot(
    objects: list[bpy.types.Object],
    camera: bpy.types.Object,
    depsgraph: bpy.types.Depsgraph | None,
) -> list[bpy.types.Object]:
    """Sort Grease Pencil objects by ``dot(P_ob - P_cam, Y_cam)`` in world space."""
    mw_c = (
        camera.evaluated_get(depsgraph).matrix_world
        if depsgraph is not None
        else camera.matrix_world
    )
    y_world = (mw_c.to_3x3() @ Vector((0.0, 0.0, -1.0))).normalized()
    cam_p = mw_c.translation

    def sort_key(ob: bpy.types.Object) -> float:
        mw_o = (
            ob.evaluated_get(depsgraph).matrix_world
            if depsgraph is not None
            else ob.matrix_world
        )
        v = mw_o.translation - cam_p
        return float(v.dot(y_world))

    return sorted(objects, key=sort_key)


def render_resolution_px(scene: bpy.types.Scene) -> tuple[int, int]:
    """Final render size in pixels (matches Blender render dimensions)."""
    r = scene.render
    scale = r.resolution_percentage * 0.01
    w = max(1, int(round(r.resolution_x * scale)))
    h = max(1, int(round(r.resolution_y * scale)))
    return w, h


def scene_framerate(scene: bpy.types.Scene) -> int | float:
    """Scene frame rate: ``RenderSettings.fps`` / ``fps_base`` (matches timeline)."""
    r = scene.render
    base = float(r.fps_base) if getattr(r, "fps_base", None) not in (None, 0) else 1.0
    fr = float(r.fps) / base
    rounded = round(fr)
    if abs(fr - rounded) < 1e-5:
        return int(rounded)
    return round(fr, 4)


def ob_matrix_world(
    scene: bpy.types.Scene,
    depsgraph: bpy.types.Depsgraph | None,
    ob: bpy.types.Object,
    frame: int,
) -> Matrix:
    scene.frame_set(int(frame))
    if depsgraph is not None:
        return ob.evaluated_get(depsgraph).matrix_world.copy()
    return ob.matrix_world.copy()


def layer_matrix_world(
    scene: bpy.types.Scene,
    depsgraph: bpy.types.Depsgraph | None,
    ob: bpy.types.Object,
    lay: bpy.types.GreasePencilLayer,
    frame: int,
) -> Matrix:
    """Object world matrix multiplied by active layer local matrix at frame."""
    mw_ob = ob_matrix_world(scene, depsgraph, ob, frame)
    scene.frame_set(int(frame))
    return mw_ob @ lay.matrix_local.copy()


def ensure_object_action_channelbag(ob: bpy.types.Object) -> Any | None:
    """
    Blender 4.x+: layered actions store F-curves on a channelbag for the active
    ``action_slot``; legacy builds expose ``action.fcurves`` directly.
    """
    ad = getattr(ob, "animation_data", None)
    if ad is None or ad.action is None:
        return None
    action = ad.action
    if getattr(action, "is_empty", False):
        return None
    slot = getattr(ad, "action_slot", None)
    if slot is None:
        slots = getattr(action, "slots", None)
        if slots and len(slots) > 0:
            slot = slots[0]
    if slot is None:
        return None
    try:
        return action_ensure_channelbag_for_slot(action, slot)
    except (ImportError, TypeError, AttributeError, RuntimeError):
        return None


def ensure_id_action_channelbag(id_block: Any) -> Any | None:
    ad = getattr(id_block, "animation_data", None)
    if ad is None or ad.action is None:
        return None
    action = ad.action
    if getattr(action, "is_empty", False):
        return None
    slot = getattr(ad, "action_slot", None)
    if slot is None:
        slots = getattr(action, "slots", None)
        if slots and len(slots) > 0:
            slot = slots[0]
    if slot is None:
        return None
    try:
        return action_ensure_channelbag_for_slot(action, slot)
    except (ImportError, TypeError, AttributeError, RuntimeError):
        return None


def iter_object_transform_fcurves(ob: bpy.types.Object):
    """Yield F-curves that drive object transform (channelbag or legacy action)."""
    ad = getattr(ob, "animation_data", None)
    if ad is None or ad.action is None:
        return
    bag = ensure_object_action_channelbag(ob)
    if bag is not None:
        for fc in bag.fcurves:
            if fc.data_path in _OBJECT_TRANSFORM_PATHS:
                yield fc
        return
    action = ad.action
    fcs = getattr(action, "fcurves", None)
    if not fcs:
        return
    for fc in fcs:
        if fc.data_path in _OBJECT_TRANSFORM_PATHS:
            yield fc


def iter_layer_transform_fcurves(gp: bpy.types.GreasePencil, layer_name: str):
    """Yield F-curves driving one GP layer transform on GP data action."""
    ad = getattr(gp, "animation_data", None)
    if ad is None or ad.action is None:
        return
    prefix = f'layers["{layer_name}"].'
    suffixes = ("translation", "rotation", "scale")
    bag = ensure_id_action_channelbag(gp)
    if bag is not None:
        for fc in bag.fcurves:
            dp = str(fc.data_path)
            if dp.startswith(prefix) and any(dp.endswith(s) for s in suffixes):
                yield fc
        return
    action = ad.action
    fcs = getattr(action, "fcurves", None)
    if not fcs:
        return
    for fc in fcs:
        dp = str(fc.data_path)
        if dp.startswith(prefix) and any(dp.endswith(s) for s in suffixes):
            yield fc


def collect_object_transform_keyframes(
    ob: bpy.types.Object, frame_start: int, frame_end: int
) -> set[int]:
    """Keyframe times (scene frames) on the object that affect transform."""
    out: set[int] = set()
    for fc in iter_object_transform_fcurves(ob):
        for kp in fc.keyframe_points:
            t = int(round(float(kp.co[0])))
            if frame_start <= t <= frame_end:
                out.add(t)
    return out


def collect_layer_transform_keyframes(
    gp: bpy.types.GreasePencil,
    layer_name: str,
    frame_start: int,
    frame_end: int,
) -> set[int]:
    out: set[int] = set()
    for fc in iter_layer_transform_fcurves(gp, layer_name):
        for kp in fc.keyframe_points:
            t = int(round(float(kp.co[0])))
            if frame_start <= t <= frame_end:
                out.add(t)
    return out


def collect_gp_keyframes_in_range(
    layers: list[bpy.types.GreasePencilLayer],
    frame_start: int,
    frame_end: int,
) -> list[int]:
    keys: set[int] = set()
    for lay in layers:
        for fr in lay.frames:
            fn = int(fr.frame_number)
            if frame_start <= fn <= frame_end:
                keys.add(fn)
    if not keys:
        keys.add(int(frame_start))
    return sorted(keys)


def gp_layer_mask_refs(layer: bpy.types.GreasePencilLayer) -> list[tuple[str, bool]]:
    """Collect mask layer refs from a GP layer as ``(name, invert)``."""
    if not bool(getattr(layer, "use_masks", False)):
        return []
    refs: list[tuple[str, bool]] = []
    for m in list(getattr(layer, "mask_layers", []) or []):
        name = str(getattr(m, "name", "") or "").strip()
        if not name:
            continue
        refs.append((name, bool(getattr(m, "invert", False))))
    return refs


def gp_layer_sample_frames(
    lay: bpy.types.GreasePencilLayer, frame_start: int, frame_end: int
) -> tuple[list[int], list[int]]:
    """
    GP layer keyframes in the export range and frames to sample.

    ``lay_keys_in_range`` lists frames with a GP key on this layer.
    ``lay_sample_frames`` also includes ``frame_start`` for held art at range start.
    """
    lay_keyframes = {int(fr.frame_number) for fr in lay.frames}
    lay_keys_in_range = sorted(fn for fn in lay_keyframes if frame_start <= fn <= frame_end)
    lay_sample_frames = sorted(set(lay_keys_in_range) | {int(frame_start)})
    return lay_keys_in_range, lay_sample_frames


def gp_layer_max_stroke_count(
    lay: bpy.types.GreasePencilLayer, lay_sample_frames: list[int]
) -> int:
    """Maximum stroke count on ``lay`` across sample frames (0 if never drawn)."""
    stroke_counts: list[int] = []
    for frame_num in lay_sample_frames:
        fr = lay.get_frame_at(int(frame_num))
        if fr and fr.drawing:
            stroke_counts.append(len(fr.drawing.strokes))
        else:
            stroke_counts.append(0)
    return max(stroke_counts) if stroke_counts else 0


def gp_layer_transform_frames(
    lay: bpy.types.GreasePencilLayer,
    ob: bpy.types.Object,
    gp: bpy.types.GreasePencil,
    lay_sample_frames: list[int],
    ip: int,
    op: int,
    bake_animation: bool,
) -> list[int]:
    """
    Frame numbers at which to sample the layer's Lottie transform (``ks``).

    When ``bake_animation`` is true, every frame in ``[ip, op]`` is used.
    Otherwise keys come from shape samples plus object/layer F-curves.
    """
    if bake_animation:
        return list(range(ip, op + 1))
    return sorted(
        set(lay_sample_frames)
        | collect_object_transform_keyframes(ob, ip, op)
        | collect_layer_transform_keyframes(gp, lay.name, ip, op)
        | {ip}
    )


def stroke_curve_is_bezier(drawing, stroke_index: int) -> bool:
    attr = drawing.attributes.get("curve_type")
    if attr is None or stroke_index < 0 or stroke_index >= len(attr.data):
        return False
    try:
        return int(attr.data[stroke_index].value) == GP_CURVE_TYPE_BEZIER
    except (TypeError, ValueError, ReferenceError):
        return False


def stroke_fill_id(drawing, stroke_index: int) -> int:
    """
    Curve-domain ``fill_id`` for a stroke, or synthetic ``-stroke_index`` when unshared.

    Blender 5.1 can represent one filled curve as multiple stroke parts that
    share ``fill_id`` (holes/inner rings).
    """
    attr = drawing.attributes.get("fill_id")
    if attr is None or stroke_index < 0 or stroke_index >= len(attr.data):
        return -stroke_index
    try:
        fill_id = int(attr.data[stroke_index].value)
        if fill_id == 0:
            return -stroke_index
        return fill_id
    except (TypeError, ValueError, ReferenceError):
        return -stroke_index


def gp_material_for_stroke(
    gp: bpy.types.GreasePencil, stroke
) -> bpy.types.Material | None:
    mats = gp.materials
    mi = stroke.material_index
    if 0 <= mi < len(mats) and mats[mi] is not None:
        mat = mats[mi]
        if getattr(mat, "is_grease_pencil", False):
            return mat
    return None


def collect_fill_group_members(
    lay: bpy.types.GreasePencilLayer, lay_sample_frames: list[int]
) -> dict[int, set[int]]:
    """Map each ``fill_id`` to stroke indices present on any sample frame."""
    fill_group_members: dict[int, set[int]] = {}
    for frame_num in lay_sample_frames:
        fr_grp = lay.get_frame_at(int(frame_num))
        if not fr_grp or not fr_grp.drawing:
            continue
        dr_grp = fr_grp.drawing
        for si_grp, _st_grp in enumerate(dr_grp.strokes):
            fid = stroke_fill_id(dr_grp, si_grp)
            fill_group_members.setdefault(fid, set()).add(int(si_grp))
    return fill_group_members
