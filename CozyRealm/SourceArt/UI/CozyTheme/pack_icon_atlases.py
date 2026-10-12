"""아이콘 원본을 종류별 공유 텍스처로 배치한다. 외부 패키지는 Pillow만 필요하다.

기존 이미지 이름과 원본 연결은 atlas_manifest.json에 보존한다.
게임용 영역은 128픽셀 칸, 실제 그림은 최대 112픽셀이며 경계에 8픽셀 이상 여백을 둔다.
"""
from pathlib import Path
import json, math
from PIL import Image

ROOT = Path(__file__).resolve().parent
GROUPS = {
    'Resources': ['Wheat', 'Flour', 'Gold', 'Talisman', 'Shard', 'Clock', 'RedBean', 'Rice', 'RiceFlour'],
    'Facilities': ['Shrine', 'Mill', 'Nagaya', 'Workshop', 'Shop', 'FieldManager', 'Field', 'Storage'],
    'Common': ['Onibi', 'Sakura', 'Lock', 'Preview'],
}
NEW_CELLS = {'RedBean': (0, 0), 'Rice': (1, 0), 'RiceFlour': (2, 0), 'Lock': (0, 1), 'Preview': (1, 1)}

def tight_image(im):
    im = im.convert('RGBA')
    bbox = im.getchannel('A').point(lambda a: 255 if a >= 16 else 0).getbbox()
    if bbox is None:
        raise ValueError('비어 있는 아이콘 원본')
    return im.crop(bbox)

def build():
    painted = ROOT / 'Painted'
    painted.mkdir(exist_ok=True)
    for name, (gx,gy) in NEW_CELLS.items():
        target = painted / ('T_Cozy_' + name + '.png')
        if not target.exists():
            new_sheet = Image.open(ROOT / 'CozyAdditionalIcons_20261011_draft.png').convert('RGBA')
            cw, ch = new_sheet.width // 3, new_sheet.height // 2
            new_sheet.crop((gx*cw,gy*ch,(gx+1)*cw,(gy+1)*ch)).save(target)
    out = ROOT / 'Atlases'
    out.mkdir(exist_ok=True)
    manifest = {'version': 1, 'cellSize': 128, 'innerSize': 112, 'padding': 8, 'groups': {}, 'icons': {}}
    for group, names in GROUPS.items():
        columns = {'Resources':3, 'Facilities':4, 'Common':2}[group]
        rows = math.ceil(len(names) / columns)
        atlas = Image.new('RGBA', (columns * 128, rows * 128), (0, 0, 0, 0))
        atlas_name = 'T_CozyAtlas_' + group
        manifest['groups'][group] = {'texture': atlas_name, 'width': atlas.width, 'height': atlas.height}
        for index, name in enumerate(names):
            original = ROOT / ('T_Cozy_' + name + '.png')
            if name in NEW_CELLS:
                original = painted / ('T_Cozy_' + name + '.png')
                raw = Image.open(original)
                source_name = 'Painted/' + original.name
            else:
                raw = Image.open(original)
                source_name = original.name
            icon = tight_image(raw)
            icon.thumbnail((112, 112), Image.Resampling.LANCZOS)
            x, y = index % columns * 128, index // columns * 128
            atlas.alpha_composite(icon, (x + (128-icon.width)//2, y + (128-icon.height)//2))
            manifest['icons'][name] = {'group': group, 'texture': atlas_name, 'origin': [x,y], 'size': [128,128],
                                       'source': source_name, 'paintedSize': list(icon.size)}
        atlas.save(out / (atlas_name + '.png'))
    aliases = ROOT / 'atlas_aliases.json'
    if aliases.exists():manifest['aliases'] = json.loads(aliases.read_text(encoding='utf-8'))
    (out / 'atlas_manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')
    return manifest

if __name__ == '__main__':
    result = build()
    print('공유 아이콘 시트 제작 완료:', len(result['icons']), '개 /', len(result['groups']), '장')
