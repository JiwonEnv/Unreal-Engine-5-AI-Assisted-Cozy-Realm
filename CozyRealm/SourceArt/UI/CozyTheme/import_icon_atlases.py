"""언리얼 편집기 Python에서 실행하는 공유 아이콘 가져오기 도구.

먼저 일반 Python으로 pack_icon_atlases.py를 실행한 다음,
언리얼 명령 입력란에서 py <이 파일의 절대 경로>를 실행한다.
플레이 중 실행은 금지한다. 기존 이미지 이름·글꼴·배치·색 설정은 유지한다.
"""
import pathlib, json
import unreal

ROOT=pathlib.Path(__file__).resolve().parent
DATA=json.loads((ROOT/'Atlases/atlas_manifest.json').read_text(encoding='utf-8'))
FOLDER='/Game/CozyRealm/UI/Textures/CozyTheme/Atlases'
THEME='/Game/CozyRealm/UI/Theme/DA_UiTheme_CozyVillage'
editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
if editor.get_game_world() is not None:
    raise RuntimeError('플레이를 종료한 뒤 아이콘 시트를 가져오세요.')
tasks=[]
for group in DATA['groups'].values():
    task=unreal.AssetImportTask()
    task.set_editor_property('filename',str(ROOT/'Atlases'/(group['texture']+'.png')))
    task.set_editor_property('destination_path',FOLDER)
    task.set_editor_property('destination_name',group['texture'])
    task.set_editor_property('automated',True)
    task.set_editor_property('replace_existing',True)
    task.set_editor_property('save',True)
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
textures={}
for group in DATA['groups'].values():
    tex=unreal.load_asset(FOLDER+'/'+group['texture'])
    if tex is None:raise RuntimeError('공유 텍스처 가져오기 실패: '+group['texture'])
    tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    tex.set_editor_property('lod_group',unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property('srgb',True)
    tex.set_editor_property('never_stream',True)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    textures[group['texture']]=tex
theme=unreal.load_asset(THEME)
previous={str(k).casefold():v for k,v in theme.get_editor_property('atlas_images').items()}
atlas={}
for name,alias in DATA['aliases'].items():
    icon=DATA['icons'][alias['icon']]
    entry=unreal.CozyUiAtlasImage()
    entry.set_editor_property('texture',textures[icon['texture']])
    group=DATA['groups'][icon['group']]
    entry.set_editor_property('texture_size',unreal.Vector2D(group['width'],group['height']))
    entry.set_editor_property('origin',unreal.Vector2D(*icon['origin']))
    entry.set_editor_property('size',unreal.Vector2D(*icon['size']))
    # 원래 이름별 색을 유지한다. 알파와 비활성 색은 화면 부품이 별도로 처리한다.
    color=alias['tint']['specifiedColor']
    tint=unreal.SlateColor(specified_color=unreal.LinearColor(color['r'],color['g'],color['b'],color['a']))
    entry.set_editor_property('tint',previous[name.casefold()].get_editor_property('tint') if name.casefold() in previous else tint)
    atlas[name]=entry
images={k:v for k,v in theme.get_editor_property('images').items() if str(k).casefold() not in {n.casefold() for n in atlas}}
theme.set_editor_property('atlas_images',atlas)
theme.set_editor_property('images',images)
unreal.EditorAssetLibrary.save_loaded_asset(theme)
unreal.log('공유 아이콘 가져오기 완료: '+str(len(atlas))+'개 이름 / '+str(len(textures))+'장')
