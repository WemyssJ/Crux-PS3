# Assembles and builds the installable PS3 .pkg from crux.ppu.elf + game data.
# Run after the PS3 Makefile build (needs crux.ppu.elf to already exist).
# Produces Build\PS3\<content-id>.pkg.
#
# Lives in buildscripts\, not build\ -- Windows paths are case-insensitive,
# so a "build\" scripts folder and the "Build\" output folder would actually
# be the SAME directory on disk, silently mixing scripts into the deliverable
# folder. Learned that the hard way once already; don't reintroduce it.
#
# Layout make_package_npdrm expects (matches D:\PS3\samples\sdk\network\np\hddgame):
#   <stage>\PARAM.SFO
#   <stage>\ICON0.PNG
#   <stage>\USRDIR\EBOOT.BIN        <- crux.ppu.elf, NPDRM-signed via make_fself_npdrm
#   <stage>\USRDIR\...              <- game data (shaders, sprites, levels)

$ErrorActionPreference = "Stop"
$root = "D:\ClaudeCode\Crux PS3"
$ps3bin = "D:\PS3\host-win32\bin"
$stage = "$root\buildscripts\pkg_stage"
$usrdir = "$stage\USRDIR"

if (-not (Test-Path "$root\crux.ppu.elf")) {
    Write-Error "crux.ppu.elf not found -- build the PS3 target first."
    exit 1
}

Remove-Item -Recurse -Force $stage -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $usrdir | Out-Null

Write-Output "Signing EBOOT.BIN (make_fself_npdrm)..."
& "$ps3bin\make_fself_npdrm.exe" "$root\crux.ppu.elf" "$usrdir\EBOOT.BIN"
if ($LASTEXITCODE -ne 0) { Write-Error "make_fself_npdrm failed (exit $LASTEXITCODE)"; exit 1 }

Copy-Item "$root\vs_quad.vpo" "$usrdir\" -Force
Copy-Item "$root\fs_quad.fpo" "$usrdir\" -Force
Copy-Item -Recurse "$root\data" "$usrdir\data" -Force

Write-Output "Generating PARAM.SFO..."
powershell -File "$root\buildscripts\make_param_sfo.ps1" -OutFile "$stage\PARAM.SFO"
Copy-Item "$root\packaging\ICON0.PNG" "$stage\ICON0.PNG" -Force

Write-Output "Building package (make_package_npdrm)..."
# make_package_npdrm writes the .pkg into ITS OWN current working directory
# (not the content dir passed as an argument) -- pin cwd to $stage so the
# output lands somewhere predictable instead of wherever this script's
# caller happened to be standing.
Push-Location $stage
& "$ps3bin\make_package_npdrm.exe" "$root\packaging\package.conf" $stage
$pkgExit = $LASTEXITCODE
Pop-Location
if ($pkgExit -ne 0) { Write-Error "make_package_npdrm failed (exit $pkgExit)"; exit 1 }

$pkg = Get-ChildItem "$stage\*.pkg" -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $pkg) {
    Write-Error "make_package_npdrm reported success but no .pkg was found in $stage"
    exit 1
}

New-Item -ItemType Directory -Force -Path "$root\Build\PS3" | Out-Null
Move-Item -Force $pkg.FullName "$root\Build\PS3\"
Write-Output "Package created: Build\PS3\$($pkg.Name)"
