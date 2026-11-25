import bpy
import json
import os
from pathlib import Path
import re
import subprocess
import time

class IKONIKA_PG_Export(bpy.types.PropertyGroup):
    folder: bpy.props.StringProperty(name='Animation folder', subtype='DIR_PATH')
    clip: bpy.props.StringProperty(name='Clip ID')

def FindAnimationFolder(context):
    Settings = context.scene.ikonika_export
    if Settings.folder:
        return Path(bpy.path.abspath(Settings.folder)).resolve()
    if not bpy.data.filepath:
        return None
    return next((Parent for Parent in Path(bpy.data.filepath).resolve().parents if (Parent / 'shared').is_dir() and (Parent / 'demo').is_dir()), None)

class IKONIKA_OT_OpenDemo(bpy.types.Operator):
    bl_idname = 'ikonika.open_demo'
    bl_label = 'Open Demo'

    def execute(self, context):
        Folder = FindAnimationFolder(context)
        if not Folder or not (Folder / 'demo' / 'preview.cmd').is_file():
            self.report({'ERROR'}, 'Choose the animation folder containing demo/preview.cmd')
            return {'CANCELLED'}
        subprocess.Popen(['cmd', '/d', '/c', str(Folder / 'demo' / 'preview.cmd')], creationflags=subprocess.CREATE_NO_WINDOW)
        return {'FINISHED'}

class IKONIKA_OT_Export(bpy.types.Operator):
    bl_idname = 'ikonika.export_lufi'
    bl_label = 'Export to Ikonika Demo'
    bl_description = 'Export and verify the scene frame range using the active camera; reload the demo afterward'
    Process = None
    Timer = None
    Log = None
    Snapshot = None
    Original = None

    def execute(self, context):
        Settings = context.scene.ikonika_export
        if not bpy.data.filepath:
            self.report({'ERROR'}, 'Save the Blender file in animation/sources first')
            return {'CANCELLED'}
        if context.window_manager.get('ikonika_export_running', False):
            self.report({'ERROR'}, 'An Ikonika export is already running')
            return {'CANCELLED'}
        Source = Path(bpy.data.filepath).resolve()
        Folder = FindAnimationFolder(context)
        if not Folder or not (Folder / 'demo' / 'ikonika-demo-windows.zip').exists():
            self.report({'ERROR'}, 'Choose the animation folder containing demo/ikonika-demo-windows.zip')
            return {'CANCELLED'}
        Identifier = Settings.clip or re.sub(r'[^a-z0-9]+', '-', Source.stem.lower().removeprefix('lufi_')).strip('-')
        if not re.fullmatch('[a-z0-9][a-z0-9-]*', Identifier):
            self.report({'ERROR'}, 'Clip ID must contain lowercase letters, digits, and hyphens')
            return {'CANCELLED'}
        Work = Folder / 'local' / Identifier
        Work.mkdir(parents=True, exist_ok=True)
        self.Snapshot = Work / ('snapshot-' + str(time.time_ns()) + '.blend')
        self.Original = (bpy.data.filepath, bpy.context.scene.frame_current)
        bpy.ops.wm.save_as_mainfile(filepath=str(self.Snapshot), copy=True, check_existing=False)
        Engine = Folder.parent / 'scripts' / 'anim-export' / 'pipeline.py'
        if not Engine.exists():
            Engine = Path(__file__).resolve().parent / 'engine' / 'pipeline.py'
        self.Log = (Work / 'export.log').open('w')
        try:
            self.Process = subprocess.Popen([bpy.app.binary_path, '--background', '--disable-autoexec', '--python-exit-code', '1', '--python', str(Engine), '--', '--source', str(self.Snapshot), '--clip', Identifier, '--assets', str(Folder), '--verify'], stdout=self.Log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
        except Exception as Error:
            self.Log.close()
            self.Snapshot.unlink(missing_ok=True)
            self.report({'ERROR'}, str(Error))
            return {'CANCELLED'}
        context.window_manager['ikonika_export_running'] = True
        self.Timer = context.window_manager.event_timer_add(0.5, window=context.window)
        context.window_manager.modal_handler_add(self)
        self.report({'INFO'}, 'Export running; keep Blender open. Log: ' + str(Work / 'export.log'))
        return {'RUNNING_MODAL'}

    def modal(self, context, event):
        if event.type != 'TIMER' or self.Process.poll() is None:
            return {'PASS_THROUGH'}
        context.window_manager.event_timer_remove(self.Timer)
        context.window_manager['ikonika_export_running'] = False
        self.Log.close()
        self.Snapshot.unlink(missing_ok=True)
        if self.Process.returncode:
            self.report({'ERROR'}, 'Export failed. See animation/local/<clip>/export.log and logs; published assets were not replaced')
            return {'CANCELLED'}
        self.report({'INFO'}, 'Export verified. Press Reload in the Ikonika demo')
        return {'FINISHED'}

class IKONIKA_PT_Export(bpy.types.Panel):
    bl_label = 'Ikonika Animation'
    bl_idname = 'IKONIKA_PT_export'
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Ikonika'

    def draw(self, context):
        Layout = self.layout
        Settings = context.scene.ikonika_export
        Layout.prop(Settings, 'folder')
        Layout.prop(Settings, 'clip')
        Layout.prop(context.scene, 'camera')
        Row = Layout.row(align=True)
        Row.prop(context.scene, 'frame_start', text='Start')
        Row.prop(context.scene, 'frame_end', text='End')
        Layout.prop(context.scene.render, 'fps')
        Layout.prop(context.scene.render, 'fps_base')
        Layout.operator('ikonika.export_lufi')
        Layout.operator('ikonika.open_demo')

Classes = (IKONIKA_PG_Export, IKONIKA_OT_OpenDemo, IKONIKA_OT_Export, IKONIKA_PT_Export)

def register():
    for Class in Classes:
        bpy.utils.register_class(Class)
    bpy.types.Scene.ikonika_export = bpy.props.PointerProperty(type=IKONIKA_PG_Export)

def unregister():
    del bpy.types.Scene.ikonika_export
    for Class in reversed(Classes):
        bpy.utils.unregister_class(Class)
