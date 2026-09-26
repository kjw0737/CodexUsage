# Use Windows PowerShell's stable System.Drawing assembly for the C# mask helper.
if ($PSVersionTable.PSVersion.Major -ge 6) {
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath
    if ($LASTEXITCODE -ne 0) { throw 'Icon generation failed.' }
    return
}
$ErrorActionPreference = 'Stop'
# Rebuild the multi-resolution application icon without external dependencies.
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
# Extract the yellow knot, removing the old background and small chart badge.
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
public static class UsageIconMask {
 public static Bitmap Extract(string path) {
  using (var source = new Bitmap(path)) {
   var mask = new Bitmap(source.Width, source.Height);
   int left=source.Width, top=source.Height, right=0, bottom=0;
   for(int y=0;y<source.Height;y++) for(int x=0;x<source.Width;x++) {
    if(x >= source.Width*0.70 && y >= source.Height*0.68) continue;
    Color c=source.GetPixel(x,y);
    if(c.R < 100 || c.G < 75 || c.R-c.B < 65 || c.G-c.B < 55) continue;
    int alpha=Math.Min(255,Math.Max(0,(c.G-c.B)*255/185));
    mask.SetPixel(x,y,Color.FromArgb(alpha,239,228,128));
    left=Math.Min(left,x); top=Math.Min(top,y); right=Math.Max(right,x); bottom=Math.Max(bottom,y);
   }
   if(right<=left || bottom<=top) throw new InvalidOperationException("Yellow symbol not found");
   var crop=mask.Clone(new Rectangle(left,top,right-left+1,bottom-top+1),System.Drawing.Imaging.PixelFormat.Format32bppArgb);
   mask.Dispose(); return crop;
  }
 }
}
"@
$symbol = [UsageIconMask]::Extract((Join-Path $root 'CodexUsage\res\CodexUsage-source.png'))
$frames = @()
foreach ($size in @(16,20,24,32,40,48,64,128,256)) {
    $bitmap = [Drawing.Bitmap]::new($size,$size)
    $g = [Drawing.Graphics]::FromImage($bitmap)
    $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    # Fill the entire icon area with the knot. Draw the badge at each target
    # resolution so 16px bars remain solid, aligned pixels rather than a blur.
    $g.DrawImage($symbol, [Drawing.Rectangle]::new(0,0,$size,$size))
    $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::None
    $half = [int]($size / 2)
    $dark = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(255,25,28,33))
    $g.FillRectangle($dark,$half,$half,$size-$half,$size-$half)
    $gap = [Math]::Max(1,[int][Math]::Round($size/32.0))
    $barWidth = [Math]::Max(1,[int][Math]::Floor(($half - 2*$gap)/3.0))
    $colors = @([Drawing.Color]::FromArgb(239,68,68),[Drawing.Color]::FromArgb(250,204,21),[Drawing.Color]::FromArgb(34,197,94))
    for($bar=0;$bar -lt 3;$bar++) {
        $x = $half + $bar*($barWidth+$gap)
        $width = if($bar -eq 2) { $size-$x } else { $barWidth }
        $height = [int][Math]::Round($half * @(0.5,0.75,1.0)[$bar])
        $brush = [Drawing.SolidBrush]::new($colors[$bar])
        $g.FillRectangle($brush,$x,$size-$height,$width,$height)
        $brush.Dispose()
    }
    $dark.Dispose()
    if ($size -eq 16) { $bitmap.Save((Join-Path $root 'CodexUsage\res\CodexUsage-16.png'),[Drawing.Imaging.ImageFormat]::Png) }
    if ($size -eq 32) { $bitmap.Save((Join-Path $root 'CodexUsage\res\CodexUsage-32.png'),[Drawing.Imaging.ImageFormat]::Png) }
    $stream = [IO.MemoryStream]::new()
    $bitmap.Save($stream,[Drawing.Imaging.ImageFormat]::Png)
    $frames += ,($stream.ToArray())
    if ($size -eq 256) { $bitmap.Save((Join-Path $root 'CodexUsage\res\CodexUsage.png'),[Drawing.Imaging.ImageFormat]::Png) }
    $stream.Dispose(); $g.Dispose(); $bitmap.Dispose()
}
$file = [IO.File]::Create((Join-Path $root 'CodexUsage\res\CodexUsage.ico'))
$writer = [IO.BinaryWriter]::new($file)
$writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$frames.Count)
$offset = 6 + 16 * $frames.Count
$sizes = @(16,20,24,32,40,48,64,128,256)
for ($i=0; $i -lt $frames.Count; $i++) {
    $dim = if($sizes[$i] -eq 256) { 0 } else { $sizes[$i] }
    $writer.Write([byte]$dim); $writer.Write([byte]$dim); $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]1); $writer.Write([uint16]32)
    $writer.Write([uint32]$frames[$i].Length); $writer.Write([uint32]$offset)
    $offset += $frames[$i].Length
}
foreach ($frame in $frames) { $writer.Write([byte[]]$frame) }
$writer.Dispose()

$symbol.Dispose()
