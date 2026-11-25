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

Export size metrics and human-readable ``.export.log.txt`` reports.

"""

from __future__ import annotations

import json
import math
import os
from typing import Any


def export_log_path_for_output(output_path: str) -> str:
    """Path for the sidecar log file next to a Lottie export."""
    base, _ = os.path.splitext(os.path.abspath(output_path))
    return base + ".export.log.txt"


def json_payload_bytes(obj: Any) -> int:
    """UTF-8 size of compact JSON (for log byte estimates)."""
    return len(json.dumps(obj, separators=(",", ":")).encode("utf-8"))


def accumulate_frame_stat(
    frame_stats_out: dict[int, dict[str, float]] | None,
    frame: int,
    *,
    transform_keys: int = 0,
    shape_keys: int = 0,
    skipped_shapes: int = 0,
    size_bytes: int = 0,
) -> None:
    """Add per-frame export counters when ``frame_stats_out`` is provided."""
    if frame_stats_out is None:
        return
    st = frame_stats_out.setdefault(
        int(frame),
        {
            "transform_key_count": 0.0,
            "shape_key_count": 0.0,
            "skipped_shape_count": 0.0,
            "size_bytes": 0.0,
        },
    )
    st["transform_key_count"] += float(transform_keys)
    st["shape_key_count"] += float(shape_keys)
    st["skipped_shape_count"] += float(skipped_shapes)
    st["size_bytes"] += float(size_bytes)


def count_lottie_property_keys(prop: Any) -> int:
    if not isinstance(prop, dict):
        return 0
    k = prop.get("k")
    if isinstance(k, list):
        return len(k)
    if k is not None:
        return 1
    return 0


def count_layer_transform_keys(layer_ks: dict[str, Any]) -> int:
    return (
        count_lottie_property_keys(layer_ks.get("p"))
        + count_lottie_property_keys(layer_ks.get("r"))
        + count_lottie_property_keys(layer_ks.get("s"))
    )


def accumulate_layer_transform_frame_stats(
    layer_ks: dict[str, Any],
    ip: int,
    frame_stats_out: dict[int, dict[str, float]] | None,
) -> None:
    """Record per-frame transform key counts and serialized sizes into export stats."""
    for kname in ("p", "r", "s"):
        prop = layer_ks.get(kname)
        if not isinstance(prop, dict):
            continue
        kval = prop.get("k")
        if not isinstance(kval, list):
            continue
        for kv in kval:
            if not isinstance(kv, dict):
                continue
            t = int(kv.get("t", ip))
            accumulate_frame_stat(
                frame_stats_out,
                t,
                transform_keys=1,
                size_bytes=json_payload_bytes(kv),
            )


def _layers_by_ind_for_log(data: dict[str, Any]) -> dict[int, dict[str, Any]]:
    """
    Map ``ind`` → layer dict for export log size reporting.

    When layers are packed into a precomposition asset, root ``layers`` only
    contains a thin ``ty`` 0 reference; prefer full ``ty`` 4 shape layers from
    ``assets[*].layers`` with the same ``ind``.
    """
    by_ind: dict[int, dict[str, Any]] = {}
    for lay in data.get("layers") or []:
        if not isinstance(lay, dict):
            continue
        try:
            ind = int(lay.get("ind", 0))
        except (TypeError, ValueError):
            continue
        if ind > 0:
            by_ind[ind] = lay
    for asset in data.get("assets") or []:
        if not isinstance(asset, dict):
            continue
        for lay in asset.get("layers") or []:
            if not isinstance(lay, dict):
                continue
            try:
                ind = int(lay.get("ind", 0))
            except (TypeError, ValueError):
                continue
            if ind <= 0:
                continue
            if int(lay.get("ty", -1)) == 4:
                by_ind[ind] = lay
            elif ind not in by_ind:
                by_ind[ind] = lay
    return by_ind


def write_export_log_file(
    output_path: str,
    data: dict[str, Any],
    layer_stats: list[dict[str, Any]],
    frame_stats: dict[int, dict[str, float]] | None,
    *,
    optimize_stroke_keys: bool,
    bake_animation: bool,
) -> str:
    """Write ``.export.log.txt`` next to ``output_path`` and return that path."""
    layer_by_ind = _layers_by_ind_for_log(data)

    lines: list[str] = []
    lines.append("Lottie export report")
    lines.append(f"output: {output_path}")
    lines.append(
        f"format: {os.path.splitext(output_path)[1].lower() or '.json'}"
    )
    lines.append(
        f"composition: {data.get('nm', '')} | {data.get('w')}x{data.get('h')} | fr={data.get('fr')} | ip={data.get('ip')} op={data.get('op')}"
    )
    lines.append(
        f"options: bake_animation={int(bool(bake_animation))}, optimize_stroke_keys={int(bool(optimize_stroke_keys))}"
    )
    lines.append("")
    lines.append(
        "layer      | ind | size_kb | sampled_frames | exported_transform_keys | exported_shape_keys | skipped_shape_keys | source_layer_keys | strokes(max) | groups"
    )

    total_layer_bytes = 0
    total_transform_keys = 0
    total_shape_keys = 0
    total_skipped_shape_keys = 0
    total_sampled_frames = 0

    for st in layer_stats:
        ind = int(st.get("ind", 0))
        lay = layer_by_ind.get(ind)
        layer_bytes = json_payload_bytes(lay) if lay is not None else 0
        total_layer_bytes += layer_bytes
        total_transform_keys += int(st.get("transform_key_count", 0))
        total_shape_keys += int(st.get("shape_key_count", 0))
        total_skipped_shape_keys += int(st.get("shape_key_skipped", 0))
        total_sampled_frames += int(st.get("sampled_frame_count", 0))
        lines.append(
            f"{st.get('name', ''):<10} | {ind:>3} | {layer_bytes / 1024.0:>7.2f} | "
            f"{int(st.get('sampled_frame_count', 0)):>14} | {int(st.get('transform_key_count', 0)):>23} | "
            f"{int(st.get('shape_key_count', 0)):>19} | {int(st.get('shape_key_skipped', 0)):>18} | "
            f"{int(st.get('source_layer_key_count', 0)):>17} | {int(st.get('stroke_count', 0)):>12} | "
            f"{int(st.get('shape_group_count', 0)):>6}"
        )

    lines.append("")
    lines.append("totals")
    lines.append(f"layers_exported: {len(layer_stats)}")
    lines.append(f"layers_payload_kb: {total_layer_bytes / 1024.0:.2f}")
    lines.append(f"sampled_frames_total: {total_sampled_frames}")
    lines.append(f"transform_keys_total: {total_transform_keys}")
    lines.append(f"shape_keys_total: {total_shape_keys}")
    lines.append(f"shape_keys_skipped_total: {total_skipped_shape_keys}")
    lines.append(f"file_size_kb: {os.path.getsize(output_path) / 1024.0:.2f}")

    if frame_stats:
        lines.append("")
        lines.append("per-frame")
        lines.append(
            "frame | transform_keys | shape_keys | skipped_shapes | size_kb | bar"
        )
        ip = int(data.get("ip", 0))
        op = int(data.get("op", ip))
        if op < ip:
            ip, op = op, ip
        for fr in range(ip, op + 1):
            st = frame_stats.get(
                fr,
                {
                    "transform_key_count": 0.0,
                    "shape_key_count": 0.0,
                    "skipped_shape_count": 0.0,
                    "size_bytes": 0.0,
                },
            )
            kb = float(st.get("size_bytes", 0.0)) / 1024.0
            whole = int(math.floor(kb)) if kb > 0 else 0
            frac = kb - float(whole)
            bars = "█" * whole
            frac_chars = ["▏", "▎", "▍", "▌", "▋", "▊", "▉"]
            frac_idx = int(round(frac * 8.0))
            if frac_idx >= 8:
                bars = "█" * (whole + 1)
            elif frac_idx > 0:
                bars += frac_chars[frac_idx - 1]
            if kb > 0 and len(bars) == 0:
                bars = "▏"
            lines.append(
                f"{fr:>5} | {int(st.get('transform_key_count', 0)):>14} | "
                f"{int(st.get('shape_key_count', 0)):>10} | {int(st.get('skipped_shape_count', 0)):>14} | "
                f"{kb:>7.2f} | {bars}"
            )

    log_path = export_log_path_for_output(output_path)
    with open(log_path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return log_path
