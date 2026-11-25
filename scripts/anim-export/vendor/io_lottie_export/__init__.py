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

Grease Pencil to Lottie exporter add-on module (operators, file output, UI).
"""

import os
import bpy  # type: ignore
from bpy_extras.io_utils import ExportHelper  # type: ignore

from .exporter import export_gp_vector_lottie
from .utils import gp_objects_for_export


bl_info = {
    "name": "Grease Pencil Lottie Export",
    "author": "multlabs",
    "version": (0, 1, 2),
    "blender": (5, 0, 0),
    "location": "File > Export",
    "description": "Export Grease Pencil objects to Lottie JSON, dotLottie, or TGS",
    "warning": "",
    "doc_url": "https://t.me/multlabs",
    "category": "Import-Export",
}


def _report_tgs_telegram_hints(
    op: bpy.types.Operator,
    scene: bpy.types.Scene,
    frame_start: int,
    frame_end: int,
    out_path: str,
) -> None:
    """
    Telegram animated stickers (``.tgs``) are usually previewed only if the
    animation passes client checks; common limits are about **3s** length and
    **~64KB** gzipped. Our export matches Blender's inclusive frame range
    (same as composition ``ip`` / ``op`` in Lottie).
    """
    r = scene.render
    base = float(r.fps_base) if getattr(
        r, "fps_base", None) not in (None, 0) else 1.0
    fr = float(r.fps) / base
    if fr <= 0:
        return
    fs = min(int(frame_start), int(frame_end))
    fe = max(int(frame_start), int(frame_end))
    nframes = fe - fs + 1
    dur = nframes / fr
    if dur > 3.0 + 1e-6:
        op.report(
            {"WARNING"},
            f"TGS: duration ~{dur:.2f}s is over Telegram's ~3s animated sticker limit; "
            f"Telegram will not except this file as a sticker. "
            f"Use a shorter export frame range (≤ {int(3 * fr)} frames at {fr:.0f} fps).",
        )
    try:
        sz = os.path.getsize(out_path)
        if sz > 64 * 1024:
            op.report(
                {"WARNING"},
                f"TGS: {sz / 1024:.1f}KB exceeds the common ~64KB sticker limit; "
                "Telegram will not except this file as a sticker.",
            )
    except OSError:
        pass


class GPVectorLottieExporter(bpy.types.Operator, ExportHelper):
    """Export Grease Pencil object(s) to Lottie JSON, dotLottie, or TGS (vector paths + fills)."""

    bl_idname = "lottie_exporter.gp_vector"
    bl_label = "Grease Pencil to Lottie (vector)"
    bl_options = {"REGISTER", "UNDO"}

    filename_ext = ".json"
    filter_glob: bpy.props.StringProperty(
        default="*.json;*.lottie;*.tgs", options={"HIDDEN"}
    )  # type: ignore

    frame_start: bpy.props.IntProperty(
        name="Start", default=1, min=0, max=1000000)  # type: ignore
    frame_end: bpy.props.IntProperty(
        name="End", default=250, min=0, max=1000000)  # type: ignore
    use_custom_frame_range: bpy.props.BoolProperty(
        name="Custom Frame Range",
        description="Use custom start/end instead of scene frame range",
        default=False,
    )  # type: ignore
    float_precision: bpy.props.IntProperty(
        name="Float Precision",
        description="Decimal places for 2D coordinates",
        default=1,
        min=0,
        max=10,
        soft_min=0,
        soft_max=5,
    )  # type: ignore
    bake_animation: bpy.props.BoolProperty(
        name="Bake Animation",
        description=(
            "Sample GP object transform every frame in the export range "
            "(Useful for animation with constraints, drivers, 360° rotation). "
            "Disables F-curve-based transform keys"
        ),
        default=False,
    )  # type: ignore
    optimize_stroke_keys: bpy.props.BoolProperty(
        name="Skip Unchanged Stroke Keys",
        description=(
            "Compare each stroke's path/fill/stroke/width against the previously exported "
            "key for that stroke and skip identical samples. Reduces file size but can "
            "increase export time on heavy scenes"
        ),
        default=False,
    )  # type: ignore
    write_export_log: bpy.props.BoolProperty(
        name="Output Size Export Log",
        description=(
            "Write a separate .export.log.txt file with per-layer and per-frame metrics"
        ),
        default=False,
    )  # type: ignore
    pack_layers_as_precomps: bpy.props.BoolProperty(
        name="Pack Layers as Precomposition",
        description=(
            "Put all exported Grease Pencil layers into one Lottie precomposition asset; "
            "the root composition contains a single static precomposition layer. "
            "May be required by some Lottie players to display the animation correctly."
        ),
        default=False,
    )  # type: ignore
    format: bpy.props.EnumProperty(
        name="Format",
        description="The format of the exported file",
        items=[
            ("JSON", "Lottie JSON (.json)",
             "Lottie JSON (.json) - larger file size, human readable"),
            ("LOTTIE", "Compressed DotLottie (.lottie)",
             "Compressed DotLottie - smaller file size"),
            ("TGS", "Telegram TGS (.tgs)",
             "Lottie-based animation format used for animated stickers and emoji",
             ),
        ],
        default="JSON",
    )  # type: ignore
    collection: bpy.props.StringProperty(
        name="Source Collection",
        description="Collection name passed by Blender Collection export handler",
        default="",
    )  # type: ignore
    export_scope: bpy.props.EnumProperty(
        name="Source",
        description="Which Grease Pencil objects to export",
        items=[
            ("ALL", "All in View Layer",
             "Export all Grease Pencil objects in the active view layer"),
            ("SELECTED", "Selected Only",
             "Export only selected Grease Pencil objects"),
            ("COLLECTION", "Collection",
             "Export Grease Pencil objects from a specific collection"),
        ],
        default="ALL",
    )  # type: ignore
    export_collection: bpy.props.StringProperty(
        name="Collection",
        description="Collection to export from (includes nested children)",
        default="",
    )  # type: ignore

    def draw(self, context):
        layout = self.layout

        # Are we inside the File browser
        is_file_browser = context.space_data.type == 'FILE_BROWSER'

        if is_file_browser:
            layout.prop(self, "export_scope")
            if self.export_scope == "COLLECTION":
                layout.prop_search(self, "export_collection",
                                   bpy.data, "collections", text="Collection")
        layout.prop(self, "bake_animation")
        layout.separator()
        layout.prop(self, "use_custom_frame_range")
        col = layout.column(align=True)
        col.enabled = bool(self.use_custom_frame_range)
        col.prop(self, "frame_start")
        col.prop(self, "frame_end")
        layout.separator()
        layout.label(text="Output options")
        layout.prop(self, "format")
        layout.prop(self, "float_precision")
        layout.prop(self, "optimize_stroke_keys")
        layout.prop(self, "write_export_log")
        layout.prop(self, "pack_layers_as_precomps")

    @classmethod
    def poll(cls, context):
        return any(ob.type == "GREASEPENCIL" and ob.data is not None for ob in context.view_layer.objects)

    def invoke(self, context, event):
        self.frame_start = int(context.scene.frame_start)
        self.frame_end = int(context.scene.frame_end)
        context.window_manager.fileselect_add(self)
        return {"RUNNING_MODAL"}

    def execute(self, context):
        # FileHandler-based collection export path: Blender passes collection name here.
        collection_from_handler = (
            bpy.data.collections.get(
                self.collection) if self.collection else None
        )

        if collection_from_handler is not None:
            objects = gp_objects_for_export(
                context.view_layer,
                only_selected=False,
                collection=collection_from_handler,
            )
        elif self.export_scope == "COLLECTION":
            chosen_collection = bpy.data.collections.get(
                self.export_collection) if self.export_collection else None
            if chosen_collection is None:
                self.report({"ERROR"}, "Choose a collection to export.")
                return {"CANCELLED"}
            objects = gp_objects_for_export(
                context.view_layer,
                only_selected=False,
                collection=chosen_collection,
            )
        else:
            selected = bool(self.export_scope == "SELECTED")
            objects = gp_objects_for_export(
                context.view_layer,
                only_selected=selected,
                collection=None,
            )
        scene_frame_start = int(context.scene.frame_start)
        scene_frame_end = int(context.scene.frame_end)
        frame_start = int(
            self.frame_start) if self.use_custom_frame_range else scene_frame_start
        frame_end = int(
            self.frame_end) if self.use_custom_frame_range else scene_frame_end
        if not objects:
            if self.export_scope == "COLLECTION":
                self.report(
                    {"ERROR"}, "No Grease Pencil objects found in selected collection (within active view layer).")
            else:
                self.report(
                    {"ERROR"},
                    "No selected Grease Pencil objects."
                    if self.export_scope == "SELECTED"
                    else "No Grease Pencil objects in this view layer.",
                )
            return {"CANCELLED"}
        try:
            out_path = export_gp_vector_lottie(
                self.filepath,
                objects,
                context.scene,
                frame_start=frame_start,
                frame_end=frame_end,
                float_precision=int(self.float_precision),
                depsgraph=context.evaluated_depsgraph_get(),
                bake_animation=bool(self.bake_animation),
                optimize_stroke_keys=bool(self.optimize_stroke_keys),
                write_export_log=bool(self.write_export_log),
                use_dotlottie=self.format == "LOTTIE",
                use_tgs=self.format == "TGS",
                pack_layers_as_precomps=bool(self.pack_layers_as_precomps),
            )
        except Exception as err:
            self.report({"ERROR"}, str(err))
            return {"CANCELLED"}
        self.report({"INFO"}, f"Wrote {out_path}")

        if self.format == "TGS":
            _report_tgs_telegram_hints(
                self, context.scene, frame_start, frame_end, out_path)
        return {"FINISHED"}


class IO_FH_lottie(bpy.types.FileHandler):
    """Enable Collection Export integration for Lottie exporter."""

    bl_idname = "IO_FH_lottie"
    bl_label = "Lottie"
    bl_export_operator = GPVectorLottieExporter.bl_idname
    bl_file_extensions = ".json;.lottie;.tgs"


classes = (GPVectorLottieExporter, IO_FH_lottie)


def _gp_vector_export_menu(self, context):
    default_path = os.path.splitext(bpy.data.filepath)[0] + ".json"
    self.layout.operator(
        GPVectorLottieExporter.bl_idname,
        text="Grease Pencil to Lottie (vector)",
    ).filepath = os.path.splitext(default_path)[0] + "_vector.json"


def register() -> None:
    for c in classes:
        bpy.utils.register_class(c)
    bpy.types.TOPBAR_MT_file_export.append(_gp_vector_export_menu)


def unregister() -> None:
    bpy.types.TOPBAR_MT_file_export.remove(_gp_vector_export_menu)
    for c in classes:
        bpy.utils.unregister_class(c)
