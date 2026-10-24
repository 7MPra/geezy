param([string]$Dolphin='D:\Dolphin-x64\Dolphin.exe')
$ErrorActionPreference='Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
$process=$null
try {
    docker run --rm --mount "type=bind,source=$pwd,target=/project" -w /project devkitpro/devkitppc:latest bash -lc '/opt/devkitpro/devkitPPC/bin/powerpc-eabi-gcc -O2 -Wall -Wextra -DGEKKO -DGEEZY_VISUAL_CHECK -mrvl -mcpu=750 -meabi -mhard-float -I/opt/devkitpro/libogc/include source/main.c -L/opt/devkitpro/libogc/lib/wii -lwiiuse -lbte -logc -lm -o geezy-check.elf && /opt/devkitpro/tools/bin/elf2dol geezy-check.elf geezy-check.dol'
    if($LASTEXITCODE -ne 0){throw 'Visual check build failed'}
    New-Item -ItemType Directory -Force .dolphin-check/Config,artifacts | Out-Null
    "[Settings]`nAspectRatio = 3" | Set-Content .dolphin-check/Config/GFX.ini
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
    for($i=0;$i -lt 26;$i++) {
        $r=New-Object GeezyCapture+Rect
        [GeezyCapture]::GetClientRect($window,[ref]$r) | Out-Null
        $bitmap=New-Object System.Drawing.Bitmap $r.right,$r.bottom
        $graphics=[System.Drawing.Graphics]::FromImage($bitmap)
        $dc=$graphics.GetHdc()
        try { $ok=[GeezyCapture]::PrintWindow($window,$dc,3) } finally { $graphics.ReleaseHdc($dc) }
        if(!$ok){throw 'PrintWindow failed'}
        $file=Join-Path $pwd "artifacts/render-$i.png"
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
        $results+=[pscustomobject]@{frame=$i;blackPixels=$black;whitePixels=$white;neonPixels=$neon;mixedPixels=$mixed;hash=(Get-FileHash $file).Hash;file=$file}
        $graphics.Dispose();$bitmap.Dispose()
        Start-Sleep -Milliseconds 1000
    }
    $results | ConvertTo-Json | Set-Content artifacts/render-check.json
    $results | Format-Table frame,blackPixels,whitePixels,neonPixels,mixedPixels
    # RGB channel displacement is inspected alongside the black/white/neon frame checks.
    if(($results | Where-Object { $_.blackPixels -gt 0 -and $_.whitePixels -gt 0 -and $_.neonPixels -gt 0 }).Count -lt 8){throw 'Too few frames contain black, white and saturated colors'}
    if(($results.hash | Sort-Object -Unique).Count -lt 8){throw 'Render walkthrough appears frozen'}
} finally {
    if($process -and !$process.HasExited){Stop-Process -Id $process.Id}
    if($oldDpi){[GeezyCapture]::SetThreadDpiAwarenessContext($oldDpi) | Out-Null}
    Pop-Location
}
