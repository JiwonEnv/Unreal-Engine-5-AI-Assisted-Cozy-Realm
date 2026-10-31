# 시설마다 고유한 기능과 전면 구성을 가진 조립본.
def mark(code,features,proposal='시설별 배치·세부 비례는 제안'):
    c=collections[code];c['V07_반영특징']=' / '.join(features);c['미확정']=proposal
    configs.append({'ID':code,'시설':c.name,'반영특징':features,'미확정':proposal})
def finish(code,rotate=0,figureloc=(-2.95,-2.3,0)):
    c=collections[code]
    if rotate:
        mat=Matrix.Rotation(rotate,4,'Z')
        for o in c.objects:
            if o!=roots[code]:o.matrix_basis=mat@o.matrix_basis
        if abs(abs(rotate)-math.pi/2)<1e-6:c['설계_폭_cm'],c['설계_깊이_cm']=c['설계_깊이_cm'],c['설계_폭_cm']
    o=figure(c,figureloc);o.parent=roots[code]
    c.instance_offset=roots[code].location

# B01 신사: 본체의 특징은 금줄·종이 장식·격자문·헌금함이다.
c=collections['B01'];shell(c,annex=True)
# 뒤쪽에 배치한 중심부. 격자문은 문짝 원형을 재사용한다.
box(c,(0,-.55,.42),(3.4,.12,2.35),3,.01,label='신사_중심부_뒷벽')
place('A03-01',c,(0,-.66,.42));place('A03_V07_격자문짝',c,(-.49,-.75,.43))
code='B01_V07_땋은금줄'
if code not in modules:
    m=Mesh();steps=72;n=6
    for strand in range(3):
        vs=[]
        for i in range(steps+1):
            t=i/steps;x=-1.35+2.7*t;z=.24*(2*t-1)**2;a=t*math.pi*18+strand*2*math.pi/3
            cy=.043*math.cos(a);cz=z+.043*math.sin(a)
            for j in range(n):
                b=j*2*math.pi/n;vs.append((x,cy+.030*math.cos(b),cz+.030*math.sin(b)))
        fs=[tuple(reversed(range(n))),tuple(range(steps*n,(steps+1)*n))]+[(i*n+j,i*n+(j+1)%n,(i+1)*n+(j+1)%n,(i+1)*n+j) for i in range(steps) for j in range(n)]
        m.poly(vs,fs,10)
    asset(code,'신사_땋은금줄',m,0,'개별 UV·베이크 예정','중앙 매듭 기준')
place(code,c,(0,-2.19,2.49))
for x in [-.72,.72]:place('B01-종이장식',c,(x,-2.22,2.55))
rod(c,(0,-2.20,2.48),(0,-2.20,2.18),.025,10,'중앙_장식끈')
lathe('B01_V07_술장식','금줄_술장식',[(.035,0),(.09,.02),(.055,.28),(.08,.31),(.03,.37)],10,12)
place('B01_V07_술장식',c,(0,-2.20,1.99))
furniture(c,(0,-1.18,.42),1.25,.70,.72,True)
for i in range(7):box(c,(-.54+i*.18,-1.18,1.14),(.12,.70,.055),label='헌금함_상단살')
ring(c,(0,-1.56,.80),.15,.02,5,(math.pi/2,0,0),'헌금함_문양')
bar(c,(-.12,-1.57,.80),(.12,-1.57,.80),.025,'문양_가로');bar(c,(0,-1.57,.68),(0,-1.57,.92),.025,'문양_세로')
for y in [.3,1.4]:
    for x in [-2,2]:box(c,(x,y,.8),(.10,.10,.55),label='신사_낮은난간기둥')
for x in [-2,2]:bar(c,(x,.3,1.35),(x,1.4,1.35),.10,'신사_난간')
for j,h in enumerate([.28,.14]):box(c,(0,-2.40-.35*j,0),(1.8,.35,h),2,.025,label='신사_입구계단')
mark('B01',['곡선 지붕·굵은 기둥','금줄·종이·술 장식','격자 중심부와 헌금함','작은 옆 지붕·입구 계단']);finish('B01')

# B03 판매소: 전면 카운터와 판매용 상자, 천 가림막.
c=collections['B03'];shell(c,annex=True)
top=furniture(c,(0,-1.45,.42),2.8,.78,.90,True)
for x in [-.67,.20]:
    crate(c,(x,-1.45,top),True)
    for j in range(2):box(c,(x,-1.45,top+.50+j*.055),(.47,.38,.05),10,.022,label='접힌_판매물품')
crate(c,(1.05,-1.45,top));noren(c,-1.35,-2.13,2.63,4,.675)
mark('B03',['전면 판매 카운터','상자·접힌 물품 진열','4폭 천 가림막','제분소형 공용 골조']);finish('B03')

# B04 나가야: 200cm 반복 문 구간 3개를 긴 측면에 조립한다.
c=collections['B04'];shell(c,6,side_modes={(2,i):'door' for i in range(3)})
for x in [-1,1]:
    place('A01_V07_상부회벽_보완',c,(x,-3,1.44))
    for j in range(6):place('A01_V07_하부널_보완',c,(x-.70+j*.28,-3.10,.58))
    for z in [.54,1.36]:place('W02_V07_180',c,(x-.9,-3.10,z))
place('A04_V07_받침',c,(0,-3,.42));place('W01_V07_뒷중앙기둥',c,(0,-3,.80))
mark('B04',['긴 연속 지붕','격자 문 3개·반복 기둥','각 출입구 계단','막힌 끝벽과 박공'],'3개 출입구는 시안 대응용이며 방 수·확장 단계 확정 아님')
finish('B04',-math.pi/2,(-4.25,-2.7,0))

# B05 후신소: 건축 목공 작업대와 목재·톱·망치.
c=collections['B05'];shell(c,annex=True)
top=furniture(c,(0,-.95,.42),2.45,.85,.75)
for x in [-.65,.1,.65]:tool(c,(x,-.95,top),'망치' if x<0 else '톱',(0,.30,0))
for z in [.42,.50,.58]:box(c,(.55,.50,z),(1.3,.25,.07),label='적층_목재널')
for x in [-1.25,-.8]:box(c,(x,1.45,.42),(.20,.12,1.40),rot=(0,-.25,0),label='뒤_세워둔목재')
crate(c,(1.0,-.8,.42));crate(c,(1.0,-.8,.92),True)
for x in [2.45,2.72,2.9]:box(c,(x,1.05,.42),(.16,.12,1.25),rot=(0,-.22,0),label='옆_보관목재')
mark('B05',['목공 작업대','톱·망치·적층 목재','옆 지붕 아래 목재 보관','작업 공간을 연 전면']);finish('B05')

# B06 밭 관리 시설: 씨앗함과 농기구를 중심으로 한다.
c=collections['B06'];shell(c,annex=True)
cabinet(c,(-.45,-.6,.42),1.40);barrel(c,(1.05,-.8,.42))
tool(c,(.96,-.85,.76),'삽',(0,.13,0));tool(c,(1.12,-.8,.76),'쇠스랑',(0,-.13,0))
crate(c,(-1.20,-1.25,.42),True)
lathe('B06_V07_씨앗그릇','씨앗_바구니',[(.15,0),(.24,.20),(.26,.24),(.22,.24),(.14,.04)],0,12)
place('B06_V07_씨앗그릇',c,(-.45,-.6,1.57))
for i in range(3):box(c,(-.60+i*.16,-.6,1.62),(.07,.06,.10),11,.015,label='씨앗_표시')
mark('B06',['2열3단 씨앗함','서랍 고리손잡이','삽·쇠스랑·수납통','소형 관리 작업 공간']);finish('B06')

# B07 공통 밭: 실제 알파 텍스처를 사용하는 plane 군집.
def field(c,W,D,tea=False):
    for x in [-W/2,W/2]:
        for y in [-D/2,D/2]:box(c,(x,y,0),(.35,.35,.55),label='밭_모서리기둥')
    for z in [.12,.32]:
        for y in [-D/2,D/2]:box(c,(0,y,z),(W-.35,.13,.15),label='밭_가로테두리')
        for x in [-W/2,W/2]:box(c,(x,0,z),(.13,D-.35,.15),label='밭_세로테두리')
    if not tea:
        box(c,(0,0,0),(W-.18,D-.18,.12),6,.02,label='공용_토양판')
        for i in range(5):
            for j in range(5):
                x=-1.35+i*.66;y=-1.35+j*.66
                for k,(dx,dy) in enumerate([(0,0),(.10,.05),(-.08,-.05)]):plantcard(c,(x+dx,y+dy,.12),'밀',(i+j+k)%4,k*math.pi/3+.12*(i%3))
    else:
        for y in [-1.15,0,1.15]:
            box(c,(0,y,.03),(W-.45,.79,.12),6,.02,label='차밭_토양이랑')
            for i in range(7):
                x=-2.25+i*.75;rod(c,(x,y,.15),(x,y,.55),.035,0,'차나무_줄기')
                for k in range(6):
                    a=k*math.pi/3;o=plantcard(c,(x+.06*math.cos(a),y+.06*math.sin(a),.12),'차잎',i%4,a);o.rotation_euler.x=.12*(k%2)
c=collections['B07'];field(c,4,4)
mark('B07',['목재 테두리·모서리 기둥','토양판','공용 밀 아틀라스 + plane 군집'],'밀 카드 텍스처는 제작 방식 검토용 초안');finish('B07',0,(-2.7,-2.2,0))

# B08 제빵소: 빵 선반과 우측 실제 아치 오븐·굴뚝.
c=collections['B08'];shell(c,annex=True)
top=furniture(c,(-.3,-1.20,.42),2.45,.75,.80,True)
furniture(c,(-.75,-.95,top),1.35,.50,.62,False,True)
for x in [-1.05,-.65,-.25]:
    bread(c,(x,-.95,top+.13));bread(c,(x,-.95,top+.62))
tray(c,(.55,-1.30,top));bread(c,(.45,-1.30,top+.055));bread(c,(.75,-1.30,top+.055),1)
oven(c,(2.56,1.05,.42));chimney(c,(2.56,1.30,1.87),1.8);chimney(c,(-.7,1.2,2.5),2.4)
crate(c,(-.7,-1.2,.42));sack(c,(1.1,.60,.42))
mark('B08',['전면 빵 판매대·2단 선반','실제 아치 개구부가 있는 오븐','굴뚝 2개','오븐을 덮는 작은 지붕']);finish('B08')

# B09 창고: 닫힌 이중문과 낮은 앞 박공지붕, 적재 소품.
c=collections['B09'];shell(c,annex=True)
for x in [-1.52,1.52]:box(c,(x,-2,.42),(.78,.14,2.36),3,.01,label='창고_문옆벽')
box(c,(0,-2,2.32),(2.25,.14,.46),3,.01,label='창고_문위벽')
for x in [-1.06,1.06]:box(c,(x,-2.10,.42),(.12,.20,1.90),label='창고_이중문틀')
box(c,(0,-2.10,2.23),(2.24,.20,.10),label='창고_문틀상부')
place('A03-02',c,(-1,-2.12,.43));place('A03-02',c,(1,-2.12,.43),(0,0,math.pi))
for x in [-.14,.14]:ring(c,(x,-2.22,1.23),.075,.015,5,(math.pi/2,0,0),'창고문_고리')
roof(c,1.6,-2.7,-.4,.8)
for x in [-1.6,1.6]:
    box(c,(x,-3.4,0),(.8,.8,.42),2,.045,label='창고앞_기초')
    place('A04_V07_받침',c,(x,-3.4,.42))
    place(derived('U13_V07',(.85,.85,1.75/2.2)),c,(x,-3.4,.8),(0,0,0 if x<0 else math.pi))
bar(c,(-1.6,-3.4,2.50),(1.6,-3.4,2.50),.22,'앞지붕_받침보')
box(c,(0,-2.645,.35),(3.2,.71,.07),0,0,label='창고앞_발판')
for x in [-.8,0,.8]:box(c,(x,-3.4,0),(.8,.8,.42),2,.045,label='창고앞_기초')
for j,h in enumerate([.28,.14]):box(c,(0,-3.98-j*.35,0),(1.8,.35,h),2,.025,label='창고_계단')
for p in [(2.9,-1.4,0),(2.9,-1.4,.5),(3.65,-1.4,0)]:crate(c,p)
barrel(c,(3.45,-.60,0));sack(c,(2.75,-2.3,0));sack(c,(3.5,-2.2,0))
mark('B09',['큰 본체와 낮은 앞 박공지붕','이중문·고리손잡이','입구 계단','자루·통·상자 적재']);finish('B09',0,(-2.8,-3.4,0))

# B10 화과자 가게: 판매소 카운터를 쓰되 과자 장반과 3폭 가림막으로 구분.
c=collections['B10'];shell(c,annex=True)
top=furniture(c,(0,-1.1,.42),2.30,.78,.85,True);tray(c,(0,-1.1,top),1.12,.58)
for x in [-.32,0,.32]:sweets(c,(x,-1.1,top+.05))
furniture(c,(.1,1.30,.42),2.3,.55,1.45,False,True);noren(c,-1.15,-2.14,2.63,3,.77)
mark('B10',['화과자 전용 진열 장반','꽃모양 과자','3폭 천 가림막','공용 카운터·뒷선반']);finish('B10')

# B11 공방: 후신소와 다른 L형 장비 작업대·바이스·벽 도구.
c=collections['B11'];shell(c,annex=True)
top=furniture(c,(-.65,-.65,.42),1.8,.75,.75)
furniture(c,(.65,.70,.42),1.8,.65,.75);vice(c,(-.9,-.65,top))
for x in [-1.2,-.45,.45,1.1]:
    box(c,(x,1.85,1.42),(.10,.10,.90),label='벽공구판_세로')
bar(c,(-1.4,1.78,1.95),(1.3,1.78,1.95),.10,'벽공구판_가로')
for x,k in [(-1.1,'톱'),(-.4,'망치'),(.45,'삽')]:tool(c,(x,1.70,1.38),k)
crate(c,(1.05,-1.35,.42));barrel(c,(2.75,1.1,.42))
for x in [2.60,2.80,2.95]:tool(c,(x,1.1,1.0),'망치',(0,(x-2.8)*.5,0))
lantern(c,(-1.65,-1.9,2.22))
mark('B11',['L형 장비 작업 공간','바이스·벽 공구판','초롱','옆 지붕의 공구통']);finish('B11')

# B12 찻집 겸 여관: 긴 반복 골조에서 찻집 개방 구간과 숙박 출입구를 나눈다.
c=collections['B12'];modes={(2,0):'open',(2,1):'open',(2,2):'door',(2,3):'door'}
shell(c,8,side_modes=modes,roof_mode='step')
# 차 마시는 공간 뒤의 낮은 분리벽과 테이블·벤치.
for y in [-3,-1]:
    box(c,(-.55,y,.42),(.12,1.72,2.36),3,.01,label='찻집_뒤벽')
    furniture(c,(.65,y,.42),1.05,.70,.40)
    for yy in [y-.62,y+.62]:furniture(c,(.65,yy,.42),.95,.28,.31)
    pot(c,(.55,y,.82));cup(c,(.95,y-.18,.82));cup(c,(.95,y+.18,.82))
    noren(c,2.10,y-.9,2.62,3,.60,math.pi/2)
furniture(c,(1.3,.05,.42),.9,.65,.85,True);cup(c,(1.30,.05,1.27))
lantern(c,(2.18,-.1,2.30))
# 숙박 출입구의 작은 차양 2개. 구조와 개폐용 문짝은 별개다.
for y in [1,3]:
    for j in range(2):place('R01_V07_옆지붕',c,(1.90,y-.275+j*.55,2.80))
    bar(c,(2.75,y-.55,2.38),(2.75,y+.55,2.38),.14,'여관입구_차양받침')
    for yy in [y-.45,y+.45]:bar(c,(2.10,yy,2.0),(2.75,yy,2.38),.12,'여관입구_차양버팀')
mark('B12',['긴 찻집·여관 복합동','높이가 다른 두 지붕 구간','개방 찻집의 대접상·벤치·차도구','반복 숙박 출입구와 작은 차양'],'방 수·시설 면적·구간 배치는 외형 제안이며 게임 기능 확정 아님')
finish('B12',-math.pi/2,(-5.05,-2.7,0))

# B13 차밭: 줄기 메시와 잎 카드 분리, 세 줄의 반복 이랑.
c=collections['B13'];field(c,6,4,True)
mark('B13',['긴 목재 테두리','3줄 토양 이랑','분리된 줄기와 차 잎 알파 카드'],'차나무 수·줄 수·면적은 시안 대응 제안, 잎 텍스처는 초안');finish('B13',0,(-3.75,-2.2,0))
