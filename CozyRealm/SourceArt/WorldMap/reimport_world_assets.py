"""산 월드 전용 임포트 사본만 갱신한다. 시설 원본 Blender와 영지 에셋은 보존한다."""
from pathlib import Path
import unreal
import json
source=Path(unreal.Paths.project_dir())/'SourceArt/WorldMap'
scope={'__name__':'world_import_helpers'}
script=source/'build_mountain_world.py'
exec(compile(script.read_text(encoding='utf-8'),str(script),'exec'),scope)
meshes=scope['imports'](force=True)
report={}
for name,mesh in meshes.items():
    extent=mesh.get_bounds().box_extent
    report[name]={'dimensions_cm':[extent.x*2,extent.y*2,extent.z*2]}
(source/'import_bounds_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('MOUNTAIN_WORLD_REIMPORT_OK')
