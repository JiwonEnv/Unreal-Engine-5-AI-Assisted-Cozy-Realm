import bpy,json
from pathlib import Path
from mathutils import Vector
p=Path(__file__).resolve().parent/'V07_시설전체_제분소기준'
f=p/'CozyRealm_V07_시설13종_공용모듈_140cm.blend'
bpy.ops.wm.open_mainfile(filepath=str(f))
lib=bpy.data.collections['00_공용모듈_원형_실제메시공유'];current=bpy.data.collections['00_현재사용_공용원형']
sources={o['원형_ID']:o for o in lib.all_objects if o.type=='MESH'}
cols={c['관리_ID']:c for c in bpy.data.collections if c.get('관리_ID')};c=cols['B12']
# 기존 90도 회전을 역변환해 부재 위치만 읽는다. 전체 회전은 유지한다.
def oldxy(o):return (-o.location.y,o.location.x)
newrecords=[]
for oldcode,newcode in [('W01_V07_직선','W01_V07_여관높은기둥235'),('W01_V07_뒷중앙기둥','W01_V07_여관뒷중앙235')]:
    src=sources[oldcode];me=src.data.copy()
    for v in me.vertices:v.co.z*=2.35/2.05
    me.update();ob=bpy.data.objects.new(newcode,me);current.objects.link(ob)
    for key in src.keys():ob[key]=src[key]
    ob['원형_ID']=newcode;ob['사용시설']='B12';ob.asset_mark();ob.location=(len(current.objects)%10*5,len(current.objects)//10*5,0)
    sources[newcode]=ob
    newrecords.append({'ID':newcode,'이름':'여관 높은 구간 기둥','크기_cm':[round(v*100,3) for v in ob.dimensions],'피벗':ob['피벗_기준'],'재질':ob['재질_경로'],'삼각형':sum(len(p.vertices)-2 for p in me.polygons)})
    for o in c.objects:
        x,y=oldxy(o)
        if o.get('공용_원형')==oldcode and y>=-.001:
            o.data=me;o['공용_원형']=newcode
for o in list(c.objects):
    x,y=oldxy(o);code=o.get('공용_원형','')
    # 기존 기둥과 같은 평면에서 겹치던 37cm 연장 블록 제거.
    if code.startswith('KBOX_0_') and abs(o.location.z-2.78)<.001 and len(o.data.vertices)>0:
        size=[max(v.co[a] for v in o.data.vertices)-min(v.co[a] for v in o.data.vertices) for a in range(3)]
        if abs(size[2]-.37)<.002:bpy.data.objects.remove(o,do_unlink=True);continue
        if abs(size[2]-.30)<.002 and abs(abs(x)-2)<.001:
            # 상부 띠의 바깥 면은 보보다 2cm 안으로 들인다.
            new=me=o.data.copy()
            # 오브젝트는 -90도 회전된 상태, 메시 좌표는 원래 방향이다.
            for v in me.vertices:v.co.x*=.20/.24
            # 이것도 재사용 가능한 규격 원형으로 등록한다.
            stripcode='KBOX_여관상부띠_20'
            if stripcode not in sources:
                src=bpy.data.objects.new(stripcode,me);current.objects.link(src)
                for key in o.keys():
                    if key!='공용_원형':src[key]=o[key]
                src['원형_ID']=stripcode;src['피벗_기준']='밑면 중심';src['사용시설']='B12';src.asset_mark();sources[stripcode]=src
                newrecords.append({'ID':stripcode,'이름':'여관 상부 채움띠','크기_cm':[20,174,30],'피벗':'밑면 중심','재질':'TS01','삼각형':sum(len(p.vertices)-2 for p in me.polygons)})
            o.data=sources[stripcode].data;o['공용_원형']=stripcode
    if code=='W02_V07_200' and y>=-.001 and abs(o.location.z-2.80)<.001:o.location.z=3.10
    if code=='W02_V07_200' and y>=-.001 and abs(o.location.z-3.10)<.001:
        # 같은 위치의 중복 배치는 아래에서 정리한다.
        pass
    if code=='W02_V07_400' and abs(y-4)<.001 and abs(o.location.z-2.85)<.001:o.location.z=3.15
    if code=='W04_V07' and y>=-.001:o.location.z=2.86
# 같은 공유 메시·위치·회전의 중복 배치를 제거한다.
seen=set()
for o in list(c.objects):
    if not o.get('공용_원형'):continue
    key=(o.get('공용_원형'),tuple(round(v,5) for v in o.location),tuple(round(v,5) for v in o.rotation_euler))
    if key in seen:bpy.data.objects.remove(o,do_unlink=True)
    else:seen.add(key)
# 사용 원형 목록과 배치 통계를 저장 파일과 일치시킨다.
usage={}
for code,col in cols.items():
    for o in col.objects:
        if o.get('공용_원형'):usage.setdefault(o['공용_원형'],set()).add(code)
legacy=bpy.data.collections['99_이전검토원형_사용안함']
for code,src in sources.items():
    src['사용시설']=', '.join(sorted(usage.get(code,[])))
    target=current if code in usage else legacy
    for owner in list(src.users_collection):
        if owner!=target:owner.objects.unlink(src)
    if src.name not in target.objects:target.objects.link(src)
for i,src in enumerate(current.objects):src.location=((i%10)*5,(i//10)*5,0)
data=json.loads((p/'시설별_제작목록.json').read_text(encoding='utf-8'));data['추가원형']+=newrecords
for row in data['시설']:
    if row['ID']=='B12':
        obs=[o for o in c.objects if o.get('공용_원형')]
        row['배치수']=len(obs);row['사용원형수']=len({o['공용_원형'] for o in obs});row['실제삼각형수']=sum(sum(len(f.vertices)-2 for f in o.data.polygons) for o in obs)
c['삼각형수_조립합계']=next(r['실제삼각형수'] for r in data['시설'] if r['ID']=='B12')
(p/'시설별_제작목록.json').write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
bpy.context.window.scene=bpy.data.scenes['B03_판매소_단독검토'];bpy.context.view_layer.update()
bpy.ops.wm.save_as_mainfile(filepath=str(f))
scene=bpy.data.scenes['B12_찻집겸여관_단독검토'];scene.render.filepath=str(p/'B12_찻집겸여관_모델.png');bpy.ops.render.render(write_still=True,scene=scene.name)
root=next(o for o in c.objects if o.type=='EMPTY');target=root.location+Vector((0,0,1.9));cam=scene.camera;size=cam.data.ortho_scale
cam.location=target+Vector((size*.7,size,size*.25));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();scene.render.resolution_x=1400;scene.render.resolution_y=1200;scene.render.filepath=str(p/'B12_후면_검토.png');bpy.ops.render.render(write_still=True,scene=scene.name)
scene=bpy.data.scenes['01_시설13종_조립검토'];scene.cycles.samples=24;scene.cycles.use_denoising=True;scene.render.resolution_x=2000;scene.render.resolution_y=1700;scene.render.filepath=str(p/'시설13종_전체배치.png');bpy.ops.render.render(write_still=True,scene=scene.name)
print('TEA_SUPPORTS_FIXED',flush=True)
