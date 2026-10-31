import bpy, json, bmesh
from pathlib import Path
from mathutils import Vector
p=Path(__file__).resolve().parent/'V07_접합수정'
bpy.ops.wm.open_mainfile(filepath=str(p/'CozyRealm_V07_처마접합수정_제분소재구성.blend'))
lib=bpy.data.collections['00_공용모듈_원형_실제메시공유']
sources={o['원형_ID']:o for o in lib.all_objects if o.type=='MESH'}
result=[]
def check(name,ok,detail=''):
    result.append({'검사':name,'통과':bool(ok),'내용':detail})
instances=[o for c in bpy.data.collections if c.get('관리_ID') for o in c.objects if o.type=='MESH' and o.get('공용_원형')]
check('원형 메시 연결',all(o.data==sources[o['공용_원형']].data for o in instances),len(instances))
check('배치 배율 1',all((o.scale-Vector((1,1,1))).length<1e-6 for o in instances))
check('원형 배율 1',all((o.scale-Vector((1,1,1))).length<1e-6 for o in sources.values()))
for c in bpy.data.collections:
    if c.get('관리_ID'):
        root=next(o for o in c.objects if o.type=='EMPTY')
        check(c.name+' 조립 원점',(root.location-Vector(c.instance_offset)).length<1e-6)
bad=[];deg=[]
for code,o in sources.items():
    bm=bmesh.new();bm.from_mesh(o.data)
    if any(not e.is_manifold for e in bm.edges):bad.append(code)
    if any(f.calc_area()<1e-10 for f in bm.faces):deg.append(code)
    bm.free()
check('닫힌 원형 표면',not bad,bad);check('퇴화 면 없음',not deg,deg)
check('UV 초안 존재',all(len(o.data.uv_layers)>0 for o in sources.values()))
fig=[o for o in bpy.data.objects if o.name.startswith('주인공_높이140cm')]
check('140cm 기준 인체',all(abs(max(v.co.z for v in o.data.vertices)-min(v.co.z for v in o.data.vertices)-1.4)<1e-6 for o in fig))
for c in bpy.data.collections:
    if not c.get('관리_ID') or c.get('관리_ID') in ['B07','B13']:continue
    for side in [-1,1]:
        fascia=[o for o in c.objects if str(o.get('공용_원형','')).startswith('R02_') and o.location.x*side>2.2 and '옆처마' not in o.get('공용_원형','')]
        verts=[o.matrix_basis@v.co for o in fascia for v in o.data.vertices]
        expected=2.30 if c.get('관리_ID')=='B02' else 2.5
        check(c.name+f' 처마 {side} 실측',bool(verts) and abs(min(v.y for v in verts)+expected)<1e-5 and abs(max(v.y for v in verts)-expected)<1e-5)
ok=all(r['통과'] for r in result)
(p/'저장파일_재검증.json').write_text(json.dumps({'통과':ok,'검사':result},ensure_ascii=False,indent=2),encoding='utf-8')
print('V07_SAVED_FILE_CHECK',len(result),[r for r in result if not r['통과']])
assert ok
