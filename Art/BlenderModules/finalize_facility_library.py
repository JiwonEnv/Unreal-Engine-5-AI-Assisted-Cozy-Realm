import bpy
from pathlib import Path
p=Path(__file__).resolve().parent/'V07_시설전체_제분소기준'/'CozyRealm_V07_시설13종_공용모듈_140cm.blend'
bpy.ops.wm.open_mainfile(filepath=str(p))
current=bpy.data.collections['00_현재사용_공용원형']
for i,o in enumerate(current.objects):o.location=((i%10)*5,(i//10)*5,0)
bpy.context.window.scene=bpy.data.scenes['B03_판매소_단독검토']
bpy.ops.wm.save_as_mainfile(filepath=str(p))
print('LIBRARY_LAYOUT_SAVED',len(current.objects))
