import bpy, math, json, csv, os
from mathutils import Vector
from pathlib import Path

OUT = Path(__file__).resolve().parent
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
main = bpy.context.scene
main.name = '01_시설13종_조립검토'

def setup(scene):
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0
    scene.unit_settings.length_unit = 'CENTIMETERS'
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = 24
    scene.render.resolution_percentage = 100
    scene.world = bpy.data.worlds.new(scene.name + '_환경')
    scene.world.use_nodes = True
    scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.32,.36,.41,1)
    scene.world.node_tree.nodes['Background'].inputs[1].default_value = .45
    scene.view_settings.view_transform = 'AgX'
setup(main)

def material(name, color, rough=.8):
    m = bpy.data.materials.new(name); m.diffuse_color = (*color,1); m.use_nodes = True
    bs = m.node_tree.nodes.get('Principled BSDF'); bs.inputs['Base Color'].default_value = (*color,1); bs.inputs['Roughness'].default_value = rough
    return m
M = [material('TS01_木材_仮色',(.32,.14,.064)), material('TS01_지붕_색변형',(.24,.29,.22)),
     material('T01_석재_임시색',(.39,.37,.33)), material('T02_회벽_임시색',(.76,.69,.54)),
     material('AT02_천_임시색',(.34,.40,.48)), material('AT02_금속_임시색',(.15,.18,.19),.5),
     material('T03_흙_임시색',(.23,.16,.09)), material('AT01_잎카드_임시색',(.22,.36,.10)),
     material('기준인체_140cm',(.19,.50,.65)), material('안내문자',(.12,.16,.20)),
     material('AT02_종이_임시색',(.85,.75,.51)), material('AT02_음식_임시색',(.65,.40,.15))]
M[0].name = 'TS01_목재_임시색'
modules = {}
library = bpy.data.collections.new('00_공용모듈_원형_실제메시공유')
records = []

class Mesh:
    def __init__(self): self.v=[]; self.f=[]; self.mi=[]
    def poly(self, verts, faces, mat):
        k=len(self.v); self.v.extend(verts); self.f.extend([tuple(k+i for i in f) for f in faces]); self.mi.extend([mat]*len(faces))
    def box(self, x,y,z, sx,sy,sz,mat=0):
        v=[(x+dx*sx/2,y+dy*sy/2,z+dz*sz/2) for dx,dy,dz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        self.poly(v,[(0,3,2,1),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7),(4,5,6,7)],mat)
    def sweep(self, centers, widths, depths, mat=0):
        verts=[]
        for (x,y,z),w,d in zip(centers,widths,depths): verts.extend([(x-w/2,y-d/2,z),(x+w/2,y-d/2,z),(x+w/2,y+d/2,z),(x-w/2,y+d/2,z)])
        faces=[(3,2,1,0)]
        for i in range(len(centers)-1):
            for j in range(4): faces.append((4*i+j,4*i+(j+1)%4,4*(i+1)+(j+1)%4,4*(i+1)+j))
        faces.append(tuple(4*(len(centers)-1)+j for j in range(4)))
        self.poly(verts,faces,mat)
    def cylinder(self,x,y,z,r,depth,mat=0,n=12):
        vs=[(x+r*math.cos(a*2*math.pi/n),y+r*math.sin(a*2*math.pi/n),z+h) for h in [0,depth] for a in range(n)]
        fs=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        self.poly(vs,fs,mat)

def uv_map(obj):
    me=obj.data; uv=me.uv_layers.new(name='UV0_공용재질_배치안')
    if '고유' in obj.get('재질_경로','') or '개별' in obj.get('재질_경로',''):
        main.collection.objects.link(obj)
        bpy.context.view_layer.objects.active=obj
        obj.select_set(True)
        bpy.ops.object.mode_set(mode='EDIT');bpy.ops.mesh.select_all(action='SELECT')
        bpy.ops.uv.smart_project(island_margin=.035)
        bpy.ops.object.mode_set(mode='OBJECT');obj.select_set(False)
        uv.name='UV0_개별베이크_초안'
        main.collection.objects.unlink(obj)
        return
    dims=[max(v.co[i] for v in me.vertices)-min(v.co[i] for v in me.vertices) for i in range(3)]
    long_axis=max(range(3),key=lambda i:dims[i])
    for p in me.polygons:
        normal_axis=max(range(3),key=lambda i:abs(p.normal[i])); axes=[i for i in range(3) if i!=normal_axis]
        if p.material_index in (0,1):
            if normal_axis==long_axis:
                for li in p.loop_indices:
                    co=me.vertices[me.loops[li].vertex_index].co
                    uv.data[li].uv=(.80+((co[axes[0]]/.4)%1)*.16,.80+((co[axes[1]]/.4)%1)*.16)
            else:
                a=long_axis; b=next(i for i in axes if i!=a)
                vals=[me.vertices[me.loops[li].vertex_index].co[b] for li in p.loop_indices]; lo=min(vals); span=max(vals)-lo or 1
                band=.08 if p.material_index==0 else .40
                for li in p.loop_indices:
                    co=me.vertices[me.loops[li].vertex_index].co; uv.data[li].uv=(co[a]/1.0,band+(co[b]-lo)/span*.20)
        else:
            for li in p.loop_indices:
                co=me.vertices[me.loops[li].vertex_index].co; uv.data[li].uv=(co[axes[0]],co[axes[1]])

def asset(code,name,mesh,bevel=.008,route='TS01',pivot='밑면 중심'):
    me=bpy.data.meshes.new(code+'_공유메시'); me.from_pydata(mesh.v,[],mesh.f); me.update()
    ob=bpy.data.objects.new(code+'_'+name,me); library.objects.link(ob)
    for mat in M: me.materials.append(mat)
    for p,mi in zip(me.polygons,mesh.mi): p.material_index=mi
    if bevel:
        mod=ob.modifiers.new('큰 모서리_1분할','BEVEL'); mod.width=bevel; mod.segments=1
        mod.affect='EDGES'
        # 임시로 현재 씬에 링크하여 모디파이어를 실제 메시로 적용한다.
        main.collection.objects.link(ob)
        bpy.context.view_layer.objects.active=ob
        bpy.ops.object.modifier_apply(modifier=mod.name)
        main.collection.objects.unlink(ob)
    me=ob.data
    ob['원형_ID']=code; ob['재질_경로']=route; ob['피벗_기준']=pivot; ob['제작_상태']='로우폴 조립 검토 / 하이폴·베이크·최종 텍스처 미완료'
    uv_map(ob)
    ob.asset_mark()
    modules[code]=ob
    dims=[round(v*100,2) for v in ob.dimensions]
    records.append(dict(ID=code,이름=name,크기_cm=dims,피벗=pivot,재질=route,삼각형=sum(len(p.vertices)-2 for p in me.polygons)))
    return ob
def box_asset(code,name,size,mat=0,pivot='밑면 중심',route='TS01',bevel=.008):
    m=Mesh(); m.box(0,0,size[2]/2,*size,mat); return asset(code,name,m,bevel,route,pivot)
def beam(code,name,L,section=.18):
    m=Mesh(); m.box(L/2,0,0,L,section,section); return asset(code,name,m,.008,'TS01','시작 접합면 중심 / +X 길이')

box_asset('W01-01','기본기둥_220',(.18,.18,2.2))
box_asset('W01-02','짧은기둥_100',(.18,.18,1.0))
for code,L in [('W02-01',1),('W02-02',2),('W02-03',4)]: beam(code,'직선보_'+str(int(L*100)),L)
m=Mesh(); m.sweep([(0,0,0),(.12,0,.20),(.30,0,.42),(.55,0,.60),(.80,0,.68)],[.16]*5,[.16]*5)
asset('W03','휜버팀보_U14',m,.015,'고유 UV·베이크 예정','하부 접합점')
box_asset('W04-01','접합블록',(.24,.24,.24))
box_asset('W04-02','끝캡',(.22,.22,.06))
for code,L in [('R01-01',1.50),('R01-02',.75),('R01-03',.40)]:
    m=Mesh(); m.box(L/2,0,.035,L,.50,.07,1); asset(code,'지붕널_'+str(int(L*100)),m,.012,'TS01 지붕 색변형','시작면 / +X 경사방향')
beam('R02','처마_200',2,.16)
beam('R03-01','용마루_200',2,.22)
box_asset('R03-02','용마루끝',(.26,.22,.26),1,route='TS01')
beam('R04','박공마감_140',1.4,.14)
for code,w in [('A01-01',1.82),('A01-02',.82)]:
    m=Mesh(); m.box(0,0,1.0,w,.10,2.0,3); m.box(0,-.07,.10,w,.08,.18);m.box(0,-.07,1.90,w,.08,.18)
    asset(code,'회벽패널_'+str(int(w*100)),m,.006,'TS01+T02')
# 실제 구멍을 가진 창 벽: 개구부 70×80cm, 벽 밑에서 80cm.
m=Mesh();m.box(-.655,0,1,.51,.10,2,3);m.box(.655,0,1,.51,.10,2,3);m.box(0,0,.4,.80,.10,.8,3);m.box(0,0,1.8,.80,.10,.4,3)
asset('A01-03','창개구부벽_182',m,.004,'T02')
m=Mesh()
for x in [-.39,.39]:m.box(x,0,.40,.08,.16,.80)
for z in [.04,.76]:m.box(0,0,z,.86,.16,.08)
for x in [-.20,0,.20]:m.box(x,0,.40,.035,.07,.64)
asset('A02','창틀_창살_86x80',m,.004)
m=Mesh()
for x in [-.55,.55]:m.box(x,0,.95,.10,.18,1.90)
m.box(0,0,1.85,1.20,.18,.10)
asset('A03-01','문틀_유효100x180',m,.006)
m=Mesh()
for i in range(5):m.box(.10+i*.196,0,.89,.19,.055,1.78)
for z in [.23,1.52]:m.box(.49,-.045,z,.98,.045,.10)
asset('A03-02','문짝_98x178',m,.004,'TS01','좌측 경첩 밑 / +X 폭')
box_asset('A04-01','기둥받침석_30',(.30,.30,.30),2,route='T01',bevel=.025)
box_asset('A04-02','둘레기초석_100', (1,.40,.30),2,route='T01',bevel=.022)
box_asset('A04-03','계단판_100', (1,.35,.15),2,route='T01',bevel=.012)
# 실제 조립에 필요한 추가 원형. 초기 24변형과 구분한다.
m=Mesh();m.sweep([(0,0,0),(-.10,0,.55),(-.20,0,1.1),(-.16,0,1.65),(0,0,2.2)],[.24,.24,.22,.20,.20],[.20]*5)
asset('U13','대표휜기둥_220',m,.018,'고유 UV·베이크 예정')
beam('X01','가구널_100',1,.08)
box_asset('X02','가구다리_75',(.08,.08,.75))
box_asset('X03','상판널_120', (1.2,.20,.06))
# 문 개구부를 보존하는 추가 채움벽.
m=Mesh();m.box(-.705,0,1,.41,.10,2,3);m.box(.705,0,1,.41,.10,2,3);m.box(0,0,1.95,1,.10,.10,3)
asset('X04','문개구부벽_182',m,.004,'T02')
# 경사 30도에서 쓰는 지붕 밑 박공 채움. 실제 삼각형 단면.
m=Mesh();m.poly([(-2,0,0),(2,0,0),(0,0,1.155),(-2,.10,0),(2,.10,0),(0,.10,1.155)],[(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],3)
asset('X05','박공채움_400',m,.0,'T02','박공 하단 중심')
# 시설 전용 특징 부재·기능품: 공용 건축 24개와 별도 Collection으로 분류한다.
m=Mesh();m.sweep([(0,0,0),(.4,0,.12),(.8,0,.35),(1.2,0,.65),(1.6,0,1.05),(2,0,1.4)],[.20]*6,[.18]*6)
asset('U15','신사곡선부재',m,.012,'고유 UV·베이크 예정')
for code,z in [('U16-하',.24),('U16-상',.22)]:
    m=Mesh();m.cylinder(0,0,0,.55,z,2,16);asset(code,'맷돌',m,.015,'고유 UV·베이크 예정')
m=Mesh();m.sweep([(0,0,0),(0,0,.55)],[.18,.90],[.18,.90]);asset('P05-호퍼','곡물호퍼',m,.006)
box_asset('P05-축','맷돌축',(.08,.08,.62))
m=Mesh();m.box(0,0,.8,.06,.06,1.6,5);m.box(.48,0,.25,.96,.06,.06,5);asset('P05-손잡이','회전손잡이',m,.003)
m=Mesh();m.box(0,0,.10,.8,.42,.20);asset('S04-서랍','씨앗함서랍',m,.004)
m=Mesh();m.cylinder(0,0,0,.12,.13,10,12);asset('P12-찻잔','찻잔_블록아웃',m,.004,'개별 UV 예정')
m=Mesh();m.cylinder(0,0,0,.15,.12,11,10);asset('P08-빵','빵_블록아웃',m,.015,'개별 UV 예정')
# 오븐은 앞쪽 실제 개구부를 남긴 분리 블록 조립 원형.
m=Mesh();m.box(-.48,0,.42,.26,1,.84,2);m.box(.48,0,.42,.26,1,.84,2);m.box(0,.40,.42,.70,.20,.84,2);m.box(0,0,.88,1.22,1,.18,2);m.box(.35,.18,1.20,.26,.26,.50,2)
asset('B08-오븐','오븐_입구분리',m,.025,'개별 UV 예정')
box_asset('B11-바이스','바이스_블록아웃',(.28,.20,.25),5,route='개별 UV 예정')
m=Mesh();centers=[(-.95,0,.18),(-.5,0,.05),(0,0,0),(.5,0,.05),(.95,0,.18)]
verts=[]
for i,c in enumerate(centers):
    tangent=Vector(centers[min(i+1,4)])-Vector(centers[max(i-1,0)]);tangent.normalize();a=Vector((0,1,0));b=tangent.cross(a)
    for j in range(8):verts.append(Vector(c)+.035*(a*math.cos(j*math.pi/4)+b*math.sin(j*math.pi/4)))
faces=[tuple(reversed(range(8))),tuple(range(32,40))]
for i in range(4):
    for j in range(8):faces.append((i*8+j,i*8+(j+1)%8,(i+1)*8+(j+1)%8,(i+1)*8+j))
m.poly(verts,faces,10);asset('B01-금줄','신사금줄',m,0,'개별 UV 예정','중앙 최저점')
m=Mesh();profile=[(-.06,0),(.06,0),(.02,-.13),(.09,-.20),(.01,-.35),(-.08,-.28),(-.02,-.15),(-.09,-.08)]
v=[(x,y,z) for y in [-.006,.006] for x,z in profile];n=len(profile)
f=[tuple(reversed(range(n))),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
m.poly(v,f,10);asset('B01-종이장식','신사종이장식',m,0,'개별 UV 예정','상단 매듭 중심')

stats=[]; facility_collections=[]; derived={}
def put(code,col,loc=(0,0,0),rot=(0,0,0),scale=(1,1,1),name=None):
    base=code
    if tuple(scale)!=(1,1,1):
        key=(code,tuple(round(s,6) for s in scale))
        if key not in derived:
            source=modules[code];vcode=code+'_V'+str(len(derived)+1).zfill(2)
            me=source.data.copy();me.name=vcode+'_치수적용_공유메시'
            for v in me.vertices:
                for ax in range(3):v.co[ax]*=scale[ax]
            me.update();ob=bpy.data.objects.new(vcode+'_규격파생',me);library.objects.link(ob)
            for k in source.keys():ob[k]=source[k]
            ob['원형_ID']=vcode;ob['파생_기준']=code
            me.uv_layers.remove(me.uv_layers.active);uv_map(ob)
            ob.asset_mark();modules[vcode]=ob;derived[key]=vcode
            dims=[round((max(v.co[a] for v in me.vertices)-min(v.co[a] for v in me.vertices))*100,3) for a in range(3)]
            records.append(dict(ID=vcode,이름='치수적용 파생',크기_cm=dims,피벗=source['피벗_기준'],재질=source['재질_경로'],삼각형=sum(len(p.vertices)-2 for p in me.polygons),파생기준=code))
        code=derived[key]
    source=modules[code];o=bpy.data.objects.new(name or source.name,source.data);col.objects.link(o);o.location=loc;o.rotation_euler=rot
    o['공용_원형']=code;o['분류_기준']=base;o['피벗_기준']=source['피벗_기준'];o['재질_경로']=source['재질_경로'];return o
def text(col,body,loc,size=.22):
    c=bpy.data.curves.new('안내','FONT');c.body=body;c.size=size;c.align_x='CENTER';c.extrude=0
    c.font=bpy.data.fonts.get('맑은 고딕') or bpy.data.fonts.load('C:/Windows/Fonts/malgun.ttf')
    c.font.name='맑은 고딕'
    o=bpy.data.objects.new(body,c);col.objects.link(o);o.location=loc;c.materials.append(M[9]);return o
def loose_box(col,name,loc,size,mat):
    m=Mesh();m.box(0,0,size[2]/2,*size,mat);me=bpy.data.meshes.new(name);me.from_pydata(m.v,[],m.f);me.materials.append(M[mat]);ob=bpy.data.objects.new(name,me);col.objects.link(ob);ob.location=loc;return ob
def figure(col,loc):
    # 고정 140cm 비교 인체. 전체 최대 Z가 정확히 1.4m.
    m=Mesh();m.box(-.085,0,.31,.12,.16,.62,8);m.box(.085,0,.31,.12,.16,.62,8);m.box(0,0,.91,.32,.18,.58,8);m.box(0,0,1.30,.22,.22,.20,8)
    m.box(-.22,0,.92,.10,.14,.48,8);m.box(.22,0,.92,.10,.14,.48,8)
    me=bpy.data.meshes.get('기준인체_정확히140cm')
    if me is None:
        me=bpy.data.meshes.new('기준인체_정확히140cm');me.from_pydata(m.v,[],m.f);[me.materials.append(x) for x in M];me.update()
        for p in me.polygons:p.material_index=8
    o=bpy.data.objects.new('주인공_높이140cm_비교용',me);col.objects.link(o);o.location=loc;o['확정_사용자_키_cm']=140;return o
def furniture(col,kind,pos):
    x,y,z=pos
    H=.75 if kind=='작업대' else .85 if kind=='카운터' else 1.40
    for dx in [-.52,.52]:
        for dy in [-.22,.22]:put('X02',col,(x+dx,y+dy,z),scale=(1,1,H/.75))
    levels=[H-.06] if kind!='선반' else [.25,.78,1.34]
    for h in levels:
        for dy in [-.2,0,.2]:put('X03',col,(x,y+dy,z+h))
    if kind=='카운터':
        for i in range(6):put('X03',col,(x-.5+i*.20,y-.27,z+.40),(0,math.pi/2,math.pi/2),scale=(.63,1,1))
def ground_frame(col,w,d):
    for x in [-w/2,w/2]:
        for y in [-d/2,d/2]:put('W04-01',col,(x,y,0))
    for y in [-d/2,d/2]:
        for i in range(int(w)):put('W02-01',col,(-w/2+i,y,.14))
    for x in [-w/2,w/2]:
        for i in range(int(d)):put('W02-01',col,(x,-d/2+i,.14),(0,0,math.pi/2))
def plants(col,w,d,tea=False):
    # 얇은 면의 임시 카드. 최종 알파 텍스처는 미제작.
    me=bpy.data.meshes.get('차잎카드_임시' if tea else '밀카드_임시')
    if not me:
        me=bpy.data.meshes.new('차잎카드_임시' if tea else '밀카드_임시')
        me.from_pydata([(-.25,0,0),(.25,0,0),(.22,0,.65),(-.22,0,.65),(0,-.25,0),(0,.25,0),(0,.22,.65),(0,-.22,.65)],[],[(0,1,2,3),(4,5,6,7)]);me.materials.append(M[7]);me.uv_layers.new(name='UV0_카드_임시')
    for i in range(int(w/.6)):
        for j in range(int(d/.6)):
            o=bpy.data.objects.new('차나무_잎카드_임시' if tea else '밀_교차카드_임시',me);col.objects.link(o);o.location=(-w/2+.4+i*.6,-d/2+.4+j*.6,.10)
def building(code,name,w=4,d=4,closed=False,curved=True,kind=None):
    col=bpy.data.collections.new(code+'_'+name);main.collection.children.link(col);facility_collections.append(col)
    if code in ['B07','B13']:
        ground_frame(col,w,d);loose_box(col,'토양_분리',(-0.,0,0),(w-.2,d-.2,.10),6);plants(col,w-.3,d-.3,code=='B13');figure(col,(-w/2-.65,-d/2,0))
    else:
        floor=.30;posttop=2.50
        # 기초석 단위 길이 100cm. 출입 정면 중앙은 계단으로 연결한다.
        for y in [-d/2,d/2]:
            for i in range(int(w)):put('A04-02',col,(-w/2+.5+i,y,0))
        for x in [-w/2,w/2]:
            for i in range(int(d)):put('A04-02',col,(x,-d/2+.5+i,0),(0,0,math.pi/2))
        loose_box(col,'보행바닥_분리',(0,0,.23),(w-.40,d-.40,.07),0)
        for j in range(2):put('A04-03',col,(0,-d/2-.35*(j+1),.15 if j==0 else 0))
        xs=[-w/2+2*i for i in range(int(w/2)+1)]; ys=[-d/2+2*i for i in range(int(d/2)+1)]
        for x in xs:
            for y in [-d/2,d/2]:
                if y<0 and w==4 and x==0:continue
                put('A04-01',col,(x,y,0));front=y<0;usecurve=curved and front
                put('U13' if usecurve else 'W01-01',col,(x,y,floor),(0,0,0 if x<0 else math.pi))
        for x in [-w/2,w/2]:
            for y in ys[1:-1]:put('A04-01',col,(x,y,0));put('W01-01',col,(x,y,floor))
        for y in [-d/2,d/2]:
            if y<0 and w==4:put('W02-03',col,(-w/2,y,posttop))
            else:
                for x in xs[:-1]:put('W02-02',col,(x,y,posttop))
        for x in [-w/2,w/2]:
            for y in ys[:-1]:put('W02-02',col,(x,y,posttop),(0,0,math.pi/2))
        for x in xs[:-1]:
            put('W02-02',col,(x,d/2,.39))
            if closed:put('W02-02',col,(x,-d/2,.39))
        for x in [-w/2,w/2]:
            for y in ys[:-1]:put('W02-02',col,(x,y,.39),(0,0,math.pi/2))
        # 후벽·양측 벽은 표준 개구부 패널 반복. 앞쪽은 폐쇄 시설만 문 구간.
        for x in xs[:-1]:put('A01-01',col,(x+1,d/2,floor+.1))
        for x in [-w/2,w/2]:
            for y in ys[:-1]:
                a=math.pi/2
                put('A01-03',col,(x,y+1,floor+.1),(0,0,a));put('A02',col,(x,y+1,floor+.9),(0,0,a))
        if code=='B01':
            for x in [-1,1]:put('A04-01',col,(x,-d/2,0));put('W01-01',col,(x,-d/2,floor))
            for x in [-1.5,1.5]:put('A01-02',col,(x,-d/2,floor+.1))
            put('X04',col,(0,-d/2,floor+.1));put('A03-01',col,(0,-d/2-.025,floor));put('A03-02',col,(-.49,-d/2-.04,floor+.01))
            put('B01-금줄',col,(0,-d/2-.20,2.2))
            for x in [-.6,.6]:put('B01-종이장식',col,(x,-d/2-.20,2.25))
        elif closed:
            for i,x in enumerate(xs[:-1]):
                center=x+1
                put('X04',col,(center,-d/2,floor+.1));put('A03-01',col,(center,-d/2-.025,floor))
                put('A03-02',col,(center-.49,-d/2-.04,floor+.01))
        else:
            # 공간을 막지 않는 코너 버팀보. +X 방향과 반대 방향을 회전으로 재사용.
            put('W03',col,(-w/2,-d/2,1.70));put('W03',col,(w/2,-d/2,1.70),(0,0,math.pi))
        # 지붕 30도. 널은 0.50m 폭·1.40m 유효 경사 길이로 겹쳐 조립한다.
        roofbase=2.58; slope=math.radians(30);ridge=roofbase+(w/2)*math.tan(slope)
        for y in [-d/2-.10,d/2]:put('X05',col,(0,y,roofbase),scale=(w/4,1,w/4))
        nrows=math.ceil((w/2+.35)/math.cos(slope)/1.40)
        for side in [-1,1]:
            for row in range(nrows):
                length=1.50 if row<nrows-1 else ((w/2+.35)/math.cos(slope)-(nrows-1)*1.40)
                for j in range(int((d+.8)/.5)+1):
                    dist=row*1.40
                    put('R01-01',col,(side*dist*math.cos(slope),-d/2-.4+j*.5,ridge-dist*math.sin(slope)+(nrows-1-row)*.035),(0,slope,0 if side==1 else math.pi),scale=(length/1.50,1,1))
        ridge_length=d+.8
        for y in range(math.ceil(ridge_length/2)):
            put('R03-01',col,(0,-d/2-.4+2*y,ridge+.10),(0,0,math.pi/2),scale=(min(2,ridge_length-2*y)/2,1,1))
        for y in [-d/2-.45,d/2+.5]:put('R03-02',col,(0,y,ridge))
        for side in [-1,1]:
            for y in ys[:-1]:put('R02',col,(side*(w/2+.35),y,roofbase-.20),(0,0,math.pi/2))
            for y in [-d/2-.40,d/2+.40]:
                length=(w/2+.35)/math.cos(slope)
                for t in range(math.ceil(length/1.4)):
                    dist=t*1.4;ll=min(1.4,length-dist)
                    put('R04',col,(side*dist*math.cos(slope),y,ridge-dist*math.sin(slope)),(0,slope,0 if side==1 else math.pi),scale=(ll/1.4,1,1))
        if code=='B01':
            for x in [-w/2,w/2]:
                for y in [-d/2-.5,d/2+.5]:put('U15',col,(x,y,2.25),(0,0,0 if x<0 else math.pi))
            furniture(col,'카운터',(0,-.9,floor))
        if code=='B02':
            for z,c in [(floor,'U16-하'),(floor+.26,'U16-상')]:put(c,col,(-.65,-.80,z))
            put('P05-축',col,(-.65,-.80,floor+.44));put('P05-호퍼',col,(-.65,-.80,1.2));put('P05-손잡이',col,(-.65,-.80,floor+.45))
        if kind=='카운터':furniture(col,'카운터',(0,-1.25,floor))
        if kind=='작업대':furniture(col,'작업대',(0,-.8,floor))
        if code in ['B08','B09','B10','B11']:furniture(col,'선반',(1,1.3,floor))
        if code=='B06':
            furniture(col,'선반',(0,.5,floor))
            for z in [.4,.7,1.0]:put('S04-서랍',col,(0,.3,z))
        if code=='B08':
            put('B08-오븐',col,(1.1,-.6,floor));put('P08-빵',col,(-.8,-1.2,1.15))
        if code=='B10':
            for x in [-.35,0,.35]:put('P08-빵',col,(x,-1.25,1.15))
        if code=='B11':put('B11-바이스',col,(.35,-.8,1.05))
        if code=='B12':
            furniture(col,'작업대',(-2,-.8,floor))
            for x in [-2.2,-1.8]:put('P12-찻잔',col,(x,-.8,1.05))
        if code in ['B03','B10','B12']:
            # 차양은 별도 천 메시. 공용 건물 골조에 합치지 않는다.
            ob=loose_box(col,'차양천_분리_로우폴',(0,-d/2-.27,1.94),(w-.5,.50,.035),4)
        figure(col,(-w/2-.65,-d/2-.35,0))
    # 시설 Collection은 원점 조립, 전시 위치만 부모 Empty로 이동한다.
    idx=len(facility_collections)-1;offset=((idx%4)*10,(idx//4)*10,0)
    root=bpy.data.objects.new(code+'_조립원점_200cm그리드',None);col.objects.link(root)
    for ob in list(col.objects):
        if ob!=root:ob.parent=root
    root.location=offset
    col.instance_offset=offset
    col['관리_ID']=code;col['설계_폭_cm']=w*100;col['설계_깊이_cm']=d*100;col['상태']='1차 로우폴 조립 제안';col['공용메시_공유']=True
    col.asset_mark()
    text(main.collection,code+' '+name,(offset[0],offset[1]-d/2-1.7,.02),.30)
    meshes=[o for o in col.objects if o.type=='MESH' and '기준인체' not in o.data.name]
    stats.append(dict(ID=code,이름=name,면적_cm=[w*100,d*100],배치메시수=len(meshes),삼각형수=sum(sum(len(p.vertices)-2 for p in o.data.polygons) for o in meshes),사용원형수=len(set(o.get('공용_원형','비표준') for o in meshes))))
    return col

specs=[('B01','신사',4,4,True,False,None),('B02','제분소',4,4,False,True,None),('B03','판매소',4,4,False,True,'카운터'),('B04','나가야',8,4,True,False,None),('B05','후신소',4,4,False,True,'작업대'),('B06','밭관리시설',4,4,False,True,None),('B07','공통밭',4,4,False,False,None),('B08','제빵소',4,4,False,True,None),('B09','창고',4,4,True,False,None),('B10','화과자가게',4,4,False,True,'카운터'),('B11','공방',4,4,False,True,'작업대'),('B12','찻집겸여관',8,4,True,False,'카운터'),('B13','차밭',6,4,False,False,None)]
for args in specs:building(*args)

kit=bpy.data.scenes.new('02_공용모듈_원형모음');kit.name='02_공용모듈_원형모음';setup(kit);kit.collection.children.link(library)
core=bpy.data.collections.new('01_기본건축24변형');extra=bpy.data.collections.new('02_추가규격부품');feature=bpy.data.collections.new('03_특징부재와기능품');variants=bpy.data.collections.new('04_치수적용_규격파생')
for c in [core,extra,feature,variants]:library.children.link(c)
for i,(code,ob) in enumerate(modules.items()):
    library.objects.unlink(ob);target=variants if '_V' in code else core if i<24 else extra if code.startswith('X') else feature;target.objects.link(ob)
    ob.location=((i%6)*5,(i//6)*4,0)
    text(kit.collection,code,(ob.location.x,ob.location.y-.65,0),.24)
figure(kit.collection,(-2,-1,0))
text(kit.collection,'140 cm',(-2,-1.5,0),.22)

def studio(scene,target,scale,res=(1600,1200)):
    c=bpy.data.cameras.new(scene.name+'_카메라');o=bpy.data.objects.new('검토카메라',c);scene.collection.objects.link(o)
    o.location=Vector(target)+Vector((scale*.7,-scale,scale*.57));o.rotation_euler=(Vector(target)-o.location).to_track_quat('-Z','Y').to_euler();c.type='ORTHO';c.ortho_scale=scale;scene.camera=o
    for off,energy,size in [((3,-4,9),1800,8),((-6,-1,5),1100,7)]:
        l=bpy.data.lights.new('면광원','AREA');lo=bpy.data.objects.new('면광원',l);scene.collection.objects.link(lo);lo.location=Vector(target)+Vector(off);lo.rotation_euler=(Vector(target)-lo.location).to_track_quat('-Z','Y').to_euler();l.energy=energy;l.shape='DISK';l.size=size
    sun=bpy.data.lights.new('부드러운주광','SUN');sun.energy=2;sun.angle=.35;so=bpy.data.objects.new('부드러운주광',sun);scene.collection.objects.link(so);so.rotation_euler=(.45,-.3,-.4)
    scene.render.resolution_x,scene.render.resolution_y=res
    floor=loose_box(scene.collection,'전시바닥',(target[0],target[1],-.08),(scale*2,scale*2,.05),3)
    for area in bpy.context.screen.areas if bpy.context.screen else []:
        if area.type=='VIEW_3D':area.spaces.active.region_3d.view_distance=scale;area.spaces.active.region_3d.view_location=Vector(target)
studio(main,(14,14,0),54,(1920,1600));studio(kit,(11,(len(modules)//6)*2,0),max(42,(len(modules)//6)*4.6),(1920,1600))
detail=bpy.data.scenes.new('03_제분소_140cm비율검토');setup(detail);detail.collection.children.link(facility_collections[1]);studio(detail,(9.6,0,1.6),9,(1600,1400))
shrine=bpy.data.scenes.new('04_신사_140cm비율검토');setup(shrine);shrine.collection.children.link(facility_collections[0]);studio(shrine,(-.4,0,1.6),9,(1600,1400))

# 새 파일 안에서만 작업한다. 기존 사용자 파일을 열거나 수정하지 않는다.
main['사용자_확정']='주인공 키 140cm / 공용 모듈 직접 로우폴 제작·시설별 재사용'
main['AI_제안']='그 외 문·골조·시설 크기는 1차 제작 제안. 최종 채택·게임 카메라 검증 전.'
main['기준_스냅_cm']=200
main['문_유효폭_cm']=100;main['문_유효높이_cm']=180
main['UV_주의']='공용 트림/타일 배치안만 있음. 실제 TS01·고유 베이크·라이트맵·충돌·LOD 미제작.'
readme=bpy.data.texts.new('먼저읽기_한국어')
readme.write('주인공 140cm 기준의 로우폴 모듈 조립안\n씬 01: 시설 13종 / 씬 02: 공용 원형 / 씬 03: 제분소 / 씬 04: 신사\n초기 공용 24변형과 추가 X부품·특징 기능품은 별도 하위 Collection입니다.\n시설의 각 부품은 원형과 실제 Mesh 데이터를 공유합니다. Alt+D와 같은 링크 복제입니다.\n단위는 미터 좌표, 화면 표시는 cm입니다. 1m=100cm. 기준 인체 최대 높이 1.4m.\n문 통과 100x180cm, 바닥 높이30cm, 계단 단높이15cm, 기둥220cm.\n마감 재질은 임시색입니다. 하이폴·베이크·최종 텍스처·충돌·LOD·엔진 확인은 미완료입니다.\n차양/잎카드/기능 장비는 1차 블록아웃입니다. 시설 단계별 확장·사계절 외형은 확정하지 않았습니다.\n')
bpy.context.window.scene=detail
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            space=area.spaces.active
            space.overlay.show_object_origins_all=True
            space.shading.color_type='MATERIAL'
            space.region_3d.view_rotation=detail.camera.rotation_euler.to_quaternion()
            space.region_3d.view_location=(9.6,0,1.6)
            space.region_3d.view_distance=9
            space.region_3d.view_perspective='ORTHO'
for ob in bpy.context.selected_objects:ob.select_set(False)
bpy.context.view_layer.objects.active=None
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'CozyRealm_공용모듈과시설_140cm_로우폴.blend'))
(OUT/'제작_규격과검증.json').write_text(json.dumps(dict(단위='m / cm 표시',주인공_cm=140,공용원형=records,시설=stats),ensure_ascii=False,indent=2),encoding='utf-8')
for scene,filename in [(detail,'제분소_140cm_비율.png'),(shrine,'신사_140cm_비율.png'),(kit,'공용모듈_원형모음.png'),(main,'시설13종_조립전체.png')]:
    scene.render.filepath=str(OUT/filename);bpy.ops.render.render(write_still=True,scene=scene.name)
print('COZYREALM_DONE',len(records),len(stats))

