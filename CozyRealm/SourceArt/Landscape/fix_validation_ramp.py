"""언리얼 에디터 Python: 검증 맵의 경사로 위치만 고쳐 저장한다. (Claude 후속 2026-10-20)

경사로(길이 800cm · 30도 · 두께 20cm)의 윗면이 언덕 상판(Z 420)보다 약 25cm 낮게 끝나
주인공이 이음매에서 한 번 턱을 오르며 떠 보였다. 윗면 끝은 블록 끝보다 5cm 안쪽·8.66cm 위다.
딱 맞대면 비스듬한 끝면(5cm 폭, 법선 Z 0.5)이 드러나 길찾기·브러시가 그 면에 걸리므로
윗면 끝을 언덕 안쪽 X 203·Z 420에 두어 끝면을 언덕 속에 숨긴다 → 중심 (-138.4, 0, 211.34).
언덕 가장자리 X 200에서 경사로 윗면은 Z 418.3 (턱 약 1.7cm), 아래쪽 끝은 X -489.8에서 Z 20.
"""
import unreal

TARGET = '/Game/CozyRealm/Tests/Terrain/L_TerrainValidation'
LABEL = '검증_고정경사로'
LOCATION = unreal.Vector(-138.4, 0, 211.34)


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(TARGET)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ramp = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == LABEL), None)
    if not ramp:
        raise RuntimeError('경사로를 찾지 못함: ' + LABEL)
    before = ramp.get_actor_location()
    ramp.modify()
    ramp.set_actor_location(LOCATION, False, False)
    unreal.log('경사로 위치: {} -> {}'.format(before, ramp.get_actor_location()))
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET), '검증 맵 저장 실패'
    unreal.log('검증 맵 경사로 위치 수정 저장 완료')


if __name__ == '__main__':
    main()
