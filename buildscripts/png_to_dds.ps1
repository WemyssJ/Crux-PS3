# Converts a PNG to an uncompressed 32bpp BGRA DDS (the simplest, most
# widely-supported DDS variant -- classic D3D "A8R8G8B8" layout). No DDS
# encoder tool is available anywhere on this system (no ImageMagick, no
# NVIDIA Texture Tools, no texconv), and installing one wouldn't guarantee a
# DDS flavor dds2gtf.exe actually accepts -- writing the header by hand
# (same approach as buildscripts/make_param_sfo.ps1) gives full control over
# exactly what goes in. Format reference: the classic DDS_HEADER +
# DDS_PIXELFORMAT layout (no DX10 extension header needed for this format).
#
# Fields are written as explicit little-endian byte arrays rather than via
# hex-literal arithmetic -- PowerShell's default integer literal typing
# (Int32 for anything that fits, promoting awkwardly otherwise) makes
# 0xFF000000-style 32-bit-with-high-bit-set constants surprisingly painful
# to get losslessly into a uint32; four unambiguous 0-255 byte literals sidestep
# the whole problem.
#
# Usage: powershell -File buildscripts\png_to_dds.ps1 -InFile <png> -OutFile <dds>

param(
    [Parameter(Mandatory=$true)][string]$InFile,
    [Parameter(Mandatory=$true)][string]$OutFile
)

Add-Type -AssemblyName System.Drawing

$bmp = New-Object System.Drawing.Bitmap $InFile
$w = $bmp.Width
$h = $bmp.Height

# Format32bppArgb stores each pixel as B,G,R,A in memory (little-endian) --
# exactly the byte order the DDS A8R8G8B8 pixel format below expects, so no
# channel swizzling is needed between GDI+ and the DDS bytes.
$rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
$bmpData = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

$stride = $bmpData.Stride
$pixelBytes = New-Object byte[] ($stride * $h)
[System.Runtime.InteropServices.Marshal]::Copy($bmpData.Scan0, $pixelBytes, 0, $pixelBytes.Length)
$bmp.UnlockBits($bmpData)
$bmp.Dispose()

function LE32([uint32]$v)
{
    # Byte-by-byte via bitwise shifts, NOT division -- PowerShell's `/`
    # returns a float, and casting that float to [uint32] ROUNDS to the
    # nearest integer rather than truncating (e.g. 499/256 = 1.949 rounds
    # UP to 2, not down to 1). That corrupted any value whose low byte was
    # >= 128 -- e.g. height=499 got encoded as byte1=2 instead of 1,
    # decoding back as 755. Bitwise shifts on the uint32 sidestep floating-
    # point entirely, so there's no rounding to get wrong.
    return [byte[]](
        [byte]($v -band 0xFF),
        [byte](($v -shr 8) -band 0xFF),
        [byte](($v -shr 16) -band 0xFF),
        [byte](($v -shr 24) -band 0xFF)
    )
}

$out = New-Object System.Collections.Generic.List[byte]

# Magic "DDS "
$out.AddRange([byte[]](0x44, 0x44, 0x53, 0x20))

# DDS_HEADER (124 bytes)
$out.AddRange([byte[]](LE32 124))                        # dwSize
$out.AddRange([byte[]](0x0F, 0x10, 0x00, 0x00))   # dwFlags: CAPS|HEIGHT|WIDTH|PITCH|PIXELFORMAT
$out.AddRange([byte[]](LE32 ([uint32]$h)))                # dwHeight
$out.AddRange([byte[]](LE32 ([uint32]$w)))                # dwWidth
$out.AddRange([byte[]](LE32 ([uint32]($w * 4))))          # dwPitchOrLinearSize (bytes per row, 4bpp)
$out.AddRange([byte[]](LE32 0))                           # dwDepth
$out.AddRange([byte[]](LE32 0))                           # dwMipMapCount
for ($i = 0; $i -lt 11; $i++) { $out.AddRange([byte[]](LE32 0)) } # dwReserved1[11]

# DDS_PIXELFORMAT (32 bytes)
$out.AddRange([byte[]](LE32 32))                          # dwSize
$out.AddRange([byte[]](0x41, 0x00, 0x00, 0x00))   # dwFlags: ALPHAPIXELS|RGB
$out.AddRange([byte[]](LE32 0))                           # dwFourCC (0 = uncompressed)
$out.AddRange([byte[]](LE32 32))                          # dwRGBBitCount
$out.AddRange([byte[]](0x00, 0x00, 0xFF, 0x00))   # dwRBitMask   = 0x00FF0000
$out.AddRange([byte[]](0x00, 0xFF, 0x00, 0x00))   # dwGBitMask   = 0x0000FF00
$out.AddRange([byte[]](0xFF, 0x00, 0x00, 0x00))   # dwBBitMask   = 0x000000FF
$out.AddRange([byte[]](0x00, 0x00, 0x00, 0xFF))   # dwABitMask   = 0xFF000000

$out.AddRange([byte[]](0x00, 0x10, 0x00, 0x00))   # dwCaps: TEXTURE (0x1000)
$out.AddRange([byte[]](LE32 0))                           # dwCaps2
$out.AddRange([byte[]](LE32 0))                           # dwCaps3
$out.AddRange([byte[]](LE32 0))                           # dwCaps4
$out.AddRange([byte[]](LE32 0))                           # dwReserved2

$out.AddRange($pixelBytes)

[System.IO.File]::WriteAllBytes($OutFile, $out.ToArray())
Write-Output "Wrote $OutFile (${w}x${h}, $($out.Count) bytes)"
