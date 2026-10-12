# 마을 테마용 자물쇠·미리보기 아이콘 2장을 SVG로 그려 투명 PNG로 만든다 (헤드리스 크롬 캡처)
# 결과: T_Cozy_Lock.png · T_Cozy_Preview.png (512×512, 투명 배경, 글자 없음)
# 다시 만들기: python make_lock_preview.py  (크롬 필요)
import os, subprocess, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
CHROME = r'C:\Program Files\Google\Chrome\Application\chrome.exe'
INK = '#4a2a17'  # 짙은 갈색 윤곽 (기존 아이콘과 같은 계열)

LOCK = f'''
<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
 <defs>
  <linearGradient id="body" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffe27a"/><stop offset="0.55" stop-color="#f4b531"/><stop offset="1" stop-color="#c9811c"/></linearGradient>
  <linearGradient id="shackle" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#b9b3a6"/><stop offset="0.45" stop-color="#f1ede4"/><stop offset="1" stop-color="#9c9586"/></linearGradient>
 </defs>
 <path d="M150 238 V170 a106 106 0 0 1 212 0 V238" fill="none" stroke="{INK}" stroke-width="64" stroke-linecap="round"/>
 <path d="M150 238 V170 a106 106 0 0 1 212 0 V238" fill="none" stroke="url(#shackle)" stroke-width="36" stroke-linecap="round"/>
 <rect x="86" y="218" width="340" height="250" rx="54" fill="url(#body)" stroke="{INK}" stroke-width="16"/>
 <rect x="110" y="236" width="292" height="40" rx="20" fill="#fff6c8" opacity="0.65"/>
 <path d="M118 420 Q256 452 394 420" fill="none" stroke="#a96512" stroke-width="10" stroke-linecap="round" opacity="0.6"/>
 <circle cx="256" cy="322" r="34" fill="#5b3a22" stroke="{INK}" stroke-width="8"/>
 <path d="M240 340 L232 404 H280 L272 340 Z" fill="#5b3a22" stroke="{INK}" stroke-width="8" stroke-linejoin="round"/>
 <circle cx="248" cy="314" r="9" fill="#9a7a5c"/>
 <g fill="#ffc1d6" stroke="{INK}" stroke-width="5">
  <path d="M398 250 c18 -22 52 -8 44 18 c26 4 26 40 -2 40 c4 28 -32 38 -42 12 c-22 16 -48 -10 -30 -30 c-20 -14 -6 -46 30 -40z"/>
 </g>
 <circle cx="406" cy="276" r="9" fill="#f5c94a" stroke="{INK}" stroke-width="4"/>
</svg>'''

PREVIEW = f'''
<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
 <defs>
  <radialGradient id="glass" cx="0.38" cy="0.35" r="0.7"><stop offset="0" stop-color="#ffffff"/><stop offset="0.4" stop-color="#cfeefb"/><stop offset="1" stop-color="#6fb3d8"/></radialGradient>
  <linearGradient id="rim" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#ffe27a"/><stop offset="0.6" stop-color="#e9a62a"/><stop offset="1" stop-color="#b8731a"/></linearGradient>
  <linearGradient id="wood" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#c98a4b"/><stop offset="0.5" stop-color="#9a5e2c"/><stop offset="1" stop-color="#6e3f1c"/></linearGradient>
 </defs>
 <g transform="rotate(45 360 360)">
  <rect x="318" y="300" width="84" height="196" rx="34" fill="url(#wood)" stroke="{INK}" stroke-width="16"/>
  <rect x="312" y="292" width="96" height="44" rx="16" fill="url(#rim)" stroke="{INK}" stroke-width="12"/>
  <path d="M342 360 V470" stroke="#e0aa6c" stroke-width="10" stroke-linecap="round" opacity="0.7"/>
 </g>
 <circle cx="210" cy="210" r="150" fill="url(#rim)" stroke="{INK}" stroke-width="18"/>
 <circle cx="210" cy="210" r="114" fill="url(#glass)" stroke="{INK}" stroke-width="10"/>
 <path d="M140 170 a80 80 0 0 1 58 -54" fill="none" stroke="#ffffff" stroke-width="18" stroke-linecap="round" opacity="0.9"/>
 <circle cx="128" cy="214" r="9" fill="#ffffff" opacity="0.9"/>
 <g transform="translate(232 232) scale(0.9)" stroke="{INK}" stroke-width="6" fill="#ffb7cf">
  <path d="M0 -50 c22 -30 60 -6 44 22 c34 0 34 44 0 44 c12 32 -26 50 -44 24 c-18 26 -56 8 -44 -24 c-34 0 -34 -44 0 -44 c-16 -28 22 -52 44 -22z"/>
 </g>
 <circle cx="232" cy="236" r="12" fill="#f5c94a" stroke="{INK}" stroke-width="5"/>
</svg>'''

def render(svg, out):
    with tempfile.TemporaryDirectory() as tmp:
        page = os.path.join(tmp, 'icon.html')
        with open(page, 'w', encoding='utf-8') as f:
            f.write('<html><body style="margin:0;background:transparent">' + svg + '</body></html>')
        subprocess.run([CHROME, '--headless=new', '--disable-gpu', '--hide-scrollbars', '--default-background-color=00000000',
                        '--window-size=512,512', f'--screenshot={out}', 'file:///' + page.replace('\\', '/')], check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

render(LOCK, os.path.join(HERE, 'T_Cozy_Lock.png'))
render(PREVIEW, os.path.join(HERE, 'T_Cozy_Preview.png'))
print('만듦 T_Cozy_Lock.png · T_Cozy_Preview.png')
