# ---------------------------------------------------------------------------
# Monta textures/atlas.png a partir de um pack de texturas de blocos 16x16.
#
# POR QUE ESTE SCRIPT EXISTE
# O atlas nao e versionado. Ele e derivado das texturas do Minecraft, que sao
# da Mojang/Microsoft e nao podem ser redistribuidas. Cada pessoa gera o seu
# a partir do pack que ja tem na propria maquina.
#
# COMO USAR
#   powershell -ExecutionPolicy Bypass -File tools\build_atlas.ps1 `
#       -Source "E:\caminho\para\assets\minecraft\textures\blocks"
#
# Se o -Source for omitido, usa o caminho padrao abaixo.
#
# O RESULTADO
# Uma imagem 256x256 com grade de 16x16 tiles de 16 pixels. A ordem dos tiles
# TEM que bater com o enum AtlasTile em Texture.h: o indice do tile e a unica
# ligacao entre esta lista e o codigo. Mexeu aqui, mexe la.
#
# Onde achar as texturas: qualquer resource pack 16x16, ou o proprio
# .minecraft\versions\<versao>\<versao>.jar (e um zip; as texturas de bloco
# ficam em assets/minecraft/textures/block ou blocks, dependendo da versao).
# ---------------------------------------------------------------------------

param(
    [string]$Source = "E:\programming\textures\blocks",
    [string]$Output = "$PSScriptRoot\..\textures\atlas.png"
)

Add-Type -AssemblyName System.Drawing

# A ORDEM IMPORTA: indice na lista = valor no enum AtlasTile (Texture.h).
$tiles = @(
    "dirt",             # 0  TILE_DIRT
    "grass_top",        # 1  TILE_GRASS_TOP      (cinza; tingido em Block.cpp)
    "grass_side",       # 2  TILE_GRASS_SIDE
    "stone",            # 3  TILE_STONE
    "cobblestone",      # 4  TILE_COBBLESTONE
    "sand",             # 5  TILE_SAND
    "gravel",           # 6  TILE_GRAVEL
    "bedrock",          # 7  TILE_BEDROCK
    "planks_oak",       # 8  TILE_PLANKS_OAK
    "log_oak",          # 9  TILE_LOG_OAK
    "log_oak_top",      # 10 TILE_LOG_OAK_TOP
    "leaves_oak",       # 11 TILE_LEAVES_OAK     (cinza; tingido em Block.cpp)
    "coal_ore",         # 12 TILE_COAL_ORE
    "iron_ore",         # 13 TILE_IRON_ORE
    "gold_ore",         # 14 TILE_GOLD_ORE
    "diamond_ore",      # 15 TILE_DIAMOND_ORE
    "glass",            # 16 TILE_GLASS
    "brick",            # 17 TILE_BRICK
    "sandstone_normal", # 18 TILE_SANDSTONE
    "snow",             # 19 TILE_SNOW
    "ice",              # 20 TILE_ICE
    "clay",             # 21 TILE_CLAY
    "obsidian",         # 22 TILE_OBSIDIAN
    "water_still"       # 23 TILE_WATER  (tira animada; usa so o 1o frame)
)

$TILE = 16
$GRID = 16

if (-not (Test-Path $Source)) {
    Write-Error "Pasta de texturas nao encontrada: $Source"
    exit 1
}

$outDir = Split-Path -Parent $Output
if (-not (Test-Path $outDir)) {
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
}

$atlas = New-Object System.Drawing.Bitmap ($TILE * $GRID), ($TILE * $GRID), ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($atlas)

# SourceCopy preserva o alpha em vez de misturar com o fundo, e NearestNeighbor
# mantem o pixel quadrado. Sem os dois, a textura sai borrada ou sem transparencia.
$g.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half

$missing = @()

for ($i = 0; $i -lt $tiles.Count; $i++) {
    $path = Join-Path $Source ($tiles[$i] + ".png")

    if (-not (Test-Path $path)) {
        $missing += $tiles[$i]
        continue
    }

    $img = [System.Drawing.Image]::FromFile($path)

    $col = $i % $GRID
    $row = [math]::Floor($i / $GRID)
    $dst = New-Object System.Drawing.Rectangle ($col * $TILE), ($row * $TILE), $TILE, $TILE

    # Recorta so os primeiros 16x16 da origem: cobre texturas animadas como a
    # agua, que vem como uma tira vertical de varios frames.
    $g.DrawImage($img, $dst, 0, 0, $TILE, $TILE, [System.Drawing.GraphicsUnit]::Pixel)
    $img.Dispose()
}

$g.Dispose()
$atlas.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
$atlas.Dispose()

Write-Host "atlas gerado: $Output"
Write-Host "$($tiles.Count - $missing.Count) de $($tiles.Count) tiles"

if ($missing.Count -gt 0) {
    Write-Warning "faltaram (ficaram transparentes no atlas): $($missing -join ', ')"
}
