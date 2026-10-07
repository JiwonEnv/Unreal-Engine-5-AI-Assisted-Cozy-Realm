# 캡처 이미지의 일부를 잘라 확대 (작은 글씨 확인용) · System.Drawing 사용
# 사용법:
#   powershell -ExecutionPolicy Bypass -File crop_image.ps1 -Src shot.png -X 318 -Y 236 -Width 390 -Height 46 -Out zoom.png -Scale 3
param(
    [Parameter(Mandatory = $true)][string]$Src,
    [Parameter(Mandatory = $true)][int]$X, [Parameter(Mandatory = $true)][int]$Y,
    [Parameter(Mandatory = $true)][int]$Width, [Parameter(Mandatory = $true)][int]$Height,
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$Scale = 3
)
try { [Console]::OutputEncoding = [System.Text.Encoding]::UTF8 } catch {}
Add-Type -AssemblyName System.Drawing
$im = [System.Drawing.Image]::FromFile((Resolve-Path $Src).Path)
$bmp = New-Object System.Drawing.Bitmap ($Width * $Scale), ($Height * $Scale)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = 'HighQualityBicubic'
$g.DrawImage($im, (New-Object System.Drawing.Rectangle 0, 0, ($Width * $Scale), ($Height * $Scale)), (New-Object System.Drawing.Rectangle $X, $Y, $Width, $Height), 'Pixel')
$bmp.Save([System.IO.Path]::GetFullPath($Out)); $g.Dispose(); $im.Dispose()
Write-Output "saved=$Out"
