from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import json, zipfile
p=Path(__file__).parent
src=p.parent/'시설별_추가소품_2026-10-31'
items=json.loads((src/'생성목록.json').read_text(encoding='utf-8'))
font=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',20)
small=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',16)
views=['01_정면','02_측면','03_후면']
report=[]
pages=[]
for start in range(0,26,7):
    subset=items[start:start+7]
    sheet=Image.new('RGB',(1280,80+len(subset)*320),(244,242,238))
    draw=ImageDraw.Draw(sheet)
    for c,label in enumerate(['원본','정면','오른쪽 측면','후면']):
        draw.text((c*320+10,15),label,font=font,fill=(35,35,35))
    for row,it in enumerate(subset):
        name=f"{it['시설ID']}_{it['시설']}_{it['소품']}"
        folder=p/name
        files=[src/it['파일명']]+[folder/(v+'.png') for v in views]
        for col,f in enumerate(files):
            if not f.exists():
                if col:report.append({'소품':name,'시점':views[col-1],'누락':True})
                continue
            with Image.open(f) as image:
                image.verify()
            with Image.open(f) as image:
                size=image.size
                thumb=image.convert('RGB');thumb.thumbnail((300,280))
            sheet.paste(thumb,(col*320+(320-thumb.width)//2,65+row*320))
            if col:report.append({'소품':name,'시점':views[col-1],'파일':str(f),'크기':list(size),'누락':False})
        draw.text((10,65+row*320+285),name,font=small,fill=(35,35,35))
    out=p/f'전체비교_{start//7+1:02d}.jpg'
    sheet.save(out,quality=91)
    pages.append(out)
(p/'파일검사.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
lines=['# 시설 추가 소품 — 원본과 삼면도','', '26종 프랍의 정면·오른쪽 측면·후면을 각각 별도 PNG로 생성한 디자인 추정도다. 실제 동일 3D 모델을 렌더한 정투영도가 아니므로 치수·윤곽·숨은 구조가 완벽히 일치한다고 보증하지 않는다.','', '- 각 프랍 폴더: 01_정면.png / 02_측면.png / 03_후면.png','- 원본은 인접 폴더 시설별_추가소품_2026-10-31에 보존한다.','- 원본과 새 시점 간 형태 불일치는 Tripo 투입 전에 비교한다. 불일치가 크면 원본 한 장으로 초안을 만든 뒤 Blender에서 구조를 고정하고 실제 렌더 삼면도를 쓰는 편이 안정적이다.','- 원본 사진의 보이지 않던 구조는 AI 추정 제안이다. 기존 v7 확정 기준을 변경하지 않는다.','- 각 기록 JSON에 기준 이미지와 실제 사용 프롬프트를 저장했다. 내장 이미지 생성 도구를 사용했다.','- Tripo 모델 생성 및 크레딧 사용은 실행하지 않았다.','', '## 소품별 폴더','']
for it in items:
    name=f"{it['시설ID']}_{it['시설']}_{it['소품']}"
    lines.append(f'- [{name}]({name}/01_정면.png)')
(p/'사용안내.md').write_text('\n'.join(lines),encoding='utf-8')
missing=[x for x in report if x['누락']]
print(json.dumps({'이미지수':len(report)-len(missing),'누락':missing},ensure_ascii=False))
if not missing:
    target=p.parent/'시설별_추가소품_26종_원본과삼면도_104장.zip'
    with zipfile.ZipFile(target,'w',zipfile.ZIP_STORED) as z:
        for it in items:
            z.write(src/it['파일명'],'원본26장/'+it['파일명'])
        for f in p.rglob('*'):
            if f.is_file() and f.suffix!='.py' and '이전검토' not in f.name:z.write(f,'삼면도78장/'+str(f.relative_to(p)))
    with zipfile.ZipFile(target) as z:
        assert z.testzip() is None
        assert len([n for n in z.namelist() if n.endswith('.png')])==104
    print(str(target))
