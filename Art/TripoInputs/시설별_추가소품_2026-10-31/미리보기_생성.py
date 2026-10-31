from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import json
p=Path(__file__).parent
items=json.loads((p/'생성목록.json').read_text(encoding='utf-8'))
font=ImageFont.truetype('C:/Windows/Fonts/malgun.ttf',19)
canvas=Image.new('RGB',(1400,7*300),(242,240,235))
d=ImageDraw.Draw(canvas)
report=[]
for i,it in enumerate(items):
    f=p/it['파일명']
    im=Image.open(f).convert('RGB')
    report.append({'파일':f.name,'크기':list(im.size)})
    im.thumbnail((330,260))
    x=(i%4)*350+(350-im.width)//2;y=(i//4)*300
    canvas.paste(im,(x,y))
    d.text(((i%4)*350+10,y+267),it['시설ID']+' '+it['소품'],font=font,fill=(40,40,40))
canvas.save(p/'시설별_추가소품_전체미리보기.jpg',quality=92)
(p/'파일검사.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
lines=['# 시설별 추가 소품 — Tripo 단독 입력 이미지','', '13개 시설 × 2개 소품 = 총 26개 개별 PNG. 내장 이미지 생성 도구로 만든 외형 제안이며, 실제 메시·베이크 결과 또는 사용자 최종 확정 디자인이 아니다.','', '## 사용 방법','', '- 생성목록의 개별 PNG 하나씩 Tripo 이미지 입력에 사용한다. 전체 미리보기는 입력하지 않는다.','- 이번 작업은 이미지 제작까지이며 Tripo 3D 생성·크레딧 사용은 실행하지 않았다.','- 이미지별 실제 해상도는 파일검사.json에 기록했다. 프롬프트의 2048 요청과 실제 출력 크기는 다를 수 있다.','- 생성된 3D는 Blender에서 140cm 주인공 기준 크기, 밑면 중심 피벗, 두께, 구멍, 토폴로지, UV를 검토한다.','- 수레 바퀴·저울·손잡이·바구니 끈·체 망은 생성 후 분리/수정이 필요할 수 있다.','- 목재는 넓은 면과 선택적인 긴 홈을 기준으로 재수정한다. 잔결은 최종 재질에서 줄일 수 있다.','- 자루·상자·바구니·통·초롱은 여러 시설에서 재사용한다. 기존 P01~P12 관리 항목을 자동으로 늘리거나 별도 신규 제작을 확정한 것이 아니다.','- 각 시설의 기능과 노션 v7 조형·재질 기준을 참고했으며, 새 소품 선정과 세부 외형은 AI 제안이다.','', '## 시설별 도안','', '| 시설 | 소품 1 | 소품 2 |','| --- | --- | --- |']
for i in range(0,len(items),2):
    a,b=items[i:i+2]
    lines.append(f"| {a['시설ID']} {a['시설']} | [{a['소품']}]({a['파일명']}) | [{b['소품']}]({b['파일명']}) |")
lines+=['','노션 기준 출처: https://app.notion.com/p/3fe10d86fdd581c1bf74c6aa14686d5f','', '전체 이미지별 최종 프롬프트는 생성목록.json에 보존했다.']
(p/'사용안내.md').write_text('\n'.join(lines),encoding='utf-8')
print('이미지 26개 검사 및 전체 미리보기 생성 완료')
