"""신규 월드만 백업 후 저광량 검토 조명에 맞는 수동 노출로 보정한다."""
import unreal
from datetime import datetime

target='/Game/CozyRealm/Maps/L_MountainWorld'
backup='/Game/CozyRealm/Tests/MountainWorld/L_BeforeExposure_'+datetime.now().strftime('%Y%m%d_%H%M%S')
assert unreal.EditorAssetLibrary.duplicate_asset(target,backup)
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level(target)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for actor in actors:
    if isinstance(actor,unreal.PostProcessVolume):
        settings=actor.get_editor_property('settings')
        settings.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True)
        settings.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
        actor.set_editor_property('settings',settings)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert unreal.EditorLoadingAndSavingUtils.save_map(world,target)
unreal.log('MOUNTAIN_EXPOSURE_FIXED '+backup)
