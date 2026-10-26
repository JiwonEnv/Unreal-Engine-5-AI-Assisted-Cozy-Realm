param([switch]$GenerateMap, [switch]$BuildGame, [switch]$RegenerateMap, [switch]$Verify, [switch]$GentleRoutes, [string]$FastPchDirectory)
$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$ProjectFile = Join-Path $ProjectRoot 'CozyRealm/CozyRealm.uproject'
$EngineRoot = 'C:/Program Files/Epic Games/UE_5.8/Engine'
$LogRoot = Join-Path $ProjectRoot 'output'
New-Item -ItemType Directory -Path $LogRoot -Force | Out-Null

# 같은 PCH를 쓰는 CozyRealm 모듈의 증분 빌드에서만 선택적으로 사용한다.
# 원본은 그대로 두고 사본 경로만 이 프로세스의 컴파일러 옵션에 지정한다.
# 일반 빌드는 옵션 없이 실행하면 되며 시스템 환경 변수는 변경하지 않는다.
$taskOriginalClOptions = $env:_CL_
if ($FastPchDirectory -and -not ($GenerateMap -or $RegenerateMap -or $Verify)) {
    $taskBuildTarget = if ($BuildGame) { 'UnrealGame' } else { 'UnrealEditor' }
    $taskRsp = Join-Path $ProjectRoot "CozyRealm/Intermediate/Build/Win64/x64/$taskBuildTarget/Development/CozyRealm/CozyMountainWorld.cpp.obj.rsp"
    $taskMatch = [regex]::Match((Get-Content -LiteralPath $taskRsp -Raw), '/Fp"([^"]+)"')
    if (-not $taskMatch.Success) { throw '공유 PCH 경로를 찾지 못했습니다. 빠른 캐시 옵션 없이 빌드하세요.' }
    $taskSourcePch = Get-Item -LiteralPath $taskMatch.Groups[1].Value
    New-Item -ItemType Directory -Path $FastPchDirectory -Force | Out-Null
    $taskCachedPch = Join-Path $FastPchDirectory $taskSourcePch.Name
    $taskCachedInfo = Get-Item -LiteralPath $taskCachedPch -ErrorAction SilentlyContinue
    if (-not $taskCachedInfo -or $taskCachedInfo.Length -ne $taskSourcePch.Length -or $taskCachedInfo.LastWriteTimeUtc -ne $taskSourcePch.LastWriteTimeUtc) {
        Copy-Item -LiteralPath $taskSourcePch.FullName -Destination $taskCachedPch -Force
    }
    if ((Get-FileHash -LiteralPath $taskSourcePch.FullName).Hash -ne (Get-FileHash -LiteralPath $taskCachedPch).Hash) { throw 'PCH 사본 검증 실패' }
    $env:_CL_ = ($taskOriginalClOptions + ' /Fp"' + $taskCachedPch + '"').Trim()
}

try {
if ($Verify) {
    $taskVerifyStarted = [DateTime]::UtcNow
    & "$EngineRoot/Binaries/Win64/UnrealEditor-Cmd.exe" $ProjectFile '/Game/CozyRealm/Maps/L_MountainWorld' -game -NullRHI -unattended -nosound -nosplash -DisablePython '-ExecCmds=Cozy.MountainVerify,quit' "-abslog=$LogRoot/world_collision_verify.log" *> "$LogRoot/world_collision_verify_console.log"
} elseif ($RegenerateMap) {
    $ScriptPath = Join-Path $ProjectRoot 'CozyRealm/SourceArt/WorldMap/regenerate_mountain_world.py'
    [string[]]$taskExtraFlags = @()
    if ($GentleRoutes) { $taskExtraFlags = @('-GentleWorldRoutes') }
    & "$EngineRoot/Binaries/Win64/UnrealEditor-Cmd.exe" $ProjectFile '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' '-run=pythonscript' "-script=$ScriptPath" -unattended -nop4 -nosplash -NullRHI @taskExtraFlags "-abslog=$LogRoot/world_map_regenerate.log" *> "$LogRoot/world_map_regenerate_console.log"
} elseif ($BuildGame) {
    & "$EngineRoot/Build/BatchFiles/Build.bat" CozyRealm Win64 Development "-Project=$ProjectFile" -NoUBA -WaitMutex *> "$LogRoot/world_game_build.log"
} elseif ($GenerateMap) {
    $ScriptPath = Join-Path $ProjectRoot 'CozyRealm/SourceArt/WorldMap/build_mountain_world.py'
    & "$EngineRoot/Binaries/Win64/UnrealEditor-Cmd.exe" $ProjectFile '-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities' '-run=pythonscript' "-script=$ScriptPath" -unattended -nop4 -nosplash -NullRHI '-abslog=E:/Project/UE5_CozyRealm/output/world_map_generate.log' *> "$LogRoot/world_map_generate_console.log"
} else {
    & "$EngineRoot/Build/BatchFiles/Build.bat" CozyRealmEditor Win64 Development "-Project=$ProjectFile" -NoUBA -WaitMutex *> "$LogRoot/world_editor_build.log"
}
if ($LASTEXITCODE -ne 0) { throw "월드 제작 명령 실패: $LASTEXITCODE. output 로그를 확인하세요." }
if ($Verify) {
    $taskReport = Get-Item -LiteralPath (Join-Path $ProjectRoot 'CozyRealm/Saved/Automation/MountainWorld/verify.txt')
    if ($taskReport.LastWriteTimeUtc -lt $taskVerifyStarted.AddSeconds(-2)) { throw '새 충돌 검사 결과가 생성되지 않았습니다.' }
    $taskReportText = Get-Content -LiteralPath $taskReport.FullName -Raw
    if ($taskReportText -notmatch '실제 충돌 조회 \d+회 / 실패 0건') { throw '월드 충돌 검사 실패. verify.txt를 확인하세요.' }
    if ($GentleRoutes) {
        $taskMaxSlope = ([regex]::Matches($taskReportText, '최대 ([0-9.]+)도') | ForEach-Object { [double]$_.Groups[1].Value } | Measure-Object -Maximum).Maximum
        if ($taskMaxSlope -gt 30) { throw "길 완화 목표(30도 이하) 미달: $taskMaxSlope 도" }
    }
}
} finally {
    if ($null -eq $taskOriginalClOptions) { Remove-Item Env:_CL_ -ErrorAction SilentlyContinue }
    else { $env:_CL_ = $taskOriginalClOptions }
}
