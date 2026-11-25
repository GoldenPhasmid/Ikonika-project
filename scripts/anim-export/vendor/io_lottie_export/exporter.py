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

Core implementation: Grease Pencil scenes to Lottie JSON / dotLottie / TGS.
"""


import copy
import gzip
import json
import os
import zipfile
from dataclasses import dataclass, field
from typing import Any

import bpy  # type: ignore
from bpy_extras.object_utils import world_to_camera_view  # type: ignore
from mathutils import Matrix, Vector  # type: ignore
from .export_log import (
    accumulate_frame_stat,
    accumulate_layer_transform_frame_stats,
    count_layer_transform_keys,
    json_payload_bytes,
    write_export_log_file,
)
from .latin_translit import to_latin_name
from .graphic_utils import (
    dedupe_baked_transform_triplets,
    jtj_largest_eigenvalue,
    linearrgb_to_srgb,
    mean_point_radius,
    rnd,
    vangle_deg,
    vlen,
)
from .utils import (
    collect_fill_group_members,
    gp_layer_mask_refs,
    gp_layer_max_stroke_count,
    gp_layer_sample_frames,
    gp_layer_transform_frames,
    gp_material_for_stroke,
    gp_objects_sorted_by_camera_z_dot,
    iter_layer_transform_fcurves,
    iter_object_transform_fcurves,
    layer_matrix_world,
    ob_matrix_world,
    render_resolution_px,
    scene_framerate,
    stroke_curve_is_bezier,
    stroke_fill_id,
)

_COLOR_DECIMALS = 2


def _gp_scene_linear_rgba_to_lottie(r: float, g: float, b: float, a: float) -> list[float]:
    """Grease Pencil material colors are scene-linear; Lottie ``c`` expects display sRGB."""
    nd = _COLOR_DECIMALS
    return [
        rnd(linearrgb_to_srgb(float(r)), nd),
        rnd(linearrgb_to_srgb(float(g)), nd),
        rnd(linearrgb_to_srgb(float(b)), nd),
        rnd(float(a), nd),
    ]


def _write_lottie_output(
    filepath: str,
    data: dict[str, Any],
    *,
    use_dotlottie: bool = False,
    use_tgs: bool = False,
) -> str:
    """
    Write plain JSON, a **dotLottie** ZIP, or **TGS** (gzip of the same JSON).

    Returns the absolute path that was written. For dotLottie / TGS, the
    extension is forced to ``.lottie`` / ``.tgs`` even if the file picker used
    ``.json``.
    """
    path = os.path.abspath(bpy.path.abspath(filepath))
    if use_dotlottie:
        path = os.path.splitext(path)[0] + ".lottie"
        anim_bytes = json.dumps(data, separators=(",", ":")).encode("utf-8")
        manifest: dict[str, Any] = {
            "version": "1.0.0",
            "generator": "svg_blender_lottie",
            "animations": [{"id": "main"}],
        }
        manifest_bytes = json.dumps(
            manifest, separators=(",", ":")).encode("utf-8")
        with zipfile.ZipFile(path, "w") as zf:
            zf.writestr(
                "manifest.json",
                manifest_bytes,
                compress_type=zipfile.ZIP_DEFLATED,
            )
            zf.writestr(
                "animations/main.json",
                anim_bytes,
                compress_type=zipfile.ZIP_DEFLATED,
            )
        return path
    if use_tgs:
        path = os.path.splitext(path)[0] + ".tgs"
        anim_bytes = json.dumps(data, separators=(",", ":")).encode("utf-8")
        with open(path, "wb") as f:
            f.write(gzip.compress(anim_bytes, compresslevel=9, mtime=0))
        return path
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, separators=(",", ":"))
    return path


def _json_compact_whole_floats(obj: Any) -> Any:
    """Walk export data so ``json`` writes whole numbers without a ``.0`` suffix (smaller files). """
    if isinstance(obj, bool):
        return obj
    if isinstance(obj, float):
        return int(obj) if obj.is_integer() else obj
    if isinstance(obj, dict):
        return {k: _json_compact_whole_floats(v) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_json_compact_whole_floats(v) for v in obj]
    if isinstance(obj, tuple):
        return tuple(_json_compact_whole_floats(v) for v in obj)
    return obj


def _world_to_lottie_pixel_xy(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    rw: int,
    rh: int,
    world_co: Vector,
) -> tuple[float, float]:
    """
    World position → pixel in Lottie comp space (unrounded).

    ``world_to_camera_view`` gives NDC with (0,0) bottom-left of the camera
    frame; Lottie uses top-left with Y increasing downward.
    """
    v = world_to_camera_view(scene, camera, world_co)
    x = float(v.x) * float(rw)
    y = (1.0 - float(v.y)) * float(rh)
    return (x, y)


def _gp_point_world_mw(mw: Matrix, local: Vector) -> Vector:
    """World position using an explicit object world matrix (for frozen-pose path bake)."""
    return mw @ Vector((local.x, local.y, local.z))


def _screen_pivot_and_axes(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    mw: Matrix,
    rw: int,
    rh: int,
) -> tuple[tuple[float, float], tuple[float, float], tuple[float, float]]:
    """Pivot (object origin) and 2D axis legs from projected local +X / +Y in Lottie pixels."""
    piv = _world_to_lottie_pixel_xy(scene, camera, rw, rh, mw.translation)
    px = _world_to_lottie_pixel_xy(
        scene, camera, rw, rh, mw @ Vector((1.0, 0.0, 0.0)))
    py = _world_to_lottie_pixel_xy(
        scene, camera, rw, rh, mw @ Vector((0.0, 0.0, 1.0)))
    vx = (float(px[0] - piv[0]), float(px[1] - piv[1]))
    vy = (float(py[0] - piv[0]), float(py[1] - piv[1]))
    return (float(piv[0]), float(piv[1])), vx, vy


def _fcurve_interpolation_outgoing_at_frame(fc: Any, frame: int) -> str | None:
    """
    Blender ``KeyframePoint.interpolation`` applies to the segment **from** this
    key to the next. Returns ``None`` if ``frame`` is not a key time on ``fc``.
    """
    kps = fc.keyframe_points
    if not kps:
        return None
    fr = float(frame)
    tol = 0.01
    for kp in kps:
        if abs(float(kp.co[0]) - fr) <= tol:
            return str(kp.interpolation)
    return None


def _infer_transform_segment_easing(
    ob: bpy.types.Object, sample_frames: list[int]
) -> dict[str, list[str]]:
    """
    Per-channel labels per segment between consecutive ``sample_frames``:
    ``hold``, ``linear``, or ``ease`` (Lottie bezier handles), from object
    transform F-curves.
    """
    n = len(sample_frames)
    if n < 2:
        return {"p": [], "r": [], "s": []}
    fcurves = list(iter_object_transform_fcurves(ob))
    out: dict[str, list[str]] = {"p": [], "r": [], "s": []}
    by_ch: dict[str, list[Any]] = {"p": [], "r": [], "s": []}
    for fc in fcurves:
        dp = str(fc.data_path)
        if dp in ("location", "delta_location"):
            by_ch["p"].append(fc)
        elif dp in ("scale", "delta_scale"):
            by_ch["s"].append(fc)
        elif dp.startswith("rotation_") or dp.startswith("delta_rotation_"):
            by_ch["r"].append(fc)
    for j in range(n - 1):
        t0 = int(sample_frames[j])
        for ch in ("p", "r", "s"):
            interps: list[str] = []
            for fc in by_ch[ch]:
                ip = _fcurve_interpolation_outgoing_at_frame(fc, t0)
                if ip is not None:
                    interps.append(ip.upper())
            if any(x == "CONSTANT" for x in interps):
                out[ch].append("hold")
            elif interps and all(x == "LINEAR" for x in interps):
                out[ch].append("linear")
            elif not interps:
                out[ch].append("linear")
            else:
                out[ch].append("ease")
    return out


def _infer_layer_transform_segment_easing(
    gp: bpy.types.GreasePencil, layer_name: str, sample_frames: list[int]
) -> dict[str, list[str]]:
    n = len(sample_frames)
    if n < 2:
        return {"p": [], "r": [], "s": []}
    fcurves = list(iter_layer_transform_fcurves(gp, layer_name))
    out: dict[str, list[str]] = {"p": [], "r": [], "s": []}
    by_ch: dict[str, list[Any]] = {"p": [], "r": [], "s": []}
    for fc in fcurves:
        dp = str(fc.data_path)
        if dp.endswith("translation"):
            by_ch["p"].append(fc)
        elif dp.endswith("rotation"):
            by_ch["r"].append(fc)
        elif dp.endswith("scale"):
            by_ch["s"].append(fc)
    for j in range(n - 1):
        t0 = int(sample_frames[j])
        for ch in ("p", "r", "s"):
            interps: list[str] = []
            for fc in by_ch[ch]:
                ip = _fcurve_interpolation_outgoing_at_frame(fc, t0)
                if ip is not None:
                    interps.append(ip.upper())
            if any(x == "CONSTANT" for x in interps):
                out[ch].append("hold")
            elif interps and all(x == "LINEAR" for x in interps):
                out[ch].append("linear")
            elif not interps:
                out[ch].append("linear")
            else:
                out[ch].append("ease")
    return out


def _merge_segment_easing(
    a: dict[str, list[str]], b: dict[str, list[str]]
) -> dict[str, list[str]]:
    out: dict[str, list[str]] = {"p": [], "r": [], "s": []}
    for ch in ("p", "r", "s"):
        aa_list = list(a.get(ch, []))
        bb_list = list(b.get(ch, []))
        n = max(len(aa_list), len(bb_list))
        ch_out: list[str] = []
        for i in range(n):
            aa = aa_list[i] if i < len(aa_list) else "linear"
            bb = bb_list[i] if i < len(bb_list) else "linear"
            if aa == "hold" or bb == "hold":
                ch_out.append("hold")
            elif aa == "ease" or bb == "ease":
                ch_out.append("ease")
            else:
                ch_out.append("linear")
        out[ch] = ch_out
    return out


def _apply_lottie_keyframe_easing(
    keys: list[dict[str, Any]],
    segment_styles: list[str],
    *,
    force_linear_handles: bool = False,
) -> None:
    """
    Lottie spec §3.2.1: ``o`` / ``i`` ease the segment **to** the next key;
    the **last** key carries only ``t`` and ``s``.

    ``segment_styles`` entries: ``hold`` → ``h``: 1 (no ``o``/``i``); otherwise
    ``linear`` → ``(0,0)/(0,1)``; ``ease`` → ``(0.33,0)/(0.66,1)``.

    When ``force_linear_handles`` is true (vector-valued ``s``), non-hold
    segments always get linear ``o``/``i`` regardless of ``segment_styles``.
    """
    n = len(keys)
    if n == 0:
        return
    if n == 1:
        keys[0].pop("o", None)
        keys[0].pop("i", None)
        keys[0].pop("h", None)
        return
    for idx in range(n - 1):
        style = segment_styles[idx] if idx < len(segment_styles) else "linear"
        kf = keys[idx]
        if style == "hold":
            kf["h"] = 1
            kf.pop("o", None)
            kf.pop("i", None)
        else:
            kf.pop("h", None)
            if force_linear_handles or style == "linear":
                kf["o"] = {"x": 0.0, "y": 0.0}
                kf["i"] = {"x": 1.0, "y": 1.0}
            else:
                kf["o"] = {"x": 0.33, "y": 0.0}
                kf["i"] = {"x": 0.66, "y": 1.0}
    keys[-1].pop("o", None)
    keys[-1].pop("i", None)
    keys[-1].pop("h", None)


def _layer_ks_for_matrix_transform(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    rw: int,
    rh: int,
    mw_ref: Matrix,
    sample_frames: list[int],
    pos_decimals: int,
    mw_at_frame,
    *,
    bake_animation: bool = False,
    seg_easing: dict[str, list[str]] | None = None,
) -> dict[str, Any]:
    def _wrap_deg(a: float) -> float:
        while a > 180.0:
            a -= 360.0
        while a < -180.0:
            a += 360.0
        return a

    nd = max(0, min(10, int(pos_decimals)))
    piv0, vx0, vy0 = _screen_pivot_and_axes(scene, camera, mw_ref, rw, rh)
    len_x0 = max(1e-6, vlen(vx0))
    len_y0 = max(1e-6, vlen(vy0))
    det_ref = float(vx0[0] * vy0[1] - vx0[1] * vy0[0])

    p_keys: list[dict[str, Any]] = []
    r_keys: list[dict[str, Any]] = []
    s_keys: list[dict[str, Any]] = []

    for t in sample_frames:
        mw_t = mw_at_frame(int(t))
        piv_t, vx_t, vy_t = _screen_pivot_and_axes(scene, camera, mw_t, rw, rh)
        r_deg = _wrap_deg(vangle_deg(vx_t) - vangle_deg(vx0))
        sx = 100.0 * vlen(vx_t) / len_x0
        sy = 100.0 * vlen(vy_t) / len_y0
        det_t = float(vx_t[0] * vy_t[1] - vx_t[1] * vy_t[0])
        # Keep mirror on scale (preferred for GP/Lottie workflows) instead of
        # encoding it as a +180 degree rotation with positive scales.
        if det_ref * det_t < 0.0:
            r_alt = _wrap_deg(r_deg - 180.0)
            if abs(r_alt) <= abs(r_deg):
                r_deg = r_alt
                sx = -sx
        p_keys.append(
            {"t": int(t), "s": [rnd(piv_t[0], nd), rnd(piv_t[1], nd)]})
        r_keys.append({"t": int(t), "s": [rnd(r_deg, 2)]})
        s_keys.append({"t": int(t), "s": [rnd(sx, 2), rnd(sy, 2)]})

    def _all_same_s(keys: list[dict[str, Any]]) -> bool:
        b = keys[0]["s"]
        return all(k["s"] == b for k in keys[1:])

    if bake_animation:
        pos_tol = max(1e-6, 10.0 ** (-max(2, int(pos_decimals) + 1)))
        p_keys, r_keys, s_keys = dedupe_baked_transform_triplets(
            p_keys, r_keys, s_keys, pos_tol=pos_tol, rot_tol_deg=0.05, scale_tol=0.05
        )
        easing_p = ["hold"] * max(0, len(p_keys) - 1)
        easing_r = ["hold"] * max(0, len(p_keys) - 1)
        easing_s = ["hold"] * max(0, len(p_keys) - 1)
    else:
        default = ["linear"] * max(0, len(p_keys) - 1)
        easing_p = list((seg_easing or {}).get("p", default))
        easing_r = list((seg_easing or {}).get("r", default))
        easing_s = list((seg_easing or {}).get("s", default))

    if len(p_keys) == 1 or _all_same_s(p_keys):
        p_prop: dict[str, Any] = {"a": 0, "k": p_keys[0]["s"]}
    else:
        _apply_lottie_keyframe_easing(p_keys, easing_p)
        p_prop = {"a": 1, "k": p_keys}

    if len(r_keys) == 1 or _all_same_s(r_keys):
        r_prop = {"a": 0, "k": float(r_keys[0]["s"][0])}
    else:
        _apply_lottie_keyframe_easing(r_keys, easing_r)
        r_prop = {"a": 1, "k": r_keys}

    if len(s_keys) == 1 or _all_same_s(s_keys):
        s_prop = {"a": 0, "k": s_keys[0]["s"]}
    else:
        _apply_lottie_keyframe_easing(s_keys, easing_s)
        s_prop = {"a": 1, "k": s_keys}

    return {
        "o": {"a": 0, "k": 100},
        "r": r_prop,
        "p": p_prop,
        "a": {"a": 0, "k": [rnd(piv0[0], nd), rnd(piv0[1], nd)]},
        "s": s_prop,
        "sk": {"a": 0, "k": 0.0},
        "sa": {"a": 0, "k": 0.0},
    }


def _group_transform() -> dict[str, Any]:
    return {
        "ty": "tr",
        "p": {"a": 0, "k": [0.0, 0.0]},
        "a": {"a": 0, "k": [0.0, 0.0]},
        "s": {"a": 0, "k": [100.0, 100.0]},
        "r": {"a": 0, "k": 0.0},
        "o": {"a": 0, "k": 100},
        "sk": {"a": 0, "k": 0.0},
        "sa": {"a": 0, "k": 0.0},
        "nm": "Transform",
    }


def _lottie_color_property_from_keyframes(
    keys: list[dict[str, Any]],
) -> dict[str, Any]:
    """
    Build ``c`` for ``fl`` / ``st`` (Lottie **Color** = RGB in ``0..1``, 3 components).
    """
    if not keys:
        return {"a": 0, "k": [1.0, 1.0, 1.0]}
    if len(keys) == 1:
        s = list(keys[0]["s"])
        return {"a": 0, "k": s[:3]}
    return {"a": 1, "k": [{**kf, "s": list(kf["s"])[:3]} for kf in keys]}


def _lottie_style_color_from_rgba_keyframes(
    keys_rgba: list[dict[str, Any]],
) -> dict[str, Any]:
    """``c`` from keyframes whose ``s`` is ``[r,g,b,a]`` (material); export RGB only."""
    keys_rgb: list[dict[str, Any]] = []
    for k in keys_rgba:
        s = k["s"]
        keys_rgb.append(
            {
                "t": k["t"],
                "s": [
                    rnd(float(s[0]), _COLOR_DECIMALS),
                    rnd(float(s[1]), _COLOR_DECIMALS),
                    rnd(float(s[2]), _COLOR_DECIMALS),
                ],
                "h": int(k.get("h", 1)),
            }
        )
    return _lottie_color_property_from_keyframes(keys_rgb)


def _lottie_style_opacity_from_rgba_keyframes(
    keys_rgba: list[dict[str, Any]],
) -> dict[str, Any]:
    """Style ``o`` (0..100 per spec); alpha taken from the 4th component of ``s``."""
    keys_o: list[dict[str, Any]] = []
    for k in keys_rgba:
        s = k["s"]
        a = float(s[3]) if len(s) > 3 else 1.0
        a = max(0.0, min(1.0, a))
        keys_o.append(
            {
                "t": k["t"],
                "s": [rnd(a * 100.0, 3)],
                "h": int(k.get("h", 1)),
            }
        )
    return _lottie_scalar_property_from_keyframes(keys_o)


def _lottie_scalar_property_from_keyframes(
    keys: list[dict[str, Any]],
) -> dict[str, Any]:
    """Same single-key pitfall for stroke ``w`` (``KeyframedValueProperty``)."""
    if not keys:
        return {"a": 0, "k": 1.0}
    if len(keys) == 1:
        s = keys[0]["s"]
        if isinstance(s, (list, tuple)) and len(s) >= 1:
            return {"a": 0, "k": float(s[0])}
        return {"a": 0, "k": float(s)}
    return {"a": 1, "k": keys}


def _lottie_path_property_from_keyframes(
    keys: list[dict[str, Any]],
    *,
    force_animated: bool = False,
) -> dict[str, Any]:
    """Path property for ``sh.ks``; use static form for single-key streams."""
    if not keys:
        return {"a": 0, "k": {"c": False, "v": [], "i": [], "o": []}}
    if len(keys) == 1 and not force_animated:
        s = keys[0].get("s")
        if isinstance(s, list) and s:
            return {"a": 0, "k": s[0]}
        return {"a": 0, "k": {"c": False, "v": [], "i": [], "o": []}}
    return {"a": 1, "k": keys}


def _dedupe_keyframes_consecutive(
    prop: dict[str, Any], *, kind: str, collapse_to_static: bool = True
) -> int:
    """
    Remove consecutive duplicate keyframes from one animated Lottie property.

    Works for shape paths, scalar properties, and RGB/RGBA vectors.
    Converts single-key animated properties to static ``a:0``.
    """
    if not isinstance(prop, dict):
        return 0
    if int(prop.get("a", 0)) != 1:
        return 0
    keys = prop.get("k")
    if not isinstance(keys, list) or not keys:
        return 0
    if not all(isinstance(kf, dict) and "s" in kf for kf in keys):
        return 0

    out: list[dict[str, Any]] = [keys[0]]
    prev_s = keys[0].get("s")
    prev_h = int(keys[0].get("h", 0))
    for kf in keys[1:]:
        s = kf.get("s")
        h = int(kf.get("h", 0))
        if s == prev_s and h == prev_h:
            continue
        out.append(kf)
        prev_s = s
        prev_h = h

    removed = len(keys) - len(out)

    if len(out) == 1 and collapse_to_static:
        s0 = out[0].get("s")
        prop["a"] = 0
        if kind == "path":
            if isinstance(s0, list) and s0:
                prop["k"] = s0[0]
            else:
                prop["k"] = {"c": False, "v": [], "i": [], "o": []}
            return removed
        if isinstance(s0, (list, tuple)):
            if kind == "scalar":
                prop["k"] = float(s0[0]) if len(s0) >= 1 else 0.0
            else:
                prop["k"] = list(s0)
        else:
            prop["k"] = s0
        return removed

    prop["k"] = out
    return removed


def _dedupe_layer_shape_keys_in_place(
    layer: dict[str, Any], *, collapse_path_static: bool = True
) -> int:
    """Post-pass compression for duplicate consecutive keys in exported shapes."""
    skipped_paths = 0

    def walk_items(items: list[dict[str, Any]]) -> None:
        nonlocal skipped_paths
        for it in items:
            ty = it.get("ty")
            if ty == "gr":
                sub = it.get("it")
                if isinstance(sub, list):
                    walk_items(sub)
                continue
            if ty == "sh":
                skipped_paths += _dedupe_keyframes_consecutive(
                    it.get("ks", {}),
                    kind="path",
                    collapse_to_static=collapse_path_static,
                )
            elif ty == "st":
                _dedupe_keyframes_consecutive(it.get("o", {}), kind="scalar")
                _dedupe_keyframes_consecutive(it.get("w", {}), kind="scalar")
                _dedupe_keyframes_consecutive(it.get("c", {}), kind="vector")
            elif ty == "fl":
                _dedupe_keyframes_consecutive(it.get("o", {}), kind="scalar")
                _dedupe_keyframes_consecutive(it.get("c", {}), kind="vector")

    shapes = layer.get("shapes")
    if isinstance(shapes, list):
        walk_items(shapes)
    return skipped_paths


def _fill_color_from_stroke(gp: bpy.types.GreasePencil, stroke, drawing, stroke_index: int) -> list[float]:
    mat = gp_material_for_stroke(gp, stroke)
    opacity = stroke.fill_opacity
    opacity = 1

    opacityattr = drawing.attributes.get("fill_opacity")
    if opacityattr is not None:
        opacity = opacityattr.data[stroke_index].value
    else:
        opacity = 1

    if mat is None:
        return _gp_scene_linear_rgba_to_lottie(0.75, 0.75, 0.75, opacity)
    gpm = mat.grease_pencil
    c = gpm.fill_color
    # print(f"fill_color_from_stroke c: {c[0]}, {c[1]}, {c[2]}, {c[3]}, opacity: {opacity}")
    rgba = _gp_scene_linear_rgba_to_lottie(c[0], c[1], c[2], c[3] * opacity)

    # apply show_fill attribute for latest versions of Blender
    # if not bool(getattr(gpm, "show_fill", True)):
    #    rgba[3] = 0.0
    return rgba


def _stroke_color_from_stroke(gp: bpy.types.GreasePencil, stroke) -> list[float]:
    mat = gp_material_for_stroke(gp, stroke)
    median = 0.0
    for point in stroke.points:
        median += point.opacity
    median /= len(stroke.points)
    if mat is None:
        return _gp_scene_linear_rgba_to_lottie(0.0, 0.0, 0.0, median)
    gpm = mat.grease_pencil
    c = gpm.color
    rgba = _gp_scene_linear_rgba_to_lottie(c[0], c[1], c[2], c[3] * median)
    # print(f"stroke_color_from_stroke c: {c[0]}, {c[1]}, {c[2]}, {c[3]}, opacity: {median}")
    # if not bool(getattr(gpm, "show_stroke", True)):
    #    rgba[3] = 0.0
    return rgba


def _pixels_per_grease_plane_unit(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    mw_path: Matrix,
    stroke,
    rw: int,
    rh: int,
) -> float:
    """
    Largest singular value of the map from GP drawing-plane units → Lottie pixels.
    """
    pts = stroke.points
    if not pts:
        return 1.0
    mid = pts[len(pts) // 2].position
    base = Vector((mid.x, mid.y, mid.z))
    ox, oy = _world_to_lottie_pixel_xy(
        scene, camera, rw, rh, _gp_point_world_mw(mw_path, base))
    px1, py1 = _world_to_lottie_pixel_xy(
        scene,
        camera,
        rw,
        rh,
        _gp_point_world_mw(mw_path, base + Vector((1.0, 0.0, 0.0))),
    )
    px2, py2 = _world_to_lottie_pixel_xy(
        scene,
        camera,
        rw,
        rh,
        _gp_point_world_mw(mw_path, base + Vector((0.0, 1.0, 0.0))),
    )
    sux, suy = float(px1 - ox), float(py1 - oy)
    svx, svy = float(px2 - ox), float(py2 - oy)
    lam_max = jtj_largest_eigenvalue(sux, suy, svx, svy)
    sigma = lam_max**0.5
    if sigma < 1e-6:
        sigma = max((sux * sux + suy * suy) ** 0.5,
                    (svx * svx + svy * svy) ** 0.5)
    return max(1e-6, sigma)


def _gp_stroke_pixel_factor(gp: bpy.types.GreasePencil) -> float:
    """``GreasePencil.pixel_factor`` (0 = default 1.0 in practice)."""
    pf = float(getattr(gp, "pixel_factor", 0.0))
    return 1.0 if pf <= 1e-12 else pf


def _stroke_width_lottie_px(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    mw_path: Matrix,
    gp: bpy.types.GreasePencil,
    stroke,
    rw: int,
    rh: int,
) -> float:
    """
    Lottie stroke ``w`` (full line width in composition pixels).
    """
    r_mean = mean_point_radius(stroke)
    if r_mean <= 0.0:
        r_mean = 1.0
    pf = _gp_stroke_pixel_factor(gp)
    space = getattr(gp, "stroke_thickness_space", "WORLDSPACE")
    if space == "SCREENSPACE":
        w = 2.0 * r_mean * pf
    else:
        scale = _pixels_per_grease_plane_unit(
            scene, camera, mw_path, stroke, rw, rh)
        w = 2.0 * r_mean * scale * pf
    return rnd(max(0.25, w), 3)


def _stroke_to_path_dict(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    ob: bpy.types.Object,
    drawing,
    stroke,
    stroke_index: int,
    rw: int,
    rh: int,
    *,
    mw_path: Matrix,
    position_decimals: int,
) -> dict[str, Any] | None:
    pts = stroke.points
    n = len(pts)
    if n < 2:
        return None

    bezier = stroke_curve_is_bezier(drawing, stroke_index)
    v_out: list[list[float]] = []
    i_out: list[list[float]] = []
    o_out: list[list[float]] = []

    nd = int(position_decimals)
    for i in range(n):
        p = pts[i]
        pos = p.position
        wx = _gp_point_world_mw(mw_path, pos)
        x_raw, y_raw = _world_to_lottie_pixel_xy(scene, camera, rw, rh, wx)
        x, y = rnd(x_raw, nd), rnd(y_raw, nd)
        v_out.append([x, y])

        if bezier:
            hl = p.handle_left
            hr = p.handle_right
            if hr is not None:
                h_world = _gp_point_world_mw(mw_path, hr.position)
                hx_raw, hy_raw = _world_to_lottie_pixel_xy(
                    scene, camera, rw, rh, h_world
                )
                o_out.append(
                    [rnd(hx_raw - x_raw, nd), rnd(hy_raw - y_raw, nd)]
                )
            else:
                o_out.append([0.0, 0.0])
            if hl is not None:
                h_world = _gp_point_world_mw(mw_path, hl.position)
                hx_raw, hy_raw = _world_to_lottie_pixel_xy(
                    scene, camera, rw, rh, h_world
                )
                i_out.append(
                    [rnd(hx_raw - x_raw, nd), rnd(hy_raw - y_raw, nd)]
                )
            else:
                i_out.append([0.0, 0.0])
        else:
            i_out.append([0.0, 0.0])
            o_out.append([0.0, 0.0])

    return {
        "c": bool(getattr(stroke, "cyclic", False)),
        "v": v_out,
        "i": i_out,
        "o": o_out,
    }


def _duplicate_gp_path_dict(path: dict[str, Any]) -> dict[str, Any]:
    """Copy path geometry for an extra hold key (same shape, style hidden via alpha)."""
    return {
        "c": bool(path["c"]),
        "v": [[float(x), float(y)] for x, y in path["v"]],
        "i": [[float(x), float(y)] for x, y in path["i"]],
        "o": [[float(x), float(y)] for x, y in path["o"]],
    }


def _path_topology_signature(
    path: dict[str, Any], material_index: int
) -> tuple[int, int, int, int, int]:
    """Topology/style signature used to split incompatible Lottie path streams."""

    return (
        len(path.get("v", [])),
        len(path.get("i", [])),
        len(path.get("o", [])),
        1 if bool(path.get("c", False)) else 0,
        int(material_index),
    )


def _rgba_keyframe_is_visible(kf: dict[str, Any]) -> bool:
    s = kf.get("s")
    if not isinstance(s, (list, tuple)) or len(s) < 4:
        return False
    return float(s[3]) > 1e-5


def _shape_segment_visibility_ip_op(
    fill_keys: list[dict[str, Any]],
    stroke_keys: list[dict[str, Any]],
    *,
    comp_ip: int,
    comp_op: int,
) -> tuple[int, int]:
    """
    First and last frame where fill or stroke material alpha is visible.

    Used for per-segment ``ip`` / ``op`` on shape groups (inclusive, matching
    composition layer export in this add-on).
    """
    first_vis: int | None = None
    last_vis: int | None = None
    for keys in (fill_keys, stroke_keys):
        for kf in keys:
            if not isinstance(kf, dict) or not _rgba_keyframe_is_visible(kf):
                continue
            t = int(kf.get("t", comp_ip))
            if first_vis is None or t < first_vis:
                first_vis = t
            if last_vis is None or t > last_vis:
                last_vis = t
    if first_vis is None or last_vis is None:
        return int(comp_ip), int(comp_op)
    seg_ip = max(int(comp_ip), int(first_vis))
    seg_op = min(int(comp_op), int(last_vis))
    if seg_op < seg_ip:
        return int(comp_ip), int(comp_op)
    return seg_ip, seg_op


def _composition_name_from_export_path(filepath: str) -> str:
    """Basename of export path without extension (before ``to_latin_name``)."""
    base = os.path.splitext(os.path.basename(filepath))[0].strip()
    return base if base else "Composition"


def _apply_gp_masks_as_track_mattes(
    gp: bpy.types.GreasePencil,
    exported_layers: list[tuple[str, dict[str, Any]]],
    *,
    strict_order: bool,
) -> None:
    """
    Convert GP layer masks to Lottie track matte links.
    """
    if not exported_layers:
        return

    name_to_idx = {nm: i for i, (nm, _) in enumerate(exported_layers)}
    i = 0
    while i < len(exported_layers):

        lay_name, target_layer = exported_layers[i]
        # print(f"lay_name: {lay_name}")
        gp_lay = gp.layers.get(lay_name)
        if gp_lay is None:
            i += 1
            continue
        refs = gp_layer_mask_refs(gp_lay)
        if not refs:
            i += 1
            continue

        # print(f"mask refs: {refs}")

        # First-pass: use first valid mask layer only.
        matte_name = ""
        invert = False
        for ref_name, ref_invert in refs:
            if ref_name in name_to_idx:
                matte_name = ref_name
                invert = ref_invert
                break
        if not matte_name:
            i += 1
            continue

        matte_idx = name_to_idx[matte_name]
        # Telegram matte chain expects the matte layer directly above
        # the masked layer in stack order.
        if strict_order and matte_idx != i - 1:
            raise RuntimeError(
                "Mask layer order is invalid for Telegram export: "
                f"layer '{lay_name}' is masked by '{matte_name}', but the mask "
                "layer is not directly above the masked layer."
            )
        matte_layer = exported_layers[matte_idx][1]
        matte_ind = int(matte_layer.get("ind", 0))
        if matte_ind > 0:
            matte_layer["td"] = 1
            target_layer["tt"] = 2 if invert else 1
            target_layer["tp"] = matte_ind
        i += 1


def _neutral_layer_ks() -> dict[str, Any]:
    """Identity transform for a shape layer nested inside a precomposition asset."""
    return {
        "o": {"a": 0, "k": 100},
        "r": {"a": 0, "k": 0.0},
        "p": {"a": 0, "k": [0.0, 0.0]},
        "a": {"a": 0, "k": [0.0, 0.0]},
        "s": {"a": 0, "k": [100.0, 100.0]},
    }


def _pack_shape_layers_as_precomposition_assets(
    shape_layers: list[dict[str, Any]],
    *,
    fr: float,
    width: int,
    height: int,
    ip: int,
    op: int,
    composition_name: str,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    """
    Put all exported shape layers into one precomposition asset.

    Inner shape layers keep their animated ``ks``, shapes, and track mattes.
    The root composition has a single static precomposition layer (``ty`` 0).
    """
    if not shape_layers:
        return [], []

    inner_layers: list[dict[str, Any]] = []
    old_ind_to_new: dict[int, int] = {}

    for idx, layer in enumerate(shape_layers):
        if not isinstance(layer, dict) or int(layer.get("ty", -1)) != 4:
            raise RuntimeError(
                "pack_layers_as_precomps expects exported shape layers (ty=4)"
            )
        inner = copy.deepcopy(layer)
        old_ind = int(inner.get("ind", idx + 1))
        new_ind = idx + 1
        old_ind_to_new[old_ind] = new_ind
        inner["ind"] = new_ind
        inner_layers.append(inner)

    for inner in inner_layers:
        if "tp" in inner:
            try:
                old_tp = int(inner["tp"])
            except (TypeError, ValueError):
                continue
            new_tp = old_ind_to_new.get(old_tp)
            if new_tp is not None:
                inner["tp"] = new_tp

    asset_id = "comp_0"
    comp_nm = to_latin_name(composition_name, fallback="gp_export")
    assets = [
        {
            "id": asset_id,
            "nm": comp_nm,
            "fr": fr,
            "w": width,
            "h": height,
            "ip": ip,
            "op": op,
            "layers": inner_layers,
        }
    ]
    root_layers = [
        {
            "ddd": 0,
            "ind": 1,
            "ty": 0,
            "nm": comp_nm,
            "refId": asset_id,
            "sr": 1,
            "ks": _neutral_layer_ks(),
            "ao": 0,
            "w": width,
            "h": height,
            "ip": ip,
            "op": op,
            "st": 0,
            "bm": 0,
        }
    ]
    return assets, root_layers


@dataclass
class _GpLayerShapeExportCtx:
    """
    Immutable inputs for exporting one Grease Pencil layer to Lottie shape groups.

    Holds scene/camera/object/layer references, export range, sampling frames,
    and options shared while iterating ``fill_id`` groups on that layer.
    """

    scene: bpy.types.Scene
    camera: bpy.types.Object
    ob: bpy.types.Object
    gp: bpy.types.GreasePencil
    lay: bpy.types.GreasePencilLayer
    rw: int
    rh: int
    ip: int
    op: int
    pos_decimals: int
    mw_layer_ip: Matrix
    force_animated_paths: bool
    lay_nm: str
    lay_keys_in_range: list[int]
    lay_sample_frames: list[int]
    n_strokes: int
    frame_stats_out: dict[int, dict[str, float]] | None


@dataclass
class _FillGroupSegmentBuffers:
    """
    Mutable keyframe buffers for one ``fill_id`` group within a layer export.

    Accumulates per-member path keys and shared fill/stroke/width style keys
    until :func:`_fill_group_flush_segment` emits one Lottie shape group (``gr``).
    """

    fill_id: int
    member_indices: list[int]
    member_path_keys: dict[int, list[dict[str, Any]]
                           ] = field(default_factory=dict)
    member_last_path: dict[int, dict[str, Any]
                           | None] = field(default_factory=dict)
    member_last_sig: dict[int, tuple[int, int, int, int, int]
                          | None] = field(default_factory=dict)
    style_fill_keys: list[dict[str, Any]] = field(default_factory=list)
    style_stroke_keys: list[dict[str, Any]] = field(default_factory=list)
    style_width_keys: list[dict[str, Any]] = field(default_factory=list)
    segment_idx: int = 0
    last_fill_s: list[float] | None = None
    last_stroke_s: list[float] | None = None
    last_width_s: list[float] | None = None

    @classmethod
    def for_members(cls, fill_id: int, member_indices: list[int]) -> "_FillGroupSegmentBuffers":
        """Create empty buffers for a fill group and its stroke member indices."""
        return cls(
            fill_id=fill_id,
            member_indices=member_indices,
            member_path_keys={mi: [] for mi in member_indices},
            member_last_path={mi: None for mi in member_indices},
            member_last_sig={mi: None for mi in member_indices},
        )


def _sorted_fill_groups(
    fill_group_members: dict[int, set[int]],
) -> list[tuple[int, set[int]]]:
    """Sort fill groups by highest member stroke index (descending) for stable stack order."""
    return sorted(
        fill_group_members.items(),
        key=lambda kv: max(kv[1]) if kv[1] else -1,
        reverse=True,
    )


def _fill_group_flush_segment(
    buf: _FillGroupSegmentBuffers,
    ctx: _GpLayerShapeExportCtx,
    shapes: list[dict[str, Any]],
) -> None:
    """
    Finalize the current segment and append one Lottie shape group (``ty``: ``gr``).

    Builds path items from ``member_path_keys``, optional ``st``/``fl`` from style
    keyframes, and a group transform. When the first sample is after ``ctx.ip``,
    inserts hidden fill/stroke keys at ``ctx.ip`` only (no extra path key;
    segment ``ip``/``op`` bounds path visibility). Clears segment buffers and
    increments ``segment_idx``. No-op if there are no style keys or nothing visible.
    """
    if not buf.style_fill_keys:
        return
    # print("++")

    if int(buf.style_fill_keys[0]["t"]) > int(ctx.ip):
        t0 = int(ctx.ip)
        pre_fill = list(buf.style_fill_keys[0]["s"])
        pre_fill[3] = 0.0
        pre_stroke = list(buf.style_stroke_keys[0]["s"])
        pre_stroke[3] = 0.0
        w0 = buf.style_width_keys[0]["s"]
        buf.style_fill_keys.insert(0, {"t": t0, "s": pre_fill, "h": 1})
        buf.style_stroke_keys.insert(0, {"t": t0, "s": pre_stroke, "h": 1})
        buf.style_width_keys.insert(
            0,
            {
                "t": t0,
                "s": list(w0) if isinstance(w0, (list, tuple)) else [w0],
                "h": 1,
            },
        )

    path_items: list[dict[str, Any]] = []
    for mi in buf.member_indices:
        mk = buf.member_path_keys.get(mi, [])
        # if buf.fill_id == 10 and buf.segment_idx < 23:
        #    #print(f"layer 10 segment={buf.segment_idx} {buf.member_indices}")
        #    #print(mk)

        if not mk:
            continue
        path_items.append(
            {
                "ty": "sh",
                "nm": f"path_fill_{buf.fill_id}_{mi}_{buf.segment_idx}",
                "mn": "g",
                "ks": _lottie_path_property_from_keyframes(
                    mk, force_animated=ctx.force_animated_paths
                ),
            }
        )
    if not path_items:
        buf.style_fill_keys = []
        buf.style_stroke_keys = []
        buf.style_width_keys = []
        buf.segment_idx += 1
        return

    has_stroke_visible = any(
        float(sk["s"][3]) > 1e-5 for sk in buf.style_stroke_keys)
    has_fill_visible = any(
        float(fk["s"][3]) > 1e-5 for fk in buf.style_fill_keys)
    if not has_fill_visible and not has_stroke_visible:
        buf.style_fill_keys = []
        buf.style_stroke_keys = []
        buf.style_width_keys = []
        buf.segment_idx += 1
        return

    group_items: list[dict[str, Any]] = list(path_items)
    if has_stroke_visible:
        group_items.append(
            {
                "ty": "st",
                "nm": f"stroke_fill_{buf.fill_id}_{buf.segment_idx}",
                "o": _lottie_style_opacity_from_rgba_keyframes(buf.style_stroke_keys),
                "lc": 2,
                "lj": 2,
                "ml": 4,
                "w": _lottie_scalar_property_from_keyframes(buf.style_width_keys),
                "d": [],
                "c": _lottie_style_color_from_rgba_keyframes(buf.style_stroke_keys),
            }
        )
    if has_fill_visible:
        group_items.append(
            {
                "ty": "fl",
                "nm": f"fill_fill_{buf.fill_id}_{buf.segment_idx}",
                "o": _lottie_style_opacity_from_rgba_keyframes(buf.style_fill_keys),
                "c": _lottie_style_color_from_rgba_keyframes(buf.style_fill_keys),
                "r": 2 if len(buf.member_indices) > 1 else 1,
            }
        )
    group_items.append(_group_transform())
    seg_ip, seg_op = _shape_segment_visibility_ip_op(
        buf.style_fill_keys,
        buf.style_stroke_keys,
        comp_ip=ctx.ip,
        comp_op=ctx.op,
    )
    shapes.append(
        {
            "ty": "gr",
            "nm": f"{ctx.lay_nm}_fill_{buf.fill_id}_{buf.segment_idx}",
            "ip": seg_ip,
            "op": seg_op,
            "it": group_items,
        }
    )
    buf.style_fill_keys = []
    buf.style_stroke_keys = []
    buf.style_width_keys = []
    for mi in buf.member_indices:
        buf.member_path_keys[mi] = []
    buf.segment_idx += 1


def _fill_group_sample_members_at_frame(
    buf: _FillGroupSegmentBuffers,
    ctx: _GpLayerShapeExportCtx,
    frame_num: int,
) -> tuple[bool, bool, Any, int, Any | None]:
    """
    Sample all stroke members of a fill group at one frame.

    Updates ``buf`` path buffers and detects topology changes (``need_flush``).

    Per-stroke path signature, fill-group member set, and keyed strokes with no
    drawing on a GP key (``explicit_empty``) can set ``need_flush``; the export
    loop closes the segment via :func:`_fill_group_topology_split_at_frame`.

    Returns
    -------
    need_flush
        True when path or fill-group topology changed; flush before continuing.
    frame_explicit_empty
        True when a keyed frame marks a member stroke empty (opacity zeroed).
    rep_stroke
        Representative stroke for style on this frame, or ``None``.
    rep_shape_index
        Stroke index used with ``rep_stroke`` for fill color lookup.
    fr
        Grease Pencil frame at ``frame_num``, or ``None``.
    """
    scene = ctx.scene
    lay = ctx.lay
    ob = ctx.ob
    camera = ctx.camera
    rw, rh = ctx.rw, ctx.rh
    pos_decimals = ctx.pos_decimals
    mw_layer_ip = ctx.mw_layer_ip
    lay_keys_in_range = ctx.lay_keys_in_range
    fid = buf.fill_id
    member_indices = buf.member_indices

    need_flush = False
    frame_explicit_empty = False
    rep_stroke = None
    rep_shape_index = member_indices[0] if member_indices else 0

    scene.frame_set(int(frame_num))
    fr = lay.get_frame_at(int(frame_num))
    t = int(frame_num)
    member_has_path_now: dict[int, bool] = {mi: False for mi in member_indices}

    for shape_index in member_indices:
        explicit_empty = (
            int(frame_num) in lay_keys_in_range
            and fr is not None
            and fr.frame_number == int(frame_num)
            and (not fr.drawing or shape_index >= len(fr.drawing.strokes))
        )

        if explicit_empty and buf.member_last_path[shape_index] is not None:
            frame_explicit_empty = True
            hold_kf = {
                "t": t,
                "s": [_duplicate_gp_path_dict(buf.member_last_path[shape_index])],
                "h": 1,
            }
            
            #buf.member_last_path[shape_index] = None
            #print(f"for frame {t} add hold_kf {shape_index} {hold_kf}")
            #buf.member_path_keys[shape_index].append(hold_kf)
            accumulate_frame_stat(
                ctx.frame_stats_out,
                t,
                shape_keys=1,
                size_bytes=json_payload_bytes(hold_kf),
            )
        if not fr or not fr.drawing or shape_index >= len(fr.drawing.strokes):
            # if shape_index==15:  
            #     print(f"{lay.name} ]] frame {t} {buf.member_path_keys[shape_index]}")
            need_flush = True
            
            buf.member_last_sig[shape_index] = None
            continue

        dr = fr.drawing
        st = dr.strokes[shape_index]
        material_index = int(getattr(st, "material_index", -1))

        if stroke_fill_id(dr, shape_index) != fid:
            need_flush = True
            buf.member_last_sig[shape_index] = (0, 0, 0, 0, 0)

            continue

        if rep_stroke is None:
            rep_stroke = st
            rep_shape_index = shape_index
        path = _stroke_to_path_dict(
            scene,
            camera,
            ob,
            dr,
            st,
            shape_index,
            rw,
            rh,
            mw_path=mw_layer_ip,
            position_decimals=pos_decimals,
        )
        if path is None:
            need_flush = True
            buf.member_last_sig[shape_index] = (0, 0, 0, 0, 0)

            continue

        path_sig = _path_topology_signature(path, material_index)

        if (
            buf.member_last_sig[shape_index] is not None
            and buf.member_last_sig[shape_index] != path_sig
            and buf.member_last_path[shape_index] is not None
        ):
            need_flush = True

        # if t in (57,58,59 and buf.fill_id==16):    
        #     print(f"{ctx.lay.name} at frame {t} add {shape_index} fillid={buf.fill_id}, segment={buf.segment_idx} {need_flush} material_index={material_index}")

        path_kf = {"t": t, "s": [path], "h": 1}
        buf.member_path_keys[shape_index].append(path_kf)
        accumulate_frame_stat(
            ctx.frame_stats_out,
            t,
            shape_keys=1,
            size_bytes=json_payload_bytes(path_kf),
        )
        member_has_path_now[shape_index] = True
        buf.member_last_path[shape_index] = path
        buf.member_last_sig[shape_index] = path_sig

    return need_flush, frame_explicit_empty, rep_stroke, rep_shape_index, fr


def _fill_group_resolve_style(
    buf: _FillGroupSegmentBuffers,
    ctx: _GpLayerShapeExportCtx,
    rep_stroke: Any,
    rep_shape_index: int,
    fr: Any | None,
) -> tuple[list[float], list[float], list[float]] | None:
    """
    Resolve fill, stroke RGBA, and width for one frame.

    Uses ``rep_stroke`` when geometry exists; otherwise reuses the last style
    keys (optimized-out geometry frames). Returns ``None`` if no style source exists.
    """
    if rep_stroke is not None and fr is not None and fr.drawing:
        fill_s = _fill_color_from_stroke(
            ctx.gp, rep_stroke, fr.drawing, rep_shape_index)
        stroke_s = _stroke_color_from_stroke(ctx.gp, rep_stroke)
        width_s = [
            _stroke_width_lottie_px(
                ctx.scene, ctx.camera, ctx.mw_layer_ip, ctx.gp, rep_stroke, ctx.rw, ctx.rh
            )
        ]
        return fill_s, stroke_s, width_s
    if buf.style_fill_keys:
        return (
            list(buf.style_fill_keys[-1]["s"]),
            list(buf.style_stroke_keys[-1]["s"]),
            list(buf.style_width_keys[-1]["s"]),
        )
    return None


def _fill_group_append_style_key(
    buf: _FillGroupSegmentBuffers,
    ctx: _GpLayerShapeExportCtx,
    t: int,
    fill_s: list[float],
    stroke_s: list[float],
    width_s: list[float],
    counters: list[int],
) -> None:
    """
    Append one hold keyframe to fill/stroke/width buffers and update export stats.

    ``counters`` is ``[exported_count, skipped_count]``; increments skipped when
    style matches the previous frame's style exactly.
    """
    fill_kf = {"t": t, "s": fill_s, "h": 1}
    stroke_kf = {"t": t, "s": stroke_s, "h": 1}
    width_kf = {"t": t, "s": width_s, "h": 1}
    buf.style_fill_keys.append(fill_kf)
    buf.style_stroke_keys.append(stroke_kf)
    buf.style_width_keys.append(width_kf)
    counters[0] += 1
    style_bytes = (
        json_payload_bytes(fill_kf)
        + json_payload_bytes(stroke_kf)
        + json_payload_bytes(width_kf)
    )

    if (
        buf.last_fill_s is not None
        and fill_s == buf.last_fill_s
        and stroke_s == buf.last_stroke_s
        and width_s == buf.last_width_s
    ):
        counters[1] += 1
        accumulate_frame_stat(
            ctx.frame_stats_out, t, skipped_shapes=1, size_bytes=style_bytes
        )
    else:
        accumulate_frame_stat(
            ctx.frame_stats_out, t, shape_keys=1, size_bytes=style_bytes
        )
    buf.last_fill_s = list(fill_s)
    buf.last_stroke_s = list(stroke_s)
    buf.last_width_s = list(width_s)


def _fill_group_topology_split_at_frame(
    buf: _FillGroupSegmentBuffers,
    ctx: _GpLayerShapeExportCtx,
    shapes: list[dict[str, Any]],
    t: int,
) -> None:
    """
    Close the current segment at frame ``t`` and begin the next segment.

    Pops current-frame keys, appends hide keys (alpha 0) for the old topology,
    calls :func:`_fill_group_flush_segment`, then restores current-frame keys
    into fresh buffers for the new segment.
    """
    # print(f"{ctx.lay.name} t={t}")

    popped_member_keys: dict[int, list[dict[str, Any]]] = {}
    for mi in buf.member_indices:
        popped: list[dict[str, Any]] = []
        mk = buf.member_path_keys.get(mi, [])
        while mk and int(mk[-1].get("t", -1)) == t:
            popped.append(mk.pop())
        if popped:
            popped.reverse()
            popped_member_keys[mi] = popped

    popped_fill = None
    while buf.style_fill_keys and int(buf.style_fill_keys[-1].get("t", -1)) == t:
        popped_fill = buf.style_fill_keys.pop()
    popped_stroke = None
    while buf.style_stroke_keys and int(buf.style_stroke_keys[-1].get("t", -1)) == t:
        popped_stroke = buf.style_stroke_keys.pop()
    popped_width = None
    while buf.style_width_keys and int(buf.style_width_keys[-1].get("t", -1)) == t:
        popped_width = buf.style_width_keys.pop()

    for mi in buf.member_indices:
        mk = buf.member_path_keys.get(mi, [])
        while mk and int(mk[-1].get("t", -1)) == t:
            mk.pop()
        if mk:
            mk.append(
                {
                    "t": t,
                    "s": [_duplicate_gp_path_dict(mk[-1]["s"][0])],
                    "h": 1,
                }
            )

    # print(_duplicate_gp_path_dict(mk[-1]["s"][0]))

    while buf.style_fill_keys and int(buf.style_fill_keys[-1].get("t", -1)) == t:
        buf.style_fill_keys.pop()
    if buf.style_fill_keys:
        hide_fill = list(buf.style_fill_keys[-1]["s"])
        hide_fill[3] = 0.0
        buf.style_fill_keys.append({"t": t, "s": hide_fill, "h": 1})
    while buf.style_stroke_keys and int(buf.style_stroke_keys[-1].get("t", -1)) == t:
        buf.style_stroke_keys.pop()
    if buf.style_stroke_keys:
        hide_stroke = list(buf.style_stroke_keys[-1]["s"])
        hide_stroke[3] = 0.0
        buf.style_stroke_keys.append({"t": t, "s": hide_stroke, "h": 1})
    while buf.style_width_keys and int(buf.style_width_keys[-1].get("t", -1)) == t:
        buf.style_width_keys.pop()
    if buf.style_width_keys:
        buf.style_width_keys.append(
            {"t": t, "s": list(buf.style_width_keys[-1]["s"]), "h": 1}
        )
    # print("flush _fill_group_topology_split_at_frame")
    _fill_group_flush_segment(buf, ctx, shapes)

    for mi, popped in popped_member_keys.items():
        buf.member_path_keys[mi].extend(popped)
    if popped_fill is not None:
        buf.style_fill_keys.append(popped_fill)
    if popped_stroke is not None:
        buf.style_stroke_keys.append(popped_stroke)
    if popped_width is not None:
        buf.style_width_keys.append(popped_width)
    for mi in buf.member_indices:
        mk = buf.member_path_keys.get(mi, [])
        buf.member_path_keys[mi] = [k for k in mk if int(k.get("t", -1)) == t]
    buf.style_fill_keys = [
        k for k in buf.style_fill_keys if int(k.get("t", -1)) == t]
    buf.style_stroke_keys = [
        k for k in buf.style_stroke_keys if int(k.get("t", -1)) == t]
    buf.style_width_keys = [
        k for k in buf.style_width_keys if int(k.get("t", -1)) == t]


def _export_gp_fill_group(
    fid: int,
    members: set[int],
    ctx: _GpLayerShapeExportCtx,
    shapes: list[dict[str, Any]],
    shape_key_counters: list[int],
) -> None:
    """
    Export one ``fill_id`` group to one or more Lottie shape groups.

    Iterates ``ctx.lay_sample_frames``, sampling paths and styles per frame.
    Topology splits flush intermediate segments; a final flush closes the last
    segment. Updates ``shape_key_counters`` as ``[exported, skipped]``.
    """
    member_indices = sorted(
        (i for i in members if 0 <= i < ctx.n_strokes), reverse=True)
    if not member_indices:
        return

    buf = _FillGroupSegmentBuffers.for_members(fid, member_indices)
    for frame_num in ctx.lay_sample_frames:

        need_flush, frame_explicit_empty, rep_stroke, rep_shape_index, fr = (
            _fill_group_sample_members_at_frame(buf, ctx, frame_num)
        )
        style = _fill_group_resolve_style(
            buf, ctx, rep_stroke, rep_shape_index, fr)
        if style is None:
            continue
        fill_s, stroke_s, width_s = style
        if frame_explicit_empty:
            fill_s[3] = 0.0
            stroke_s[3] = 0.0
        _fill_group_append_style_key(
            buf, ctx, int(
                frame_num), fill_s, stroke_s, width_s, shape_key_counters
        )
        if need_flush:
            _fill_group_topology_split_at_frame(
                buf, ctx, shapes, int(frame_num))

    # print("flush _export_gp_fill_group")
    _fill_group_flush_segment(buf, ctx, shapes)


def _export_gp_layer_shape_groups(
    ctx: _GpLayerShapeExportCtx,
) -> tuple[list[dict[str, Any]], int, int]:
    """
    Export every fill group on one GP layer into Lottie shape group items.

    Returns ``(shapes, shape_key_count, shape_key_skipped)``.
    """
    shapes: list[dict[str, Any]] = []
    shape_key_counters = [0, 0]
    fill_group_members = collect_fill_group_members(
        ctx.lay, ctx.lay_sample_frames)
    for fid, members in _sorted_fill_groups(fill_group_members):
        _export_gp_fill_group(fid, members, ctx, shapes, shape_key_counters)
    return shapes, shape_key_counters[0], shape_key_counters[1]


def _build_gp_layer_shape_export_ctx(
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    ob: bpy.types.Object,
    gp: bpy.types.GreasePencil,
    lay: bpy.types.GreasePencilLayer,
    depsgraph: bpy.types.Depsgraph | None,
    *,
    rw: int,
    rh: int,
    ip: int,
    op: int,
    pos_decimals: int,
    bake_animation: bool,
    file_format: str,
    frame_stats_out: dict[int, dict[str, float]] | None,
) -> tuple[_GpLayerShapeExportCtx | None, dict[str, Any] | None]:
    """
    Build export context and layer transform ``ks`` for one GP layer.

    Returns ``(ctx, layer_ks)`` or ``(None, None)`` when the layer has no strokes
    in the sample range.
    """
    lay_keys_in_range, lay_sample_frames = gp_layer_sample_frames(lay, ip, op)
    n_strokes = gp_layer_max_stroke_count(lay, lay_sample_frames)
    if n_strokes == 0:
        return None, None
    layer_transform_frames = gp_layer_transform_frames(
        lay, ob, gp, lay_sample_frames, ip, op, bake_animation
    )
    obj_ease = _infer_transform_segment_easing(ob, layer_transform_frames)
    lay_ease = _infer_layer_transform_segment_easing(
        gp, lay.name, layer_transform_frames)
    seg_ease = _merge_segment_easing(obj_ease, lay_ease)
    mw_layer_ip = layer_matrix_world(scene, depsgraph, ob, lay, ip)
    layer_ks = _layer_ks_for_matrix_transform(
        scene,
        camera,
        rw,
        rh,
        mw_layer_ip,
        layer_transform_frames,
        pos_decimals,
        lambda f: layer_matrix_world(scene, depsgraph, ob, lay, int(f)),
        bake_animation=bake_animation,
        seg_easing=seg_ease,
    )
    accumulate_layer_transform_frame_stats(layer_ks, ip, frame_stats_out)
    lay_nm = to_latin_name(lay.name, fallback="Layer")
    ctx = _GpLayerShapeExportCtx(
        scene=scene,
        camera=camera,
        ob=ob,
        gp=gp,
        lay=lay,
        rw=rw,
        rh=rh,
        ip=ip,
        op=op,
        pos_decimals=pos_decimals,
        mw_layer_ip=mw_layer_ip,
        force_animated_paths=str(file_format).upper() == "TGS",
        lay_nm=lay_nm,
        lay_keys_in_range=lay_keys_in_range,
        lay_sample_frames=lay_sample_frames,
        n_strokes=n_strokes,
        frame_stats_out=frame_stats_out,
    )
    return ctx, layer_ks


def _append_gp_layer_shape_layer(
    ctx: _GpLayerShapeExportCtx,
    ind: int,
    layer_ks: dict[str, Any],
    stats_out: list[dict[str, Any]] | None,
) -> tuple[dict[str, Any], int, int] | None:
    """
    Build one Lottie shape layer (``ty``: 4) for a prepared layer context.

    Returns ``(layer_dict, shape_key_count, shape_key_skipped)`` or ``None`` if
    no shape groups were produced. Appends layer stats when ``stats_out`` is set.
    """
    shapes, shape_key_count, shape_key_skipped = _export_gp_layer_shape_groups(
        ctx)
    if not shapes:
        return None
    layer = {
        "ddd": 0,
        "ind": ind,
        "ty": 4,
        "nm": ctx.lay_nm,
        "sr": 1,
        "ks": layer_ks,
        "ao": 0,
        "ip": ctx.ip,
        "op": ctx.op,
        "st": 0,
        "bm": 0,
        "shapes": shapes,
    }
    if stats_out is not None:
        stats_out.append(
            {
                "name": ctx.lay_nm,
                "ind": ind,
                "sampled_frame_count": len(ctx.lay_sample_frames),
                "source_layer_key_count": len(ctx.lay_keys_in_range),
                "stroke_count": ctx.n_strokes,
                "shape_group_count": len(shapes),
                "transform_key_count": count_layer_transform_keys(layer_ks),
                "shape_key_count": shape_key_count,
                "shape_key_skipped": shape_key_skipped,
            }
        )
    return layer, shape_key_count, shape_key_skipped


def _append_gp_object_shape_layers(
    out_layers: list[dict[str, Any]],
    ob: bpy.types.Object,
    scene: bpy.types.Scene,
    camera: bpy.types.Object,
    depsgraph: bpy.types.Depsgraph | None,
    rw: int,
    rh: int,
    frame_start: int,
    frame_end: int,
    pos_decimals: int,
    bake_animation: bool,
    file_format: str,
    stats_out: list[dict[str, Any]] | None,
    frame_stats_out: dict[int, dict[str, float]] | None,
    strict_mask_order: bool,
    ind_start: int,
) -> int:
    """
    Append Lottie shape layers for all drawable GP layers on one object.

    Layers are exported in reverse GP stack order. Track mattes are applied
    after all layers are built. Returns the next free layer index ``ind``.
    """
    gp = ob.data
    assert gp is not None and ob.type == "GREASEPENCIL"
    layers = list(gp.layers)
    object_layers: list[tuple[str, dict[str, Any]]] = []
    ind = ind_start
    ip = frame_start
    op = frame_end

    for lay in reversed(layers):
        ctx, layer_ks = _build_gp_layer_shape_export_ctx(
            scene,
            camera,
            ob,
            gp,
            lay,
            depsgraph,
            rw=rw,
            rh=rh,
            ip=ip,
            op=op,
            pos_decimals=pos_decimals,
            bake_animation=bake_animation,
            file_format=file_format,
            frame_stats_out=frame_stats_out,
        )
        if ctx is None or layer_ks is None:
            continue
        built = _append_gp_layer_shape_layer(ctx, ind, layer_ks, stats_out)
        if built is None:
            continue
        layer, _, _ = built
        object_layers.append((lay.name, layer))
        ind += 1

    _apply_gp_masks_as_track_mattes(
        gp,
        object_layers,
        strict_order=strict_mask_order,
    )
    out_layers.extend(layer for _, layer in object_layers)
    return ind


def build_lottie_dict_from_grease_pencils(
    objects: list[bpy.types.Object],
    scene: bpy.types.Scene,
    *,
    frame_start: int,
    frame_end: int,
    float_precision: int = 3,
    depsgraph: bpy.types.Depsgraph | None = None,
    bake_animation: bool = False,
    file_format: str = "JSON",
    layer_stats_out: list[dict[str, Any]] | None = None,
    frame_stats_out: dict[int, dict[str, float]] | None = None,
    strict_mask_order: bool = False,
    composition_name: str | None = None,
    pack_layers_as_precomps: bool = False,
) -> dict[str, Any]:
    """
    Build a Lottie-compatible dict from one or more Grease Pencil objects.
    """
    if not objects:
        raise ValueError("No Grease Pencil objects to export")
    for ob in objects:
        if ob.type != "GREASEPENCIL" or ob.data is None:
            raise TypeError(
                f"Not a Grease Pencil object: {getattr(ob, 'name', ob)!r}")

    camera = scene.camera
    if camera is None or camera.type != "CAMERA":
        raise RuntimeError(
            "Scene needs an active camera (set scene.camera) for camera-space export."
        )

    objects = gp_objects_sorted_by_camera_z_dot(objects, camera, depsgraph)

    rw, rh = render_resolution_px(scene)
    pos_decimals = max(0, min(10, int(float_precision)))

    ip = int(frame_start)
    op = int(frame_end)

    out_layers: list[dict[str, Any]] = []
    ind = 1
    old_frame = int(scene.frame_current)
    try:
        for ob in objects:
            ind = _append_gp_object_shape_layers(
                out_layers,
                ob,
                scene,
                camera,
                depsgraph,
                rw,
                rh,
                frame_start,
                frame_end,
                pos_decimals,
                bake_animation,
                file_format,
                layer_stats_out,
                frame_stats_out,
                strict_mask_order,
                ind_start=ind,
            )
    finally:
        scene.frame_set(old_frame)

    if not out_layers:
        raise RuntimeError(
            "No drawable Grease Pencil strokes in the export range for the chosen objects."
        )

    if composition_name is not None:
        comp_nm = to_latin_name(composition_name, fallback="blender_scene")
    else:
        comp_nm = "blender_scene"

    stats_by_ind: dict[int, dict[str, Any]] = {}
    if layer_stats_out is not None:
        for st in layer_stats_out:
            if isinstance(st, dict):
                try:
                    stats_by_ind[int(st.get("ind", -1))] = st
                except (TypeError, ValueError):
                    continue

    for lay in out_layers:
        if isinstance(lay, dict):
            skipped_paths = _dedupe_layer_shape_keys_in_place(
                lay, collapse_path_static=(str(file_format).upper() != "TGS")
            )
            try:
                lay_ind = int(lay.get("ind", -1))
            except (TypeError, ValueError):
                lay_ind = -1
            st = stats_by_ind.get(lay_ind)
            if st is not None and skipped_paths > 0:
                st["shape_key_skipped"] = int(st.get("shape_key_skipped", 0)) + int(
                    skipped_paths
                )

    assets: list[dict[str, Any]] = []
    root_layers = out_layers
    if pack_layers_as_precomps:
        assets, root_layers = _pack_shape_layers_as_precomposition_assets(
            out_layers,
            fr=float(scene_framerate(scene)),
            width=rw,
            height=rh,
            ip=ip,
            op=op,
            composition_name=comp_nm,
        )

    result = {
        "v": "5.5.6",
        "fr": scene_framerate(scene),
        "ip": ip,
        "op": op,
        "w": rw,
        "h": rh,
        "nm": comp_nm,
        "ddd": 0,
        "assets": assets,
        "layers": root_layers,
        "markers": [],
        "meta": {"g": "svg_blender_lottie", "tc": "vector_gp"},
    }
    fmt = str(file_format).upper()
    if fmt == "TGS":
        result["tgs"] = 1

    return result


def export_gp_vector_lottie(
    filepath: str,
    objects: list[bpy.types.Object],
    scene: bpy.types.Scene,
    *,
    frame_start: int,
    frame_end: int,
    float_precision: int = 3,
    depsgraph: bpy.types.Depsgraph | None = None,
    bake_animation: bool = False,
    optimize_stroke_keys: bool = False,
    write_export_log: bool = False,
    use_dotlottie: bool = False,
    use_tgs: bool = False,
    pack_layers_as_precomps: bool = False,
) -> str:
    layer_stats: list[dict[str, Any]] = []
    frame_stats: dict[int, dict[str, float]] = {}
    file_format = "JSON"
    if use_dotlottie:
        file_format = "LOTTIE"
    if use_tgs:
        file_format = "TGS"
    data = build_lottie_dict_from_grease_pencils(
        objects,
        scene,
        frame_start=frame_start,
        frame_end=frame_end,
        float_precision=float_precision,
        depsgraph=depsgraph,
        bake_animation=bake_animation,
        file_format=file_format,
        layer_stats_out=layer_stats,
        frame_stats_out=frame_stats,
        strict_mask_order=bool(use_tgs),
        composition_name=_composition_name_from_export_path(filepath),
        pack_layers_as_precomps=pack_layers_as_precomps,
    )
    data = _json_compact_whole_floats(data)
    out_path = _write_lottie_output(
        filepath, data, use_dotlottie=use_dotlottie, use_tgs=use_tgs
    )
    if write_export_log:
        write_export_log_file(
            out_path,
            data,
            layer_stats,
            frame_stats,
            optimize_stroke_keys=optimize_stroke_keys,
            bake_animation=bake_animation,
        )
    return out_path
