$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$taskOutput = Join-Path $taskRoot 'output/MountainWorldBuild'
$taskLog = Join-Path $taskRoot 'output/world_package.log'
# 기존 타이틀·시작 맵 설정을 수정하지 않고 검토 맵을 명시해서 패키징한다.
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Build/BatchFiles/RunUAT.bat' BuildCookRun "-project=$taskRoot/CozyRealm/CozyRealm.uproject" -noP4 -platform=Win64 -clientconfig=Development -skipbuild -cook '-map=/Game/CozyRealm/Maps/L_MountainWorld' "-CookDir=$taskRoot/CozyRealm/Content/CozyRealm/UI/Fonts" -stage -pak -package -archive "-archivedirectory=$taskOutput" -unattended -utf8output *> $taskLog
if ($LASTEXITCODE -ne 0) { throw "월드 패키징 실패: $taskLog" }
if (-not (Test-Path -LiteralPath "$taskOutput/Windows/CozyRealm.exe")) { throw '패키지 실행 파일을 찾지 못했습니다.' }
Write-Output "월드 패키징 완료: $taskOutput/Windows/CozyRealm.exe"
