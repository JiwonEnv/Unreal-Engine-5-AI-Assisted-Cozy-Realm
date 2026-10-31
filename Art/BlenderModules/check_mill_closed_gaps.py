import bpy,json
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
p=Path(__file__).resolve().parent/'V07_지붕지지보완'
bpy.ops.wm.open_mainfile(filepath=str(p/'CozyRealm_V07_제분소_지붕과받침보완.blend'))
c=bpy.data.collections['B02_제분소'];result=[]
def hits(o,origin,direction):
    inv=o.matrix_basis.inverted();start=inv@Vector(origin);d=inv.to_3x3()@Vector(direction)
    tree=BVHTree.FromPolygons([v.co for v in o.data.vertices],[list(f.vertices) for f in o.data.polygons],all_triangles=False)
    return tree.ray_cast(start,d.normalized(),10)[0] is not None
for side in [-1,1]:
    panels=[o for o in c.objects if o.get('공용_원형')=='A01_V07_박공채움' and o.location.y*side>0]
    for x,z in [(-1,3.1),(-.5,3.4),(0,3.7),(.5,3.4),(1,3.1)]:
        ok=any(hits(o,(x,side*3,z),(0,-side,0)) for o in panels)
        result.append({'검사':('앞' if side<0 else '뒤')+f' 박공 투과 틈 확인 X{x} Z{z}','통과':ok})
rear=[o for o in c.objects if o.get('공용_원형')=='W01_V07_뒷중앙기둥']
for z in [.82,1.1,1.5,2,2.6,2.82]:
    ok=any(hits(o,(0,3,z),(0,-1,0)) for o in rear)
    result.append({'검사':f'뒷벽 중앙 투과 틈 확인 Z{z}','통과':ok})
ok=all(r['통과'] for r in result)
(p/'저장파일_박공과뒷벽_투과검사.json').write_text(json.dumps({'통과':ok,'검사':result,'범위':'선택한 16개 위치의 실제 저장 메시 검사. 전면 영업 공간·창·작은 지붕 아래 통로는 의도된 개구부.'},ensure_ascii=False,indent=2),encoding='utf-8')
print('SAVED_GAP_CHECK',len(result),[r for r in result if not r['통과']]);assert ok
