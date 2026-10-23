param([string]$Dolphin = 'D:\Dolphin-x64\Dolphin.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dol = Join-Path $root 'apps/geezy/boot.dol'
if (!(Test-Path $dol)) { throw 'Run scripts/build.ps1 first.' }
if (!(Test-Path $Dolphin)) { throw "Dolphin not found: $Dolphin" }
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
