"""원본 Blender 파일을 저장하지 않고, 월드 검토용 시설·자연물을 FBX로 내보낸다."""
import bpy
import json
import math
import random
from pathlib import Path
from mathutils import Vector
from mathutils import Matrix

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'Art/BlenderModules/V07_시설전체_제분소기준/CozyRealm_V07_시설13종_공용모듈_140cm.blend'
OUT = ROOT / 'CozyRealm/SourceArt/WorldMap/FBX'
OUT.mkdir(parents=True, exist_ok=True)
REPORT = []
bpy.ops.wm.open_mainfile(filepath=str(SOURCE))
COLS = {c.get('관리_ID'): c for c in bpy.data.collections if c.get('관리_ID')}

def export_selected(name, objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.convert(target='MESH')
    bpy.ops.object.join()
    obj = bpy.context.object
    bpy.ops.object.material_slot_remove_unused()
    bpy.context.scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    obj.name = name
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    bpy.ops.export_scene.fbx(filepath=str(OUT / (name + '.fbx')), use_selection=True,
        object_types={'MESH'}, axis_forward='-Y', axis_up='Z', global_scale=1.0,
        apply_unit_scale=True, use_mesh_modifiers=True, mesh_smooth_type='FACE',
        bake_anim=False, add_leaf_bones=False)
    REPORT.append({'name': name, 'vertices': len(obj.data.vertices), 'polygons': len(obj.data.polygons),
        'dimensions_m': list(obj.dimensions), 'materials': [{'name': m.name, 'color': list(m.diffuse_color)} for m in obj.data.materials if m]})

for code in ['B01', 'B02', 'B03', 'B04', 'B06', 'B07', 'B09', 'B13']:
    col = COLS[code]
    source_objects = list(col.all_objects)
    source_scene=next(s for s in bpy.data.scenes if any(o.name in s.objects for o in source_objects if o.type=='EMPTY'))
    bpy.context.window.scene=source_scene
    bpy.context.view_layer.update()
    roots = [o for o in source_objects if o.type == 'EMPTY' and '조립원점' in o.name]
    root = roots[0] if roots else next(o for o in source_objects if o.type == 'EMPTY')
    origin = root.matrix_world.translation.copy()
    # 원본 장면에서 평가한 월드 좌표를 메시 정점에 굽는다.
    # 빈 내보내기 장면으로 먼저 옮기면 부모/의존 그래프의 변환이 누락될 수 있다.
    depsgraph=bpy.context.evaluated_depsgraph_get()
    snapshots=[]
    for old in source_objects:
        if old.type != 'MESH' or '기준인체' in old.data.name or '비교용' in old.name or old.name.startswith('주인공_높이140cm'):
            continue
        evaluated=old.evaluated_get(depsgraph)
        data=bpy.data.meshes.new_from_object(evaluated,depsgraph=depsgraph)
        data.transform(Matrix.Translation(-origin) @ evaluated.matrix_world)
        snapshots.append((old.name,data))
    scene = bpy.data.scenes.new('Export_' + code)
    bpy.context.window.scene = scene
    copies = []
    for name,data in snapshots:
        obj=bpy.data.objects.new(name,data)
        scene.collection.objects.link(obj)
        obj.hide_render = False
        obj.hide_set(False)
        copies.append(obj)
    export_selected('SM_World_' + code, copies)

# 원본 시설에 없는 자연물은 검토용 로우폴 메시로 별도 제작한다.
scene = bpy.data.scenes.new('WorldNature')
bpy.context.window.scene = scene
def mat(name, color):
    m=bpy.data.materials.new(name); m.diffuse_color=(*color,1); return m
wood=mat('World_Trunk',(.24,.12,.055)); leaf=mat('World_Leaf',(.16,.35,.13))
leaflight=mat('World_LeafLight',(.34,.49,.16)); rock=mat('World_Rock',(.32,.35,.34))
rocklight=mat('World_RockLight',(.46,.47,.41))
random.seed(23)
def ico(location, scale, material, subdivisions=1):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions, radius=1, location=location)
    o=bpy.context.object; o.scale=scale; o.data.materials.append(material); return o
parts=[]
bpy.ops.mesh.primitive_cone_add(vertices=7,radius1=.22,radius2=.12,depth=3.6,location=(0,0,1.8))
parts.append(bpy.context.object); parts[-1].data.materials.append(wood)
for i,(p,s) in enumerate([((0,0,4),(1.7,1.5,1.8)),((1,0,3.5),(1.3,1.4,1.3)),((-.8,.5,3.3),(1.3,1.2,1.2))]):
    parts.append(ico(p,s,leaf if i%2 else leaflight,2))
export_selected('SM_World_BroadTree',parts)
parts=[]
bpy.ops.mesh.primitive_cone_add(vertices=7,radius1=.2,radius2=.08,depth=5,location=(0,0,2.5))
parts.append(bpy.context.object); parts[-1].data.materials.append(wood)
for z,r in [(2.8,1.5),(3.8,1.2),(4.8,.8)]:
    bpy.ops.mesh.primitive_cone_add(vertices=7,radius1=r,radius2=0,depth=2.6,location=(0,0,z))
    parts.append(bpy.context.object); parts[-1].data.materials.append(leaf)
export_selected('SM_World_Pine',parts)
export_selected('SM_World_Boulder',[ico((0,0,.6),(1.4,1.1,1.1),rock,1)])
parts=[]
for x,z,s in [(-1.3,1.8,1.1),(0,2.2,1.2),(1.4,1.6,1.0)]:
    o=ico((x,0,z),(s,.9,z+0.4),rock if x else rocklight,1)
    parts.append(o)
export_selected('SM_World_Cliff',parts)
(OUT.parent/'export_report.json').write_text(json.dumps({'source':str(SOURCE),'assets':REPORT},ensure_ascii=False,indent=2),encoding='utf-8')
print('WORLD_EXPORT_OK',len(REPORT),flush=True)
