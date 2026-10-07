# 임시 디자인용 개별 UI 이미지 (글자 없음 · 한 장 = 한 요소) · 크롬 헤드리스로 투명 PNG
# 2026-10-08 임시 디자인(미승인) · 다시 만들기: python make_images.py · 언리얼에는 /Game/CozyRealm/UI/Textures/Temp 로 가져옴
import os, subprocess
OUT = os.path.dirname(os.path.abspath(__file__))  # 이 폴더에 PNG를 만든다
os.makedirs(OUT, exist_ok=True)
CHROME = r'C:\Program Files\Google\Chrome\Application\chrome.exe'
BASE = '<!doctype html><html><head><meta charset="utf-8"><style>html,body{margin:0;background:transparent;overflow:hidden}</style></head><body>%s</body></html>'

PAPER, PAPER2, WOOD, WOOD2, INK, POINT = '#F3EAD7', '#EADDC2', '#9B7456', '#7C5B43', '#3B3530', '#C85A42'

def box(bg, border, shadow, r=18, bw=4, inset=''):
    return f'<div style="position:absolute;left:0;top:0;right:0;bottom:6px;background:{bg};border:{bw}px solid {border};border-radius:{r}px;box-shadow:0 5px 0 {shadow}{inset};box-sizing:border-box"></div>'

items = {
    # 이름: (폭, 높이, html)
    'T_UI_Window': (96, 96, box(PAPER, WOOD, WOOD2, 24)),
    'T_UI_Panel': (64, 64, f'<div style="position:absolute;inset:0;background:{PAPER2};border-radius:14px"></div>'),
    'T_UI_Chip': (64, 64, box('rgba(243,234,215,.94)', WOOD, WOOD2, 30)),
    'T_UI_Badge': (64, 64, f'<div style="position:absolute;inset:4px;border-radius:50%;background:radial-gradient(circle at 35% 30%,#E07A5F,{POINT});border:3px solid #FFF6E8;box-sizing:border-box"></div>'),
    'T_UI_Button_Normal': (64, 64, box(PAPER2, WOOD, WOOD2, 14)),
    'T_UI_Button_Hovered': (64, 64, box('#F6EBD3', '#B48A66', WOOD2, 14)),
    'T_UI_Button_Pressed': (64, 64, f'<div style="position:absolute;left:0;top:4px;right:0;bottom:2px;background:#E2D2B2;border:4px solid {WOOD2};border-radius:14px;box-sizing:border-box"></div>'),
    'T_UI_Button_Disabled': (64, 64, box('#E4DED4', '#C5BBAE', '#B3A99C', 14)),
    'T_UI_ButtonMain_Normal': (64, 64, box('linear-gradient(#F7E3A6,#EFCB72)', '#B98B3E', '#8E6A2C', 14)),
    'T_UI_ButtonMain_Hovered': (64, 64, box('linear-gradient(#FBEBBD,#F3D489)', '#C69849', '#8E6A2C', 14)),
    'T_UI_ButtonMain_Pressed': (64, 64, f'<div style="position:absolute;left:0;top:4px;right:0;bottom:2px;background:#E8C266;border:4px solid #8E6A2C;border-radius:14px;box-sizing:border-box"></div>'),
    'T_UI_ButtonRound_Normal': (64, 64, f'<div style="position:absolute;left:2px;top:0;width:60px;height:58px;border-radius:18px;background:{PAPER};border:3px solid {WOOD};box-shadow:0 4px 0 {WOOD2};box-sizing:border-box"></div>'),
    'T_UI_ButtonRound_Hovered': (64, 64, f'<div style="position:absolute;left:2px;top:0;width:60px;height:58px;border-radius:18px;background:#FFF6E2;border:3px solid #B48A66;box-shadow:0 4px 0 {WOOD2};box-sizing:border-box"></div>'),
    'T_UI_ButtonRound_Pressed': (64, 64, f'<div style="position:absolute;left:2px;top:4px;width:60px;height:56px;border-radius:18px;background:#E2D2B2;border:3px solid {WOOD2};box-sizing:border-box"></div>'),
    'T_UI_Gauge_Back': (64, 20, f'<div style="position:absolute;inset:0;background:#E2D7C4;border:2px solid {WOOD};border-radius:10px;box-sizing:border-box"></div>'),
    'T_UI_Gauge_Fill': (64, 20, '<div style="position:absolute;inset:2px;background:#FFFFFF;border-radius:8px"></div>'),
}

def icon(inner, bg):
    return f'<div style="position:absolute;inset:2px;border-radius:50%;background:{bg};border:2px solid rgba(0,0,0,.14);box-sizing:border-box;display:grid;place-items:center;font:700 30px \'Segoe UI Emoji\',\'Segoe UI Symbol\',sans-serif;color:{INK}">{inner}</div>'

icons = {
    'T_UI_Icon_Gold': ('●', '#F2D98A'), 'T_UI_Icon_Wheat': ('🌾', '#E8CF8E'), 'T_UI_Icon_Flour': ('◍', '#F3EBDD'),
    'T_UI_Icon_Talisman': ('✦', '#D7EEF7'), 'T_UI_Icon_Shard': ('❖', '#E9D1D5'), 'T_UI_Icon_Storage': ('📦', '#F3EAD7'),
    'T_UI_Icon_Residents': ('🏠', '#F3EAD7'), 'T_UI_Icon_Placement': ('✥', '#F3EAD7'), 'T_UI_Icon_Menu': ('☰', '#F3EAD7'),
    'T_UI_Icon_Collect': ('🧺', '#F7E3A6'), 'T_UI_Icon_Upgrade': ('⛏', '#F3EAD7'), 'T_UI_Icon_Preview': ('👁', '#E7F2F8'),
    'T_UI_Icon_Sun': ('☀', '#FBE7B8'), 'T_UI_Icon_Lock': ('🔒', '#E4E6EA'),
}
for k, (g, bg) in icons.items():
    items[k] = (64, 64, icon(g, bg))

for name, (w, h, html) in items.items():
    hp = os.path.join(OUT, name + '.html')
    open(hp, 'w', encoding='utf-8').write(BASE % html)
    subprocess.run([CHROME, '--headless=new', '--disable-gpu', '--hide-scrollbars', '--default-background-color=00000000',
                    f'--user-data-dir={os.path.join(OUT, "_chrome")}', f'--window-size={w},{h}',
                    f'--screenshot={os.path.join(OUT, name + ".png")}', 'file:///' + hp.replace('\\', '/')],
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    os.remove(hp)
print(len(items), 'images')
