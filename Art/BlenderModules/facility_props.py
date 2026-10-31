# 시설 공용 기능품. build_other_facilities.py의 모듈 캐시·배치 함수를 사용한다.
def lathe(code,name,profile,mat=0,n=12):
    if code in modules:return code
    m=Mesh();v=[]
    for k,(r,z) in enumerate(profile):
        for j in range(n):
            a=j*2*math.pi/n;v.append((r*math.cos(a),r*math.sin(a),z))
    fs=[tuple(reversed(range(n))),tuple(range((len(profile)-1)*n,len(profile)*n))]
    for k in range(len(profile)-1):
        for j in range(n):fs.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j))
    m.poly(v,fs,mat);asset(code,name,m,0,'AT02' if mat!=0 else 'TS01');return code
def ring(c,pos,r=.10,t=.018,mat=5,rot=(0,0,0),label='고리'):
    key=f'KRING_{mat}_{dimkey((r,t))}'
    if key not in modules:
        m=Mesh();n=12;s=6;v=[]
        for i in range(n):
            a=i*2*math.pi/n
            for j in range(s):
                b=j*2*math.pi/s;v.append(((r+t*math.cos(b))*math.cos(a),(r+t*math.cos(b))*math.sin(a),t*math.sin(b)))
        f=[(i*s+j,i*s+(j+1)%s,((i+1)%n)*s+(j+1)%s,((i+1)%n)*s+j) for i in range(n) for j in range(s)]
        m.poly(v,f,mat);asset(key,label,m,0,'AT02','고리 중심')
    return place(key,c,pos,rot)
def furniture(c,pos,width=2.6,depth=.75,height=.85,counter=False,shelf=False):
    x,y,z=pos
    for dx in [-width/2+.08,width/2-.08]:
        for dy in [-depth/2+.06,depth/2-.06]:box(c,(x+dx,y+dy,z),(.12,.12,height),label='가구다리')
    levels=[height-.08] if not shelf else [.12,height*.52,height-.08]
    for h in levels:
        n=max(2,round(depth/.18));seg=depth/n
        for i in range(n):box(c,(x,y-depth/2+seg/2+i*seg,z+h),(width,seg-.006,.08),label='가구상판_널')
    for h in [.10,height-.16]:
        for dy in [-depth/2+.06,depth/2-.06]:bar(c,(x-width/2,y+dy,z+h),(x+width/2,y+dy,z+h),.10,'가구_가로보')
    if counter:
        n=math.ceil((width-.18)/.25);span=(width-.18)/n
        for i in range(n):box(c,(x-width/2+.09+span/2+i*span,y-depth/2+.02,z+.13),(span-.008,.065,height-.29),label='카운터_전면널')
    return z+height
def crate(c,pos,opened=False):
    key='P02_V07_열린상자' if opened else 'P02_V07_닫힌상자'
    if key not in modules:
        m=Mesh()
        for x in [-.30,.30]:
            for y in [-.22,.22]:m.box(x,y,.25,.07,.07,.50)
        for z in [.10,.25,.40]:
            for y in [-.24,.24]:m.box(0,y,z,.65,.055,.13)
            for x in [-.32,.32]:m.box(x,0,z,.055,.48,.13)
        m.box(0,0,.035,.65,.48,.07)
        if not opened:
            for i in range(4):m.box(-.24+i*.16,0,.49,.15,.48,.05)
            m.box(0,-.28,.26,.06,.035,.46)
        asset(key,'열린수납상자' if opened else '적재상자',m,.012,'TS01')
    return place(key,c,pos)
def barrel(c,pos):
    code=lathe('P03_V07_통','나무통',[(.25,0),(.30,.08),(.34,.35),(.30,.64),(.25,.70)],0,14)
    place(code,c,pos)
    for r,z in [(.30,.10),(.34,.32),(.30,.58)]:
        band=lathe('P03_V07_테_'+str(z),'통_테두리',[(r+.015,0),(r+.015,.055),(r-.012,.055),(r-.012,0)],5,14)
        place(band,c,Vector(pos)+Vector((0,0,z)))
def sack(c,pos):
    code='U17_V07_자루'
    if code not in modules:
        m=Mesh();prof=[(.18,0),(.29,.08),(.31,.35),(.24,.61),(.075,.72),(.12,.81)];v=[];n=12
        for k,(r,z) in enumerate(prof):
            for j in range(n):
                a=j*2*math.pi/n;rr=r*(1+.07*math.sin(3*a+k));v.append((rr*math.cos(a),rr*math.sin(a),z))
        f=[tuple(reversed(range(n))),tuple(range(5*n,6*n))]+[(k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j) for k in range(5) for j in range(n)]
        m.poly(v,f,10);asset(code,'모인입구_곡물자루',m,0,'고유 UV·베이크 예정')
    place(code,c,pos);ring(c,Vector(pos)+Vector((0,0,.73)),.078,.018,0,label='자루매듭')
def bread(c,pos,kind=0):
    code=lathe('P08_V07_빵_'+str(kind),'둥근빵' if not kind else '긴빵',[(.06,0),(.17,.025),(.19,.08),(.15,.17),(.05,.21)],11,12)
    code=derived(code,(1.7, .7,1)) if kind else code;place(code,c,pos)
def tray(c,pos,width=.95,depth=.55):
    x,y,z=pos;box(c,pos,(width,depth,.045),label='진열장반_밑판')
    for xx in [-width/2,width/2]:box(c,(x+xx,y,z+.045),(.04,depth,.045),label='장반_테두리')
    for yy in [-depth/2,depth/2]:box(c,(x,y+yy,z+.045),(width,.04,.045),label='장반_테두리')
def sweets(c,pos):
    key='B10_V07_꽃과자'
    if key not in modules:
        m=Mesh();prof=[(.06,0),(.13,.035),(.14,.09),(.10,.15),(.035,.18)];n=15;v=[]
        for k,(r,z) in enumerate(prof):
            for j in range(n):
                a=j*2*math.pi/n;rr=r*(1+.15*math.cos(5*a));v.append((rr*math.cos(a),rr*math.sin(a),z))
        f=[tuple(reversed(range(n))),tuple(range(4*n,5*n))]+[(k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j) for k in range(4) for j in range(n)]
        m.poly(v,f,PINK);asset(key,'꽃모양_화과자',m,0,'개별 UV·베이크 예정')
    return place(key,c,pos)
def noren(c,x,y,z,n=4,width=.65,rotate=0):
    key='U19_V07_가림막_'+str(round(width*1000))
    if key not in modules:
        m=Mesh();v=[];count=12
        for back in [0,.014]:
            for top in [True,False]:
                for i in range(count+1):
                    xx=width*i/count;yy=.025*math.sin(i*math.pi/3)+back
                    zz=0 if top else -.55+.025*math.cos(i*math.pi/count)
                    v.append((xx,yy,zz))
        s=count+1;f=[]
        for i in range(count):f.extend([(i,i+1,s+i+1,s+i),(2*s+i,3*s+i,3*s+i+1,2*s+i+1)])
        f.extend([(0,s,3*s,2*s),(count,2*s+count,3*s+count,s+count)])
        for i in range(count):f.extend([(i,2*s+i,2*s+i+1,i+1),(s+i,s+i+1,3*s+i+1,3*s+i)])
        m.poly(v,f,4);asset(key,'천가림막_주름초안',m,0,'AT02 / 최종 주름 베이크 예정','상단 걸이점 / +X 폭')
    angle=rotate;origin=Vector((x,y,z));mat=Matrix.Rotation(angle,4,'Z')
    rod(c,origin+mat@Vector((-.08,0,0)),origin+mat@Vector((n*width+.08,0,0)),.035,0,'가림막_걸이봉')
    for j in range(n):
        place(key,c,origin+mat@Vector((j*width+.01,0,-.03)),(0,0,angle))
        ring(c,origin+mat@Vector((j*width+width/2,0,0)),.044,.010,5,(math.pi/2,0,angle),'천_걸이고리')
def cabinet(c,pos,width=1.4):
    x,y,z=pos;furniture(c,pos,width,.65,1.15,False,True)
    for ix in [-1,1]:
        for row in range(3):
            xx=x+ix*width*.245;zz=z+.13+row*.31
            box(c,(xx,y-.025,zz),(width*.46,.56,.28),label='씨앗함_서랍')
            ring(c,(xx,y-.325,zz+.14),.055,.012,5,(math.pi/2,0,0),'서랍_고리손잡이')
def tool(c,pos,kind='망치',rot=(0,0,0)):
    code='TOOL_V07_'+kind
    if code not in modules:
        m=Mesh();m.box(0,0,.24,.045,.05,.48)
        if kind=='망치':m.box(0,0,.46,.23,.09,.10,5)
        elif kind=='삽':
            m.poly([(-.11,-.02,.43),(.11,-.02,.43),(.10,-.02,.66),(0,-.02,.73),(-.10,-.02,.66),(-.11,.02,.43),(.11,.02,.43),(.10,.02,.66),(0,.02,.73),(-.10,.02,.66)],[(0,4,3,2,1),(5,6,7,8,9),(0,1,6,5),(1,2,7,6),(2,3,8,7),(3,4,9,8),(4,0,5,9)],5)
        elif kind=='쇠스랑':
            m.box(0,0,.49,.24,.05,.05,5)
            for xx in [-.10,0,.10]:m.box(xx,0,.59,.025,.03,.19,5)
        elif kind=='톱':
            m.box(0,0,.22,.13,.08,.30)
            m.box(.04,0,.57,.18,.025,.50,5)
        asset(code,kind,m,.005,'AT02 도구 / 개별 UV 예정')
    return place(code,c,pos,rot)
def vice(c,pos):
    x,y,z=pos;box(c,pos,(.35,.26,.06),5,.01,label='바이스_받침')
    for xx in [-.13,.13]:box(c,(x+xx,y,z+.06),(.07,.24,.18),5,.01,label='바이스_턱')
    rod(c,(x-.30,y,z+.12),(x+.30,y,z+.12),.022,5,'바이스_나사')
    rod(c,(x-.29,y,z-.02),(x-.29,y,z+.27),.015,5,'바이스_손잡이')
def lantern(c,pos):
    x,y,z=pos
    box(c,(x,y,z),(.22,.22,.34),10,.01,label='초롱_종이')
    for dx in [-.13,.13]:
        for dy in [-.13,.13]:box(c,(x+dx,y+dy,z),(.035,.035,.35),label='초롱_기둥')
    for h in [-.04,.35]:box(c,(x,y,z+h),(.32,.32,.045),label='초롱_마감')
    ring(c,(x,y,z+.45),.055,.014,5,(math.pi/2,0,0),'초롱_걸이')
def cup(c,pos):
    key=lathe('P12_V07_찻잔','속빈찻잔',[(.07,0),(.09,.03),(.10,.16),(.078,.16),(.06,.035)],TEA,12)
    place(key,c,pos)
def pot(c,pos):
    key=lathe('P12_V07_주전자','차주전자',[(.07,0),(.16,.04),(.19,.15),(.14,.25),(.10,.27)],TEA,14)
    place(key,c,pos);x,y,z=pos
    rod(c,(x+.12,y,z+.13),(x+.29,y,z+.26),.045,TEA,'주전자_주둥이')
    ring(c,(x-.15,y,z+.17),.12,.02,TEA,(math.pi/2,0,0),'주전자_손잡이')
    box(c,(x,y,z+.27),(.12,.12,.035),TEA,.01,label='주전자_뚜껑')
def oven(c,pos):
    x,y,z=pos
    box(c,pos,(1.10,.95,.60),2,.035,label='오븐_기초')
    for side in [-1,1]:box(c,(x+side*.42,y,z+.60),(.25,.95,.45),2,.025,label='오븐_옆벽')
    # 9개 쐐기로 실제 열린 아치를 만든다.
    key='B08_V07_아치벽돌'
    if key not in modules:
        a0=0;a1=math.pi/9;m=Mesh();v=[]
        for yy in [-.48,.48]:
            for r,a in [(.30,a0),(.55,a0),(.55,a1),(.30,a1)]:v.append((r*math.cos(a),yy,1.02+r*math.sin(a)))
        m.poly(v,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],2);asset(key,'오븐_아치벽돌',m,.012,'T01')
    # 원형 회전축은 XZ 평면의 아치 중심으로 둔다.
    for i in range(9):
        code=key+'_각도'+str(i)
        if code not in modules:
            src=modules[key].data;m=Mesh();rot=Matrix.Rotation(-i*math.pi/9,4,'Y');center=Vector((0,0,1.02))
            m.v=[tuple(center+rot@(v.co-center)) for v in src.vertices];m.f=[tuple(p.vertices) for p in src.polygons];m.mi=[p.material_index for p in src.polygons];asset(code,'아치_방향원형',m,0,'T01')
        place(code,c,(x,y,z))
    box(c,(x,y+.43,z+.60),(.72,.08,.85),DARK,0,label='오븐내부_뒤벽')
def chimney(c,pos,height=1.8):
    x,y,z=pos;h=.30;n=math.ceil(height/h)
    for j in range(n):box(c,(x,y,z+j*h),(.40,.40,h-.006),2,.012,label='굴뚝_석재단')
    box(c,(x,y,z+n*h),(.52,.52,.10),2,.02,label='굴뚝_끝마감')

# 최종 아트가 아닌, 카드 방식 검토용 공용 아틀라스 2장.
import numpy as np
def foliage_atlas(kind):
    N=1024;data=np.zeros((N,N,4),dtype=np.float32)
    def ellipse(cx,cy,rx,ry,angle,color):
        radius=max(rx,ry);x0=max(0,int(cx-radius-1));x1=min(N,int(cx+radius+2));y0=max(0,int(cy-radius-1));y1=min(N,int(cy+radius+2))
        yy,xx=np.mgrid[y0:y1,x0:x1];dx=xx-cx;dy=yy-cy;ca=math.cos(angle);sa=math.sin(angle)
        mask=((dx*ca+dy*sa)/rx)**2+((-dx*sa+dy*ca)/ry)**2<=1
        data[y0:y1,x0:x1][mask]=(*color,1)
    for tile in range(4):
        bx=(tile%2)*512;by=(tile//2)*512;cx=bx+256
        if kind=='밀':
            ellipse(cx,by+250,4,220,0,(.42,.40,.10))
            for side in [-1,1]:
                ellipse(cx+side*46,by+180,18,112,-side*.40,(.38,.47,.14))
                for j in range(6):ellipse(cx+side*18,by+295+j*25,13,24,-side*.50,(.66+.025*j,.48+.02*j,.14))
            ellipse(cx,by+460,9,22,0,(.83,.66,.25))
        else:
            ellipse(cx,by+245,5,200,.10,(.22,.22,.07))
            for j in range(4):
                for side in [-1,1]:
                    ellipse(cx+side*(62-j*4),by+150+j*75,59-j*4,24,-side*.42,(.16+j*.025,.32+j*.025,.07+j*.015))
            ellipse(cx,by+440,20,45,-.1,(.25,.43,.13))
    image=bpy.data.images.new(kind+'_공용카드_임시아틀라스',width=N,height=N,alpha=True)
    image.pixels.foreach_set(data.ravel());image.filepath_raw=str(OUT/(kind+'_카드아틀라스_초안.png'));image.file_format='PNG';image.save();image.pack()
    mi=newmat('AT01_'+kind+'_카드_임시',(.35,.45,.15));mat=M[mi]
    nt=mat.node_tree;bs=nt.nodes['Principled BSDF'];tex=nt.nodes.new('ShaderNodeTexImage');tex.image=image;tex.interpolation='Linear'
    nt.links.new(tex.outputs['Color'],bs.inputs['Base Color']);nt.links.new(tex.outputs['Alpha'],bs.inputs['Alpha']);bs.inputs['Roughness'].default_value=.9
    if hasattr(mat,'surface_render_method'):mat.surface_render_method='DITHERED'
    return mi
WHEAT=foliage_atlas('밀');LEAF=foliage_atlas('차잎')
def plantcard(c,pos,kind,tile=0,angle=0):
    code='AT01_V07_'+kind+'_'+str(tile)
    if code not in modules:
        h=.95 if kind=='밀' else .62;w=.48 if kind=='밀' else 1.25;m=Mesh()
        m.poly([(-w/2,0,0),(w/2,0,0),(w/2,0,h),(-w/2,0,h)],[(0,1,2,3)],WHEAT if kind=='밀' else LEAF)
        ob=asset(code,kind+'_알파카드',m,0,'AT01 / plane+아틀라스','밑면 중심')
        uv=ob.data.uv_layers.active;u=tile%2*.5;v=tile//2*.5
        for li,co in enumerate([(u,v),(u+.5,v),(u+.5,v+.5),(u,v+.5)]):uv.data[li].uv=co
    return place(code,c,pos,(0,0,angle))
