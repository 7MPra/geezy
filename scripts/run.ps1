param([string]$Dolphin = $env:DOLPHIN_PATH)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dol = Join-Path $root 'apps/geezy/boot.dol'
if (!(Test-Path $dol)) { throw 'Run scripts/build.ps1 first.' }

if ([string]::IsNullOrWhiteSpace($Dolphin)) {
    $cmd = Get-Command Dolphin.exe -ErrorAction SilentlyContinue
    $candidates = @(
        $(if ($cmd) { $cmd.Source })
        "$env:ProgramFiles\Dolphin-x64\Dolphin.exe"
        "$env:LOCALAPPDATA\Programs\Dolphin-x64\Dolphin.exe"
    ) + (Get-PSDrive -PSProvider FileSystem | ForEach-Object { Join-Path $_.Root 'Dolphin-x64\Dolphin.exe' })
    $Dolphin = $candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($Dolphin) -or !(Test-Path $Dolphin)) {
    throw 'Dolphin.exe not found. Pass -Dolphin <path> or set DOLPHIN_PATH.'
}

# Use a project-local profile so the user's Dolphin settings are preserved.
$profile = Join-Path $root '.dolphin-prototype'
New-Item -ItemType Directory -Force "$profile/Config" | Out-Null
$graphics = Join-Path $profile 'Config/GFX.ini'
if (!(Test-Path $graphics)) {
    "[Settings]`nAspectRatio = 3" | Set-Content $graphics
}
$controller = Join-Path $profile 'Config/WiimoteNew.ini'
if (!(Test-Path $controller)) {
    @'
[Wiimote1]
Source = 1
Device = DInput/0/Keyboard Mouse
Buttons/A = `A`
Buttons/B = `B`
Buttons/1 = `1`
Buttons/2 = `2`
Buttons/- = `Q` | `R`
Buttons/+ = `E` | `R`
Buttons/Home = `H`
D-Pad/Up = `LEFT`
D-Pad/Down = `RIGHT`
D-Pad/Left = `DOWN`
D-Pad/Right = `UP`
[Wiimote2]
Source = 0
[Wiimote3]
Source = 0
[Wiimote4]
Source = 0
'@ | Set-Content $controller
}
Start-Process -FilePath $Dolphin -ArgumentList @('-u', ('"' + $profile + '"'), '-e', ('"' + $dol + '"'))
