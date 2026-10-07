# 방금 끝난 Game Bar 녹화 파일을 기록 폴더로 옮기고 이름을 바꾼다
# 사용법:
#   powershell -ExecutionPolicy Bypass -File save_recording.ps1 -Name "2026-10-07_기능3_내용_01.mp4"
# 기본값:
#   -Source : Windows '동영상\Captures' (Game Bar 기본 저장 위치)
#   -Dest   : 바탕 화면\CozyRealm\Reocode (OneDrive 바탕 화면이면 그 경로를 자동으로 씀) · 없으면 -Source에 그대로 둠
# 녹화를 멈춘 직후에는 파일이 아직 쓰이는 중일 수 있어, 크기가 변하지 않을 때까지 기다린 뒤 옮긴다.
# 한글 파일명·경로를 쓰므로 bash가 아니라 PowerShell에서 직접 실행한다.
param(
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Source = (Join-Path ([Environment]::GetFolderPath('MyVideos')) 'Captures'),
    [string]$Dest = (Join-Path ([Environment]::GetFolderPath('Desktop')) 'CozyRealm\Reocode'),
    [int]$WithinMinutes = 30
)
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
$latest = Get-ChildItem $Source -Filter *.mp4 -ErrorAction Stop |
    Where-Object { $_.LastWriteTime -gt (Get-Date).AddMinutes(-$WithinMinutes) } |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not $latest) { Write-Output "실패: 최근 $WithinMinutes 분 안에 만든 녹화가 $Source 에 없습니다"; exit 3 }

$prev = -1
for ($i = 0; $i -lt 30; $i++) {
    $size = (Get-Item $latest.FullName).Length
    if ($size -eq $prev -and $size -gt 0) { break }
    $prev = $size; Start-Sleep -Seconds 1
}

if (-not (Test-Path $Dest)) {
    Write-Output "알림: 기록 폴더($Dest)가 없어 Windows 기본 녹화 폴더에 둡니다"
    $Dest = $Source
}
$target = Join-Path $Dest $Name
if (Test-Path $target) { Write-Output "실패: 같은 이름의 파일이 이미 있습니다 · $target"; exit 4 }
Move-Item $latest.FullName $target
Write-Output "saved: $target ($([int]((Get-Item $target).Length / 1MB)) MB · 원래 이름 $($latest.Name))"
Write-Output '다음 단계: video_frame.ps1 로 장면을 뽑아 게임 화면이 담겼는지 확인한 뒤에만 완료로 기록하세요'
