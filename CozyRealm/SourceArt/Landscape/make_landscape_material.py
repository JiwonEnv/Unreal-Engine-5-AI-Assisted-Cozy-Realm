# 영지 지형 머티리얼 만들기 · 갱신 (언리얼 에디터 안에서 실행)
#
# 하는 일
#   1) 기본 틀 M_CozyLandscape 를 LandscapeLayers.csv 표대로 다시 짠다 (칸마다 색 · 그림 · 거칠기 값 + 눈 덮임)
#   2) 계절별 인스턴스 MI_CozyLandscape_Spring/Summer/Autumn/Winter 를 만든다
#      - 새로 생긴 칸만 표의 계절 색으로 채우고, 이미 손으로 바꾼 값은 그대로 둔다
#   3) 계절 에셋 DA_Season_* 을 만든다 (영지 지형 액터의 계절 목록에 넣는 것)
#
# 실행: 언리얼 아래쪽 명령 입력란(Cmd)에
#   py E:/Project/UE5_CozyRealm/CozyRealm/SourceArt/Landscape/make_landscape_material.py
#
# 칸을 늘리려면: LandscapeLayers.csv 에 줄을 하나 더하고 → 이 파일을 다시 실행 → 영지 지형 액터의 '재질 칸 갱신'
import csv
import os
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
TABLE = os.path.join(HERE, 'LandscapeLayers.csv')
FOLDER = '/Game/CozyRealm/Environment/Landscape'
MASTER_NAME = 'M_CozyLandscape'
WHITE = '/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture'
SEASONS = [('Spring', '봄'), ('Summer', '여름'), ('Autumn', '가을'), ('Winter', '겨울')]
SNOW = {'Spring': 0.0, 'Summer': 0.0, 'Autumn': 0.0, 'Winter': 0.25}

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log('[지형 머티리얼] ' + msg)


def linear(hex_color):
    def f(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    h = hex_color.strip().lstrip('#')
    return unreal.LinearColor(f(int(h[0:2], 16)), f(int(h[2:4], 16)), f(int(h[4:6], 16)), 1.0)


def read_layers():
    with open(TABLE, encoding='utf-8-sig') as fp:
        rows = [r for r in csv.DictReader(fp) if r.get('Name') and not r['Name'].startswith('#')]
    log('표의 재질 칸 %d개: %s' % (len(rows), ', '.join(r['Name'] for r in rows)))
    return rows


def load_or_create(path, name, asset_class, factory):
    full = '%s/%s' % (path, name)
    if EAL.does_asset_exist(full):
        return EAL.load_asset(full)
    return TOOLS.create_asset(name, path, asset_class, factory)


def param(mat, cls, name, group, x, y):
    node = MEL.create_material_expression(mat, cls, x, y)
    node.set_editor_property('parameter_name', name)
    node.set_editor_property('group', group)
    return node


def build_master(rows):
    mat = load_or_create(FOLDER, MASTER_NAME, unreal.Material, unreal.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)

    def blend_node(x, y):
        node = MEL.create_material_expression(mat, unreal.MaterialExpressionLandscapeLayerBlend, x, y)
        inputs = []
        for index, row in enumerate(rows):
            item = unreal.LayerBlendInput()
            item.set_editor_property('layer_name', row['Name'])
            item.set_editor_property('blend_type', unreal.LandscapeLayerBlendType.LB_WEIGHT_BLEND)
            item.set_editor_property('preview_weight', 1.0 if index == 0 else 0.0)
            inputs.append(item)
        node.set_editor_property('layers', inputs)
        return node

    color_blend = blend_node(-500, -200)
    rough_blend = blend_node(-500, 400)

    # 그림 좌표 (모든 칸 공통 · Texture Tiling 값으로 촘촘함 조절)
    coords = MEL.create_material_expression(mat, unreal.MaterialExpressionLandscapeLayerCoords, -1700, -400)
    tiling = param(mat, unreal.MaterialExpressionScalarParameter, 'TextureTiling', '0 공통', -1700, -300)
    tiling.set_editor_property('default_value', 0.25)
    uv = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -1500, -350)
    MEL.connect_material_expressions(coords, '', uv, 'A')
    MEL.connect_material_expressions(tiling, '', uv, 'B')

    white = unreal.load_asset(WHITE)
    for index, row in enumerate(rows):
        name, group = row['Name'], '%d %s' % (index + 1, row.get('Label') or row['Name'])
        y = -800 + index * 260
        color = param(mat, unreal.MaterialExpressionVectorParameter, name + '_Color', group, -1300, y)
        color.set_editor_property('default_value', linear(row.get('Summer') or 'FFFFFF'))
        tex = param(mat, unreal.MaterialExpressionTextureSampleParameter2D, name + '_Texture', group, -1300, y + 90)
        tex.set_editor_property('texture', white)
        MEL.connect_material_expressions(uv, '', tex, 'UVs')
        tint = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -900, y)
        MEL.connect_material_expressions(color, '', tint, 'A')
        MEL.connect_material_expressions(tex, 'RGB', tint, 'B')
        MEL.connect_material_expressions(tint, '', color_blend, 'Layer ' + name)
        rough = param(mat, unreal.MaterialExpressionScalarParameter, name + '_Roughness', group, -900, y + 120)
        rough.set_editor_property('default_value', float(row.get('Roughness') or 0.9))
        MEL.connect_material_expressions(rough, '', rough_blend, 'Layer ' + name)

    # 눈 덮임: 0이면 칸 색 그대로 · 1이면 전부 눈 색 (계절 인스턴스에서 바꿈 · 게임 중에도 바꿀 수 있음)
    snow_amount = param(mat, unreal.MaterialExpressionScalarParameter, 'SnowAmount', '0 공통', -500, -600)
    snow_amount.set_editor_property('default_value', 0.0)
    snow_color = param(mat, unreal.MaterialExpressionVectorParameter, 'SnowColor', '0 공통', -500, -500)
    snow_color.set_editor_property('default_value', linear('F4F7FA'))
    lerp = MEL.create_material_expression(mat, unreal.MaterialExpressionLinearInterpolate, -200, -200)
    MEL.connect_material_expressions(color_blend, '', lerp, 'A')
    MEL.connect_material_expressions(snow_color, '', lerp, 'B')
    MEL.connect_material_expressions(snow_amount, '', lerp, 'Alpha')

    MEL.connect_material_property(lerp, '', unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(rough_blend, '', unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    log('기본 틀 다시 짬: %s (재질 칸 %d개)' % (MASTER_NAME, len(rows)))
    return mat


def overridden_names(mi):
    names = set()
    for key in ('scalar_parameter_values', 'vector_parameter_values', 'texture_parameter_values'):
        for value in mi.get_editor_property(key):
            names.add(str(value.get_editor_property('parameter_info').get_editor_property('name')))
    return names


def build_seasons(master, rows):
    made = []
    for season, label in SEASONS:
        mi = load_or_create(FOLDER, 'MI_CozyLandscape_' + season, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property('parent', master)
        have = overridden_names(mi)
        added = 0
        white = unreal.load_asset(WHITE)
        for row in rows:
            name = row['Name']
            if name + '_Color' not in have:
                MEL.set_material_instance_vector_parameter_value(mi, name + '_Color', linear(row.get(season) or row.get('Summer') or 'FFFFFF'))
                added += 1
            if name + '_Roughness' not in have:
                MEL.set_material_instance_scalar_parameter_value(mi, name + '_Roughness', float(row.get('Roughness') or 0.9))
            if name + '_Texture' not in have:
                MEL.set_material_instance_texture_parameter_value(mi, name + '_Texture', white)
        for name, value in (('SnowAmount', SNOW.get(season, 0.0)), ('TextureTiling', 0.25)):
            if name not in have:
                MEL.set_material_instance_scalar_parameter_value(mi, name, value)
        if 'SnowColor' not in have:
            MEL.set_material_instance_vector_parameter_value(mi, 'SnowColor', linear('F4F7FA'))
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
        log('%s (%s): 새 칸 %d개 채움 · 이미 바꾼 값은 그대로' % (mi.get_name(), label, added))
        made.append((season, label, mi))
    return made


def build_presets(seasons):
    preset_class = unreal.load_class(None, '/Script/CozyRealm.CozySeasonPreset')
    if not preset_class:
        log('CozySeasonPreset 클래스가 없음 (빌드 전) · 계절 에셋은 건너뜀')
        return
    for season, label, mi in seasons:
        name = 'DA_Season_' + season
        path = '%s/%s' % (FOLDER, name)
        if EAL.does_asset_exist(path):
            asset = EAL.load_asset(path)
        else:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property('data_asset_class', preset_class)
            asset = TOOLS.create_asset(name, FOLDER, preset_class, factory)
        asset.set_editor_property('display_name', label)
        asset.set_editor_property('landscape_material', mi)
        EAL.save_loaded_asset(asset)
        log('계절 에셋: %s (%s)' % (name, label))


rows = read_layers()
master = build_master(rows)
seasons = build_seasons(master, rows)
build_presets(seasons)
log('끝')
