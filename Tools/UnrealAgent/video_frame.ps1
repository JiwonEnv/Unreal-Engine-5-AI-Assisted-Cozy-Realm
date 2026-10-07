# 영상 길이를 출력하고 지정한 시각(초)의 장면을 PNG로 저장 (Windows 기본 WinRT · ffmpeg 불필요)
# 사용법:
#   powershell -ExecutionPolicy Bypass -File video_frame.ps1 -Video "D:\영상.mp4" -Seconds 3,25,50 -OutDir C:\temp\frames
# 출력 파일: <OutDir>\<영상이름>_<초>s.png · 빈 영상이면 duration=0 으로 알린다 (녹화 실패로 판단)
param(
    [Parameter(Mandatory = $true)][string]$Video,
    [string]$Seconds = '3',   # 쉼표로 여러 개 (예: 3,25,50) · -File 실행에서도 배열로 읽히도록 문자열로 받음
    [string]$OutDir = (Get-Location).Path
)
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
Add-Type -AssemblyName System.Runtime.WindowsRuntime
$asTask = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object {
    $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
function Await($op, [Type]$t) { $task = $asTask.MakeGenericMethod($t).Invoke($null, @($op)); $task.Wait(-1) | Out-Null; $task.Result }
[Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime] | Out-Null
[Windows.Media.Editing.MediaComposition, Windows.Media.Editing, ContentType = WindowsRuntime] | Out-Null
[Windows.Media.Editing.MediaClip, Windows.Media.Editing, ContentType = WindowsRuntime] | Out-Null

$full = (Resolve-Path $Video).Path
$file = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync($full)) ([Windows.Storage.StorageFile])
$clip = Await ([Windows.Media.Editing.MediaClip]::CreateFromFileAsync($file)) ([Windows.Media.Editing.MediaClip])
$comp = New-Object Windows.Media.Editing.MediaComposition
[System.Collections.Generic.ICollection[Windows.Media.Editing.MediaClip]].GetMethod('Add').Invoke($comp.Clips, @($clip)) | Out-Null
$duration = $comp.Duration.TotalSeconds
Write-Output ("duration={0:N1}" -f $duration)
if ($duration -le 0) { Write-Output '실패: 영상 길이가 0입니다'; exit 3 }

New-Item -ItemType Directory -Force $OutDir | Out-Null
$base = [System.IO.Path]::GetFileNameWithoutExtension($full)
foreach ($sec in ($Seconds -split '[,\s]+' | Where-Object { $_ } | ForEach-Object { [double]$_ })) {
    $ts = [TimeSpan]::FromSeconds([Math]::Min($sec, $duration - 0.1))
    $stream = Await ($comp.GetThumbnailAsync($ts, 1280, 0, [Windows.Media.Editing.VideoFramePrecision]::NearestFrame)) ([Windows.Graphics.Imaging.ImageStream])
    $net = [System.IO.WindowsRuntimeStreamExtensions]::AsStreamForRead($stream)
    $out = Join-Path $OutDir ("{0}_{1}s.png" -f $base, $sec)
    $fs = [System.IO.File]::Create($out); $net.CopyTo($fs); $fs.Close()
    Write-Output "saved=$out"
}
