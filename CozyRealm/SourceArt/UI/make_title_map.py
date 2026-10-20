"""언리얼 에디터 Python: 타이틀 맵 L_Title을 만든다 (Claude 2026-10-21).

빈 맵 + World Settings의 게임 모드를 CozyTitleGameMode로 지정한다.
배경은 이 맵에 넣지 않고, 실행할 때 프로젝트 설정 'Cozy Title > Background Level'(기본: 실제 영지 맵 L_Estate)의 풍경을 불러온다.
이미 있으면 게임 모드만 다시 지정해 저장한다.
"""
import unreal

TARGET = '/Game/CozyRealm/Maps/L_Title'


def main():
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        unreal.EditorLoadingAndSavingUtils.load_map(TARGET)
    else:
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level(TARGET)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    mode = unreal.load_class(None, '/Script/CozyRealm.CozyTitleGameMode')
    if not mode:
        raise RuntimeError('CozyTitleGameMode 클래스를 찾지 못함 (먼저 C++ 빌드)')
    settings = world.get_world_settings()
    settings.set_editor_property('default_game_mode', mode)
    unreal.log('타이틀 맵 게임 모드: ' + str(settings.get_editor_property('default_game_mode')))
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET), '타이틀 맵 저장 실패'
    unreal.log('타이틀 맵 저장 완료: ' + TARGET)


if __name__ == '__main__':
    main()
