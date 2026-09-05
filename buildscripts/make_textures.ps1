# Converts every sprite in data/sprites/*.png to a GTF (PS3's native texture
# format) in data/gtf/*.gtf, via PNG -> DDS (buildscripts/png_to_dds.ps1,
# hand-written -- see that file for why) -> GTF (dds2gtf.exe, the real SDK
# tool). Verified against the real tool: dds2gtf accepts the hand-written
# DDS with zero errors.

$ErrorActionPreference = "Stop"
$root = "D:\ClaudeCode\Crux PS3"
$spritesDir = "$root\data\sprites"
$gtfDir = "$root\data\gtf"
$dds2gtf = "D:\PS3\host-win32\bin\dds2gtf.exe"

New-Item -ItemType Directory -Force -Path $gtfDir | Out-Null

$pngs = Get-ChildItem "$spritesDir\*.png"
foreach ($png in $pngs) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($png.Name)
    $ddsPath = "$gtfDir\$name.dds"
    $gtfPath = "$gtfDir\$name.gtf"

    powershell -File "$root\buildscripts\png_to_dds.ps1" -InFile $png.FullName -OutFile $ddsPath | Out-Null

    Push-Location $gtfDir
    & $dds2gtf -o "$name.gtf" "$name.dds" | Out-Null
    $exit = $LASTEXITCODE
    Pop-Location

    Remove-Item $ddsPath -ErrorAction SilentlyContinue

    if ($exit -ne 0 -or -not (Test-Path $gtfPath)) {
        Write-Error "Failed to convert $($png.Name) to GTF"
        exit 1
    }
    Write-Output "  $($png.Name) -> gtf/$name.gtf"
}

Write-Output "Converted $($pngs.Count) sprites to GTF."
