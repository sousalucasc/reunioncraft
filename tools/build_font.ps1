# ---------------------------------------------------------------------------
# Monta textures/font.png, um atlas de fonte bitmap pro texto de debug.
#
# POR QUE ESTE SCRIPT EXISTE
# Mesma razao do build_atlas.ps1: o PNG nao e versionado. Ele e rasterizado
# a partir de uma fonte instalada no Windows, e redistribuir fonte de
# terceiro tem licenca propria. Cada pessoa gera o seu.
#
# COMO USAR
#   powershell -ExecutionPolicy Bypass -File tools\build_font.ps1
#
# O RESULTADO
# 256x256, grade de 16x16 celulas de 16 pixels. A celula de um caractere e
# (codigo ASCII - 32), contada da esquerda pra direita e de cima pra baixo.
# Cobre de espaco (32) ate '~' (126). Branco com fundo transparente, pra que
# a cor venha do vertice.
#
# Precisa ser MONOESPACADA, senao o espacamento fixo do renderizador
# desalinha o texto.
# ---------------------------------------------------------------------------

param(
    [string]$FontName = "Consolas",
    [int]$FontSize = 12,
    [string]$Output = "$PSScriptRoot\..\textures\font.png"
)

Add-Type -AssemblyName System.Drawing

$CELL = 16
$GRID = 16

$outDir = Split-Path -Parent $Output
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

$bmp = New-Object System.Drawing.Bitmap ($CELL * $GRID), ($CELL * $GRID), ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)

# Transparente. A cor final vem do vertice, o atlas so guarda a forma.
$g.Clear([System.Drawing.Color]::FromArgb(0, 255, 255, 255))

# AntiAlias deixa a borda com alpha parcial, o que suaviza o texto pequeno.
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit

$font = New-Object System.Drawing.Font($FontName, $FontSize, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
$brush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::White)

$fmt = New-Object System.Drawing.StringFormat
$fmt.Alignment = [System.Drawing.StringAlignment]::Center
$fmt.LineAlignment = [System.Drawing.StringAlignment]::Center

# ASCII 32 ('espaco') ate 126 ('~').
for ($code = 32; $code -le 126; $code++) {
    $idx = $code - 32
    $col = $idx % $GRID
    $row = [math]::Floor($idx / $GRID)

    $rect = New-Object System.Drawing.RectangleF ($col * $CELL), ($row * $CELL), $CELL, $CELL
    $ch = [char]$code

    $g.DrawString([string]$ch, $font, $brush, $rect, $fmt)
}

$brush.Dispose()
$font.Dispose()
$g.Dispose()

$bmp.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()

Write-Host "fonte gerada: $Output"
Write-Host "$FontName $FontSize px | celulas de ${CELL}px | ASCII 32 a 126"
