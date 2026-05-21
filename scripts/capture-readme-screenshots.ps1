param(
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$repo = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$assets = Join-Path $repo "docs\assets"
$tmp = Join-Path $env:TEMP ("openstaller-readme-" + [guid]::NewGuid().ToString("N"))
$cli = Join-Path $repo "build\$Configuration\openstaller-cli.exe"
$builder = Join-Path $repo "build\$Configuration\openstaller.exe"
$payload = Join-Path $repo "examples\sample_payload"

New-Item -ItemType Directory -Force -Path $tmp | Out-Null
New-Item -ItemType Directory -Force -Path $assets | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class OpenstallerShotWin32 {
  [StructLayout(LayoutKind.Sequential)]
  public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT rect);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hWnd, int x, int y, int w, int h, bool repaint);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint msg, IntPtr wParam, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hWnd, IntPtr hdcBlt, uint flags);
  [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hwnd, int dwAttribute, out RECT pvAttribute, int cbAttribute);
}
"@

function Get-CaptureRect {
    param([IntPtr]$Handle)

    $rect = New-Object OpenstallerShotWin32+RECT
    $dwmStatus = [OpenstallerShotWin32]::DwmGetWindowAttribute($Handle, 9, [ref]$rect, [Runtime.InteropServices.Marshal]::SizeOf($rect))
    if ($dwmStatus -eq 0 -and $rect.Right -gt $rect.Left -and $rect.Bottom -gt $rect.Top) {
        return $rect
    }

    [OpenstallerShotWin32]::GetWindowRect($Handle, [ref]$rect) | Out-Null
    return $rect
}

function Capture-Window {
    param(
        [Parameter(Mandatory=$true)][string]$Exe,
        [Parameter(Mandatory=$true)][string]$OutFile,
        [int]$Width = 0,
        [int]$Height = 0,
        [switch]$Move,
        [int]$NextClicks = 0,
        [int]$NextButtonId = 2002
    )

    $process = Start-Process -FilePath $Exe -PassThru
    try {
        for ($i = 0; $i -lt 80; $i++) {
            Start-Sleep -Milliseconds 150
            $process.Refresh()
            if ($process.MainWindowHandle -ne 0) {
                break
            }
        }

        if ($process.MainWindowHandle -eq 0) {
            throw "No main window for $Exe"
        }

        if ($Move) {
            [OpenstallerShotWin32]::MoveWindow($process.MainWindowHandle, 80, 70, $Width, $Height, $true) | Out-Null
        }

        [OpenstallerShotWin32]::ShowWindow($process.MainWindowHandle, 1) | Out-Null
        [OpenstallerShotWin32]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
        for ($click = 0; $click -lt $NextClicks; $click++) {
            [OpenstallerShotWin32]::PostMessage($process.MainWindowHandle, 0x0111, [IntPtr]::new($NextButtonId), [IntPtr]::Zero) | Out-Null
            Start-Sleep -Milliseconds 300
        }
        Start-Sleep -Milliseconds 900

        $rect = Get-CaptureRect $process.MainWindowHandle
        $captureWidth = [Math]::Max(1, $rect.Right - $rect.Left)
        $captureHeight = [Math]::Max(1, $rect.Bottom - $rect.Top)

        $bitmap = New-Object System.Drawing.Bitmap $captureWidth, $captureHeight
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $hdc = $graphics.GetHdc()
        $printed = [OpenstallerShotWin32]::PrintWindow($process.MainWindowHandle, $hdc, 2)
        $graphics.ReleaseHdc($hdc)
        if (!$printed) {
            $graphics.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bitmap.Size)
        }
        $crop = New-Object System.Drawing.Rectangle 1, 1, ([Math]::Max(1, $captureWidth - 2)), ([Math]::Max(1, $captureHeight - 2))
        $clean = $bitmap.Clone($crop, $bitmap.PixelFormat)
        $clean.Save($OutFile, [System.Drawing.Imaging.ImageFormat]::Png)
        $graphics.Dispose()
        $bitmap.Dispose()
        $clean.Dispose()
    } finally {
        if (!$process.HasExited) {
            $process.CloseMainWindow() | Out-Null
            Start-Sleep -Milliseconds 300
            if (!$process.HasExited) {
                $process.Kill()
            }
        }
    }
}

function Drive-Window {
    param(
        [Parameter(Mandatory=$true)][string]$Exe,
        [string[]]$Arguments = @(),
        [int]$NextClicks = 0,
        [int]$WaitMilliseconds = 1500
    )

    $process = Start-Process -FilePath $Exe -ArgumentList $Arguments -PassThru
    try {
        for ($i = 0; $i -lt 80; $i++) {
            Start-Sleep -Milliseconds 150
            $process.Refresh()
            if ($process.MainWindowHandle -ne 0) {
                break
            }
        }

        if ($process.MainWindowHandle -ne 0) {
            [OpenstallerShotWin32]::ShowWindow($process.MainWindowHandle, 1) | Out-Null
            [OpenstallerShotWin32]::SetForegroundWindow($process.MainWindowHandle) | Out-Null
            for ($click = 0; $click -lt $NextClicks; $click++) {
                [OpenstallerShotWin32]::PostMessage($process.MainWindowHandle, 0x0111, [IntPtr]::new(2002), [IntPtr]::Zero) | Out-Null
                Start-Sleep -Milliseconds 350
            }
            Start-Sleep -Milliseconds $WaitMilliseconds
        }
    } finally {
        if (!$process.HasExited) {
            $process.CloseMainWindow() | Out-Null
            Start-Sleep -Milliseconds 300
            if (!$process.HasExited) {
                $process.Kill()
            }
        }
    }
}

if (!(Test-Path -LiteralPath $cli)) {
    throw "Build the project first. Missing $cli"
}
if (!(Test-Path -LiteralPath $builder)) {
    throw "Build the GUI first. Missing $builder"
}

Capture-Window -Exe $builder -OutFile (Join-Path $assets "screenshot-builder.png") -Width 886 -Height 600 -Move -NextClicks 3 -NextButtonId 3002

foreach ($style in @("classic", "modern", "legacy")) {
    $out = Join-Path $tmp $style
    $installRoot = "%LOCALAPPDATA%\AtlasSuite"
    New-Item -ItemType Directory -Force -Path $out | Out-Null

    & $cli `
        --name "Atlas Suite" `
        --company "Northwind Labs" `
        --version "2.4.1" `
        --source $payload `
        --output $out `
        --install-dir $installRoot `
        --launcher "hello.txt" `
        --installer-style $style `
        --window-style resizable `
        --pages "welcome,folder,components,ready,finish" `
        --theme-accent "#0B7A75" `
        --theme-progress "#C026D3" `
        --theme-sidebar "#123C69" `
        --theme-background "#F8FAFC" `
        --theme-panel "#FFFFFF" `
        --theme-legacy-top "#0527D8" `
        --theme-legacy-bottom "#000018" `
        --online-optional "Language Pack" "https://github.com/Olegie/Openstaller/releases/download/demo/language-pack.zip" "extras\language-pack.zip" `
        --online-description "Optional language resources downloaded during setup" | Out-Host

    if ($LASTEXITCODE -ne 0) {
        throw "Package generation failed for $style"
    }

    $installer = Join-Path $out "openstaller-demo-2.4.1\installer.exe"
    if (!(Test-Path -LiteralPath $installer)) {
        $installer = Join-Path $out "atlas-suite-2.4.1\installer.exe"
    }
    if ($style -eq "classic") {
        Capture-Window -Exe $installer -OutFile (Join-Path $assets "screenshot-installer-classic.png") -Width 690 -Height 500 -Move -NextClicks 2
    } elseif ($style -eq "modern") {
        Capture-Window -Exe $installer -OutFile (Join-Path $assets "screenshot-installer-modern.png") -Width 1020 -Height 760 -Move -NextClicks 2
        Capture-Window -Exe $installer -OutFile (Join-Path $assets "screenshot-installer-modern-progress.png") -Width 1020 -Height 760 -Move -NextClicks 4
        $uninstaller = Join-Path ([Environment]::ExpandEnvironmentVariables($installRoot)) "uninstaller.exe"
        if (Test-Path -LiteralPath $uninstaller) {
            Drive-Window -Exe $uninstaller -Arguments @($installRoot) -NextClicks 2 -WaitMilliseconds 1200
        }
    } else {
        Capture-Window -Exe $installer -OutFile (Join-Path $assets "screenshot-installer-legacy.png") -NextClicks 2
    }
}

Copy-Item -LiteralPath (Join-Path $assets "screenshot-installer-classic.png") `
          -Destination (Join-Path $assets "screenshot-installer.png") `
          -Force

Get-ChildItem -Path $assets -Filter "screenshot*.png" |
    Sort-Object Name |
    Select-Object Name, Length
