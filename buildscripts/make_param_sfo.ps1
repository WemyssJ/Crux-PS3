# Generates PARAM.SFO for the Crux PS3 package. The SFO format (used by every
# PS3 title for package/game metadata) isn't produced by any tool that ships
# in the SDK's host-win32/bin -- ps3sys.exe (Tools/ps3gen) is a GUI app meant
# for official master-disc submission, not scriptable here. The binary layout
# below follows the publicly documented PS3 SFO format (header + sorted index
# table + null-terminated key table + data table).
#
# Usage: powershell -File build\make_param_sfo.ps1 -OutFile <path>

param(
    [string]$OutFile = "PARAM.SFO"
)

# Keys MUST be in ascending ASCII order -- real PS3 firmware SFO parsers rely on it.
$entries = @(
    @{ Key = "APP_VER";        Type = "str"; Value = "01.00" }
    @{ Key = "ATTRIBUTE";      Type = "int"; Value = 0 }
    @{ Key = "BOOTABLE";       Type = "int"; Value = 1 }
    @{ Key = "CATEGORY";       Type = "str"; Value = "HG" }
    @{ Key = "PARENTAL_LEVEL"; Type = "int"; Value = 0 }
    @{ Key = "PS3_SYSTEM_VER"; Type = "str"; Value = "01.0000" }
    @{ Key = "RESOLUTION";     Type = "int"; Value = 63 }
    @{ Key = "SOUND_FORMAT";   Type = "int"; Value = 1 }
    @{ Key = "TITLE";          Type = "str"; Value = "Crux" }
    @{ Key = "TITLE_ID";       Type = "str"; Value = "CRUX00001" }
    @{ Key = "VERSION";        Type = "str"; Value = "01.00" }
)

$keyBytesList = @()
$dataBytesList = @()
$indexEntries = @()

$keyOffset = 0
$dataOffset = 0

foreach ($e in $entries) {
    $keyBytes = [System.Text.Encoding]::ASCII.GetBytes($e.Key + "`0")
    $keyBytesList += ,$keyBytes

    if ($e.Type -eq "str") {
        $valBytes = [System.Text.Encoding]::UTF8.GetBytes([string]$e.Value + "`0")
        $fmt = 0x0204
    } else {
        $valBytes = [System.BitConverter]::GetBytes([int32]$e.Value)
        $fmt = 0x0404
    }
    $dataBytesList += ,$valBytes

    $indexEntries += [PSCustomObject]@{
        KeyOffset   = $keyOffset
        Fmt         = $fmt
        DataLen     = $valBytes.Length
        DataMaxLen  = $valBytes.Length
        DataOffset  = $dataOffset
    }

    $keyOffset += $keyBytes.Length
    $dataOffset += $valBytes.Length
}

$keyTableRaw = New-Object System.Collections.Generic.List[byte]
foreach ($kb in $keyBytesList) { $keyTableRaw.AddRange($kb) }
# Pad key table to a 4-byte boundary before the data table starts.
while ($keyTableRaw.Count % 4 -ne 0) { $keyTableRaw.Add(0) }

$dataTableRaw = New-Object System.Collections.Generic.List[byte]
foreach ($db in $dataBytesList) { $dataTableRaw.AddRange($db) }

$headerSize = 20
$indexSize = 16 * $entries.Count
$keyTableStart = $headerSize + $indexSize
$dataTableStart = $keyTableStart + $keyTableRaw.Count

$out = New-Object System.Collections.Generic.List[byte]
# Header: magic "\0PSF", version 1.1, key table start, data table start, entry count.
$out.AddRange([byte[]](0x00,0x50,0x53,0x46))
$out.AddRange([System.BitConverter]::GetBytes([int32]0x00000101))
$out.AddRange([System.BitConverter]::GetBytes([int32]$keyTableStart))
$out.AddRange([System.BitConverter]::GetBytes([int32]$dataTableStart))
$out.AddRange([System.BitConverter]::GetBytes([int32]$entries.Count))

foreach ($ie in $indexEntries) {
    $out.AddRange([System.BitConverter]::GetBytes([uint16]$ie.KeyOffset))
    $out.AddRange([System.BitConverter]::GetBytes([uint16]$ie.Fmt))
    $out.AddRange([System.BitConverter]::GetBytes([int32]$ie.DataLen))
    $out.AddRange([System.BitConverter]::GetBytes([int32]$ie.DataMaxLen))
    $out.AddRange([System.BitConverter]::GetBytes([int32]$ie.DataOffset))
}

$out.AddRange($keyTableRaw)
$out.AddRange($dataTableRaw)

[System.IO.File]::WriteAllBytes($OutFile, $out.ToArray())
Write-Output "Wrote $OutFile ($($out.Count) bytes, $($entries.Count) entries)"
