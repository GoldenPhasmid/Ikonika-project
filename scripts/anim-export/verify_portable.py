import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import types
import zipfile
import bpy

Root = Path(__file__).resolve().parent
Assets = Root.parents[1] / 'animation'
Test = Assets / 'local' / 'portable-check' / 'checkout with spaces' / 'animation'
if Test.exists():
    if not Test.resolve().is_relative_to((Assets / 'local' / 'portable-check').resolve()):
        raise RuntimeError('Test directory escaped its workspace')
    shutil.rmtree(Test)
(Test / 'demo').mkdir(parents=True, exist_ok=True)
(Test / 'shared').mkdir(exist_ok=True)
(Test / 'sources').mkdir(exist_ok=True)
for Name in ('ikonika-demo-windows.zip', 'preview.cmd', 'preview.ps1'):
    shutil.copy2(Assets / 'demo' / Name, Test / 'demo' / Name)
shutil.copy2(Assets / 'lufi-shared.zip', Test)
for File in (Assets / 'shared').glob('*.json'):
    shutil.copy2(File, Test / 'shared' / File.name)
Extension = Test / 'local' / 'extension'
Extension.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(Assets / 'tools' / 'ikonika-lufi-export.zip') as Package:
    Package.extractall(Extension)
Spec = importlib.util.spec_from_file_location('ikonika_portable_test', Extension / '__init__.py')
Addon = importlib.util.module_from_spec(Spec)
Spec.loader.exec_module(Addon)
Addon.register()
bpy.ops.wm.open_mainfile(filepath=str(Assets / 'sources' / 'Lufi_Animation_Main.blend'), load_ui=False)
Scene = bpy.context.scene
Scene.frame_start = 10
Scene.frame_end = 29
Scene.render.fps = 12
Scene.render.fps_base = 1.001
Scene.camera = bpy.data.objects['Camera_Desktop']
Source = Test / 'sources' / 'Short Groom.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(Source), check_existing=False)
Scene.ikonika_export.folder = str(Test)
Scene.ikonika_export.clip = 'short-groom'
Before = (bpy.data.filepath, Scene.frame_current, Scene.frame_start, Scene.frame_end, Scene.camera.name, len(Scene.objects))
os.environ['PATH'] = os.environ['SystemRoot'] + '/System32;' + os.environ['SystemRoot']
for Name in ('QTDIR', 'QT_ROOT', 'QT_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH', 'LUFI_BLENDER_EXE'):
    os.environ.pop(Name, None)
os.environ['HTTP_PROXY'] = 'http://127.0.0.1:9'
os.environ['HTTPS_PROXY'] = 'http://127.0.0.1:9'
os.environ['OPENBLAS_NUM_THREADS'] = '1'
subprocess.run(['cmd', '/d', '/c', str(Test / 'demo' / 'preview.cmd'), '--inspect'], env=os.environ | {'QT_QPA_PLATFORM': 'offscreen'}, check=True)
ArchiveHash = hashlib.sha256((Test / 'demo' / 'ikonika-demo-windows.zip').read_bytes()).hexdigest()
Executable = Test / 'local' / 'demo-runtime' / ArchiveHash[:16] / 'LufiLottieTrial.exe'
ExecutableHash = hashlib.sha256(Executable.read_bytes()).hexdigest()
Result = bpy.ops.ikonika.export_lufi()
assert 'RUNNING_MODAL' in Result, Result
Deadline = time.monotonic() + 300
while not (Test / 'shared' / 'short-groom.json').exists():
    time.sleep(0.5)
    if time.monotonic() > Deadline:
        raise RuntimeError((Test / 'local' / 'short-groom' / 'export.log').read_text())
Clip = json.loads((Test / 'shared' / 'short-groom.json').read_text())
assert Clip['frames'] == 20, Clip['frames']
assert abs(Clip['fps'] - 12 / 1.001) < 0.0001, Clip['fps']
assert Before == (bpy.data.filepath, Scene.frame_current, Scene.frame_start, Scene.frame_end, Scene.camera.name, len(Scene.objects))
Inspect = subprocess.check_output([str(Executable), '--assets', str(Test), '--inspect'], env=os.environ | {'QT_QPA_PLATFORM': 'offscreen'}, text=True)
assert any(Item['id'] == 'short-groom' and Item['frames'] == 20 for Item in json.loads(Inspect)), Inspect
assert hashlib.sha256(Executable.read_bytes()).hexdigest() == ExecutableHash
subprocess.run([str(Executable), '--assets', str(Test), '--clip', 'paw-lick', '--export-shared', str(Test / 'local' / 'old-clip.json')], env=os.environ | {'QT_QPA_PLATFORM': 'offscreen'}, check=True)
PublishedHash = hashlib.sha256((Test / 'lufi-shared.zip').read_bytes()).hexdigest()
bpy.data.objects.remove(bpy.data.objects['07a - Articulated Foreleg'], do_unlink=True)
Broken = Test / 'sources' / 'Broken.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(Broken), copy=True, check_existing=False)
with (Test / 'local' / 'rejected-export.log').open('w') as Log:
    Rejected = subprocess.run([bpy.app.binary_path, '--background', '--disable-autoexec', '--python-exit-code', '1', '--python', str(Extension / 'engine' / 'pipeline.py'), '--', '--source', str(Broken), '--assets', str(Test), '--clip', 'broken', '--verify'], stdout=Log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
assert Rejected.returncode != 0
assert hashlib.sha256((Test / 'lufi-shared.zip').read_bytes()).hexdigest() == PublishedHash
Invalid = Test / 'local' / 'invalid-package'
Invalid.mkdir()
with zipfile.ZipFile(Test / 'lufi-shared.zip') as Package, zipfile.ZipFile(Invalid / 'lufi-shared.zip', 'w', zipfile.ZIP_DEFLATED) as Output:
    for Entry in Package.infolist():
        Data = Package.read(Entry.filename)
        if Entry.filename == 'short-groom.json':
            Value = json.loads(Data)
            Value['frames'] += 1
            Data = json.dumps(Value).encode()
        Output.writestr(Entry.filename, Data)
InvalidResult = subprocess.run([str(Executable), '--assets', str(Invalid), '--inspect'], env=os.environ | {'QT_QPA_PLATFORM': 'offscreen'}, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
assert InvalidResult.returncode == 1, InvalidResult
print('NEGATIVE CHECKS PASS: failed export preserves published package; malformed timeline rejected')
print('PORTABLE CHECK PASS: extension export from frame 10, 20 frames, fractional FPS, relocated path with spaces, no Qt/MSVC/Python PATH, offline dependencies, preserved Blender source and old clip, unchanged executable', flush=True)
(Test / 'local' / 'portable-check.json').write_text(json.dumps({'archive': ArchiveHash, 'executable': ExecutableHash, 'clips': json.loads(Inspect), 'source': Before}, indent=2))
