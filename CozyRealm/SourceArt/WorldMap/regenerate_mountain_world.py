"""기존 산 월드를 사본으로 보존한 뒤 높이 계산 수정을 반영한다. 수동 조형을 덮어쓰는 개발 도구."""
import unreal
import json
from pathlib import Path
from datetime import datetime

target='/Game/CozyRealm/Maps/L_MountainWorld'
eal=unreal.EditorAssetLibrary
level=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
backup='/Game/CozyRealm/Tests/MountainWorld/L_BeforeRegeneration_'+datetime.now().strftime('%Y%m%d_%H%M%S')
eal.make_directory('/Game/CozyRealm/Tests/MountainWorld')
assert eal.duplicate_asset(target,backup),'기존 산 월드 사본 보존 실패'
assert level.load_level(target),'산 월드 로드 실패'
layouts=[a for a in actors.get_all_level_actors() if a.get_class().get_name()=='CozyMountainWorld']
assert len(layouts)==1,'레이아웃 개수 오류'
layouts[0].set_editor_property('terrain_revision',layouts[0].get_editor_property('terrain_revision')+1)
if '-GentleWorldRoutes' in unreal.SystemLibrary.get_command_line():
    areas=list(layouts[0].get_editor_property('clearings'))
    for area in areas: area.set_editor_property('falloff',1600.0)
    layouts[0].set_editor_property('clearings',areas)
    defaults=unreal.get_default_object(layouts[0].get_class())
    layouts[0].set_editor_property('routes',defaults.get_editor_property('routes'))
layouts[0].call_method('GenerateTerrain')
landscape=layouts[0].get_editor_property('landscape')
landscape.set_actor_label('Mountain_Landscape_504m')
landscape.set_folder_path('00_지형')
source=Path(unreal.Paths.project_dir())/'SourceArt/WorldMap'
script=source/'refresh_world_scenery.py'
exec(compile(script.read_text(encoding='utf-8'),str(script),'exec'),{'__name__':'__main__'})
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert unreal.EditorLoadingAndSavingUtils.save_map(world,target),'산 월드 재저장 실패'
report=source/'world_build_report.json'
data=json.loads(report.read_text(encoding='utf-8'))
data['regeneration_backup']=backup
data['terrain_revision']=layouts[0].get_editor_property('terrain_revision')
data['status']='길 꺾임 높이 계산 수정 반영. 충돌 재검사·화면 검증 필요'
report.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
unreal.log('MOUNTAIN_WORLD_REGENERATED '+backup)
