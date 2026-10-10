# 마을 테마에 없던 팥·쌀·쌀가루 아이콘 3장을 SVG로 그려 투명 PNG로 만든다 (헤드리스 크롬 캡처 · make_lock_preview.py와 같은 방식)
# 결과: T_Cozy_RedBean.png · T_Cozy_Rice.png · T_Cozy_RiceFlour.png (512×512, 투명 배경, 글자 없음)
# 다시 만들기: python make_crop_icons.py  (크롬 필요)
import os, subprocess, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CHROME = r'C:\Program Files\Google\Chrome\Application\chrome.exe'
INK = '#4a2a17'

def bean(cx, cy, rot, s=1.0):
    return (f'<g transform="translate({cx} {cy}) rotate({rot}) scale({s})">'
            f'<ellipse cx="0" cy="0" rx="46" ry="32" fill="url(#bean)" stroke="{INK}" stroke-width="10"/>'
            f'<path d="M-10 -22 q16 -6 30 4" fill="none" stroke="#fbe3d6" stroke-width="7" stroke-linecap="round"/>'
            f'<path d="M-24 6 q10 6 22 2" fill="none" stroke="#f3efe3" stroke-width="6" stroke-linecap="round"/></g>')

REDBEAN = f'''
<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
 <defs>
  <radialGradient id="bean" cx="0.35" cy="0.3" r="0.8"><stop offset="0" stop-color="#d4556b"/><stop offset="0.55" stop-color="#9c2336"/><stop offset="1" stop-color="#5e1020"/></radialGradient>
  <linearGradient id="dish" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#d79a5c"/><stop offset="1" stop-color="#8f5426"/></linearGradient>
 </defs>
 <path d="M70 300 Q256 480 442 300 Z" fill="url(#dish)" stroke="{INK}" stroke-width="16" stroke-linejoin="round"/>
 <ellipse cx="256" cy="300" rx="186" ry="44" fill="#b9773d" stroke="{INK}" stroke-width="16"/>
 {bean(180, 272, -20)}{bean(268, 262, 15)}{bean(350, 276, -8)}
 {bean(222, 220, 25)}{bean(310, 214, -18)}{bean(262, 172, 5)}
 <path d="M140 372 Q256 430 372 372" fill="none" stroke="#f0c48f" stroke-width="10" stroke-linecap="round" opacity="0.7"/>
</svg>'''

def grain(cx, cy, rot):
    return (f'<g transform="translate({cx} {cy}) rotate({rot})">'
            f'<ellipse cx="0" cy="0" rx="17" ry="30" fill="#fffdf6" stroke="{INK}" stroke-width="7"/>'
            f'<path d="M-5 -14 q6 -6 10 2" fill="none" stroke="#e7dcc5" stroke-width="4" stroke-linecap="round"/></g>')

RICE = f'''
<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
 <defs>
  <linearGradient id="bowl" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#4f6fb0"/><stop offset="1" stop-color="#2a3f72"/></linearGradient>
 </defs>
 <path d="M150 128 Q230 70 300 118 Q352 150 330 198" fill="none" stroke="{INK}" stroke-width="22" stroke-linecap="round"/>
 <path d="M150 128 Q230 70 300 118 Q352 150 330 198" fill="none" stroke="#e9c35a" stroke-width="10" stroke-linecap="round"/>
 <g fill="#f2cf63" stroke="{INK}" stroke-width="6">
  <ellipse cx="196" cy="104" rx="14" ry="24" transform="rotate(-50 196 104)"/>
  <ellipse cx="246" cy="92" rx="14" ry="24" transform="rotate(-80 246 92)"/>
  <ellipse cx="296" cy="112" rx="14" ry="24" transform="rotate(-120 296 112)"/>
  <ellipse cx="326" cy="158" rx="14" ry="24" transform="rotate(-160 326 158)"/>
 </g>
 <path d="M96 238 Q256 196 416 238 Q256 270 96 238Z" fill="#fffaf0" stroke="{INK}" stroke-width="12"/>
 {grain(170, 222, -30)}{grain(214, 206, 10)}{grain(258, 200, -15)}{grain(302, 206, 20)}{grain(344, 222, -10)}{grain(236, 236, 70)}{grain(286, 236, -60)}
 <path d="M86 244 Q256 300 426 244 Q410 420 256 432 Q102 420 86 244Z" fill="url(#bowl)" stroke="{INK}" stroke-width="16" stroke-linejoin="round"/>
 <path d="M126 300 Q256 340 386 300" fill="none" stroke="#9fb7e6" stroke-width="10" stroke-linecap="round" opacity="0.8"/>
 <g fill="#ffb7cf" stroke="{INK}" stroke-width="5" transform="translate(256 372) scale(0.55)">
  <path d="M0 -50 c22 -30 60 -6 44 22 c34 0 34 44 0 44 c12 32 -26 50 -44 24 c-18 26 -56 8 -44 -24 c-34 0 -34 -44 0 -44 c-16 -28 22 -52 44 -22z"/>
 </g>
 <circle cx="256" cy="372" r="8" fill="#f5c94a" stroke="{INK}" stroke-width="4"/>
</svg>'''

RICEFLOUR = f'''
<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
 <defs>
  <linearGradient id="sack" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#fff8ee"/><stop offset="0.6" stop-color="#f1e1cf"/><stop offset="1" stop-color="#d9c2a8"/></linearGradient>
 </defs>
 <path d="M150 170 Q120 300 128 400 Q140 452 256 456 Q372 452 384 400 Q392 300 362 170 Z" fill="url(#sack)" stroke="{INK}" stroke-width="16" stroke-linejoin="round"/>
 <path d="M150 170 Q256 120 362 170 Q330 132 256 128 Q182 132 150 170Z" fill="#ffffff" stroke="{INK}" stroke-width="14" stroke-linejoin="round"/>
 <ellipse cx="256" cy="150" rx="88" ry="26" fill="#ffffff"/>
 <path d="M166 186 Q256 212 346 186" fill="none" stroke="#e58aa6" stroke-width="22" stroke-linecap="round"/>
 <path d="M166 186 Q256 212 346 186" fill="none" stroke="{INK}" stroke-width="6" stroke-linecap="round" opacity="0.5"/>
 <path d="M346 186 q30 10 40 40 q-26 -2 -40 -18" fill="#e58aa6" stroke="{INK}" stroke-width="6"/>
 <g transform="translate(256 318)">
  <circle r="66" fill="#fde7ef" stroke="{INK}" stroke-width="8"/>
  <g fill="#fffdf6" stroke="{INK}" stroke-width="6">
   <ellipse cx="-22" cy="-8" rx="13" ry="24" transform="rotate(-25 -22 -8)"/>
   <ellipse cx="14" cy="-14" rx="13" ry="24" transform="rotate(15 14 -14)"/>
   <ellipse cx="0" cy="24" rx="13" ry="24" transform="rotate(80 0 24)"/>
  </g>
 </g>
 <path d="M160 420 Q256 440 352 420" fill="none" stroke="#c9ab8a" stroke-width="8" stroke-linecap="round" opacity="0.7"/>
</svg>'''

def render(svg, out):
    with tempfile.TemporaryDirectory() as tmp:
        page = os.path.join(tmp, 'icon.html')
        with open(page, 'w', encoding='utf-8') as f:
            f.write('<html><body style="margin:0;background:transparent">' + svg + '</body></html>')
        subprocess.run([CHROME, '--headless=new', '--disable-gpu', '--hide-scrollbars', '--default-background-color=00000000',
                        '--window-size=512,512', f'--screenshot={out}', 'file:///' + page.replace('\\', '/')], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

for name, svg in [('T_Cozy_RedBean', REDBEAN), ('T_Cozy_Rice', RICE), ('T_Cozy_RiceFlour', RICEFLOUR)]:
    render(svg, os.path.join(HERE, name + '.png'))
print('만듦 T_Cozy_RedBean.png · T_Cozy_Rice.png · T_Cozy_RiceFlour.png')
