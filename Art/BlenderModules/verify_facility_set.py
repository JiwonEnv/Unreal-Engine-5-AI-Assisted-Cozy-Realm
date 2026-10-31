import bpy,json,math,bmesh
from pathlib import Path
from mathutils import Vector
from mathutils.bvhtree import BVHTree
p=Path(__file__).resolve().parent/'V07_시설전체_제분소기준'
bpy.ops.wm.open_mainfile(filepath=str(p/'CozyRealm_V07_시설13종_공용모듈_140cm.blend'))
lib=bpy.data.collections['00_공용모듈_원형_실제메시공유'];sources={o['원형_ID']:o for o in lib.all_objects if o.type=='MESH'}
cols={c['관리_ID']:c for c in bpy.data.collections if c.get('관리_ID')};checks=[]
def check(name,ok,info=''):checks.append({'검사':name,'통과':bool(ok),'내용':info})
def verts(o):return [o.matrix_basis@v.co for v in o.data.vertices]
def boxbounds(o):
    vs=verts(o);return [(min(v[a] for v in vs),max(v[a] for v in vs)) for a in range(3)]
allplaced=[o for c in cols.values() for o in c.objects if o.get('공용_원형')]
active={o['공용_원형'] for o in allplaced}
check('시설13종 존재',set(cols)=={f'B{i:02}' for i in range(1,14)})
check('실제 공용 메시 공유',all(o.data==sources[o['공용_원형']].data for o in allplaced),{'배치수':len(allplaced),'사용원형수':len(active)})
check('모든 배치 배율1',all((o.scale-Vector((1,1,1))).length<1e-5 for o in allplaced))
check('사용 원형 UV 존재',all(len(sources[k].data.uv_layers)>0 for k in active))
current=bpy.data.collections['00_현재사용_공용원형']
check('사용 원형 분류 정확',{o['원형_ID'] for o in current.objects}==active)
check('카드 아틀라스2장 내장',sum(bool(im.packed_file) for im in bpy.data.images if im.name.endswith('공용카드_임시아틀라스'))==2)
for code,c in cols.items():
    root=next(o for o in c.objects if o.type=='EMPTY')
    check(code+' 시설 조립 원점',(Vector(c.instance_offset)-root.location).length<1e-6)
    figures=[o for o in c.objects if o.name.startswith('주인공_높이140cm')]
    check(code+' 140cm 비교 인체',len(figures)==1 and abs(boxbounds(figures[0])[2][1]-boxbounds(figures[0])[2][0]-1.4)<1e-5)
    if code!='B02':check(code+' 시설별 시안 특징 기록',bool(c.get('V07_반영특징')))

# 기둥 밑면이 실제 받침석 위에 겹치는지 검사한다.
unsupported=[];count=0
for code,c in cols.items():
    bases=[boxbounds(o) for o in c.objects if o.get('공용_원형')=='A04_V07_받침']
    posts=[o for o in c.objects if str(o.get('공용_원형','')).startswith(('U13_V07','W01_V07_직선','W01_V07_뒷중앙기둥','W01_V07_작은지붕기둥','W01_V07_여관'))]
    for o in posts:
        b=boxbounds(o);count+=1
        support=any(abs(b[2][0]-s[2][1])<.005 and min(b[0][1],s[0][1])-max(b[0][0],s[0][0])>.01 and min(b[1][1],s[1][1])-max(b[1][0],s[1][0])>.01 for s in bases)
        if not support:unsupported.append({'시설':code,'部材'.replace('部材','부재'):o.name,'밑면Z':b[2][0]})
check('기둥과 받침석 접촉',not unsupported,{'검사기둥수':count,'누락':unsupported})

bad=[];deg=[];cards=[]
for code in active:
    bm=bmesh.new();bm.from_mesh(sources[code].data)
    if code.startswith('AT01_V07_'):cards.append(code)
    elif any(not e.is_manifold for e in bm.edges):bad.append(code)
    if any(f.calc_area()<1e-10 for f in bm.faces):deg.append(code)
    bm.free()
check('입체 원형 닫힌 표면',not bad,{'열린원형':bad,'의도된 식생 plane 원형':cards})
check('퇴화면 없음',not deg,deg)

# 실제 저장 메시를 관통하는 광선으로 닫힌 벽의 중앙과 박공을 확인한다.
def rayhit(objects,p0,d):
    for o in objects:
        inv=o.matrix_basis.inverted();start=inv@Vector(p0);direction=inv.to_3x3()@Vector(d)
        tree=BVHTree.FromPolygons([v.co for v in o.data.vertices],[list(f.vertices) for f in o.data.polygons],all_triangles=False)
        if tree.ray_cast(start,direction.normalized(),20)[0] is not None:return True
    return False
for code,c in cols.items():
    if code in ['B07','B13','B02']:continue
    scene_root=next(o for o in c.objects if o.type=='EMPTY')
    # 길쭉한 시설은 조립 때 -90도 회전했으므로 검사 광선도 함께 돌린다.
    angle=-math.pi/2 if code in ['B04','B12'] else 0
    from mathutils import Matrix
    rot=Matrix.Rotation(angle,4,'Z')
    depth=6 if code=='B04' else 8 if code=='B12' else 4
    back=[o for o in c.objects if o.get('공용_원형') in ['W01_V07_뒷중앙기둥','W01_V07_여관뒷중앙235','KBOX_0_3400_3400_3700_250']]
    ok=all(rayhit(back,rot@Vector((0,depth/2+1,z)),rot@Vector((0,-1,0))) for z in [1,1.7,2.5])
    check(code+' 뒷벽 중앙 막힘',ok)
    gables=[o for o in c.objects if '박공채움' in str(o.get('공용_원형',''))]
    check(code+' 앞박공 막힘',rayhit(gables,rot@Vector((.5,-depth/2-1,3.35)),rot@Vector((0,1,0))))
check('나가야 격자문3개',len([o for o in cols['B04'].objects if o.get('공용_원형')=='A03_V07_격자문짝'])==3)
check('여관 격자문2개',len([o for o in cols['B12'].objects if o.get('공용_원형')=='A03_V07_격자문짝'])==2)
check('밀밭 알파카드 배치',any(str(o.get('공용_원형','')).startswith('AT01_V07_밀') for o in cols['B07'].objects))
check('차밭 알파카드 배치',any(str(o.get('공용_원형','')).startswith('AT01_V07_차잎') for o in cols['B13'].objects))
ok=all(c['통과'] for c in checks)
(p/'공유_피벗_지지_막음_검증.json').write_text(json.dumps({'통과':ok,'검사':checks},ensure_ascii=False,indent=2),encoding='utf-8')
print('FACILITY_SET_CHECK',len(checks),[c for c in checks if not c['통과']]);assert ok
