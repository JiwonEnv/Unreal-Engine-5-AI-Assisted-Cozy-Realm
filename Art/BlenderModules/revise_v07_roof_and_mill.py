import bpy, math, json, ast, bmesh
from pathlib import Path
from mathutils import Vector

OUT=Path(__file__).resolve().parent/'V07_접합수정'
OUT.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(OUT.parent/'CozyRealm_공용모듈과시설_140cm_로우폴.blend'))
main=bpy.data.scenes['01_시설13종_조립검토'];bpy.context.window.scene=main
library=bpy.data.collections['00_공용모듈_원형_실제메시공유']
modules={o['원형_ID']:o for o in library.all_objects if o.type=='MESH'}
M=list(modules['W01-01'].data.materials)
records=[];derived={}
tree=ast.parse((OUT.parent/'build_facilities.py').read_text(encoding='utf-8'))
needed={'Mesh','uv_map','asset','box_asset','beam','put','figure','studio','loose_box'}
for node in tree.body:
    if isinstance(node,(ast.FunctionDef,ast.ClassDef)) and node.name in needed:
        exec(compile(ast.Module(body=[node],type_ignores=[]),'builder_helpers','exec'))
def uv_map(obj):
    # 문맥 의존 편집 명령 대신 면 방향 투영 초안. 최종 베이크 UV는 후속 작업.
    me=obj.data;uv=me.uv_layers.new(name='UV0_수정본_투영초안')
    for p in me.polygons:
        drop=max(range(3),key=lambda a:abs(p.normal[a]));axes=[a for a in range(3) if a!=drop]
        for li in p.loop_indices:
            co=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=(co[axes[0]],co[axes[1]])
changed=[];roof_checks=[]
def bounds_local(o):
    return [o.matrix_basis@v.co for v in o.data.vertices]
def remove(o):bpy.data.objects.remove(o,do_unlink=True)
def new_beam(code,L,section=.18):
    if code not in modules:beam(code,'接合修正'.replace('接合修正','접합수정'),L,section)
    return code

# 일반 지붕은 실제 널의 양끝에서 처마·마루 길이를 산정한다.
for col in list(bpy.data.collections):
    cid=col.get('관리_ID')
    if not cid or cid in ['B07','B13','B02']:continue
    print('ROOF_START',cid,flush=True)
    root=next(o for o in col.objects if o.type=='EMPTY')
    tiles=[o for o in col.objects if str(o.get('분류_기준','')).startswith('R01')]
    # 이전 널 배열의 비대칭 중심을 바로잡는다. 폭은 실제 메시 기준으로 유지한다.
    verts=[v for o in tiles for v in bounds_local(o)]
    y0=min(v.y for v in verts);y1=max(v.y for v in verts);shift=-(y0+y1)/2
    for o in tiles:o.location.y+=shift
    y0+=shift;y1+=shift;L=y1-y0
    oldfas=[o for o in col.objects if o.get('분류_기준')=='R02']
    byside={-1:[],1:[]}
    for o in oldfas:byside[-1 if o.location.x<0 else 1].append(o)
    for side,old in byside.items():
        print('FASCIA',cid,side,flush=True)
        x=old[0].location.x
        for o in old:remove(o)
        eavetiles=[o for o in tiles if o.location.x*side>1]
        edge=[v for o in eavetiles for v in bounds_local(o) if v.x*side>abs(x)-.015]
        z=min(v.z for v in edge)
        # 처마 윗면은 널 밑면보다 5mm 낮다. 표면을 같은 높이에 겹치지 않는다.
        for j in range(math.ceil(L/2)):
            seg=min(2,L-j*2)
            code=new_beam('R02_FIX_'+str(round(seg*100)),seg,.18)
            o=put(code,col,(x,y0+j*2,z-.095),(0,0,math.pi/2));o.parent=root
            o['접합_역할']='지붕 하단 받침 / 널의 실제 끝 길이와 일치'
        roof_checks.append({'시설':cid,'처마방향':side,'널_Y범위_m':[round(y0,4),round(y1,4)],'처마_Y범위_m':[round(y0,4),round(y0+L,4)],'처마상단_널하단_간격_m':.005})
    # 박공 부재가 널 표면과 교차하지 않도록 지붕 아래·끝 바깥으로 이동한다.
    for o in list(col.objects):
        base=o.get('분류_기준','')
        if base=='R04':o.location.y=(y0-.075 if o.location.y<0 else y1+.075);o.location.z-=.16
        if base=='R03-01':remove(o)
        if base=='R03-02':o.location.y=y0 if o.location.y<0 else y1
    ridge=max(o.location.z for o in tiles)+.10
    for j in range(math.ceil(L/2)):
        seg=min(2,L-j*2);o=put('R03-01',col,(0,y0+2*j,ridge),(0,0,math.pi/2),scale=(seg/2,1,1));o.parent=root
    col['수정범위']='R02 실제 지붕 길이 연결 / R04 표면 겹침 제거. 시설별 V07 외형 대조는 별도 진행 필요.'
    changed.append(cid)

print('ROOF_DONE',flush=True)
# 제분소는 V07 A 시안의 큰 형상을 다시 만든다. 다른 시설을 완료 처리하지 않는다.
col=bpy.data.collections['B02_제분소'];root=next(o for o in col.objects if o.type=='EMPTY')
for o in list(col.objects):
    if o!=root:remove(o)
bpy.context.view_layer.update()
print('MILL_CLEARED',flush=True)
col['수정범위']='V07 A 제분소 실루엣 재구성: 곡선 지붕·큰 널·굵은 기둥·기초·옆 지붕·창·맷돌'
def place(code,loc=(0,0,0),rot=(0,0,0),scale=(1,1,1)):
    o=put(code,col,loc,rot,scale);o.parent=root;return o
def block(code,name,size,mat=0,bevel=.02,route='TS01'):
    if code not in modules:box_asset(code,name,size,mat,route=route,bevel=bevel)
def curved_strip(code,name,points,width,thick,mat=0,bevel=.02):
    # X 방향 곡선 단면을 Y 방향으로 압출한다. 피벗은 시작 단면의 하단 중심.
    m=Mesh();v=[]
    for x,z in points:v.extend([(x,-width/2,z),(x,width/2,z),(x,width/2,z+thick),(x,-width/2,z+thick)])
    f=[(3,2,1,0)]
    for i in range(len(points)-1):
        for j in range(4):f.append((4*i+j,4*i+(j+1)%4,4*(i+1)+(j+1)%4,4*(i+1)+j))
    f.append(tuple(4*(len(points)-1)+j for j in range(4)))
    m.poly(v,f,mat);return asset(code,name,m,bevel,'TS01' if mat==0 else 'TS01 지붕','시작 접합면 하단 / +X 길이')

block('A04_V07_기초','기초석_100x58x42',(1,.58,.42),2,.05,'T01')
print('MILL_FIRST_ASSET',flush=True)
block('A04_V07_받침','기둥받침_58',(.58,.58,.38),2,.04,'T01')
block('W01_V07_직선','굵은기둥_205',(.30,.30,2.05),0,.025)
m=Mesh();m.sweep([(0,0,0),(.07,0,.55),(.23,0,1.1),(.29,0,1.65),(.30,0,2.20)],[.42,.40,.37,.34,.34],[.36]*5)
asset('U13_V07','휜기둥_큰윤곽',m,.035,'고유 UV·베이크 예정','밑면 접합점')
print('MILL_U13',flush=True)
block('W04_V07','큰접합블록',(.48,.46,.26),0,.03)
block('A01_V07_회벽','개구부없는벽',(1.70,.14,1.98),3,.015,'T02')
block('A01_V07_하부널','하부목재널',(.27,.10,.54),0,.012)
beam('W02_V07_400','굵은보_400',4,.26)
beam('W02_V07_200','굵은보_200',2,.24)
beam('W02_V07_180','보_180',1.8,.20)
block('R03_V07_끝기둥','마루끝_세로기둥',(.26,.26,.55),0,.025)
block('A02_V07_세로','창세로틀',(.10,.20,.80),0,.012)
block('A02_V07_가로','창가로틀',(.74,.20,.10),0,.012)
block('A02_V07_창살','십자창살',(.05,.08,.62),0,.006)
m=Mesh();m.cylinder(0,0,0,.10,.07,0,10)
asset('W04_V07_목심','목심',m,.009,'TS01','목심 뒤 접합면')
m=Mesh();m.cylinder(0,0,0,.73,.35,2,16)
asset('U16_V07_맷돌','큰맷돌',m,.035,'고유 UV·베이크 예정')

# 400cm 골조와 두꺼운 기초를 유지하되 전면 두 기둥을 강조한다.
for y in [-2,2]:
    for i in range(4):place('A04_V07_기초',(-1.5+i,y,0))
for x in [-2,2]:
    for i in range(3):place('A04_V07_기초',(x,-1.14+i*1.14,0),(0,0,math.pi/2),scale=(1.14,1,1))
o=loose_box(col,'제분소_바닥', (0,0,.35),(3.42,3.42,.07),0);o.parent=root
for x in [-2,2]:
    for y in [-2,0,2]:
        place('A04_V07_받침',(x,y,.42))
        place('U13_V07' if y==-2 else 'W01_V07_직선',(x,y,.80),(0,0,0 if x<0 else math.pi))
        place('W04_V07',(x,y,2.60))
        if y==-2:
            for z in [1.00,2.62]:place('W04_V07_목심',(x+.07*(-1 if x>0 else 1),y-.20,z),(math.pi/2,0,0))
for y in [-2,2]:place('W02_V07_400',(-2,y,2.85))
for x in [-2,2]:
    for y in [-2,0]:place('W02_V07_200',(x,y,2.68),(0,0,math.pi/2))
# 후면 벽·하부 목재. 정면 박공은 막지 않고 골조를 보이게 한다.
for x in [-1,1]:place('A01_V07_회벽',(x,2,.8))
for x in [-2,2]:
    for yc in [-1,1]:
        # 작은 십자 창 하나만 앞쪽 구간에 낸다.
        if yc==-1:
            m=Mesh();m.box(-.605,0,.99,.49,.14,1.98,3);m.box(.605,0,.99,.49,.14,1.98,3);m.box(0,0,.40,.72,.14,.80,3);m.box(0,0,1.79,.72,.14,.38,3)
            code='A01_V07_창벽'
            if code not in modules:asset(code,'작은창개구벽',m,.01,'T02')
            place(code,(x,yc,.8),(0,0,math.pi/2))
            # 창틀을 벽 외측으로 배치. 바깥면 법선 ±X.
            sx=1 if x>0 else -1;xp=x+sx*.10
            for dy in [-.34,.34]:place('A02_V07_세로',(xp,yc+dy,1.58),(0,0,math.pi/2))
            for z in [1.58,2.28]:place('A02_V07_가로',(xp,yc,z),(0,0,math.pi/2))
            place('A02_V07_창살',(xp,yc,1.67),(0,0,math.pi/2))
            place('A02_V07_창살',(xp,yc+.31,1.98),(math.pi/2,0,0))
        else:place('A01_V07_회벽',(x,yc,.8),(0,0,math.pi/2))
        for j in range(6):place('A01_V07_하부널',(x-sx*.01 if yc==-1 else x,yc-.70+j*.28,.8),(0,0,math.pi/2))
        for z in [.83,1.36,2.75]:place('W02_V07_180',(x,-1.9 if yc==-1 else .1,z),(0,0,math.pi/2))

def roofz(x):
    t=x/2.45;return 4.04-1.75*t+.55*t*t
# 넓은 곡선 면을 실제 저분할 메시로 만든다.
roof_parts=[]
for side in [-1,1]:
    for row in range(3):
        a=row*.82;b=min(2.45,a+.94);lift=(2-row)*.065
        pts=[((b-a)*i/4,roofz(a+(b-a)*i/4)-roofz(a)) for i in range(5)]
        # 중간 줄은 반 폭씩 엇갈리며 양끝은 절반 폭 원형을 쓴다.
        widths=[1.15]*4 if row!=1 else [.575,1.15,1.15,1.15,.575]
        y=-2.30
        for wid in widths:
            key=f'R01_V07_{row}_{round(wid*1000)}'
            if key not in modules:curved_strip(key,'曲線'.replace('曲線','곡선')+'지붕널',pts,wid-.012,.12,1,.025)
            o=place(key,(side*a,y+wid/2,roofz(a)+lift),(0,0,0 if side==1 else math.pi));roof_parts.append(o);y+=wid
    # 지붕 하부판과 박공 마감은 덮개 아래로 확실히 내려 놓는다.
    pts=[(2.45*i/12,roofz(2.45*i/12)-roofz(0)) for i in range(13)]
    key='R01_V07_하부판'
    if key not in modules:curved_strip(key,'곡선지붕_하부판',pts,4.60,.06,0,.006)
    place(key,(0,0,roofz(0)-.09),(0,0,0 if side==1 else math.pi))
    key='R04_V07_곡선'
    if key not in modules:
        m=Mesh();vv=[]
        for i in range(25):
            x=2.45*i/24;z=roofz(x)-roofz(0)
            lift=.13 if x<.82 else .065 if x<1.64 else 0
            vv.extend([(x,-.075,z-.20),(x,.075,z-.20),(x,.075,z+lift+.065),(x,-.075,z+lift+.065)])
        ff=[(3,2,1,0)]
        for i in range(24):
            for j in range(4):ff.append((4*i+j,4*i+(j+1)%4,4*(i+1)+(j+1)%4,4*(i+1)+j))
        ff.append((96,97,98,99));m.poly(vv,ff,1)
        asset(key,'지붕끝_계단겹침마감',m,.008,'TS01 지붕','시작 접합면 / 지붕 끝 바깥')
    for y in [-2.365,2.365]:place(key,(0,y,roofz(0)),(0,0,0 if side==1 else math.pi))
    # 처마 200+200+60cm를 실제 지붕 깊이 460cm에 맞춘다.
    y=-2.30
    for seg in [2,2,.6]:
        key=new_beam('R02_V07_'+str(round(seg*100)),seg,.22)
        place(key,(side*2.42,y,roofz(2.45)-.115),(0,0,math.pi/2));y+=seg
    roof_checks.append({'시설':'B02','처마방향':side,'널_Y범위_m':[-2.30,2.30],'처마_Y범위_m':[-2.30,y],'처마상단_널하단_간격_m':.005})
# 용마루는 가로로 얕게 휘고 양끝 원목 단면이 읽히게 한다.
pts=[(i*.575, .12*((i/8-.5)*2)**2) for i in range(9)]
curved_strip('R03_V07_곡선','완만한곡선_용마루',pts,.22,.24,0,.025)
place('R03_V07_곡선',(0,-2.30,4.08),(0,0,math.pi/2))
m=Mesh();m.cylinder(0,0,0,.21,.12,0,12);asset('R03_V07_원목끝','원목단면캡',m,.014,'TS01','뒤 접합면')
place('R03_V07_원목끝',(0,-2.32,4.30),(math.pi/2,0,0))
place('R03_V07_끝기둥',(0,2.10,4.12))
# 정면 열린 박공의 내부 서까래와 중앙 짧은 지주.
beam('W01_V07_박공지주','박공지주',1.05,.16)
place('W01_V07_박공지주',(0,-2,2.86),(0,-math.pi/2,0))

# 맷돌과 호퍼는 전면 중심 가까이, 후면이 아니라 전면에서 조작하게 배치한다.
for z in [.42,.79]:place('U16_V07_맷돌',(-.45,-1.10,z))
place('P05-축',(-.45,-1.10,1.07))
place('P05-호퍼',(-.45,-1.10,1.66),scale=(1.4,1.4,1.1))
block('P05_V07_축받침','축받침',(.22,.22,.32),0,.02)
place('P05_V07_축받침',(-.45,-1.10,1.13))
beam('P05_V07_손잡이','목재회전손잡이',1.42,.10)
place('P05_V07_손잡이',(-.45,-1.10,1.30))
block('P05_V07_손잡이지지','손잡이받침',(.18,.18,.88),0,.02)
place('P05_V07_손잡이지지',(.98,-1.10,.42))

# 참고 이미지의 우측 낮은 작은 지붕과 뒤쪽 기둥을 복원한다.
for y in [.55,1.85]:
    place('A04_V07_기초',(2.72,y,0),(0,0,math.pi/2))
    place('A04_V07_받침',(2.75,y,.42))
    place('W01_V07_직선',(2.75,y,.80),scale=(.75,.75,.57))
pts=[(0,0),(.42,-.18),(.84,-.34),(1.18,-.44)]
for j in range(3):
    key='R01_V07_옆지붕'
    if key not in modules:curved_strip(key,'작은지붕널',pts,.55,.10,1,.02)
    place(key,(1.90,.65+j*.55,2.55))
beam('R02_V07_옆처마','옆지붕처마',1.65,.18)
place('R02_V07_옆처마',(3.06,.375,2.0),(0,0,math.pi/2))
for y in [.65,1.75]:place('W02_V07_180',(1.8,y,.83),scale=(.7,1,1))
o=figure(col,(-2.95,-2.3,0));o.parent=root
col['설계_폭_cm']=400;col['설계_깊이_cm']=400
col['상태']='V07 제분소 실루엣 재구성 / 재질·세부 스컬프·다른 시설 외형 대조 미완료'

# 신규 원형도 공용 라이브러리에 넣고, 동일 규격은 메시를 공유한다.
newcol=bpy.data.collections.new('05_V07_지붕접합과제분소_수정원형');library.children.link(newcol)
for i,r in enumerate(records):
    ob=modules[r['ID']]
    if ob.name in library.objects:library.objects.unlink(ob)
    newcol.objects.link(ob);ob.location=((i%7)*5,42+(i//7)*5,0)

detail=bpy.data.scenes['03_제분소_140cm비율검토']
# 기존 스튜디오의 바닥·광원·카메라를 재사용한다.
cam=detail.camera;target=Vector((10.25,0,2.0));cam.location=target+Vector((6.7,-9,5.1));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=8.6
detail.render.resolution_x=1700;detail.render.resolution_y=1500;detail.cycles.samples=32
bpy.context.window.scene=detail
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            a=area.spaces.active;a.region_3d.view_rotation=cam.rotation_euler.to_quaternion();a.region_3d.view_location=target;a.region_3d.view_distance=8.6
read=bpy.data.texts.new('V07_修正'.replace('修正','수정')+'_확인사항')
read.write('이 파일은 별도 수정본입니다.\nR02 처마 길이와 R04 겹침은 모든 일반 지붕에 수정했습니다.\nB02는 V07 A 시안의 큰 실루엣을 재구성했습니다.\n나머지 시설의 V07 디자인 일치가 완료된 것은 아닙니다.\n하이폴·재질·최종 베이크 미완료. 원형별 UV는 초안입니다.\n')
bpy.context.view_layer.update()
fail=[]
for q in roof_checks:
    if abs(q['널_Y범위_m'][0]-q['처마_Y범위_m'][0])>.0001 or abs(q['널_Y범위_m'][1]-q['처마_Y범위_m'][1])>.0001:fail.append(q)
if fail:raise RuntimeError(str(fail))
# 실제 배치된 처마 범위도 계산해서 기록한다.
for q in roof_checks:
    c=next(c for c in bpy.data.collections if c.get('관리_ID')==q['시설']);side=q['처마방향']
    fs=[o for o in c.objects if str(o.get('공용_원형','')).startswith('R02_') and o.location.x*side>2.2 and '옆처마' not in o.get('공용_원형','')]
    vs=[v for o in fs for v in bounds_local(o)]
    if vs:
        actual=[min(v.y for v in vs),max(v.y for v in vs)];q['実測'.replace('実測','실측')+'_처마_Y범위_m']=actual
        assert max(abs(actual[i]-q['널_Y범위_m'][i]) for i in range(2))<.0001
outfile=OUT/'CozyRealm_V07_처마접합수정_제분소재구성.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(outfile))
(OUT/'지붕접합_검증.json').write_text(json.dumps({'통과':True,'처마검증':roof_checks,'기존시설_접합수정':changed,'디자인재구성':['B02'],'주의':'다른 시설 디자인 및 최종 스컬프·재질·UV 검증 미완료','추가원형':records},ensure_ascii=False,indent=2),encoding='utf-8')
detail.render.filepath=str(OUT/'제분소_V07_실루엣수정.png');bpy.ops.render.render(write_still=True,scene=detail.name)
cam.location=Vector((18,-.15,4.3));target=Vector((10,0,2.2));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.ortho_scale=7.4
detail.render.filepath=str(OUT/'제분소_측면_처마접합.png');bpy.ops.render.render(write_still=True,scene=detail.name)
print('V07_ROOF_MILL_DONE',str(outfile))
