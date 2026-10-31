import bpy, math, ast, json, bmesh
from pathlib import Path
from mathutils import Vector
BASE=Path(__file__).resolve().parent
OUT=BASE/'V07_지붕지지보완';OUT.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(BASE/'V07_부분보완'/'CozyRealm_V07_제분소_부분보완.blend'))
main=bpy.data.scenes['01_시설13종_조립검토'];bpy.context.window.scene=main
library=bpy.data.collections['00_공용모듈_원형_실제메시공유']
modules={o['원형_ID']:o for o in library.all_objects if o.type=='MESH'}
M=list(modules['W01-01'].data.materials);records=[]
for filename,names in [('build_facilities.py',{'Mesh','asset','box_asset','beam'}),('revise_v07_roof_and_mill.py',{'uv_map'})]:
    for node in ast.parse((BASE/filename).read_text(encoding='utf-8')).body:
        if isinstance(node,(ast.FunctionDef,ast.ClassDef)) and node.name in names:
            exec(compile(ast.Module(body=[node],type_ignores=[]),'helpers','exec'))
col=bpy.data.collections['B02_제분소'];root=next(o for o in col.objects if o.type=='EMPTY')
def signature(c):
    return {o.name:([tuple(v.co) for v in o.data.vertices],tuple(o.location),tuple(o.rotation_euler)) for o in c.objects if o.type=='MESH'}
others={c.name:signature(c) for c in bpy.data.collections if c.get('관리_ID') and c!=col}
def place(code,loc,rot=(0,0,0)):
    source=modules[code];o=bpy.data.objects.new(source.name,source.data);col.objects.link(o)
    o.location=loc;o.rotation_euler=rot;o.parent=root
    o['공용_원형']=code;o['분류_기준']=code
    for key in ['피벗_기준','재질_경로']:o[key]=source[key]
    return o
def block(code,name,size,mat=0,bevel=.012,route='TS01'):
    return box_asset(code,name,size,mat,route=route,bevel=bevel)
def profile_asset(code,name,profile,depth,mat=0):
    m=Mesh();n=len(profile);v=[(x,y,z) for y in [-depth/2,depth/2] for x,z in profile]
    faces=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    m.poly(v,faces,mat)
    return asset(code,name,m,0,'TS01' if mat==0 else 'T02','下端'.replace('下端','하단')+' 중심 / 접합 기준')
def roofz(x):
    t=abs(x)/2.45;return 4.04-1.75*t+.55*t*t

# 앞·뒤 박공은 지붕 하부 곡선에 맞는 실제 두께 패널로 막는다.
# 패널은 가로보 뒤에 들어가므로 전면 영업 공간은 그대로 열린다.
edge=2.08;base=2.90
profile=[(-edge,0),(edge,0)]+[(edge-2*edge*i/24,roofz(edge-2*edge*i/24)-.08-base) for i in range(25)]
profile_asset('A01_V07_박공채움','곡선박공_목재채움',profile,.16,0)
for y in [-1.96,1.96]:place('A01_V07_박공채움',(0,y,base))
# 양 측벽 상단 가로보를 지붕 밑까지 올려 벽과 지붕 사이의 틈을 닫는다.
raised=[]
for o in col.objects:
    if o.get('공용_원형')=='W02_V07_200' and abs(abs(o.location.x)-2)<.001:
        o.location.z=2.80;raised.append(o)

# 뒤쪽 중앙은 누락된 기둥과 기둥 받침석으로 상·하 벽 패널을 연결한다.
block('W01_V07_뒷중앙기둥','뒷벽중앙_기둥',(.34,.34,2.05),0,.015)
rearpost=place('W01_V07_뒷중앙기둥',(0,2,.80))
rearbase=place('A04_V07_받침',(0,2,.42))

# 작은 지붕의 받침보는 실제 기둥 중심선에 놓고 서까래로 벽과 연결한다.
block('W01_V07_작은지붕기둥','작은지붕_기둥_132',(.225,.225,1.32),0,.018)
canopyposts=[]
for o in list(col.objects):
    if str(o.get('공용_원형','')).startswith('W01_V07_직선') and abs(o.location.x-2.75)<.001:
        loc=o.location.copy();bpy.data.objects.remove(o,do_unlink=True)
        canopyposts.append(place('W01_V07_작은지붕기둥',loc))
eave=next(o for o in col.objects if o.get('공용_원형')=='R02_V07_옆처마')
eave.location.x=2.75;eave.location.z=2.12
beam('R02_V07_벽붙임보','작은지붕_벽붙임보',1.65,.14)
ledger=place('R02_V07_벽붙임보',(1.97,.375,2.48),(0,0,math.pi/2))
def join_beam(code,name,a,b,section):
    a=Vector(a);b=Vector(b);delta=b-a
    if code not in modules:beam(code,name,delta.length,section)
    return place(code,a,delta.to_track_quat('X','Z').to_euler())
rafters=[];braces=[]
for y in [.55,1.20,1.85]:
    rafters.append(join_beam('R04_V07_小서까래'.replace('小','작은'),'작은지붕_연결서까래',(1.97,y,2.48),(2.75,y,2.12),.12))
for y in [.55,1.85]:
    braces.append(join_beam('W03_V07_小버팀'.replace('小','작은'),'작은지붕_사선버팀',(2.75,y,1.76),(2.40,y,2.28),.12))
# 옆에서 보이는 삼각 틈도 작은 박공 패널로 채운다.
small=[(0,0),(.78,0),(.78,.09),(.36,.27),(0,.43)]
profile_asset('A01_V07_작은박공','작은지붕_옆막음',small,.10,0)
for y in [.55,1.85]:place('A01_V07_작은박공',(1.97,y,2.10))

newcol=bpy.data.collections.new('07_V07_박공채움_중앙기둥_작은지붕받침');library.children.link(newcol)
for i,r in enumerate(records):
    o=modules[r['ID']];library.objects.unlink(o);newcol.objects.link(o);o.location=((i%6)*5,90+(i//6)*5,0)
col['지지보완']='앞뒤 곡선 박공 채움 / 측벽 상부 보 / 뒷벽 중앙 기둥과 받침 / 작은 지붕 벽붙임보·서까래·버팀'
bpy.context.view_layer.update()
def localverts(o):return [o.matrix_basis@v.co for v in o.data.vertices]
checks=[]
def check(name,ok,detail=''):checks.append({'검사':name,'통과':bool(ok),'내용':detail})
for name,sig in others.items():check(name+' 변경 없음',sig==signature(bpy.data.collections[name]))
check('앞뒤 박공 채움 2개',len([o for o in col.objects if o.get('공용_원형')=='A01_V07_박공채움'])==2)
# 박공 패널과 지붕 하부는 1cm 겹치게 맞물려 외부로 관통하는 틈을 남기지 않는다.
check('박공 상단과 하부판 맞물림',all(abs((roofz(x)-.08)-(roofz(x)-.09)-.01)<1e-6 for x in [-2,-1,0,1,2]))
check('뒷벽 중앙 틈 채움',.34>.33,'하부 널 사이 33cm / 중앙 기둥 34cm')
pv=localverts(rearpost);bv=localverts(rearbase)
check('중앙 기둥과 받침 접합',abs(min(v.z for v in pv)-max(v.z for v in bv))<1e-5)
check('작은지붕 기둥 2개',len(canopyposts)==2)
ev=localverts(eave)
for i,o in enumerate(canopyposts):
    v=localverts(o)
    check(f'작은지붕 기둥 {i+1} 받침보 접촉',min(q.x for q in v)<max(q.x for q in ev) and max(q.x for q in v)>min(q.x for q in ev) and max(q.z for q in v)>=min(q.z for q in ev))
check('서까래 양끝 연결',len(rafters)==3 and len(braces)==2,'벽붙임보→서까래→처마 받침보→기둥→기둥 받침')
placements=[o for o in col.objects if o.get('공용_원형')]
check('공용 메시 공유',all(o.data==modules[o['공용_원형']].data for o in placements))
check('배치 배율 1',all((o.scale-Vector((1,1,1))).length<1e-6 for o in placements))
bad=[]
for r in records:
    bm=bmesh.new();bm.from_mesh(modules[r['ID']].data)
    if any(not e.is_manifold for e in bm.edges) or any(f.calc_area()<1e-10 for f in bm.faces):bad.append(r['ID'])
    bm.free()
check('신규 원형 닫힌 표면·퇴화 면 없음',not bad,bad)
assert all(r['통과'] for r in checks),str(checks)
detail=bpy.data.scenes['03_제분소_140cm비율검토'];bpy.context.window.scene=detail
text=bpy.data.texts.new('지붕과받침_보완내용');text.write('앞뒤 박공의 빈 공간을 곡선 채움 패널로 막고, 뒷벽 중앙 기둥과 받침석을 보완했습니다.\n작은 지붕에는 벽붙임보·연결 서까래·사선 버팀을 추가하고, 처마 받침보를 기둥 위로 옮겼습니다.\n건축 구조 계산용 모델이 아니라 게임용 로우폴의 시각적 접합 보완입니다.\n')
outfile=OUT/'CozyRealm_V07_제분소_지붕과받침보완.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(outfile))
(OUT/'지붕과받침_검증.json').write_text(json.dumps({'통과':True,'검사':checks,'추가원형':records},ensure_ascii=False,indent=2),encoding='utf-8')
detail.render.filepath=str(OUT/'제분소_지붕받침_전체.png');bpy.ops.render.render(write_still=True,scene=detail.name)
cam=detail.camera
for title,target,off,scale in [('제분소_후면_막음확인',(10,0,2),(6,9,2.5),8),('제분소_낮은시점_앞박공',(10,-.4,2.4),(6,-10,.5),7.5),('작은지붕_받침연결',(12.55,1.2,1.95),(3.5,-4,-.35),2.9)]:
    target=Vector(target);cam.location=target+Vector(off);cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=scale
    detail.render.resolution_x=1500;detail.render.resolution_y=1300;detail.render.filepath=str(OUT/(title+'.png'));bpy.ops.render.render(write_still=True,scene=detail.name)
print('MILL_SUPPORT_DONE',len(checks))
