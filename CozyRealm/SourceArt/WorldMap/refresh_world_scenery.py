"""구역·길의 수치를 바꿔 Landscape를 재생성한 뒤 기존 검토 배치를 새 높이에 맞춘다.

Landscape 모드에서 손으로 조형한 높이를 읽는 도구가 아니다. 현재 맵만 수정하며 자동 저장하지 않는다.
"""
import unreal
import math

actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
layouts=[a for a in actors.get_all_level_actors() if a.get_class().get_name()=='CozyMountainWorld']
if len(layouts)!=1:
    raise RuntimeError('산 월드 레이아웃이 정확히 하나 있는 맵에서 실행하세요.')
layout=layouts[0]
areas=list(layout.get_editor_property('clearings'))
ground_z=layout.get_editor_property('ground_z')
updated=removed=0
for name in ['broad_trees','pines','boulders','cliffs']:
    component=layout.get_editor_property(name)
    for i in reversed(range(component.get_instance_count())):
        transform=component.get_instance_transform(i,world_space=True)
        p=transform.translation
        distance=min(math.hypot((p.x-c.center.x)/c.radius.x,(p.y-c.center.y)/c.radius.y) for c in areas)
        if layout.get_path_distance(p.x,p.y)<(1300 if name=='cliffs' else 850) or distance<1.15:
            component.remove_instance(i)
            removed+=1
            continue
        offset=-transform.scale3d.z*130 if name=='cliffs' else -12
        transform.translation=unreal.Vector(p.x,p.y,layout.get_layout_height(p.x,p.y)+ground_z+offset)
        component.update_instance_transform(i,transform,world_space=True,mark_render_state_dirty=True,teleport=True)
        updated+=1
for actor in actors.get_all_level_actors():
    if isinstance(actor,unreal.StaticMeshActor) and str(actor.get_folder_path())=='20_시설_검토배치':
        p=actor.get_actor_location()
        actor.set_actor_location(unreal.Vector(p.x,p.y,layout.get_layout_height(p.x,p.y)+ground_z),False,False)
        updated+=1
unreal.log('월드 검토 배치 높이 갱신: '+str(updated)+'개 / 새 길·평지와 겹친 장식 제거: '+str(removed)+'개. 확인 후 현재 맵을 직접 저장하세요.')
