# 대상 창(기본: 언리얼 에디터 · UE_WINDOW_TITLE 이 있으면 그 창)을 화면에서 그대로 캡처해 PNG로 저장
# MCP 없이 동작하므로 패키지 게임 검증에도 쓴다. 보호 기능: 대상 창이 맨 앞일 때만 캡처 (다른 창이 찍히지 않게)
# 사용법:
#   powershell -ExecutionPolicy Bypass -File window_capture.ps1 -Out D:\shots\pkg_01.png
#   $env:UE_WINDOW_TITLE = 'CozyRealm (64-bit*'   # 패키지 게임 창을 대상으로
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Project
)
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
. "$PSScriptRoot\_UeGuard.ps1"
Add-Type -AssemblyName System.Drawing

$proc = Assert-UeForeground $Project
Start-Sleep -Milliseconds 300
$r = New-Object UeGuardNative+RECT
[UeGuardNative]::GetWindowRect($proc.MainWindowHandle, [ref]$r) | Out-Null
$w = $r.R - $r.L; $h = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $h
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $h))
$full = [System.IO.Path]::GetFullPath($Out)
New-Item -ItemType Directory -Force ([System.IO.Path]::GetDirectoryName($full)) | Out-Null
$bmp.Save($full, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Output "saved=$full (${w}x${h} · '$($proc.MainWindowTitle)')"
