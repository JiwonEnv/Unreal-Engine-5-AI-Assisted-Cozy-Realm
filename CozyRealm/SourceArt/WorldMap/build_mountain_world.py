"""UE 에디터 Python: 새 산 월드와 검토 배치를 만든다. 기존 영지 맵은 변경하지 않는다."""
import unreal
import json
import math
import random
from pathlib import Path

PROJECT=Path(unreal.Paths.project_dir())
SOURCE=PROJECT/'SourceArt/WorldMap'
TARGET='/Game/CozyRealm/Maps/L_MountainWorld'
FOLDER='/Game/CozyRealm/Environment/MountainWorld'
EAL=unreal.EditorAssetLibrary
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
ACTORS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LEVEL=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
MEL=unreal.MaterialEditingLibrary

def material(name,color):
    path=FOLDER+'/Materials/'+name
    m=EAL.load_asset(path) if EAL.does_asset_exist(path) else None
    if m:
        MEL.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        MEL.recompile_material(m); EAL.save_loaded_asset(m)
        return m
    m=TOOLS.create_asset(name,FOLDER+'/Materials',unreal.Material,unreal.MaterialFactoryNew())
    c=MEL.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
    c.set_editor_property('constant',unreal.LinearColor(*color[:3],1))
    MEL.connect_material_property(c,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=MEL.create_material_expression(m,unreal.MaterialExpressionConstant)
    rough.set_editor_property('r',.87)
    MEL.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.set_material_usage(m,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    MEL.recompile_material(m); EAL.save_loaded_asset(m)
    return m

def imports(force=False):
    report=json.loads((SOURCE/'export_report.json').read_text(encoding='utf-8'))
    meshes={}
    for info in report['assets']:
        name=info['name']; path=FOLDER+'/Meshes/'+name
        mesh=EAL.load_asset(path) if EAL.does_asset_exist(path) else None
        if not mesh or force:
            task=unreal.AssetImportTask()
            task.set_editor_property('filename',str(SOURCE/'FBX'/(name+'.fbx')))
            task.set_editor_property('destination_path',FOLDER+'/Meshes')
            task.set_editor_property('destination_name',name)
            task.set_editor_property('automated',True)
            task.set_editor_property('save',True)
            task.set_editor_property('replace_existing',True)
            task.set_editor_property('factory',unreal.FbxFactory())
            options=unreal.FbxImportUI()
            options.set_editor_property('import_mesh',True)
            options.set_editor_property('import_materials',False)
            options.set_editor_property('import_textures',False)
            options.set_editor_property('import_as_skeletal',False)
            options.set_editor_property('mesh_type_to_import',unreal.FBXImportType.FBXIT_STATIC_MESH)
            options.static_mesh_import_data.set_editor_property('combine_meshes',True)
            options.static_mesh_import_data.set_editor_property('auto_generate_collision',True)
            task.set_editor_property('options',options)
            TOOLS.import_asset_tasks([task])
            mesh=EAL.load_asset(path)
        if not mesh: raise RuntimeError('시설 임포트 실패: '+name)
        bounds=mesh.get_bounds().box_extent
        actual=sorted([bounds.x*2,bounds.y*2])+[bounds.z*2]
        expected=sorted([v*100 for v in info['dimensions_m'][:2]])+[info['dimensions_m'][2]*100]
        if any(abs(a-b)>max(2,b*.025) for a,b in zip(actual,expected)):
            raise RuntimeError('시설 단위·치수 불일치: '+name+' / UE cm '+str(actual)+' / Blender cm '+str(expected))
        entries={e['name']:e for e in info['materials']}
        for index,slot in enumerate(mesh.get_editor_property('static_materials')):
            slot_name=str(slot.get_editor_property('imported_material_slot_name'))
            entry=entries.get(slot_name)
            if not entry:
                raise RuntimeError('FBX 재질 칸을 원본 색과 연결하지 못함: '+name+' / '+slot_name)
            mesh.set_material(index,material('M_'+entry['name'],entry['color']))
        EAL.save_loaded_asset(mesh)
        meshes[name]=mesh
    return meshes

def spawn(cls,label,pos=(0,0,0),rot=(0,0,0),folder=''):
    a=ACTORS.spawn_actor_from_class(cls,unreal.Vector(*pos),unreal.Rotator(*rot))
    a.set_actor_label(label)
    if folder: a.set_folder_path(folder)
    return a

def main():
    if EAL.does_asset_exist(TARGET) and (SOURCE/'world_build_report.json').exists():
        raise RuntimeError('기존 산 월드가 있으므로 자동 덮어쓰기를 중단합니다: '+TARGET)
    meshes=imports()
    if EAL.does_asset_exist(TARGET):
        if not LEVEL.load_level(TARGET): raise RuntimeError('생성 도중 중단된 빈 맵 로드 실패')
        if any(a.get_class().get_name()=='CozyMountainWorld' for a in ACTORS.get_all_level_actors()):
            raise RuntimeError('내용이 있는 산 월드를 발견해 보존합니다. 부분 생성 결과를 먼저 확인하세요.')
        unreal.log('앞선 생성에서 남은 빈 산 월드에 이어 제작합니다.')
    elif not LEVEL.new_level(TARGET): raise RuntimeError('새 레벨 생성 실패')
    world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property('default_game_mode',unreal.load_class(None,'/Script/CozyRealm.CozyWorldExplorerMode'))
    terrain=spawn(unreal.load_class(None,'/Script/CozyRealm.CozyMountainWorld'),'산월드_구역과동선',folder='00_지형')
    areas=list(terrain.get_editor_property('clearings'))
    for area in areas: area.set_editor_property('falloff',1600.0)
    terrain.set_editor_property('clearings',areas)
    # 원본의 계절 데이터·재질은 공유하되 원본을 수정하지 않는다.
    seasons=[EAL.load_asset('/Game/CozyRealm/Environment/GroundEditing/DA_PaintSeason_'+s) for s in ['Spring','Summer','Autumn','Winter']]
    if not all(seasons): raise RuntimeError('기존 계절·페인팅 재질이 필요합니다')
    terrain.set_editor_property('seasons',seasons)
    terrain.set_editor_property('current_season',seasons[1])
    terrain.call_method('GenerateTerrain')
    landscape=terrain.get_editor_property('landscape')
    landscape.set_actor_label('Mountain_Landscape_504m')
    landscape.set_folder_path('00_지형')
    # 자연물은 HISM으로 묶고 실제 걸어 다니는 길과 평지에는 배치하지 않는다.
    sets=[('broad_trees','SM_World_BroadTree'),('pines','SM_World_Pine'),('boulders','SM_World_Boulder'),('cliffs','SM_World_Cliff')]
    comps={prop:terrain.get_editor_property(prop) for prop,_ in sets}
    for prop,name in sets: comps[prop].set_static_mesh(meshes[name])
    clearings=list(terrain.get_editor_property('clearings'))
    def height(x,y): return terrain.get_layout_height(x,y)+20
    def clearing_distance(x,y):
        return min(math.hypot((x-c.center.x)/c.radius.x,(y-c.center.y)/c.radius.y) for c in clearings)
    random.seed(23)
    counts={k:0 for k in comps}
    for _ in range(1700):
        x=random.uniform(-23200,23200); y=random.uniform(-22000,23000)
        if terrain.get_path_distance(x,y)<850 or clearing_distance(x,y)<1.15: continue
        z=height(x,y)
        slope=math.hypot(height(x+100,y)-height(x-100,y),height(x,y+100)-height(x,y-100))/200
        if slope>.85: continue
        key='pines' if y>7000 else 'broad_trees'
        if random.random()<.18: key='boulders'
        scale=random.uniform(.8,1.7)
        transform=unreal.Transform(unreal.Vector(x,y,z-12),unreal.Rotator(0,random.uniform(0,360),0),unreal.Vector(scale,scale,scale))
        comps[key].add_instance(transform,world_space=True); counts[key]+=1
    # 절벽 조각은 급한 경사에만 묻어 배치한다. 보행 길은 Landscape가 담당한다.
    for _ in range(900):
        x=random.uniform(-21000,21000); y=random.uniform(-12000,21000)
        if terrain.get_path_distance(x,y)<1300 or clearing_distance(x,y)<1.15: continue
        dx=(height(x+150,y)-height(x-150,y))/300; dy=(height(x,y+150)-height(x,y-150))/300
        slope=math.hypot(dx,dy)
        if slope<.65: continue
        scale=random.uniform(1.8,3.8)
        transform=unreal.Transform(unreal.Vector(x,y,height(x,y)-scale*130),unreal.Rotator(0,math.degrees(math.atan2(dy,dx))+90,0),unreal.Vector(scale,scale*.7,scale))
        comps['cliffs'].add_instance(transform,world_space=True); counts['cliffs']+=1
    placements=[
        ('B01','시작_신사',(-1250,1100),0),('B04','시작_나가야',(1200,1250),0),
        ('B06','시작_밭관리',(-1250,-1100),0),('B07','시작_밭',(1000,-1100),0),
        ('B02','서쪽후보_제분소',(-11600,7100),0),('B07','서쪽후보_밭',(-9300,5700),0),
        ('B09','동쪽후보_창고',(11500,5300),0),('B03','동쪽후보_판매소',(9700,3500),0),
        ('B01','능선_전망신사',(2200,15800),0),('B13','산기슭_차밭',(3500,-13000),0)]
    for code,label,(x,y),yaw in placements:
        a=spawn(unreal.StaticMeshActor,label,(x,y,height(x,y)),(0,yaw,0),'20_시설_검토배치')
        a.static_mesh_component.set_static_mesh(meshes['SM_World_'+code])
        # 시설 검토 배치: 단순 충돌. 실제 생산시설 액터와 자동 연결하지 않는다.
        a.static_mesh_component.set_collision_profile_name('BlockAll')
    sun=spawn(unreal.DirectionalLight,'햇빛',(0,0,15000),(-38,-35,0),'30_조명')
    sun.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sun.light_component.set_editor_property('intensity',4.0)
    sun.light_component.set_editor_property('atmosphere_sun_light',True)
    spawn(unreal.SkyAtmosphere,'하늘',folder='30_조명')
    sky=spawn(unreal.SkyLight,'환경광',(0,0,1000),folder='30_조명')
    sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property('real_time_capture',True)
    fog=spawn(unreal.ExponentialHeightFog,'원경안개',(0,0,-2000),folder='30_조명')
    fog.get_component_by_class(unreal.ExponentialHeightFogComponent).set_editor_property('fog_density',.004)
    spawn(unreal.PlayerStart,'시작점',(0,0,130),folder='90_검토')
    pp=spawn(unreal.PostProcessVolume,'고정노출',folder='30_조명')
    pp.set_editor_property('unbound',True)
    settings=pp.get_editor_property('settings')
    settings.set_editor_property('override_auto_exposure_method',True)
    settings.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
    settings.set_editor_property('override_auto_exposure_bias',True)
    settings.set_editor_property('auto_exposure_bias',1.0)
    settings.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True)
    settings.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
    pp.set_editor_property('settings',settings)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,TARGET),'월드 저장 실패'
    EAL.save_directory(FOLDER)
    report={'map':TARGET,'extent_m':504,'landscape_components':64,'instances':counts,
        'clearings':[{'name':c.name,'center_cm':[c.center.x,c.center.y,c.center.z],'radius_cm':[c.radius.x,c.radius.y]} for c in clearings],
        'facilities':len(placements),'status':'맵 생성 완료. 빌드·실행·보행·영상 검증 별도 필요'}
    (SOURCE/'world_build_report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    unreal.log('MOUNTAIN_WORLD_BUILD_OK '+json.dumps(report,ensure_ascii=False))

if __name__=='__main__': main()
