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

Graphic math and color helpers for Lottie export.
"""

import math
from typing import Any


_LINEAR_SRGB_THRESHOLD = 0.04045 * (1.0 / 12.92)


def linearrgb_to_srgb(c: float) -> float:
    """
    Scene-linear RGB component → display sRGB (0..1).

    Inverse of IEC 61966-2-1 sRGB EOTF (Blender scene-linear convention).
    """
    if c <= 0.0:
        return 0.0
    if c <= _LINEAR_SRGB_THRESHOLD:
        return min(1.0, c * 12.92)
    return min(1.0, max(0.0, 1.055 * (pow(c, 1.0 / 2.4)) - 0.055))


def rnd(x: float, nd: int = 3) -> float:
    return round(float(x), nd)


def vangle_deg(v: tuple[float, float]) -> float:
    return math.degrees(math.atan2(v[1], v[0]))


def vlen(v: tuple[float, float]) -> float:
    return float(math.hypot(v[0], v[1]))


def _angle_diff_deg(a: float, b: float) -> float:
    d = (float(a) - float(b)) % 360.0
    if d > 180.0:
        d -= 360.0
    return abs(d)


def _baked_transform_triplet_similar(
    p0: dict[str, Any],
    r0: dict[str, Any],
    s0: dict[str, Any],
    p1: dict[str, Any],
    r1: dict[str, Any],
    s1: dict[str, Any],
    *,
    pos_tol: float,
    rot_tol_deg: float,
    scale_tol: float,
) -> bool:
    ps0, ps1 = p0["s"], p1["s"]
    if abs(float(ps0[0]) - float(ps1[0])) > pos_tol:
        return False
    if abs(float(ps0[1]) - float(ps1[1])) > pos_tol:
        return False
    if _angle_diff_deg(float(r0["s"][0]), float(r1["s"][0])) > rot_tol_deg:
        return False
    ss0, ss1 = s0["s"], s1["s"]
    if abs(float(ss0[0]) - float(ss1[0])) > scale_tol:
        return False
    if abs(float(ss0[1]) - float(ss1[1])) > scale_tol:
        return False
    return True


def dedupe_baked_transform_triplets(
    p_keys: list[dict[str, Any]],
    r_keys: list[dict[str, Any]],
    s_keys: list[dict[str, Any]],
    *,
    pos_tol: float,
    rot_tol_deg: float,
    scale_tol: float,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]]]:
    """Drop consecutive samples when position, rotation, and scale are all within tolerances."""
    n = len(p_keys)
    if n == 0 or n != len(r_keys) or n != len(s_keys):
        return p_keys, r_keys, s_keys
    out_p = [p_keys[0]]
    out_r = [r_keys[0]]
    out_s = [s_keys[0]]
    for i in range(1, n):
        if _baked_transform_triplet_similar(
            out_p[-1],
            out_r[-1],
            out_s[-1],
            p_keys[i],
            r_keys[i],
            s_keys[i],
            pos_tol=pos_tol,
            rot_tol_deg=rot_tol_deg,
            scale_tol=scale_tol,
        ):
            continue
        out_p.append(p_keys[i])
        out_r.append(r_keys[i])
        out_s.append(s_keys[i])
    return out_p, out_r, out_s


def mean_point_radius(stroke) -> float:
    pts = stroke.points
    if not pts:
        return 0.0
    return sum(float(p.radius) for p in pts) / float(len(pts))


def jtj_largest_eigenvalue(ax: float, ay: float, bx: float, by: float) -> float:
    """Largest eigenvalue of J.T @ J for J = [[ax, bx], [ay, by]] (columns = screen deltas)."""
    a = ax * ax + ay * ay
    b = ax * bx + ay * by
    c = bx * bx + by * by
    tr = a + c
    det_jtj = a * c - b * b
    disc = max(0.0, tr * tr - 4.0 * det_jtj)
    return max(0.0, 0.5 * (tr + disc**0.5))
