# 여러 단계를 한 번에 이어서 실행 (클릭·키·대기·캡처) · 한 단계라도 막히면 그 자리에서 멈춘다
# 사용법:
#   powershell -ExecutionPolicy Bypass -File ue_steps.ps1 "click 500 420" "key F1" "wait 1" "cap shot.png"
# 단계 종류:
#   click X Y   1280×720 기준 좌표 클릭 (ue_input.ps1 과 같은 보호 기능)
#   key NAME    키 입력 (F1, Enter, Space, 숫자 코드 …)
#   wait SEC    초 단위 대기 (소수 가능)
#   cap FILE    에디터 화면 캡처를 PNG로 저장 (상대 경로면 -CaptureDir 기준)
# 시간에 민감한 조작(예: 가공 시작 직후 정지)은 도구 호출을 나누지 말고 이 스크립트 한 번으로 이어서 실행한다.
param(
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)][string[]]$Steps,
    [double]$Delay = 0.8,
    [string]$CaptureDir = (Get-Location).Path,
    [string]$Project,
    [int]$RefWidth = 1280,    # 좌표 기준 크기 (패키지 게임은 window_capture.ps1 캡처 크기를 그대로 넣는다)
    [int]$RefHeight = 720
)
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
$here = $PSScriptRoot
foreach ($step in $Steps) {
    $parts = $step -split '\s+'
    switch ($parts[0]) {
        'wait' { Start-Sleep -Milliseconds ([int]([double]$parts[1] * 1000)); Write-Output "wait $($parts[1])" }
        'cap' {
            $out = $parts[1]
            if (-not [System.IO.Path]::IsPathRooted($out)) { $out = Join-Path $CaptureDir $out }
            $r = & python "$here\mcp_call.py" capture $out
            if ($LASTEXITCODE -ne 0) { Write-Output "중단: 캡처 실패 · $r"; exit 5 }
            Write-Output "cap $out"
        }
        { $_ -in 'click', 'key', 'wheel', 'mdrag' } {
            $args2 = @($parts[0]) + $parts[1..($parts.Count - 1)]
            if ($Project) { $args2 += @('-Project', $Project) }
            $args2 += @('-RefWidth', $RefWidth, '-RefHeight', $RefHeight)
            $r = & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\ue_input.ps1" @args2
            $code = $LASTEXITCODE
            Write-Output "$step -> $r"
            if ($code -ne 0) { Write-Output '중단: 대상 언리얼 창이 맨 앞이 아니거나 찾지 못해 남은 단계를 실행하지 않았습니다'; exit $code }
            Start-Sleep -Milliseconds ([int]($Delay * 1000))
        }
        default { Write-Output "중단: 알 수 없는 단계 '$step'"; exit 6 }
    }
}
