import bpy, math, ast, json, bmesh, os, sys, random
from pathlib import Path
from mathutils import Vector, Matrix
BASE=Path(__file__).resolve().parent
OUT=BASE/'V07_시설전체_제분소기준';OUT.mkdir(exist_ok=True)
bpy.ops.wm.open_mainfile(filepath=str(BASE/'V07_지붕지지보완'/'CozyRealm_V07_제분소_지붕과받침보완.blend'))
main=bpy.data.scenes['01_시설13종_조립검토'];bpy.context.window.scene=main
library=bpy.data.collections['00_공용모듈_원형_실제메시공유']
modules={o['원형_ID']:o for o in library.all_objects if o.type=='MESH'}
M=list(modules['W01-01'].data.materials);records=[]
for filename,names in [('build_facilities.py',{'Mesh','asset','box_asset','beam','figure','studio','loose_box'}),('revise_v07_roof_and_mill.py',{'uv_map','curved_strip'}),('close_mill_gaps.py',{'profile_asset'})]:
    for node in ast.parse((BASE/filename).read_text(encoding='utf-8')).body:
        if isinstance(node,(ast.FunctionDef,ast.ClassDef)) and node.name in names:
            exec(compile(ast.Module(body=[node],type_ignores=[]),'shared_helpers','exec'))
def newmat(name,color,rough=.8):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True
    bs=m.node_tree.nodes['Principled BSDF'];bs.inputs['Base Color'].default_value=(*color,1);bs.inputs['Roughness'].default_value=rough
    M.append(m);return len(M)-1
PINK=newmat('AT02_화과자_임시분홍',(.65,.30,.34));DARK=newmat('오븐내부_임시',(.025,.018,.012));TEA=newmat('차도구_회청색',(.26,.40,.38))
collections={c['관리_ID']:c for c in bpy.data.collections if c.get('관리_ID')}
roots={code:next(o for o in c.objects if o.type=='EMPTY') for code,c in collections.items()}
def sig(c):return {o.name:(tuple(o.location),tuple(o.rotation_euler),[tuple(v.co) for v in o.data.vertices]) for o in c.objects if o.type=='MESH'}
mill_sig=sig(collections['B02'])
for code,c in collections.items():
    if code=='B02':continue
    for ob in list(c.objects):
        if ob!=roots[code]:bpy.data.objects.remove(ob,do_unlink=True)
bpy.context.view_layer.update()
def place(code,c,loc=(0,0,0),rot=(0,0,0),name=None):
    src=modules[code];o=bpy.data.objects.new(name or src.name,src.data);c.objects.link(o)
    o.location=loc;o.rotation_euler=rot;o.parent=roots[c['관리_ID']]
    o['공용_원형']=code;o['피벗_기준']=src['피벗_기준'];o['재질_경로']=src['재질_경로'];return o
def dimkey(size):return '_'.join(str(round(s*10000)) for s in size)
def box(c,loc,size,mat=0,bevel=None,rot=(0,0,0),label='규격부재'):
    bevel=min(.025,min(size)*.15) if bevel is None else bevel
    key=f'KBOX_{mat}_{dimkey(size)}_{round(bevel*10000)}'
    if key not in modules:box_asset(key,label,size,mat,route=('TS01' if mat==0 else 'T01' if mat==2 else 'T02' if mat==3 else 'AT02'),bevel=bevel)
    return place(key,c,loc,rot)
def bar(c,a,b,section=.18,label='연결보'):
    a=Vector(a);b=Vector(b);d=b-a;key='KBAR_'+dimkey((d.length,section))
    if key not in modules:
        m=Mesh();m.box(d.length/2,0,0,d.length,section,section)
        asset(key,label,m,min(.025,section*.10),'TS01','시작 접합면 중심 / +X 길이')
    return place(key,c,a,d.to_track_quat('X','Z').to_euler())
def cyl_asset(code,name,r,h,mat=0,n=12):
    if code not in modules:
        m=Mesh();m.cylinder(0,0,0,r,h,mat,n);asset(code,name,m,min(.012,r*.10),'AT02' if mat!=0 else 'TS01')
    return code
def rod(c,a,b,r=.025,mat=0,label='원형봉'):
    a=Vector(a);d=Vector(b)-a;key=f'KROD_{mat}_{dimkey((r,d.length))}'
    cyl_asset(key,label,r,d.length,mat,10)
    return place(key,c,a,d.to_track_quat('Z','Y').to_euler())
def derived(code,factors):
    if all(abs(v-1)<1e-8 for v in factors):return code
    key=code+'_D_'+dimkey(factors)
    if key not in modules:
        m=Mesh();src=modules[code].data
        m.v=[tuple(v.co[i]*factors[i] for i in range(3)) for v in src.vertices]
        m.f=[tuple(p.vertices) for p in src.polygons];m.mi=[p.material_index for p in src.polygons]
        asset(key,'규격적용_'+code,m,0,modules[code]['재질_경로'],modules[code]['피벗_기준'])
    return key

roofs=[];frames=[];configs=[]
def roofz(x):
    t=abs(x)/2.45;return 4.04-1.75*t+.55*t*t
# 일반 시설용 박공은 하단 보 안쪽에 끝점을 두어 자체 교차가 없는 단면으로 만든다.
edge=1.96;base=2.90
gable=[(-edge,0),(edge,0)]+[(edge-2*edge*i/24,roofz(edge-2*edge*i/24)-.08-base) for i in range(25)]
profile_asset('A01_V07_시설박공채움','시설용_곡선박공채움',gable,.16,0)
def roof(c,D=4,yc=0,zoff=0,fx=1,fill=True):
    n=max(2,math.ceil((D+.6)/1.15-1e-6));L=n*1.15
    for side in [-1,1]:
        for row in range(3):
            widths=[1.15]*n if row!=1 else [.575]+[1.15]*(n-1)+[.575]
            y=yc-L/2
            for wid in widths:
                code=derived(f'R01_V07_{row}_{round(wid*1000)}',(fx,1,1))
                place(code,c,(side*row*.82*fx,y+wid/2,roofz(row*.82)+(2-row)*.065+zoff),(0,0,0 if side>0 else math.pi));y+=wid
        deck=derived('R01_V07_하부판',(fx,L/4.6,1))
        place(deck,c,(0,yc,roofz(0)-.09+zoff),(0,0,0 if side>0 else math.pi))
        edge=derived('R04_V07_곡선',(fx,1,1))
        for y in [yc-L/2-.065,yc+L/2+.065]:place(edge,c,(0,y,roofz(0)+zoff),(0,0,0 if side>0 else math.pi))
        y=yc-L/2
        while y<yc+L/2-.0001:
            seg=min(2,yc+L/2-y);bar(c,(side*2.42*fx,y,roofz(2.45)-.115+zoff),(side*2.42*fx,y+seg,roofz(2.45)-.115+zoff),.22,'처마_규격연결');y+=seg
        if fill:
            pass
    # 길이에 따른 실제 곡선 용마루 파생. 기본 460cm 원형을 재사용한다.
    ridge=derived('R03_V07_곡선',(L/4.6,1,1))
    place(ridge,c,(0,yc-L/2,4.08+zoff),(0,0,math.pi/2))
    place('R03_V07_원목끝',c,(0,yc-L/2+.05,4.30+zoff),(math.pi/2,0,0))
    place('R03_V07_캡아래목재',c,(0,yc-L/2-.12,3.87+zoff))
    place('R03_V07_끝기둥',c,(0,yc+L/2-.20,4.12+zoff))
    if fill:
        for y in [yc-D/2+.04,yc+D/2-.04]:place(derived('A01_V07_시설박공채움',(fx,1,1)),c,(0,y,2.90+zoff))
    roofs.append(dict(시설=c['관리_ID'],중심Y=yc,깊이_cm=round(D*100),널길이_cm=round(L*100),높이오프셋_cm=zoff*100,폭배율=fx))
    return L

def foundation(c,D=4):
    for x in [-2,2]:
        for y in [-D/2,D/2]:place('A04_V07_모서리기초',c,(x,y,0))
    for y in [-D/2,D/2]:
        for x in [-1.2,-.4,.4,1.2]:place('A04_V07_중간기초',c,(x,y,0))
    length=D-.8;n=math.ceil(length/.8-1e-6);seg=length/n
    code=derived('A04_V07_중간기초',(seg/.8,1,1))
    for x in [-2,2]:
        for i in range(n):place(code,c,(x,-D/2+.4+seg/2+i*seg,0),(0,0,math.pi/2))
    a=1.71;b=D/2-.29;cut=.11
    outline=[(-a+cut,-b),(a-cut,-b),(a-cut,-b+cut),(a,-b+cut),(a,b-cut),(a-cut,b-cut),(a-cut,b),(-a+cut,b),(-a+cut,b-cut),(-a,b-cut),(-a,-b+cut),(-a+cut,-b+cut)]
    key='KFLOOR_'+str(round(D*100))
    if key not in modules:
        m=Mesh();n=len(outline);v=[(x,y,z) for z in [0,.07] for x,y in outline]
        f=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        m.poly(v,f,0);asset(key,'모서리피함_보행바닥',m,0,'TS01')
    place(key,c,(0,0,.35))

def sidewall(c,x,y,window=True):
    s=1 if x>0 else -1
    place('A01_V07_상부창벽_보완' if window else 'A01_V07_상부회벽_보완',c,(x,y,1.44),(0,0,math.pi/2))
    for j in range(6):place('A01_V07_하부널_보완',c,(x+s*.10,y-.70+j*.28,.58),(0,0,math.pi/2))
    for z in [.54,1.36]:place('W02_V07_180',c,(x+s*.10,y-.9,z),(0,0,math.pi/2))
    if window:
        xp=x+s*.10
        for dy in [-.34,.34]:place('A02_V07_세로',c,(xp,y+dy,1.58),(0,0,math.pi/2))
        for z in [1.58,2.28]:place('A02_V07_가로',c,(xp,y,z),(0,0,math.pi/2))
        place('A02_V07_창살',c,(xp,y,1.67),(0,0,math.pi/2))
        place('A02_V07_창살',c,(xp,y+.31,1.98),(math.pi/2,0,0))
def backwall(c,D):
    for x in [-1,1]:
        place('A01_V07_상부회벽_보완',c,(x,D/2,1.44))
        for j in range(6):place('A01_V07_하부널_보완',c,(x-.70+j*.28,D/2+.10,.58))
        for z in [.54,1.36]:place('W02_V07_180',c,(x-.9,D/2+.10,z))
    place('A04_V07_받침',c,(0,D/2,.42));place('W01_V07_뒷중앙기둥',c,(0,D/2,.80))

def shell(c,D=4,front_curved=True,side_modes=None,roof_mode='single',annex=False):
    c['설계_폭_cm']=400;c['설계_깊이_cm']=D*100;c['규격주의']='골조 기준. 처마·계단·옆 지붕은 외곽에 추가됨.'
    foundation(c,D);ys=[-D/2+2*i for i in range(int(D/2)+1)]
    for x in [-2,2]:
        for y in ys:
            place('A04_V07_받침',c,(x,y,.42))
            usecurve=y==-D/2 and front_curved
            place('U13_V07' if usecurve else 'W01_V07_직선',c,(x,y,.80),(0,0,0 if x<0 else math.pi))
            place('W04_V07',c,(x,y,2.60))
            if usecurve:
                for z in [1,2.62]:place('W04_V07_목심',c,(x+.07*(-1 if x>0 else 1),y-.20,z),(math.pi/2,0,0))
        for y in ys[:-1]:place('W02_V07_200',c,(x,y,2.80),(0,0,math.pi/2))
    for y in [-D/2,D/2]:place('W02_V07_400',c,(-2,y,2.85))
    for x in [-2,2]:
        for i,y in enumerate(ys[:-1]):
            mode=(side_modes or {}).get((x,i),'window' if i==0 else 'solid')
            if mode in ['solid','window']:sidewall(c,x,y+1,mode=='window')
            elif mode=='door':doorbay(c,x,y+1)
    backwall(c,D)
    if roof_mode=='single':roof(c,D)
    elif roof_mode=='step':
        roof(c,4,-2,0);roof(c,4,2,.30)
        # 높은 숙박동의 지붕을 받치는 상부 띠와 기둥 연장.
        for x in [-2,2]:
            for y in [0,2,4]:box(c,(x,y,2.78),(.30,.30,.37),label='기둥상부_연장')
            for y in [1,3]:box(c,(x,y,2.78),(.24,1.74,.30),label='높은동_상부막음')
            for y in [0,2]:place('W02_V07_200',c,(x,y,3.10),(0,0,math.pi/2))
        for y in [0,4]:place('W02_V07_400',c,(-2,y,3.15))
        box(c,(0,4,2.78),(3.7,.16,.30),label='높은동_뒷벽상부')
        box(c,(0,4,2.78),(.34,.34,.37),label='뒷중앙기둥_연장')
    if annex:add_annex(c)
    frames.append({'시설':c['관리_ID'],'깊이_m':D,'측면기둥_Y':ys,'앞기둥_휜원형':front_curved})

def add_annex(c,yoffset=0):
    for y in [.55,1.85]:
        place('A04_V07_기초',c,(2.72,y+yoffset,0),(0,0,math.pi/2))
        place('A04_V07_받침',c,(2.75,y+yoffset,.42));place('W01_V07_작은지붕기둥',c,(2.75,y+yoffset,.80))
    for j in range(3):place('R01_V07_옆지붕',c,(1.90,.65+j*.55+yoffset,2.55))
    place('R02_V07_옆처마',c,(2.75,.375+yoffset,2.12),(0,0,math.pi/2))
    place('R02_V07_벽붙임보',c,(1.97,.375+yoffset,2.48),(0,0,math.pi/2))
    for y in [.55,1.20,1.85]:place('R04_V07_작은서까래',c,(1.97,y+yoffset,2.48),Vector((.78,0,-.36)).to_track_quat('X','Z').to_euler())
    for y in [.55,1.85]:
        place('W03_V07_작은버팀',c,(2.75,y+yoffset,1.76),Vector((-.35,0,.52)).to_track_quat('X','Z').to_euler())
        place('A01_V07_작은박공',c,(1.97,y+yoffset,2.10))

# 문은 문짝 원형을 따로 두어 경첩 하단 피벗을 유지한다.
m=Mesh();m.box(.49,0,.29,.98,.07,.58)
for x in [.03,.95]:m.box(x,0,.89,.06,.09,1.78)
for z in [.60,1.74]:m.box(.49,0,z,.98,.09,.07)
for i in range(7):m.box(.14+i*.116,0,1.15,.025,.055,1.10)
for z in [.95,1.35]:m.box(.49,-.035,z,.90,.04,.025)
asset('A03_V07_격자문짝','나가야_격자문짝',m,.006,'TS01','좌측 경첩 하단 / +X 폭')
def doorbay(c,x,y):
    # 200cm 반복 구간 안의 문과 옆 채움. 실제 통과 폭은 100cm.
    s=1 if x>0 else -1
    for yy in [y-.70,y+.70]:box(c,(x,yy,.42),(.14,.30,2.36),3,.008,label='문옆_채움벽')
    box(c,(x,y,2.32),(.14,1,.46),3,.008,label='문위_채움벽')
    place('A03-01',c,(x+s*.10,y,.42),(0,0,math.pi/2))
    place('A03_V07_격자문짝',c,(x+s*.105,y-.49,.43),(0,0,math.pi/2))
    for j,h in enumerate([.28,.14]):box(c,(x+s*(.25+j*.35),y,0),(1.2,.35,h),2,.015,(0,0,math.pi/2),'출입_계단')

# 기능품과 시설별 조립은 같은 전역 공용 모듈 함수를 사용한다.
exec(compile((BASE/'facility_props.py').read_text(encoding='utf-8'),'facility_props.py','exec'))
exec(compile((BASE/'facility_assemblies.py').read_text(encoding='utf-8'),'facility_assemblies.py','exec'))

# 원형은 한 군데에 보관하고 시설 배치는 동일 Mesh 데이터를 공유한다.
newcol=bpy.data.collections.new('08_V07_시설확장_공용원형');library.children.link(newcol)
for i,r in enumerate(records):
    o=modules[r['ID']]
    if len(o.data.polygons)>1:
        bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free();o.data.update()
    if o.name in library.objects:library.objects.unlink(o)
    newcol.objects.link(o);o.location=((i%10)*5,100+(i//10)*5,0)
# 실제 사용 원형과 이전 검토용 원형을 구분한다. 메시 공유는 그대로 유지된다.
usage={}
for code,c in collections.items():
    for o in c.objects:
        if o.get('공용_원형'):usage.setdefault(o['공용_원형'],set()).add(code)
current=bpy.data.collections.new('00_현재사용_공용원형');legacy=bpy.data.collections.new('99_이전검토원형_사용안함')
library.children.link(current);library.children.link(legacy);legacy.hide_viewport=True;legacy.hide_render=True
for i,(code,o) in enumerate(modules.items()):
    for owner in list(o.users_collection):owner.objects.unlink(o)
    (current if code in usage else legacy).objects.link(o)
    o['사용시설']=', '.join(sorted(usage.get(code,[])))
active=[o for o in current.objects if o.type=='MESH']
for i,o in enumerate(active):o.location=((i%10)*5,(i//10)*5,0)
for scene in bpy.data.scenes:
    if scene.name=='02_공용모듈_원형모음':
        for o in list(scene.collection.objects):
            if o.type=='FONT':bpy.data.objects.remove(o,do_unlink=True)
        if scene.camera:
            target=Vector((22.5,(len(active)//10)*2.5,0));scale=max(60,len(active)/10*5.7)
            scene.camera.location=target+Vector((scale*.7,-scale,scale*.57));scene.camera.rotation_euler=(target-scene.camera.location).to_track_quat('-Z','Y').to_euler();scene.camera.data.ortho_scale=scale
main['AI_제안']='제분소의 승인된 두께감을 사용한 시설별 로우폴 제작 제안. 방 수·배치·치수는 최종 확정 전.'
main['시설_시안기준']='Art/TripoInputs/v7_AB/Bxx-완성_A_*.png'
main['현재파일']='시설13종: 제분소 유지 + 나머지12종 재구성. 스컬프·최종재질·베이크·엔진 검증 미완료.'
statistics=[]
for code,c in sorted(collections.items()):
    placed=[o for o in c.objects if o.type=='MESH' and o.get('공용_원형')]
    tris=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in placed)
    statistics.append({'ID':code,'이름':c.name,'배치수':len(placed),'실제삼각형수':tris,'사용원형수':len(set(o['공용_원형'] for o in placed))})
    c['공용메시_공유']=True;c['확정상태']='外形提案'.replace('外形提案','외형 제안')+' / 게임 카메라·최종치수 검토 전'
    c['삼각형수_조립합계']=tris
    c.asset_mark()

# 원형 표시용 새 장면: 확장 원형도 실제 자산으로 편집할 수 있다.
kit=bpy.data.scenes.new('06_시설확장_공용원형');kit.collection.children.link(current)
kit.unit_settings.system='METRIC';kit.unit_settings.length_unit='CENTIMETERS'
scenes=[]
for code,c in sorted(collections.items()):
    if code=='B02':continue
    scene=bpy.data.scenes.new(code+'_'+c.name[4:]+'_단독검토')
    scene.collection.children.link(c);scene.unit_settings.system='METRIC';scene.unit_settings.length_unit='CENTIMETERS'
    scene.render.engine='CYCLES';scene.world=bpy.data.scenes['03_제분소_140cm비율검토'].world
    scene.view_settings.view_transform='AgX';scene.cycles.use_denoising=True
    pos=roots[code].location;size=12.4 if code=='B12' else 10.6 if code=='B04' else 9.8 if code=='B09' else 9.0 if code not in ['B07','B13'] else 8.4
    target=(pos.x+.35,pos.y,1.9 if code not in ['B07','B13'] else .4)
    # 참고 이미지와 같은 방향에서 주요 기능을 볼 수 있도록 단독 카메라 배치.
    studio(scene,target,size,(1600,1400));scene.cycles.samples=24
    cam=scene.camera;cam.location=Vector(target)+Vector((size*.7,-size,size*(.56 if code in ['B07','B13'] else .32)));cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
    if code in ['B04','B12']:
        cam=scene.camera;cam.location=Vector(target)+Vector((-size*.68,-size, size*.32));cam.rotation_euler=(Vector(target)-cam.location).to_track_quat('-Z','Y').to_euler()
    scenes.append((code,scene))
detail=next(s for code,s in scenes if code=='B03');bpy.context.window.scene=detail
for screen in bpy.data.screens:
    for a in screen.areas:
        if a.type=='VIEW_3D':
            a.spaces.active.region_3d.view_location=roots['B03'].location+Vector((0,0,1.8));a.spaces.active.region_3d.view_distance=9
            a.spaces.active.region_3d.view_rotation=detail.camera.rotation_euler.to_quaternion();a.spaces.active.region_3d.view_perspective='ORTHO'
read=bpy.data.texts.new('시설전체_사용안내');read.write('제분소를 유지하고 다른 12종 시설을 V07 A 시안의 특징에 따라 재구성했습니다.\n각 Bxx Collection의 조립 원점은 바닥 중심입니다. 공용 원형은 라이브러리 08 하위에 있습니다.\nAlt+D로 같은 원형을 연결 복제합니다. 편집 모드 수정은 동일 Mesh를 공유하는 모든 시설에 반영됩니다.\n모든 치수·배치는 주인공140cm 외 제안값입니다. 원형의 최종 UV·베이크·재질·LOD·충돌 미완료.\n밭과 차 잎 카드는 임시 공용 아틀라스를 포함하며 최종 아트 텍스처가 아닙니다.\n')
bpy.context.view_layer.update()
assert mill_sig==sig(collections['B02']),'제분소 메시 또는 배치 변경 감지'
outblend=OUT/'CozyRealm_V07_시설13종_공용모듈_140cm.blend'
bpy.ops.wm.save_as_mainfile(filepath=str(outblend))
(OUT/'시설별_제작목록.json').write_text(json.dumps({'시설':statistics,'형태구성':configs,'지붕규격':roofs,'추가원형':records},ensure_ascii=False,indent=2),encoding='utf-8')
print('FACILITIES_SAVED',len(statistics),len(records),flush=True)
if '--no-render' in sys.argv:
    print('GEOMETRY_ONLY_DONE',flush=True)
    raise SystemExit(0)
for code,scene in scenes:
    scene.render.filepath=str(OUT/(code+'_'+collections[code].name[4:]+'_모델.png'));bpy.ops.render.render(write_still=True,scene=scene.name)
    print('RENDER_DONE',code,flush=True)
# 전체 전시 장면의 기존 카메라는 보존하고 출력만 추가한다.
main.render.resolution_x=2000;main.render.resolution_y=1700;main.cycles.samples=24
main.render.filepath=str(OUT/'시설13종_전체배치.png');bpy.ops.render.render(write_still=True,scene=main.name)
print('ALL_FACILITIES_DONE',str(outblend),flush=True)
