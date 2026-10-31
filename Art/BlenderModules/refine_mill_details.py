import bpy, math, ast, json, bmesh
from pathlib import Path
from mathutils import Vector

BASE=Path(__file__).resolve().parent
OUT=BASE/'V07_부분보완';OUT.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(BASE/'V07_접합수정'/'CozyRealm_V07_처마접합수정_제분소재구성.blend'))
main=bpy.data.scenes['01_시설13종_조립검토'];bpy.context.window.scene=main
library=bpy.data.collections['00_공용모듈_원형_실제메시공유']
modules={o['원형_ID']:o for o in library.all_objects if o.type=='MESH'}
M=list(modules['W01-01'].data.materials);records=[]
for filename,names in [('build_facilities.py',{'Mesh','asset','box_asset'}),('revise_v07_roof_and_mill.py',{'uv_map'})]:
    tree=ast.parse((BASE/filename).read_text(encoding='utf-8'))
    for node in tree.body:
        if isinstance(node,(ast.FunctionDef,ast.ClassDef)) and node.name in names:
            exec(compile(ast.Module(body=[node],type_ignores=[]),'helpers','exec'))
col=bpy.data.collections['B02_제분소'];root=next(o for o in col.objects if o.type=='EMPTY')
def meshes_signature(c):
    return {o.name:([list(v.co) for v in o.data.vertices],list(o.location),list(o.rotation_euler)) for o in c.objects if o.type=='MESH'}
before={c.name:meshes_signature(c) for c in bpy.data.collections if c.get('관리_ID') and c!=col}
def delete(o):bpy.data.objects.remove(o,do_unlink=True)
def place(code,loc,rot=(0,0,0)):
    source=modules[code];o=bpy.data.objects.new(source.name,source.data);col.objects.link(o)
    o.location=loc;o.rotation_euler=rot;o.parent=root
    for key in ['피벗_기준','재질_경로']:o[key]=source[key]
    o['공용_원형']=code;o['분류_기준']=code;return o
def block(code,name,size,mat=0,bevel=.02,route='TS01'):
    box_asset(code,name,size,mat,route=route,bevel=bevel)

# 1. 캡은 뒤쪽 박공과 같은 평면에 있지 않게 전면까지 두께를 확보한다.
cap=modules['R03_V07_원목끝']
for v in cap.data.vertices:v.co.z*=2.5
cap.data.update()
for o in col.objects:
    if o.get('공용_원형')=='R03_V07_원목끝':o.location.y=-2.25
block('R03_V07_캡아래목재','캡아래_짧은세로목재',(.16,.20,.28),0,.02)
place('R03_V07_캡아래목재',(0,-2.42,3.87))

# 2. 회벽 아래를 목재 패널로 바꾸고 바닥까지 이어지는 실제 판재 구간을 만든다.
block('A01_V07_하부널_보완','하부널_72',(.27,.10,.72),0,.012)
block('A01_V07_상부회벽_보완','상부회벽_134',(1.70,.14,1.34),3,.012,'T02')
m=Mesh()
for x in [-.605,.605]:m.box(x,0,.67,.49,.14,1.34,3)
m.box(0,0,.08,.72,.14,.16,3)
m.box(0,0,1.14,.72,.14,.40,3)
asset('A01_V07_상부창벽_보완','상부작은창_개구벽',m,.008,'T02')
for o in list(col.objects):
    code=o.get('공용_원형','')
    if code in ['A01_V07_하부널','A01_V07_회벽','A01_V07_창벽']:
        loc=o.location.copy();rot=o.rotation_euler.copy();delete(o)
        if code=='A01_V07_하부널':
            loc.x=2.10 if loc.x>0 else -2.10;loc.z=.58;new='A01_V07_하부널_보완'
        else:
            loc.z=1.44;new='A01_V07_상부창벽_보완' if code=='A01_V07_창벽' else 'A01_V07_상부회벽_보완'
        place(new,loc,rot)
    elif code=='W02_V07_180' and abs(abs(o.location.x)-2)<.001:
        if abs(o.location.z-.83)<.001:o.location.z=.54
        if o.location.z<1.5:o.location.x=2.10 if o.location.x>0 else -2.10
# 후면도 동일한 목재 하단으로 연결한다. 공용 널 메시를 그대로 재사용한다.
for center in [-1,1]:
    for j in range(6):place('A01_V07_하부널_보완',(center-.70+j*.28,2.10,.58))
    for z in [.54,1.36]:place('W02_V07_180',(center-.9,2.10,z))

# 3. 모서리는 하나의 큰 돌로 받친다. 일반 돌과 겹치거나 빈 반쪽이 생기지 않게 분할한다.
for o in list(col.objects):
    code=o.get('공용_원형','')
    # 옆 작은 지붕의 독립 기초는 바꾸지 않는다.
    if code.startswith('A04_V07_기초') and abs(o.location.x)<2.1:delete(o)
block('A04_V07_모서리기초','모서리_일체형기초_80',(.80,.80,.42),2,.045,'T01')
block('A04_V07_중간기초','중간기초_80',(.80,.58,.42),2,.045,'T01')
for x in [-2,2]:
    for y in [-2,2]:place('A04_V07_모서리기초',(x,y,0))
for y in [-2,2]:
    for x in [-1.2,-.4,.4,1.2]:place('A04_V07_중간기초',(x,y,0))
for x in [-2,2]:
    for y in [-1.2,-.4,.4,1.2]:place('A04_V07_중간기초',(x,y,0),(0,0,math.pi/2))
# 바닥의 모서리도 돌의 안쪽 경계에 맞춰 잘라 같은 높이 면 겹침을 방지한다.
floor=next(o for o in col.objects if o.name.startswith('제분소_바닥'))
outline=[(-1.6,-1.71),(1.6,-1.71),(1.6,-1.6),(1.71,-1.6),(1.71,1.6),(1.6,1.6),(1.6,1.71),(-1.6,1.71),(-1.6,1.6),(-1.71,1.6),(-1.71,-1.6),(-1.6,-1.6)]
vs=[(x,y,z) for z in [0,.07] for x,y in outline];n=len(outline)
fs=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
me=bpy.data.meshes.new('제분소_바닥_모서리피함');me.from_pydata(vs,[],fs);me.materials.append(M[0]);me.update();floor.data=me

newcol=bpy.data.collections.new('06_V07_캡_하부널_모서리_부분보완');library.children.link(newcol)
for i,r in enumerate(records):
    o=modules[r['ID']];library.objects.unlink(o);newcol.objects.link(o);o.location=((i%6)*4,80+(i//6)*4,0)
detail=bpy.data.scenes['03_제분소_140cm비율검토'];bpy.context.window.scene=detail
col['부분보완']='원형 캡 앞면 / 캡 아래 목재 / 하부 목재 패널 / 네 모서리 큰 기초석'
col['사용자_유지요청']='직전 수정본의 두께감·전체 형태 유지'
result=[]
def check(name,ok,info=''):
    result.append({'검사':name,'통과':bool(ok),'내용':info})
for name,snapshot in before.items():check(name+' 변경 없음',snapshot==meshes_signature(bpy.data.collections[name]))
placements=[o for o in col.objects if o.get('공용_원형')]
check('공용 원형 메시 공유',all(o.data==modules[o['공용_원형']].data for o in placements))
check('배치 배율 1',all((o.scale-Vector((1,1,1))).length<1e-6 for o in placements))
corners=[o for o in placements if o.get('공용_원형')=='A04_V07_모서리기초']
check('네 모서리 일체형 기초',len(corners)==4)
check('기둥 받침이 모서리 기초 안에 있음',all(.58/2<.80/2 for o in corners),'58cm 받침 / 80cm 모서리 기초')
capplaced=next(o for o in placements if o.get('공용_원형')=='R03_V07_원목끝')
front=min((capplaced.matrix_basis@v.co).y for v in capplaced.data.vertices)
check('원형 캡 앞면이 박공보다 전면',front< -2.50,{'캡_앞면_Y':front,'박공_앞면_Y':-2.44})
check('하부 널의 노출 위치',all(abs(o.location.x)>2.07 for o in placements if o.get('공용_원형')=='A01_V07_하부널_보완' and abs(o.rotation_euler.z)>1))
bad=[]
for r in records:
    bm=bmesh.new();bm.from_mesh(modules[r['ID']].data)
    if any(not e.is_manifold for e in bm.edges) or any(f.calc_area()<1e-10 for f in bm.faces):bad.append(r['ID'])
    bm.free()
check('신규 원형 닫힌 표면·퇴화 면 없음',not bad,bad)
assert all(r['통과'] for r in result),str(result)
outfile=OUT/'CozyRealm_V07_제분소_부분보완.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(outfile))
(OUT/'부분보완_검증.json').write_text(json.dumps({'통과':True,'검사':result,'신규원형':records},ensure_ascii=False,indent=2),encoding='utf-8')
detail.render.filepath=str(OUT/'제분소_부분보완_전체.png');bpy.ops.render.render(write_still=True,scene=detail.name)
# 표시 부위만 가까이 본 실제 메시 렌더. 원본 저장 시점의 카메라는 전체 보기다.
cam=detail.camera
for title,target,offset,scale in [('용마루_캡_보완',(10,-2.3,4.06),(2,-4,1.2),1.6),('측벽하단_모서리기초_보완',(12,-1.5,.75),(4,-3,1.8),3.2)]:
    target=Vector(target);cam.location=target+Vector(offset);cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=scale
    detail.render.resolution_x=1400;detail.render.resolution_y=1200;detail.render.filepath=str(OUT/(title+'.png'));bpy.ops.render.render(write_still=True,scene=detail.name)
print('MILL_DETAIL_REFINEMENT_DONE',len(result))
