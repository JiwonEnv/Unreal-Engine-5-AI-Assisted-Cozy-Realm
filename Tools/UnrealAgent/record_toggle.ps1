# Xbox Game Bar 녹화 시작/중지 (Win+Alt+R) · 언리얼 에디터 창이 맨 앞일 때만 보낸다
# Game Bar는 '맨 앞 창'을 녹화하므로, 보호 기능이 없으면 Claude·Codex 창이 녹화될 수 있다.
# 사용법: powershell -ExecutionPolicy Bypass -File record_toggle.ps1   (한 번 = 시작, 다시 한 번 = 중지)
param([string]$Project)
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
. "$PSScriptRoot\_UeGuard.ps1"

$proc = Assert-UeForeground $Project
[UeGuardNative]::keybd_event(0x5B, 0, 0, [UIntPtr]::Zero)   # Win
[UeGuardNative]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)   # Alt
[UeGuardNative]::keybd_event(0x52, 0, 0, [UIntPtr]::Zero)   # R
Start-Sleep -Milliseconds 100
[UeGuardNative]::keybd_event(0x52, 0, 2, [UIntPtr]::Zero)
[UeGuardNative]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
[UeGuardNative]::keybd_event(0x5B, 0, 2, [UIntPtr]::Zero)
Write-Output "ok: Win+Alt+R 전송 ($(Get-Date -Format HH:mm:ss)) · 대상 '$($proc.MainWindowTitle)'"
