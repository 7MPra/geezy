param([string]$Dolphin=$env:DOLPHIN_PATH,[switch]$Lattice,[switch]$Checker,[switch]$Sculptures,[switch]$Trail,[switch]$Graphics,[switch]$Stencil,[switch]$Worlds,[switch]$Gpu,[switch]$Master,[switch]$Profile,[ValidateRange(0,3)][int]$Beams=0)
$ErrorActionPreference='Stop'
if([string]::IsNullOrWhiteSpace($Dolphin)) {
    $cmd=Get-Command Dolphin.exe -ErrorAction SilentlyContinue
    $candidates=@(
        $(if($cmd){$cmd.Source})
        "$env:ProgramFiles\Dolphin-x64\Dolphin.exe"
        "$env:LOCALAPPDATA\Programs\Dolphin-x64\Dolphin.exe"
    )+(Get-PSDrive -PSProvider FileSystem | ForEach-Object { Join-Path $_.Root 'Dolphin-x64\Dolphin.exe' })
    $Dolphin=$candidates | Where-Object { $_ -and (Test-Path $_) } | Select-Object -First 1
}
if([string]::IsNullOrWhiteSpace($Dolphin) -or !(Test-Path $Dolphin)){throw 'Dolphin.exe not found. Pass -Dolphin <path> or set DOLPHIN_PATH.'}
if((@($Lattice,$Checker,$Sculptures,$Trail,$Graphics,$Stencil,$Worlds,$Gpu,$Master) | Where-Object {$_}).Count -gt 1){throw 'Choose one diagnostic mode'}
if($Profile -and ($Gpu -or $Master -or $Stencil -or $Checker -or $Trail -or $Lattice)){throw 'Profile requires the complete render path'}
Push-Location (Split-Path $PSScriptRoot -Parent)
$process=$null
try {
    $build='/opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc -O2 -Wall -Wextra -DGEKKO -DGEEZY_VISUAL_CHECK -mrvl -mcpu=750 -meabi -mhard-float -I/opt/devkitpro/libogc/include source/main.c -L/opt/devkitpro/libogc/lib/wii -lwiiuse -lbte -logc -lm -o geezy-check.elf && /opt/devkitpro/tools/bin/elf2dol geezy-check.elf geezy-check.dol'
    if($Lattice){$build=$build.Replace('-DGEEZY_VISUAL_CHECK',"-DGEEZY_VISUAL_CHECK -DGEEZY_LATTICE_CHECK -DGEEZY_BEAM_STYLE=$Beams")}
    if($Checker){$build=$build.Replace('-DGEEZY_VISUAL_CHECK','-DGEEZY_VISUAL_CHECK -DGEEZY_CHECKER_CHECK')}
    if($Sculptures){$build=$build.Replace('-DGEEZY_VISUAL_CHECK','-DGEEZY_VISUAL_CHECK -DGEEZY_SCULPTURE_CHECK')}
    if($Trail){$build=$build.Replace('-DGEEZY_VISUAL_CHECK','-DGEEZY_VISUAL_CHECK -DGEEZY_TRAIL_CHECK')}
    if($Graphics){$build=$build.Replace('-DGEEZY_VISUAL_CHECK','-DGEEZY_VISUAL_CHECK -DGEEZY_GRAPHICS_CHECK')}
    if($Master){$build=$build.Replace('-DGEEZY_VISUAL_CHECK','-DGEEZY_VISUAL_CHECK -DGEEZY_MASTER_CHECK')}
    if($Gpu){$build=$build.Replace('-DGEEZY_VISUAL_CHECK','-DGEEZY_VISUAL_CHECK -DGEEZY_GPU_CHECK')}
    if($Worlds){$build=$build.Replace('-DGEEZY_VISUAL_CHECK','-DGEEZY_VISUAL_CHECK -DGEEZY_WORLDS_CHECK')}
    if($Stencil){$build=$build.Replace('-DGEEZY_VISUAL_CHECK','-DGEEZY_VISUAL_CHECK -DGEEZY_STENCIL_CHECK')}
    if($Profile){$build=$build.Replace('-DGEEZY_VISUAL_CHECK','-DGEEZY_VISUAL_CHECK -DGEEZY_PROFILE')}
    docker run --rm --mount "type=bind,source=$pwd,target=/project" -w /project devkitpro/devkitppc:latest bash -lc $build
    if($LASTEXITCODE -ne 0){throw 'Visual check build failed'}
    New-Item -ItemType Directory -Force .dolphin-check/Config,artifacts | Out-Null
    "[Settings]`nAspectRatio = 3" | Set-Content .dolphin-check/Config/GFX.ini
    if($Gpu -or $Master){Add-Content .dolphin-check/Config/GFX.ini "[Hacks]`nEFBToTextureEnable = False`nDeferEFBCopies = False"}
    "[Interface]`nConfirmStop = False" | Set-Content .dolphin-check/Config/Dolphin.ini
    Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class GeezyCapture {
public delegate bool EnumProc(IntPtr h,IntPtr l);
[DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc p,IntPtr l);
[DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
[DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
[DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h,out Rect r);
[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
[DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr ctx);
public struct Rect {public int left,top,right,bottom;}
public static IntPtr Find(uint pid) {
    IntPtr result=IntPtr.Zero;
    EnumWindows((h,l)=>{uint id;GetWindowThreadProcessId(h,out id);var s=new StringBuilder(512);
        GetWindowText(h,s,512);if(id==pid && s.ToString().Contains("JIT64"))result=h;return true;},IntPtr.Zero);
    return result;
}
public static int BlackPixels(byte[] pixels) {
    int count=0;for(int i=0;i<pixels.Length;i+=4)if(pixels[i]==0 && pixels[i+1]==0 && pixels[i+2]==0)count++;
    return count;
}
public static int WhitePixels(byte[] pixels) {
    int count=0;for(int i=0;i<pixels.Length;i+=4)if(pixels[i]>=252 && pixels[i+1]>=252 && pixels[i+2]>=252)count++;
    return count;
}
public static int NeonPixels(byte[] pixels) {
    int count=0;for(int i=0;i<pixels.Length;i+=4){
        int lo=Math.Min(pixels[i],Math.Min(pixels[i+1],pixels[i+2]));
        int hi=Math.Max(pixels[i],Math.Max(pixels[i+1],pixels[i+2]));
        if(lo<=3 && hi>=252)count++;
    }return count;
}
}
"@
    $oldDpi=[GeezyCapture]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))
    Add-Type -AssemblyName System.Drawing
    $process=Start-Process $Dolphin -ArgumentList @('-u',('"'+$pwd+'/.dolphin-check"'),'-e',('"'+$pwd+'/geezy-check.dol"')) -WindowStyle Hidden -PassThru
    $window=[IntPtr]::Zero
    for($attempt=0;$attempt -lt 30;$attempt++) {
        $window=[GeezyCapture]::Find($process.Id)
        if($window -ne [IntPtr]::Zero){break}
        Start-Sleep -Milliseconds 500
    }
    if($window -eq [IntPtr]::Zero){throw 'Dolphin renderer not found'}
    Start-Sleep -Seconds 2
    $results=@()
    $count=if($Master){8}elseif($Gpu){45}elseif($Worlds){156}elseif($Sculptures){244}elseif($Graphics){156}elseif($Stencil){64}elseif($Trail){240}elseif($Checker){96}elseif($Lattice){64}else{26}
    $prefix=if($Master){'master'}elseif($Gpu){'gpu'}elseif($Worlds){'worlds'}elseif($Graphics){'graphics'}elseif($Stencil){'stencil'}elseif($Sculptures){'sculptures'}elseif($Trail){'trail'}elseif($Lattice){"lattice-$Beams"}elseif($Checker){'checker'}else{'render'}
    if($Profile){$prefix="profile-$prefix"}
    $timer=[Diagnostics.Stopwatch]::StartNew()
    for($i=0;$i -lt $count;$i++) {
        $r=New-Object GeezyCapture+Rect
        [GeezyCapture]::GetClientRect($window,[ref]$r) | Out-Null
        $bitmap=New-Object System.Drawing.Bitmap $r.right,$r.bottom
        $captureGraphics=[System.Drawing.Graphics]::FromImage($bitmap)
        $dc=$captureGraphics.GetHdc()
        try { $ok=[GeezyCapture]::PrintWindow($window,$dc,3) } finally { $captureGraphics.ReleaseHdc($dc) }
        if(!$ok){throw 'PrintWindow failed'}
        $file=Join-Path $pwd "artifacts/$prefix-$i.png"
        $bitmap.Save($file)
        $rect=New-Object System.Drawing.Rectangle 0,0,$bitmap.Width,$bitmap.Height
        $data=$bitmap.LockBits($rect,[System.Drawing.Imaging.ImageLockMode]::ReadOnly,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $bytes=New-Object byte[] ([Math]::Abs($data.Stride)*$data.Height)
        [Runtime.InteropServices.Marshal]::Copy($data.Scan0,$bytes,0,$bytes.Length)
        $bitmap.UnlockBits($data)
        $black=[GeezyCapture]::BlackPixels($bytes)
        $white=[GeezyCapture]::WhitePixels($bytes)
        $neon=[GeezyCapture]::NeonPixels($bytes)
        $mixed=$bytes.Length/4-$black-$white-$neon
        $results+=[pscustomobject]@{gpuPass=($bitmap.GetPixel(4,$bitmap.Height-4).G -eq 255 -and $bitmap.GetPixel(4,$bitmap.Height-4).R -eq 0);world=[int][Math]::Round(($bitmap.GetPixel(4,$bitmap.Height-4).R-32)/32.0);theme=[int][Math]::Round(($bitmap.GetPixel(4,$bitmap.Height-4).G-32)/48.0);frame=$i;seconds=$timer.Elapsed.TotalSeconds;pixels=$bytes.Length/4;blackPixels=$black;whitePixels=$white;neonPixels=$neon;mixedPixels=$mixed;centerBlue=($bitmap.GetPixel([int]($bitmap.Width/2),[int]($bitmap.Height/2)).B -eq 255 -and $bitmap.GetPixel([int]($bitmap.Width/2),[int]($bitmap.Height/2)).R -eq 0);cornerBlue=($bitmap.GetPixel(5,5).B -eq 255 -and $bitmap.GetPixel(5,5).R -eq 0);hash=(Get-FileHash $file).Hash;file=$file}
        $captureGraphics.Dispose();$bitmap.Dispose()
        Start-Sleep -Milliseconds $(if($Sculptures -or $Graphics -or $Worlds){500}elseif($Trail){10}elseif($Lattice -or $Checker -or $Stencil){100}else{1000})
    }
    $results | ConvertTo-Json | Set-Content "artifacts/$prefix-check.json"
    $results | Format-Table frame,blackPixels,whitePixels,neonPixels,mixedPixels
    # RGB channel displacement is inspected alongside the black/white/neon frame checks.
    if(!$Master -and !$Gpu -and !$Lattice -and !$Checker -and !$Trail -and !$Stencil -and ($results | Where-Object { $_.blackPixels -gt 0 -and $_.whitePixels -gt 0 -and $_.neonPixels -gt 0 }).Count -lt 8){throw 'Too few frames contain black, white and saturated colors'}
    if(($Gpu -or $Master) -and !$results[-1].gpuPass){throw 'GPU reference comparison failed or did not finish'}
    if($Worlds) {
        $seen=@($results | Group-Object world,theme)
        if($seen.Count -ne 15){throw 'Not all 5 worlds x 3 themes were captured'}
        foreach($group in $seen) {
            if(@($group.Group.hash | Sort-Object -Unique).Count -lt 2){throw "World/theme $($group.Name) appears frozen"}
        }
    }
    if($Trail) {
        if(!($results | Where-Object { $_.blackPixels -eq $_.pixels })){throw 'Trail did not decay to exact black'}
        if(!($results | Where-Object { $_.whitePixels -gt 1000 })){throw 'Trail source was not rendered'}
        if(!($results | Where-Object { $_.whitePixels -eq 0 -and $_.mixedPixels -gt 1000 })){throw 'Recursive trail was not rendered'}
    }
    if(!$Master -and !$Gpu -and !$Trail -and ($results.hash | Sort-Object -Unique).Count -lt 8){throw 'Render walkthrough appears frozen'}
    if($Stencil) {
        foreach($frame in $results) {
            if(!$frame.centerBlue -or $frame.cornerBlue){throw 'Z-buffer window did not preserve the center and cover the outside'}
        }
    }
    if($Lattice) {
        foreach($frame in $results) {
            if($frame.pixels-$frame.blackPixels -lt 1000){throw 'Lattice vanished during travel'}
        }
        for($i=1;$i -lt $results.Count;$i++) {
            if([Math]::Abs($results[$i].blackPixels-$results[$i-1].blackPixels)/$results[$i].pixels -gt 0.1){throw 'Lattice coverage jumped between consecutive captures'}
        }
    }
} finally {
    if($process -and !$process.HasExited){Stop-Process -Id $process.Id}
    if($oldDpi){[GeezyCapture]::SetThreadDpiAwarenessContext($oldDpi) | Out-Null}
    Pop-Location
}
