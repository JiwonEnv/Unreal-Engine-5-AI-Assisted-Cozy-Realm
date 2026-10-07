# 공통 보호 함수 · 다른 스크립트가 dot-source로 불러 쓴다 (. "$PSScriptRoot\_UeGuard.ps1")
# 원칙: 대상 언리얼 에디터 창을 정확히 하나 찾고, 그 창이 실제로 맨 앞에 왔을 때만 입력을 보낸다.
#       조건이 하나라도 맞지 않으면 아무 입력도 보내지 않고 종료 코드로 알린다.
#   종료 코드 2 = BLOCKED (맨 앞 창이 대상이 아님) · 3 = NOTFOUND (대상 창 없음) · 4 = AMBIGUOUS (후보가 여러 개)
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}

if (-not ('UeGuardNative' -as [type])) {
    Add-Type @"
using System; using System.Runtime.InteropServices;
public class UeGuardNative {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, UIntPtr e);
  [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  public struct RECT { public int L, T, R, B; }
}
"@
}

function Get-UeProjectName([string]$Project) {
    if ($Project) { return $Project }
    if ($env:UE_PROJECT_NAME) { return $env:UE_PROJECT_NAME }
    return 'CozyRealm'
}

# 창 제목이 "<프로젝트> - Unreal Editor" 형태인 UnrealEditor 메인 창을 찾는다
function Find-UeWindow([string]$Project) {
    $name = Get-UeProjectName $Project
    $candidates = @(Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue |
        Where-Object { $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle -like "$name*Unreal Editor*" })
    if ($candidates.Count -eq 0) {
        Write-Output "NOTFOUND: '$name - Unreal Editor' 창이 없습니다 (에디터가 꺼져 있거나 프로젝트 이름이 다름)"
        exit 3
    }
    if ($candidates.Count -gt 1) {
        Write-Output "AMBIGUOUS: '$name' 에디터 창이 $($candidates.Count)개입니다 · 하나만 남기고 다시 실행하세요"
        exit 4
    }
    return $candidates[0]
}

# 대상 창을 맨 앞으로 가져온 뒤, 실제로 맨 앞인지 다시 확인한다 · 아니면 입력 없이 종료
function Assert-UeForeground([string]$Project) {
    $proc = Find-UeWindow $Project
    $h = $proc.MainWindowHandle
    if ([UeGuardNative]::IsIconic($h)) { [UeGuardNative]::ShowWindow($h, 9) | Out-Null; Start-Sleep -Milliseconds 300 }
    [UeGuardNative]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 250
    $fg = [UeGuardNative]::GetForegroundWindow()
    if ($fg -ne $h) {
        $fgName = (Get-Process | Where-Object { $_.MainWindowHandle -eq $fg } | Select-Object -First 1).ProcessName
        if (-not $fgName) { $fgName = '알 수 없음' }
        Write-Output "BLOCKED: 맨 앞 창이 언리얼 에디터가 아닙니다 (맨 앞=$fgName) · 입력을 보내지 않았습니다"
        exit 2
    }
    return $proc
}

# 기준 해상도(기본 1280×720, CaptureEditorImage 결과와 같음) 좌표를 실제 화면 좌표로 바꾼다
function Convert-UePoint($proc, [double]$X, [double]$Y, [int]$RefWidth = 1280, [int]$RefHeight = 720) {
    $r = New-Object UeGuardNative+RECT
    [UeGuardNative]::GetWindowRect($proc.MainWindowHandle, [ref]$r) | Out-Null
    $sx = [int]($r.L + $X * ($r.R - $r.L) / $RefWidth)
    $sy = [int]($r.T + $Y * ($r.B - $r.T) / $RefHeight)
    return @($sx, $sy)
}

# 키 이름 → 가상 키 코드 (숫자를 그대로 줘도 됨)
function Resolve-UeKey([string]$Key) {
    $map = @{ 'F1'=0x70; 'F2'=0x71; 'F3'=0x72; 'F4'=0x73; 'F5'=0x74; 'F6'=0x75; 'F7'=0x76; 'F8'=0x77;
              'F9'=0x78; 'F10'=0x79; 'F11'=0x7A; 'F12'=0x7B; 'ESC'=0x1B; 'ESCAPE'=0x1B; 'ENTER'=0x0D;
              'SPACE'=0x20; 'TAB'=0x09; 'BACKSPACE'=0x08; 'LEFT'=0x25; 'UP'=0x26; 'RIGHT'=0x27; 'DOWN'=0x28 }
    $k = $Key.ToUpperInvariant()
    if ($map.ContainsKey($k)) { return [byte]$map[$k] }
    if ($k -match '^0X[0-9A-F]+$') { return [byte][Convert]::ToInt32($k.Substring(2), 16) }
    if ($k -match '^\d+$') { return [byte][int]$k }
    if ($k.Length -eq 1) { return [byte][char]$k }
    throw "알 수 없는 키: $Key"
}
