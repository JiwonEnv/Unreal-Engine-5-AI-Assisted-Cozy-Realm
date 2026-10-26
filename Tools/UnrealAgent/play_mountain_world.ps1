$ErrorActionPreference = 'Stop'
$taskRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$taskExe = Join-Path $taskRoot 'output/MountainWorldBuild/Windows/CozyRealm.exe'
$taskUser = Join-Path $taskRoot 'output/MountainWorldPackagedUser'
if (-not (Test-Path -LiteralPath $taskExe)) { throw '먼저 게임 빌드와 package_mountain_world.ps1을 실행하세요.' }
Start-Process -FilePath $taskExe -ArgumentList @('/Game/CozyRealm/Maps/L_MountainWorld','-windowed','-ResX=1600','-ResY=900',"-UserDir=`"$taskUser`"") -WindowStyle Hidden
