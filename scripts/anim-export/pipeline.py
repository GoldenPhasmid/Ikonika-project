import argparse
import contextlib
import importlib
import hashlib
import json
import os
from pathlib import Path
import re
import runpy
import shutil
import subprocess
import sys
import zipfile

Root = Path(__file__).resolve().parent
sys.dont_write_bytecode = True
os.environ['OPENBLAS_NUM_THREADS'] = '1'
sys.path.insert(0, str(Root))

def Tool(Name, Override=None, Candidates=()):
    Value = os.environ.get(Override, '') if Override else ''
    Found = Value or shutil.which(Name) or next((str(Path(Item)) for Item in Candidates if Path(Item).is_file()), '')
    if not Found:
        raise RuntimeError('Cannot find ' + Name + '. Set ' + (Override or 'PATH') + ' for developer builds.')
    return Path(Found).resolve()

def DeveloperBuild(Assets, Package):
    QtOverride = os.environ.get('QT_ROOT') or os.environ.get('QTDIR')
    Deploy = Tool('windeployqt', 'LUFI_WINDEPLOYQT', [Path(QtOverride) / 'bin' / 'windeployqt.exe'] if QtOverride else [])
    Qt = Path(os.environ.get('QT_ROOT', Deploy.parent.parent)).resolve()
    Cmake = Tool('cmake', 'LUFI_CMAKE', [Qt.parents[1] / 'Tools' / 'CMake_64' / 'bin' / 'cmake.exe'])
    Demo = Assets / 'demo'
    Build = Assets / 'local' / 'verify-build'
    subprocess.run([str(Cmake), '-S', str(Demo), '-B', str(Build), '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DCMAKE_PREFIX_PATH=' + Qt.as_posix()], check=True)
    subprocess.run([str(Cmake), '--build', str(Build), '--config', 'Release'], check=True)
    if not Package:
        return
    Output = Assets / 'local' / 'demo-package'
    if Output.exists():
        if not Output.resolve().is_relative_to((Assets / 'local').resolve()):
            raise RuntimeError('Package staging directory escaped animation/local')
        shutil.rmtree(Output)
    Output.mkdir(exist_ok=True)
    shutil.copy2(Build / 'Release' / 'LufiLottieTrial.exe', Output)
    subprocess.run([str(Deploy), '--release', '--compiler-runtime', '--no-translations', '--qmldir', str(Demo), '--dir', str(Output), str(Output / 'LufiLottieTrial.exe')], check=True)
    (Output / 'qt.conf').write_text('[Paths]\nPrefix=.\nPlugins=.\nQmlImports=qml\n')
    for Plugin in ('qoffscreen.dll', 'qminimal.dll'):
        shutil.copy2(Qt / 'plugins' / 'platforms' / Plugin, Output / 'platforms' / Plugin)
    Locator = Path(os.environ.get('ProgramFiles(x86)', '')) / 'Microsoft Visual Studio' / 'Installer' / 'vswhere.exe'
    Installation = subprocess.check_output([str(Locator), '-latest', '-products', '*', '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    RuntimeFolders = sorted((Path(Installation) / 'VC' / 'Redist' / 'MSVC').glob('*/x64/Microsoft.VC143.CRT'))
    if not RuntimeFolders:
        raise RuntimeError('MSVC redistributable DLLs not found')
    for File in RuntimeFolders[-1].glob('*.dll'):
        shutil.copy2(File, Output / File.name)
    BuildInfo = {'qt': subprocess.check_output([str(Qt / 'bin' / 'qmake.exe'), '-query', 'QT_VERSION'], text=True).strip(), 'sourceHashes': {File.name: hashlib.sha256(File.read_bytes()).hexdigest() for File in sorted(Demo.iterdir()) if File.suffix in ('.cpp', '.h', '.qml') or File.name == 'CMakeLists.txt'}}
    (Output / 'build-info.json').write_text(json.dumps(BuildInfo, indent=2))
    Notices = Demo / 'licenses'
    if Notices.exists():
        shutil.copytree(Notices, Output / 'licenses', dirs_exist_ok=True)
    with zipfile.ZipFile(Demo / 'ikonika-demo-windows.zip.tmp', 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as Archive:
        for File in sorted(Output.rglob('*')):
            if not File.is_file():
                continue
            Info = zipfile.ZipInfo(File.relative_to(Output).as_posix(), (1980, 1, 1, 0, 0, 0))
            Info.compress_type = zipfile.ZIP_DEFLATED
            Archive.writestr(Info, File.read_bytes(), compresslevel=9)
    os.replace(Demo / 'ikonika-demo-windows.zip.tmp', Demo / 'ikonika-demo-windows.zip')
    PackageExtension(Assets)
    print('Portable demo:', Demo / 'ikonika-demo-windows.zip', flush=True)

def PackageExtension(Assets):
    Destination = Assets / 'tools'
    Destination.mkdir(exist_ok=True)
    Files = [(Path('__init__.py'), Root / 'blender_extension' / '__init__.py'), (Path('blender_manifest.toml'), Root / 'blender_extension' / 'blender_manifest.toml'), (Path('LICENSE'), Root / 'vendor' / 'io_lottie_export' / 'LICENSE')]
    Files += [(Path('engine') / File.relative_to(Root), File) for File in Root.rglob('*') if File.is_file() and 'blender_extension' not in File.parts and '__pycache__' not in File.parts and (File.suffix in ('.py', '.json', '.md') or File.name == 'LICENSE')]
    with zipfile.ZipFile(Destination / 'ikonika-lufi-export.zip', 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as Archive:
        for Name, File in sorted(Files):
            Info = zipfile.ZipInfo(Name.as_posix(), (1980, 1, 1, 0, 0, 0))
            Info.compress_type = zipfile.ZIP_DEFLATED
            Archive.writestr(Info, File.read_bytes(), compresslevel=9)

def ExtractDemo(Assets):
    Archive = Assets / 'demo' / 'ikonika-demo-windows.zip'
    if not Archive.exists():
        raise RuntimeError('Portable demo missing. Developer: run bake.cmd --package-demo once.')
    Digest = hashlib.sha256(Archive.read_bytes()).hexdigest()
    Destination = Assets / 'local' / 'demo-runtime' / Digest[:16]
    if not (Destination / '.complete').exists():
        Destination.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(Archive) as Package:
            Package.extractall(Destination)
        (Destination / '.complete').write_text(Digest)
    return Destination / 'LufiLottieTrial.exe'

def Main():
    import bpy
    Parser = argparse.ArgumentParser(description='Lufi export running inside Blender; animator exports need no external toolchain.')
    Parser.add_argument('--assets', type=Path, default=Root.parents[1] / 'animation')
    Parser.add_argument('--source', type=Path)
    Parser.add_argument('--clip')
    Parser.add_argument('--camera')
    Parser.add_argument('--rigid-layout', type=Path, help='Use the rigid part list from a compatible character when starting a fresh bundle.')
    Parser.add_argument('--start', type=int)
    Parser.add_argument('--end', type=int)
    Parser.add_argument('--skip-export', action='store_true')
    Parser.add_argument('--verify', action='store_true')
    Parser.add_argument('--build-demo', action='store_true')
    Parser.add_argument('--package-demo', action='store_true')
    Parser.add_argument('--package-extension', action='store_true')
    Args = Parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
    Assets = Args.assets.resolve()
    if Args.build_demo or Args.package_demo:
        DeveloperBuild(Assets, Args.package_demo)
        return
    if Args.package_extension:
        PackageExtension(Assets)
        return
    MainSource = Assets / 'sources' / 'Lufi_Animation_Main.blend'
    Source = (Args.source or MainSource).resolve()
    Identifier = Args.clip or ('paw-lick' if Source == MainSource.resolve() else
        re.sub(r'[^a-z0-9]+', '-', Source.stem.lower().removeprefix('lufi_')).strip('-'))
    if not re.fullmatch('[a-z0-9][a-z0-9-]*', Identifier):
        raise ValueError('Clip identifier must contain lowercase letters, digits, and hyphens')
    Work = Assets / 'local' / Identifier
    Work.mkdir(parents=True, exist_ok=True)
    Settings = Work / 'export-settings.json'
    if Args.skip_export:
        if not Settings.exists():
            raise RuntimeError('No export-settings.json for ' + Identifier + '; run a full export first.')
        Metadata = json.loads(Settings.read_text())
        Source = Path(Metadata['source'])
    else:
        SampledSource = Work / 'sampled-source.blend'
        if Source != SampledSource:
            shutil.copyfile(Source, SampledSource)
        Source = SampledSource
        bpy.ops.wm.open_mainfile(filepath=str(Source), load_ui=False)
        Scene = bpy.context.scene
        Metadata = {'source': str(Source), 'start': Args.start if Args.start is not None else Scene.frame_start, 'end': Args.end if Args.end is not None else Scene.frame_end, 'camera': Args.camera or (Scene.camera.name if Scene.camera else None)}
        Profile = json.loads((Root / 'lufi-profile.json').read_text())
        if not Metadata['camera'] or Metadata['camera'] not in Scene.objects or Scene.objects[Metadata['camera']].type != 'CAMERA':
            raise ValueError('Select a camera before exporting')
        Missing = [Name for Name in Profile['required'] if Name not in Scene.objects]
        if Missing:
            raise ValueError('Lufi profile requires: ' + ', '.join(Missing))
        Scene.frame_set(Metadata['start'])
        Foreleg = Scene.objects[Profile['foreleg']]
        if any(not Layer.current_frame() or len(Layer.current_frame().drawing.strokes) != 1 for Layer in Foreleg.data.layers):
            raise ValueError('Each articulated foreleg layer must have one stroke')
        if Metadata['end'] < Metadata['start']:
            raise ValueError('Frame end precedes frame start')
        if Scene.render.resolution_percentage != 100:
            raise ValueError('Set output resolution percentage to 100 before exporting')
        Settings.write_text(json.dumps(Metadata, indent=2))
    os.environ.update(LUFI_ANIMATION_ROOT=str(Assets), LUFI_WORK_DIR=str(Work), LUFI_CLIP_ID=Identifier, LUFI_FRAME_START=str(Metadata['start']), LUFI_FRAME_END=str(Metadata['end']), LUFI_CAMERA=Metadata['camera'])
    import animation_paths
    importlib.reload(animation_paths)
    Logs = Work / 'logs'
    Logs.mkdir(exist_ok=True)
    def Stage(Name, Script, Arguments=()):
        print(Name, flush=True)
        Previous = sys.argv
        sys.argv = [str(Root / Script), *map(str, Arguments)]
        try:
            with (Logs / (Name + '.log')).open('w') as Log, contextlib.redirect_stdout(Log), contextlib.redirect_stderr(Log):
                runpy.run_path(str(Root / Script), run_name='__main__')
        except Exception as Error:
            raise RuntimeError(Name + ' failed; see ' + str(Logs / (Name + '.log'))) from Error
        finally:
            sys.argv = Previous
    if not Args.skip_export:
        Stage('export', 'export_trial.py')
    Stage('prepare-geometry', 'optimize.py')
    Candidate = Work / 'candidate'
    Candidate.mkdir(exist_ok=True)
    Shared = Assets / 'shared'
    for Old in Candidate.glob('*.json'):
        Old.unlink()
    if Shared.exists():
        for File in Shared.glob('*.json'):
            shutil.copy2(File, Candidate / File.name)
    BuilderArguments = ['--clip', Identifier, '--output', Candidate, '--bundle', Candidate / 'lufi-shared.zip']
    if (Shared / 'character.json').exists():
        BuilderArguments += ['--reuse', '--character', Shared / 'character.json']
    elif Args.rigid_layout:
        BuilderArguments += ['--rigid-layout', Args.rigid_layout.resolve()]
    Stage('build-shared', 'build_shared.py', BuilderArguments)
    if Args.verify:
        bpy.ops.wm.open_mainfile(filepath=str(Source), load_ui=False)
        Stage('verify-blender', 'verify_export.py')
        Demo = ExtractDemo(Assets)
        with (Logs / 'assemble-verifier.log').open('w') as Log:
            subprocess.run([str(Demo), '--assets', str(Candidate), '--clip', Identifier, '--export-shared', str(Work / 'shared-assembled.json')], env=os.environ | {'QT_QPA_PLATFORM': 'offscreen'}, stdout=Log, stderr=subprocess.STDOUT, check=True)
        Stage('verify-shared', 'verify_shared.py')
    Shared.mkdir(exist_ok=True)
    for File in Candidate.glob('*.json'):
        Temporary = Shared / (File.name + '.tmp')
        shutil.copyfile(File, Temporary)
        os.replace(Temporary, Shared / File.name)
    Temporary = Assets / 'lufi-shared.zip.tmp'
    shutil.copyfile(Candidate / 'lufi-shared.zip', Temporary)
    os.replace(Temporary, Assets / 'lufi-shared.zip')
    print('Exported ' + Identifier + ': ' + str(Assets / 'lufi-shared.zip'), flush=True)

if __name__ == '__main__':
    Main()
