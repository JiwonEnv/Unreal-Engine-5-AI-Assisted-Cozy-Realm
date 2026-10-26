"""산 월드 사본 재질에 HISM 셰이더 사용 플래그를 명시해서 저장한다."""
import unreal
eal=unreal.EditorAssetLibrary
folder='/Game/CozyRealm/Environment/MountainWorld/Materials'
count=0
for path in eal.list_assets(folder,recursive=True):
    material=eal.load_asset(path)
    if isinstance(material,unreal.Material):
        unreal.MaterialEditingLibrary.set_material_usage(material,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
        unreal.MaterialEditingLibrary.recompile_material(material)
        assert eal.save_loaded_asset(material)
        count+=1
assert count>0
unreal.log('MOUNTAIN_INSTANCING_MATERIALS_FIXED '+str(count))
