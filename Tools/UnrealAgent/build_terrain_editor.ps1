$ErrorActionPreference='Stop'
$taskRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
# 사용자 에디터가 사용하는 게임 DLL을 재빌드하지 않고 새 편집기 모듈만 빌드한다.
New-Item -ItemType Directory -Path "$taskRoot/output" -Force | Out-Null
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/Build.bat' CozyRealmEditor Win64 Development "-Project=$taskRoot/CozyRealm/CozyRealm.uproject" -Module=CozyTerrainEditor -MaxParallelActions=1 -NoUBA -NoLiveCoding -NoHotReloadFromIDE -WaitMutex *> "$taskRoot/output/terrain_editor_build_module.log"
if ($LASTEXITCODE -ne 0) { throw '지형 편집기 빌드 실패. output/terrain_editor_build_module.log를 확인하세요.' }
# 모듈 단독 빌드는 전체 타깃 메타데이터를 갱신하지 않는다. 새 플러그인 위치에만 탐색 파일을 만든다.
$taskBuildId=(Get-Content "$taskRoot/CozyRealm/Binaries/Win64/UnrealEditor.modules" -Raw | ConvertFrom-Json).BuildId
$taskEngineBuildId=(Get-Content 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.modules' -Raw | ConvertFrom-Json).BuildId
if($taskBuildId -ne $taskEngineBuildId){throw '게임과 엔진 빌드 식별자가 달라 플러그인 탐색 파일을 만들지 않았습니다.'}
@{BuildId=$taskBuildId;Modules=@{CozyTerrainEditor='UnrealEditor-CozyTerrainEditor.dll'}} | ConvertTo-Json | Set-Content "$taskRoot/CozyRealm/Plugins/CozyTerrainEditor/Binaries/Win64/UnrealEditor.modules" -Encoding utf8
