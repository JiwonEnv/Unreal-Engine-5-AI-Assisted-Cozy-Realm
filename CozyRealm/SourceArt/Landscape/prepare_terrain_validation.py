"""언리얼 에디터 Python: 원본 맵/재질을 복사해 지형 기능 검증 맵을 만든다."""
import unreal
import os
import struct
import zlib

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
FOLDER = '/Game/CozyRealm/Environment/GroundEditing'
SOURCE = '/Game/CozyRealm/Maps/L_Estate'
TARGET = '/Game/CozyRealm/Tests/Terrain/L_TerrainValidation'


def default_mask():
    path = os.path.join(unreal.Paths.project_saved_dir(), 'TerrainDefaultMask.png')
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)
    with open(path, 'wb') as fp:
        fp.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', 1, 1, 8, 6, 0, 0, 0))
                 + chunk(b'IDAT', zlib.compress(b'\x00\xff\x00\x00\x00')) + chunk(b'IEND', b''))
    task = unreal.AssetImportTask()
    task.set_editor_property('filename', path)
    task.set_editor_property('destination_path', FOLDER)
    task.set_editor_property('destination_name', 'T_DefaultPaintMask')
    task.set_editor_property('automated', True)
    task.set_editor_property('save', True)
    TOOLS.import_asset_tasks([task])
    texture = EAL.load_asset(FOLDER + '/T_DefaultPaintMask')
    texture.set_editor_property('srgb', False)
    texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    EAL.save_loaded_asset(texture)
    return texture


def node(mat, kind):
    return MEL.create_material_expression_in_function(mat, kind) if isinstance(mat, unreal.MaterialFunction) else MEL.create_material_expression(mat, kind)


def param(mat, kind, name, value):
    result = node(mat, kind)
    result.set_editor_property('parameter_name', name)
    result.set_editor_property('default_value', value)
    return result


def connect(source, output, target, pin):
    if not MEL.connect_material_expressions(source, output, target, pin):
        raise RuntimeError('재질 연결 실패: ' + pin)


def add_paint(mat):
    if not isinstance(mat, unreal.MaterialFunction):
        base = MEL.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR)
        output = MEL.get_material_property_input_node_output_name(mat, unreal.MaterialProperty.MP_BASE_COLOR)
        if not base:
            base = build_mesh_base(mat)
            output = ''
        call = node(mat, unreal.MaterialExpressionMaterialFunctionCall)
        call.set_material_function(EAL.load_asset(FOLDER + '/MF_CozyGroundPaint'))
        connect(base, output, call, 'BaseColor')
        MEL.connect_material_property(call, 'PaintedColor', unreal.MaterialProperty.MP_BASE_COLOR)
        MEL.layout_material_expressions(mat)
        MEL.recompile_material(mat)
        EAL.save_loaded_asset(mat)
        return
    base = node(mat, unreal.MaterialExpressionFunctionInput)
    base.set_editor_property('input_name', 'BaseColor')
    base.set_editor_property('input_type', unreal.FunctionInputType.FUNCTION_INPUT_VECTOR3)
    base_output = ''
    world = node(mat, unreal.MaterialExpressionWorldPosition)
    uv = node(mat, unreal.MaterialExpressionCustom)
    inputs = []
    for name in ['P', 'Origin', 'AxisU', 'AxisV', 'Bounds']:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    uv.set_editor_property('inputs', inputs)
    uv.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    # Bounds는 별도 RGBA 연결로 전달한다.
    uv.set_editor_property('code', 'float3 d=P-Origin; return (float2(dot(d,AxisU),dot(d,AxisV))-Bounds.xy)/max(Bounds.zw,float2(0.001,0.001));')
    connect(world, '', uv, 'P')
    for name, value in [('Origin', (0, 0, 0, 0)), ('AxisU', (1, 0, 0, 0)), ('AxisV', (0, 1, 0, 0))]:
        p = param(mat, unreal.MaterialExpressionVectorParameter, 'CozyPaint' + name, unreal.LinearColor(*value))
        connect(p, '', uv, name)
    bounds = param(mat, unreal.MaterialExpressionVectorParameter, 'CozyPaintBounds', unreal.LinearColor(-50, -50, 100, 100))
    rgba = node(mat, unreal.MaterialExpressionAppendVector)
    connect(bounds, 'RGB', rgba, 'A')
    connect(bounds, 'A', rgba, 'B')
    connect(rgba, '', uv, 'Bounds')
    mask = node(mat, unreal.MaterialExpressionTextureSampleParameter2D)
    mask.set_editor_property('parameter_name', 'CozyPaintMask')
    mask.set_editor_property('texture', EAL.load_asset(FOLDER + '/T_DefaultPaintMask'))
    mask.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    connect(uv, '', mask, 'UVs')
    acc = node(mat, unreal.MaterialExpressionMultiply)
    connect(base, base_output, acc, 'A')
    connect(mask, 'R', acc, 'B')
    for channel, name, color in [('G', 'Dirt', (.3, .18, .08, 1)), ('B', 'Stone', (.42, .43, .45, 1)), ('A', 'Sand', (.65, .5, .28, 1))]:
        tint = param(mat, unreal.MaterialExpressionVectorParameter, 'CozyPaint' + name, unreal.LinearColor(*color))
        weighted = node(mat, unreal.MaterialExpressionMultiply)
        connect(tint, '', weighted, 'A')
        connect(mask, channel, weighted, 'B')
        add = node(mat, unreal.MaterialExpressionAdd)
        connect(acc, '', add, 'A')
        connect(weighted, '', add, 'B')
        acc = add
    enabled = param(mat, unreal.MaterialExpressionScalarParameter, 'CozyPaintEnabled', 0.0)
    blend = node(mat, unreal.MaterialExpressionLinearInterpolate)
    connect(base, base_output, blend, 'A')
    connect(acc, '', blend, 'B')
    inside = node(mat, unreal.MaterialExpressionCustom)
    inside_inputs = []
    for name in ['UV', 'Enabled']:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inside_inputs.append(item)
    inside.set_editor_property('inputs', inside_inputs)
    inside.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    inside.set_editor_property('code', 'return Enabled * step(0,UV.x)*step(0,UV.y)*step(UV.x,1)*step(UV.y,1);')
    connect(uv, '', inside, 'UV'); connect(enabled, '', inside, 'Enabled')
    connect(inside, '', blend, 'Alpha')
    output = node(mat, unreal.MaterialExpressionFunctionOutput)
    output.set_editor_property('output_name', 'PaintedColor')
    connect(blend, '', output, '')
    MEL.update_material_function(mat)
    EAL.save_loaded_asset(mat)


def build_mesh_base(mat):
    color = param(mat, unreal.MaterialExpressionVectorParameter, 'GrassA_Color', unreal.LinearColor(.283, .462, .212, 1))
    tex = node(mat, unreal.MaterialExpressionTextureSampleParameter2D)
    tex.set_editor_property('parameter_name', 'GrassA_Texture')
    tex.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/WhiteSquareTexture'))
    world = node(mat, unreal.MaterialExpressionWorldPosition)
    xy = node(mat, unreal.MaterialExpressionComponentMask)
    xy.set_editor_property('r', True); xy.set_editor_property('g', True)
    origin = param(mat, unreal.MaterialExpressionVectorParameter, 'CozyTextureOrigin', unreal.LinearColor(-8064, -8064, 20, 0))
    relative = node(mat, unreal.MaterialExpressionSubtract)
    connect(world, '', relative, 'A'); connect(origin, '', relative, 'B')
    connect(relative, '', xy, '')
    tiling = param(mat, unreal.MaterialExpressionScalarParameter, 'TextureTiling', .25)
    scale = node(mat, unreal.MaterialExpressionDivide)
    scale.set_editor_property('const_b', 64.0)
    connect(tiling, '', scale, 'A')
    uv = node(mat, unreal.MaterialExpressionMultiply)
    connect(xy, '', uv, 'A'); connect(scale, '', uv, 'B'); connect(uv, '', tex, 'UVs')
    tint = node(mat, unreal.MaterialExpressionMultiply)
    connect(color, '', tint, 'A'); connect(tex, 'RGB', tint, 'B')
    snow = param(mat, unreal.MaterialExpressionScalarParameter, 'SnowAmount', 0.0)
    snow_color = param(mat, unreal.MaterialExpressionVectorParameter, 'SnowColor', unreal.LinearColor(.9, .93, .96, 1))
    blend = node(mat, unreal.MaterialExpressionLinearInterpolate)
    connect(tint, '', blend, 'A'); connect(snow_color, '', blend, 'B'); connect(snow, '', blend, 'Alpha')
    rough = param(mat, unreal.MaterialExpressionScalarParameter, 'GrassA_Roughness', .9)
    MEL.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    return blend


def main():
    EAL.make_directory(FOLDER)
    EAL.make_directory('/Game/CozyRealm/Tests/Terrain')
    if not EAL.does_asset_exist(FOLDER + '/T_DefaultPaintMask'):
        default_mask()
    if not EAL.does_asset_exist(FOLDER + '/MF_CozyGroundPaint'):
        function = TOOLS.create_asset('MF_CozyGroundPaint', FOLDER, unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
        add_paint(function)
    landscape_master = EAL.load_asset(FOLDER + '/M_LandscapePaint')
    if not landscape_master:
        landscape_master = EAL.duplicate_asset('/Game/CozyRealm/Environment/Landscape/M_CozyLandscape', FOLDER + '/M_LandscapePaint')
        add_paint(landscape_master)
    mesh_material = EAL.load_asset(FOLDER + '/M_GroundPaint')
    if not mesh_material:
        mesh_material = TOOLS.create_asset('M_GroundPaint', FOLDER, unreal.Material, unreal.MaterialFactoryNew())
        add_paint(mesh_material)
    if not EAL.does_asset_exist(TARGET):
        EAL.duplicate_asset(SOURCE, TARGET)
    unreal.EditorLoadingAndSavingUtils.load_map(TARGET)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    terrain_cls = unreal.load_class(None, '/Script/CozyRealm.CozyEstateTerrain')
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.LandscapeProxy):
            actor.set_editor_property('use_dynamic_material_instance', True)
            mi = EAL.load_asset(FOLDER + '/MI_LandscapePaint') or EAL.duplicate_asset('/Game/CozyRealm/Environment/Landscape/MI_CozyLandscape_Summer', FOLDER + '/MI_LandscapePaint')
            mi.set_editor_property('parent', landscape_master)
            MEL.update_material_instance(mi)
            EAL.save_loaded_asset(mi)
            actor.set_editor_property('landscape_material', mi)
        if actor.get_class() == terrain_cls:
            presets = []
            current_name = actor.get_editor_property('current_season').get_name() if actor.get_editor_property('current_season') else 'DA_Season_Summer'
            current = None
            for season in ['Spring', 'Summer', 'Autumn', 'Winter']:
                season_mi = EAL.load_asset(FOLDER + '/MI_Paint_' + season) or EAL.duplicate_asset('/Game/CozyRealm/Environment/Landscape/MI_CozyLandscape_' + season, FOLDER + '/MI_Paint_' + season)
                season_mi.set_editor_property('parent', landscape_master)
                MEL.update_material_instance(season_mi); EAL.save_loaded_asset(season_mi)
                preset = EAL.load_asset(FOLDER + '/DA_PaintSeason_' + season) or EAL.duplicate_asset('/Game/CozyRealm/Environment/Landscape/DA_Season_' + season, FOLDER + '/DA_PaintSeason_' + season)
                preset.set_editor_property('landscape_material', season_mi); EAL.save_loaded_asset(preset)
                presets.append(preset)
                if current_name.endswith(season): current = preset
            actor.set_editor_property('seasons', presets)
            actor.set_editor_property('current_season', current or presets[1])
    ground_cls = unreal.load_class(None, '/Script/CozyRealm.CozyGroundMesh')
    cube = unreal.load_asset('/Engine/BasicShapes/Cube')
    for label, position, scale, rotation, number in [
        ('검증_언덕_평평한상판', (600, 0, 220), (8, 8, 4), (0, 0, 0), 1),
        ('검증_고정경사로', (-100, 0, 210), (8, 4, .2), (30, 0, 0), 2),
        ('검증_위아래표면_고립상판', (-800, 0, 500), (4, 4, .2), (0, 0, 0), 3),
    ]:
        actor = next((a for a in actors.get_all_level_actors() if a.get_actor_label() == label), None)
        # Rotator의 Python 위치 인자 순서에 의존하지 않고 축 이름을 명시한다.
        angles = unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2])
        if not actor:
            actor = actors.spawn_actor_from_class(ground_cls, unreal.Vector(*position), angles)
        actor.modify()
        actor.set_actor_rotation(angles, False)
        unreal.log(label + ' 회전 확인: ' + str(actor.get_actor_rotation()))
        actor.set_actor_label(label)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        mesh = actor.static_mesh_component
        mesh.set_static_mesh(cube)
        mesh.set_material(0, mesh_material)
        surface = actor.get_component_by_class(unreal.load_class(None, '/Script/CozyRealm.CozySurfaceComponent'))
        surface.set_editor_property('surface_id', unreal.GuidLibrary.parse_string_to_guid('10000000-0000-0000-0000-%012d' % number)[0])
        surface.set_editor_property('paint_domain_id', unreal.GuidLibrary.parse_string_to_guid('20000000-0000-0000-0000-%012d' % number)[0])
        surface.set_editor_property('paint_min', unreal.Vector2D(-50, -50))
        surface.set_editor_property('paint_max', unreal.Vector2D(50, 50))
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    assert unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET), '검증 맵 저장 실패'
    unreal.log('검증 맵 생성: ' + TARGET)


if __name__ == '__main__':
    main()
