import bpy, math
from pathlib import Path
from mathutils import Vector
out=Path(__file__).resolve().parent
path=out/'CozyRealm_공용모듈과시설_140cm_로우폴.blend'
bpy.ops.wm.open_mainfile(filepath=str(path))
scene=bpy.data.scenes.new('05_피벗_접합기준_확인')
scene.unit_settings.system='METRIC';scene.unit_settings.length_unit='CENTIMETERS'
scene.world=bpy.data.worlds.new('피벗검토_환경');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.5,.5,.5,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.5
scene.render.engine='CYCLES';scene.cycles.samples=24
scene.render.resolution_x=1800;scene.render.resolution_y=1200
bpy.context.window.scene=scene
library=bpy.data.collections['00_공용모듈_원형_실제메시공유']
sources={o['원형_ID']:o for o in library.all_objects if o.type=='MESH'}
def mat(name,color):
    m=bpy.data.materials.new(name);m.diffuse_color=(*color,1);m.use_nodes=True;m.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(*color,1);return m
colors=[mat('피벗_X_빨강',(.8,.03,.03)),mat('피벗_Y_초록',(.03,.6,.06)),mat('피벗_Z_파랑',(.03,.14,.9))]
font=bpy.data.fonts.get('맑은 고딕')
def label(body,loc,size=.19):
    c=bpy.data.curves.new('피벗안내','FONT');c.body=body;c.size=size;c.align_x='CENTER';c.font=font
    o=bpy.data.objects.new(body,c);scene.collection.objects.link(o);o.location=loc;c.materials.append(bpy.data.materials['안내문자'])
def arrow(start,end,color):
    delta=Vector(end)-Vector(start);mid=(Vector(start)+Vector(end))*.5
    bpy.ops.mesh.primitive_cylinder_add(vertices=8,radius=.018,depth=delta.length,location=mid)
    o=bpy.context.object;o.name='원점축_가이드';o.rotation_euler=delta.to_track_quat('Z','Y').to_euler();o.data.materials.append(color)
    bpy.ops.mesh.primitive_cone_add(vertices=8,radius1=.055,depth=.14,location=end)
    o=bpy.context.object;o.name='원점축_화살표';o.rotation_euler=delta.to_track_quat('Z','Y').to_euler();o.data.materials.append(color)
items=[('W01-01','기둥: 밑면 중심'),('W02-02','보: 시작 접합면 중심'),('R01-01','지붕 널: 시작 접합면'),('A03-02','문짝: 경첩 밑'),('A04-01','받침석: 밑면 중심'),('U13','휜 기둥: 밑면 접합점')]
for i,(code,desc) in enumerate(items):
    p=Vector(((i%3)*4,(i//3)*4,.12));o=bpy.data.objects.new(code+'_피벗확인',sources[code].data);scene.collection.objects.link(o);o.location=p
    for axis in range(3):
        v=Vector((0,0,0));v[axis]=.6;arrow(p,p+v,colors[axis])
    label(code+' / '+desc,(p.x+.5,p.y-.6,.04))
label('화살표 교차점 = 실제 Object Origin / 빨강 X · 초록 Y · 파랑 Z',(4,-1.6,.04),.24)
floor=mat('피벗전시바닥',(.72,.74,.75))
bpy.ops.mesh.primitive_plane_add(size=200,location=(4,2,-.01));bpy.context.object.data.materials.append(floor)
sun=bpy.data.lights.new('주광','SUN');sun.energy=2;sun.angle=.3;o=bpy.data.objects.new('주광',sun);scene.collection.objects.link(o);o.rotation_euler=(.5,-.4,-.3)
c=bpy.data.cameras.new('피벗카메라');o=bpy.data.objects.new('피벗카메라',c);scene.collection.objects.link(o);o.location=(12,-12,12);target=Vector((4.7,2,.5));o.rotation_euler=(target-o.location).to_track_quat('-Z','Y').to_euler();c.type='ORTHO';c.ortho_scale=14.5;scene.camera=o
scene.render.filepath=str(out/'피벗_위치_실제메시.png');bpy.ops.render.render(write_still=True,scene=scene.name)
bpy.context.window.scene=bpy.data.scenes['03_제분소_140cm비율검토']
bpy.ops.wm.save_as_mainfile(filepath=str(path))
