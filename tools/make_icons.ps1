# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 deuter0n
#
# Generates the app and tray icons from the SVG artwork in assets/icon/.
# Run it by hand when the artwork changes and commit the .ico files; the build
# itself needs no image tools.
#
#   powershell -ExecutionPolicy Bypass -File tools\make_icons.ps1
#
# Microsoft Edge (installed with Windows) draws each SVG at each exact size,
# so small sizes stay sharp instead of being scaled down from a big image.
#  - app.ico: 16-256 px; the simplified drawing (app-small.svg) at 24 px and below
#  - tray_<state>.ico: 16, 20, 24 and 32 px, for the four engine states

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$iconDir = Join-Path $root "assets\icon"
$edge = @("${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
          "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $edge) { throw "Microsoft Edge not found." }

# An icon image in the classic format: a BITMAPINFOHEADER (with double height),
# BGRA pixels bottom-up, then a 1-bit mask (all zeros: the alpha channel decides).
function ConvertTo-IconBitmap([Drawing.Bitmap]$bmp) {
    $s = $bmp.Width
    $ms = New-Object IO.MemoryStream
    $w = New-Object IO.BinaryWriter $ms
    $maskRow = [int][Math]::Ceiling($s / 32) * 4
    $w.Write([UInt32]40); $w.Write([Int32]$s); $w.Write([Int32]($s * 2))
    $w.Write([UInt16]1); $w.Write([UInt16]32); $w.Write([UInt32]0)
    $w.Write([UInt32]($s * $s * 4 + $maskRow * $s)); $w.Write([Int32]0); $w.Write([Int32]0)
    $w.Write([UInt32]0); $w.Write([UInt32]0)
    $rect = New-Object Drawing.Rectangle 0, 0, $s, $s
    $data = $bmp.LockBits($rect, [Drawing.Imaging.ImageLockMode]::ReadOnly, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $pixels = New-Object byte[] ($data.Stride * $s)
    [Runtime.InteropServices.Marshal]::Copy($data.Scan0, $pixels, 0, $pixels.Length)
    $bmp.UnlockBits($data)
    for ($row = $s - 1; $row -ge 0; $row--) { $w.Write($pixels, $row * $data.Stride, $s * 4) }
    $w.Write((New-Object byte[] ($maskRow * $s)))
    $w.Flush()
    return , $ms.ToArray()
}

# What to draw: each output file with its sizes, and which SVG to use per size.
$appSizes = 16, 20, 24, 32, 48, 64, 128, 256
$traySizes = 16, 20, 24, 32
$outputs = @(
    @{ File = "app.ico"; Sizes = $appSizes; Svg = { param($s) if ($s -le 24) { "app-small.svg" } else { "app-full.svg" } } }
)
foreach ($state in "running", "stopped", "waiting", "error") {
    $outputs += @{ File = "tray_$state.ico"; Sizes = $traySizes; Svg = [scriptblock]::Create("'tray-$state.svg'") }
}

# Lay every icon out on one page: one row per output file, 8 px apart.
$gap = 8
$y = 0
$cells = @()
$html = '<!doctype html><html><head><style>html,body{margin:0;background:transparent}img{position:absolute}</style></head><body>'
foreach ($o in $outputs) {
    $x = 0
    foreach ($s in $o.Sizes) {
        $svg = & $o.Svg $s
        $bytes = [IO.File]::ReadAllBytes((Join-Path $iconDir $svg))
        $uri = "data:image/svg+xml;base64," + [Convert]::ToBase64String($bytes)
        $html += "<img src=`"$uri`" style=`"left:${x}px;top:${y}px;width:${s}px;height:${s}px`">"
        $cells += @{ File = $o.File; Size = $s; X = $x; Y = $y }
        $x += $s + $gap
    }
    $y += ($o.Sizes | Measure-Object -Maximum).Maximum + $gap
}
$html += '</body></html>'

$work = Join-Path ([IO.Path]::GetTempPath()) ("gfx-icons-" + [Guid]::NewGuid())
New-Item -ItemType Directory -Path $work | Out-Null
try {
    $page = Join-Path $work "icons.html"
    $shot = Join-Path $work "icons.png"
    [IO.File]::WriteAllText($page, $html)
    $width = 1000; $height = $y + 100
    $edgeArgs = "--headless=new", "--disable-gpu", "--no-first-run", "--hide-scrollbars", "--force-device-scale-factor=1",
                "--default-background-color=00000000", "`"--user-data-dir=$work\profile`"", "--window-size=$width,$height",
                "`"--screenshot=$shot`"", ([Uri]$page).AbsoluteUri
    Start-Process -FilePath $edge -ArgumentList $edgeArgs -Wait -WindowStyle Hidden `
        -RedirectStandardError (Join-Path $work "edge.log") -RedirectStandardOutput (Join-Path $work "edge.out")
    if (-not (Test-Path $shot)) { throw "Edge didn't produce a screenshot." }

    $sheet = [Drawing.Bitmap]::FromFile($shot)
    try {
        foreach ($o in $outputs) {
            # Cut out each size: 256 px as PNG, smaller sizes as classic 32-bit
            # bitmaps, which every tool reads.
            $images = @()
            foreach ($c in $cells | Where-Object { $_.File -eq $o.File }) {
                $bmp = $sheet.Clone((New-Object Drawing.Rectangle $c.X, $c.Y, $c.Size, $c.Size), [Drawing.Imaging.PixelFormat]::Format32bppArgb)
                if ($c.Size -ge 256) {
                    $ms = New-Object IO.MemoryStream
                    $bmp.Save($ms, [Drawing.Imaging.ImageFormat]::Png)
                    $data = $ms.ToArray()
                } else {
                    $data = ConvertTo-IconBitmap $bmp
                }
                $bmp.Dispose()
                $images += @{ Size = $c.Size; Data = $data }
            }

            # .ico: a 6-byte header, a 16-byte entry per image, then the PNGs.
            $out = New-Object IO.MemoryStream
            $w = New-Object IO.BinaryWriter $out
            $w.Write([UInt16]0); $w.Write([UInt16]1); $w.Write([UInt16]$images.Count)
            $offset = 6 + 16 * $images.Count
            foreach ($img in $images) {
                $dim = if ($img.Size -ge 256) { 0 } else { $img.Size }   # 0 means 256
                $w.Write([Byte]$dim); $w.Write([Byte]$dim); $w.Write([Byte]0); $w.Write([Byte]0)
                $w.Write([UInt16]1); $w.Write([UInt16]32)
                $w.Write([UInt32]$img.Data.Length); $w.Write([UInt32]$offset)
                $offset += $img.Data.Length
            }
            foreach ($img in $images) { $w.Write($img.Data) }
            $w.Flush()
            [IO.File]::WriteAllBytes((Join-Path $iconDir $o.File), $out.ToArray())
            Write-Host ("{0,-18} {1}" -f $o.File, (($o.Sizes | ForEach-Object { "$_" }) -join ", "))
        }
    } finally {
        $sheet.Dispose()
    }
} finally {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}
