# 언리얼 에디터 창에 클릭·키 입력 한 번 보내기 (보호 기능: 대상 창이 맨 앞일 때만)
# 사용법:
#   powershell -ExecutionPolicy Bypass -File ue_input.ps1 check            # 입력 없이 대상 창 확인만
#   powershell -ExecutionPolicy Bypass -File ue_input.ps1 click 500 420    # 1280×720 기준 좌표 클릭
#   powershell -ExecutionPolicy Bypass -File ue_input.ps1 key F1           # 키 (F1·Enter·Space·숫자 코드 등)
# 주의: PIE 중 Esc는 열린 게임 창이 아니라 PIE 자체를 끈다 → 게임 창은 화면의 '닫기' 버튼으로 닫는다.
param(
    [Parameter(Position = 0, Mandatory = $true)][ValidateSet('check', 'click', 'key')][string]$Action,
    [Parameter(Position = 1)][string]$A,
    [Parameter(Position = 2)][double]$B = 0,
    [string]$Project,
    [int]$RefWidth = 1280,
    [int]$RefHeight = 720
)
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
. "$PSScriptRoot\_UeGuard.ps1"

$proc = Assert-UeForeground $Project

switch ($Action) {
    'check' {
        Write-Output "ok: 대상 창 '$($proc.MainWindowTitle)' (PID $($proc.Id)) 이 맨 앞입니다"
    }
    'click' {
        if (-not $A) { throw 'click 에는 X Y 좌표가 필요합니다' }
        $p = Convert-UePoint $proc ([double]$A) $B $RefWidth $RefHeight
        [UeGuardNative]::SetCursorPos($p[0], $p[1]) | Out-Null
        Start-Sleep -Milliseconds 120
        [UeGuardNative]::mouse_event(0x2, 0, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 80
        [UeGuardNative]::mouse_event(0x4, 0, 0, 0, [UIntPtr]::Zero)
        Write-Output "ok: click $A,$B → 화면 $($p[0]),$($p[1])"
    }
    'key' {
        if (-not $A) { throw 'key 에는 키 이름이 필요합니다' }
        $vk = Resolve-UeKey $A
        [UeGuardNative]::keybd_event($vk, 0, 0, [UIntPtr]::Zero)
        Start-Sleep -Milliseconds 80
        [UeGuardNative]::keybd_event($vk, 0, 2, [UIntPtr]::Zero)
        Write-Output "ok: key $A"
    }
}
