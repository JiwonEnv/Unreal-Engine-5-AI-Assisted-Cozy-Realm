from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import json,zipfile
p=Path(__file__).parent
items=json.loads((p/'제작목록.json').read_text(encoding='utf-8'))
views=['00_원본','01_정면','02_측면','03_후면']
font=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',18)
report=[]
for start in range(0,13,7):
    subset=items[start:start+7]
    canvas=Image.new('RGB',(1280,60+len(subset)*310),(243,241,237))
    d=ImageDraw.Draw(canvas)
    for c,v in enumerate(views):d.text((c*320+12,12),v[3:],font=font,fill=(35,35,35))
    for row,it in enumerate(subset):
        for col,v in enumerate(views):
            f=p/it['폴더']/(v+'.png')
            rec={'소품':it['소품'],'시점':v,'파일':str(f),'존재':f.exists()}
            if f.exists():
                with Image.open(f) as im:im.verify()
                with Image.open(f) as im:
                    rec['크기']=list(im.size)
                    thumb=im.convert('RGB');thumb.thumbnail((300,270))
                canvas.paste(thumb,(col*320+(320-thumb.width)//2,50+row*310))
            report.append(rec)
        d.text((12,50+row*310+275),it['폴더'],font=font,fill=(35,35,35))
    canvas.save(p/f'시점비교_{start//7+1:02d}.jpg',quality=92)
canvas=Image.new('RGB',(1280,4*325),(243,241,237));d=ImageDraw.Draw(canvas)
for i,it in enumerate(items):
    f=p/it['폴더']/'00_원본.png'
    if f.exists():
        with Image.open(f) as im:thumb=im.convert('RGB');thumb.thumbnail((300,280))
        canvas.paste(thumb,((i%4)*320+(320-thumb.width)//2,(i//4)*325))
    d.text(((i%4)*320+8,(i//4)*325+287),it['시설ID']+' '+it['소품'],font=font,fill=(35,35,35))
canvas.save(p/'추가13종_전체미리보기.jpg',quality=92)
(p/'파일검사.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
lines=['# 시설별 추가 소품 2차','', '기존 26종에 추가하는 13종 외형 제안. 각 소품은 원본 단독 컷 + 정면·오른쪽 측면·후면 3장, 총 52장이다.','', '- 내장 이미지 생성 도구로 제작했다. 실제 메시, 베이크, 동일 모델에서 렌더한 정확한 정투영도가 아니다.','- 보이지 않던 면과 새 소품 세부 디자인은 AI 제안이며 기존 v7 확정 기준을 바꾸지 않는다.','- Tripo에 쓰기 전 원본과 시점별 형상이 맞는지 확인한다. 불일치하면 원본 단독 컷을 우선하고 Blender에서 구조를 고정한다.','- 크기·피벗·접합·폴리곤·UV는 Blender에서 별도 검토한다.','- 이번 작업에서 Tripo 3D 생성·크레딧 사용은 실행하지 않았다.','- 각 이미지에 대응하는 기록 JSON에 실제 프롬프트와 원본 경로를 남겼다.','', '| 시설 | 추가 소품 |','| --- | --- |']
for it in items:lines.append(f"| {it['시설ID']} {it['시설']} | [{it['소품']}]({it['폴더']}/00_원본.png) |")
(p/'사용안내.md').write_text('\n'.join(lines),encoding='utf-8')
missing=[r for r in report if not r['존재']]
print(json.dumps({'완료':len(report)-len(missing),'누락수':len(missing)},ensure_ascii=False))
if not missing:
    target=p.parent/'시설별_추가소품_2차_13종_원본과삼면도_52장.zip'
    with zipfile.ZipFile(target,'w',zipfile.ZIP_STORED) as z:
        for it in items:
            for v in views:
                f=p/it['폴더']/(v+'.png');z.write(f,str(f.relative_to(p)))
            for f in (p/it['폴더']).glob('*_기록.json'):z.write(f,str(f.relative_to(p)))
        for f in list(p.glob('*.json'))+list(p.glob('*.jpg'))+list(p.glob('*.md')):z.write(f,f.name)
    with zipfile.ZipFile(target) as z:
        assert z.testzip() is None
        assert sum(n.endswith('.png') for n in z.namelist())==52
    print(str(target))
