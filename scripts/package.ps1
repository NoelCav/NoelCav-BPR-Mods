# Builds the release zips into .\dist\. Every zip mirrors the game folder, so installing is
# "extract into the folder that has BurnoutPR.exe":
#   mod-manager.dll, imgui.dll      shared by every mod (in every zip)
#   mods\<mod>.dll                  the mod itself
# File names carry no version, so https://github.com/<owner>/<repo>/releases/latest/download/<zip>
# always points at the newest release.
#
#   .\scripts\package.ps1             build Release x86, then package
#   .\scripts\package.ps1 -NoBuild    package what's already built

param(
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$Root = Split-Path -Parent $PSScriptRoot
$Configuration = "Release x86"
$Mods = @("teleport", "dashboard", "controls", "junkyard", "camera", "borderless")
$Prefix = "NoelCav-BPR"

if (-not $NoBuild)
{
    $MSBuild = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
    & $MSBuild (Join-Path $Root "mods.slnx") /p:Configuration=Release /p:Platform=x86 /m /v:minimal /nologo
    if ($LASTEXITCODE -ne 0)
    {
        throw "Build failed."
    }
}

$Dist = Join-Path $Root "dist"
$Staging = Join-Path $Dist "staging"
Remove-Item -Path $Dist -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $Staging -Force | Out-Null

$Core = @(
    (Join-Path $Root "libraries\mod-manager\bin\$Configuration\mod-manager.dll"),
    (Join-Path $Root "vendor\imgui\bin\$Configuration\imgui.dll")
)
$Docs = @(
    (Join-Path $Root "LICENSE"),
    (Join-Path $Root "docs\INSTALL.txt")
)

function New-Package([string]$Name, [string[]]$ModNames)
{
    $Folder = Join-Path $Staging $Name
    New-Item -ItemType Directory -Path (Join-Path $Folder "mods") -Force | Out-Null
    Copy-Item -Path ($Core + $Docs) -Destination $Folder
    foreach ($Mod in $ModNames)
    {
        Copy-Item -Path (Join-Path $Root "mods\$Mod\bin\$Configuration\$Mod.dll") -Destination (Join-Path $Folder "mods")
    }
    # Entry names written by hand: Compress-Archive and ZipFile.CreateFromDirectory in Windows
    # PowerShell 5.1 store "mods\x.dll" with a backslash, which some extractors turn into a file
    # literally named that.
    $Zip = [IO.Compression.ZipFile]::Open((Join-Path $Dist "$Name.zip"), "Create")
    try
    {
        foreach ($File in Get-ChildItem -Path $Folder -Recurse -File)
        {
            $Entry = $File.FullName.Substring($Folder.Length + 1).Replace("\", "/")
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($Zip, $File.FullName, $Entry) | Out-Null
        }
    }
    finally
    {
        $Zip.Dispose()
    }
    Write-Host "Packaged $Name.zip"
}

New-Package "$Prefix-All-Mods" $Mods
foreach ($Mod in $Mods)
{
    New-Package "$Prefix-$((Get-Culture).TextInfo.ToTitleCase($Mod))" @($Mod)
}

Remove-Item -Path $Staging -Recurse -Force
