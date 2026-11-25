from pathlib import Path
import os
import json

Root = Path(__file__).resolve().parent
Repository = Root.parents[1]
Assets = Path(os.environ.get('LUFI_ANIMATION_ROOT', Repository / 'animation')).resolve()
ClipId = os.environ.get('LUFI_CLIP_ID', 'export')
Work = Path(os.environ.get('LUFI_WORK_DIR', Assets / 'local' / ClipId)).resolve()
Work.mkdir(parents=True, exist_ok=True)
Profile = json.loads((Root / 'lufi-profile.json').read_text())
CameraName = os.environ.get('LUFI_CAMERA', Profile['camera'])
Start = int(os.environ.get('LUFI_FRAME_START', '1'))
End = int(os.environ.get('LUFI_FRAME_END', str(Start)))
FrameCount = End - Start + 1
