"""검증된 공통 도색 함수를 기존 지형의 출력에 연결한다. 기존 레이어 그래프는 보존한다."""
import os
import runpy
import unreal

library = runpy.run_path(os.path.join(os.path.dirname(__file__), 'prepare_terrain_validation.py'), run_name='ground_paint_library')
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
path = '/Game/CozyRealm/Environment/Landscape/M_CozyLandscape'
backup = '/Game/CozyRealm/Tests/Terrain/M_CozyLandscape_BeforeRuntimePaint'
material = eal.load_asset(path)
assert material, '기존 지형 머티리얼이 없습니다'
output = mel.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
if isinstance(output, unreal.MaterialExpressionMaterialFunctionCall) and output.get_editor_property('material_function').get_name() == 'MF_CozyGroundPaint':
    unreal.log('공통 도색 함수가 이미 연결되어 있습니다')
else:
    if not eal.does_asset_exist(backup):
        assert eal.duplicate_asset(path, backup), '원본 머티리얼 사본 보관 실패'
    library['add_paint'](material)
    unreal.log('기존 레이어를 유지한 런타임 도색 연결 완료')
