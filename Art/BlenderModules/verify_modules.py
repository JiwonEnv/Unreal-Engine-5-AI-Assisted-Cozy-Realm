import bpy, json, math, bmesh
from pathlib import Path
from mathutils import Vector, Matrix
out=Path(__file__).resolve().parent
path=out/'CozyRealm_공용모듈과시설_140cm_로우폴.blend'
bpy.ops.wm.open_mainfile(filepath=str(path))
library=bpy.data.collections['00_공용모듈_원형_실제메시공유']
sources={o['원형_ID']:o for o in library.all_objects if o.type=='MESH'}
fail=[]; result=[]
def bounds(o):return [(min(v.co[a] for v in o.data.vertices),max(v.co[a] for v in o.data.vertices)) for a in range(3)]
def check(name,ok,detail):
    result.append({'검사':name,'통과':bool(ok),'내용':detail})
    if not ok:fail.append(name)
for code,L in [('W02-01',1),('W02-02',2),('W02-03',4)]:
    b=bounds(sources[code]);check(code+' 시작 접합면 피벗',abs(b[0][0])<1e-5 and abs(b[0][1]-L)<1e-5,{'X범위_m':b[0],'기준길이_m':L})
for code in ['W01-01','W01-02','A01-01','A01-02','A01-03','A03-01','A04-01','A04-02','A04-03','U13']:
    o=sources[code];b=bounds(o);check(code+' 바닥 피벗',abs(b[2][0])<1e-5,{'밑면Z_m':b[2][0],'피벗':o['피벗_기준']})
door=sources['A03-02'];b=bounds(door)
check('문짝 경첩 피벗',abs(b[0][0])<.011 and abs(b[2][0])<1e-5,{'메시_X범위':b[0],'문폭_cm':(b[0][1]-b[0][0])*100})
check('문틀 통과 규격',bpy.data.scenes['01_시설13종_조립검토']['문_유효폭_cm']==100 and bpy.data.scenes['01_시설13종_조립검토']['문_유효높이_cm']==180,'100×180cm / 140cm 주인공보다 40cm 높음')
instances=[]
for c in bpy.data.collections:
    if c.get('관리_ID'):
        for o in c.objects:
            if o.type=='MESH' and o.get('공용_원형'):instances.append(o)
        root=next(o for o in c.objects if o.type=='EMPTY')
        check(c.name+' 시설 원점',Vector(c.instance_offset).almost_equal(root.location) if hasattr(Vector(),'almost_equal') else (Vector(c.instance_offset)-root.location).length<1e-5,'Collection 인스턴스 오프셋 = 바닥 중심 조립 원점')
check('실제 메시 공유',all(o.data==sources[o['공용_원형']].data for o in instances),{'공유배치수':len(instances),'원형수':len(sources)})
check('단위 스케일 적용',all((o.scale-Vector((1,1,1))).length<1e-5 for o in list(sources.values())+instances),'길이 파생은 메시 좌표·UV에 적용. 배치 스케일 모두 1,1,1')
figures=[o for o in bpy.data.objects if o.type=='MESH' and o.name.startswith('주인공_높이140cm')]
check('140cm 기준 인체',all(abs(bounds(o)[2][1]-bounds(o)[2][0]-1.4)<1e-5 for o in figures),{'배치수':len(figures),'높이_m':1.4})
bad=[];deg=[]
for code,o in sources.items():
    bm=bmesh.new();bm.from_mesh(o.data)
    if any(not e.is_manifold for e in bm.edges):bad.append(code)
    if any(f.calc_area()<1e-10 for f in bm.faces):deg.append(code)
    bm.free()
check('원형 닫힌 표면',not bad,{'열린원형':bad,'주의':'분리된 박스 조립품은 연결된 단일 표면을 뜻하지 않음. 내부 겹침 최적화 후속.'})
check('퇴화 면 없음',not deg,deg)
check('모든 원형 UV 존재',all(len(o.data.uv_layers)>0 for o in sources.values()),'공용 트림/타일 UV 초안 및 특징 부품 고유 UV 초안. 베이크 검증 전.')
(out/'피벗_메시공유_검증.json').write_text(json.dumps({'통과':not fail,'실패':fail,'검사':result},ensure_ascii=False,indent=2),encoding='utf-8')
print('검증:',len(result),'실패:',fail)
if fail:raise RuntimeError(str(fail))
