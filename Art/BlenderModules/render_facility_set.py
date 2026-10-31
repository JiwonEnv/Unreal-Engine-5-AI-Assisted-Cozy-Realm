import bpy
from pathlib import Path
from mathutils import Vector
p=Path(__file__).resolve().parent/'V07_시설전체_제분소기준'
bpy.ops.wm.open_mainfile(filepath=str(p/'CozyRealm_V07_시설13종_공용모듈_140cm.blend'))
cols={c['관리_ID']:c for c in bpy.data.collections if c.get('관리_ID')}
for code,c in sorted(cols.items()):
    if code=='B02':continue
    scene=bpy.data.scenes[code+'_'+c.name[4:]+'_단독검토'];bpy.context.window.scene=scene
    scene.render.filepath=str(p/(code+'_'+c.name[4:]+'_모델.png'));bpy.ops.render.render(write_still=True,scene=scene.name)
    print('FINAL_RENDER',code,flush=True)
for code in ['B04','B09','B12']:
    c=cols[code];scene=bpy.data.scenes[code+'_'+c.name[4:]+'_단독검토'];bpy.context.window.scene=scene
    root=next(o for o in c.objects if o.type=='EMPTY');target=root.location+Vector((0,0,1.9));cam=scene.camera;size=cam.data.ortho_scale
    cam.location=target+Vector((size*.7,size,size*.25));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
    scene.render.resolution_x=1400;scene.render.resolution_y=1200;scene.render.filepath=str(p/(code+'_후면_검토.png'));bpy.ops.render.render(write_still=True,scene=scene.name)
scene=bpy.data.scenes['01_시설13종_조립검토'];scene.render.resolution_x=2000;scene.render.resolution_y=1700;scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.render.filepath=str(p/'시설13종_전체배치.png');bpy.ops.render.render(write_still=True,scene=scene.name)
print('FINAL_RENDERS_DONE',flush=True)
