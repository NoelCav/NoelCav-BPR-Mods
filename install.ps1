$Configurations = (
    "Debug x86",
    "Release x86"
)

Write-Host -Object "Choose configuration:"
for ($i = 0; $i -lt $Configurations.Length; ++$i)
{
    Write-Host -Object "$($i + 1) - $($Configurations[$i])"
}

$Choice = Read-Host -Prompt ">"
$Configuration = $Configurations[$Choice - 1]

# Every mod is independent: each one is a single DLL (plus an optional "rsc\" assets folder)
# that needs only the shared mod manager. Pick any combination.
$Mods = @(Get-ChildItem -Path ".\mods" -Directory | Where-Object {
    Test-Path -Path (Join-Path -Path $_.FullName -ChildPath "bin\$Configuration\$($_.Name).dll")
})

if ($Mods.Length -eq 0)
{
    Write-Host -Object "No built mods found for '$Configuration'. Build the solution first."
    exit 1
}

Write-Host -Object "Choose mods to install (e.g. 1,3), or press Enter for all:"
for ($i = 0; $i -lt $Mods.Length; ++$i)
{
    Write-Host -Object "$($i + 1) - $($Mods[$i].Name)"
}

$ModChoice = Read-Host -Prompt ">"
if (-not [string]::IsNullOrWhiteSpace($ModChoice))
{
    $Mods = @($ModChoice.Split(",") | ForEach-Object { $Mods[[int]$_.Trim() - 1] })
}

$BprDirectory = Get-ItemPropertyValue -Path "HKLM:\SOFTWARE\WOW6432Node\Criterion\BurnoutPR\" -Name "Install Dir"
$BprModsDirectory = Join-Path -Path $BprDirectory -ChildPath "mods"
New-Item -ItemType Directory -Path $BprModsDirectory -Force | Out-Null

# Shared by every mod.
Copy-Item -Path ".\libraries\mod-manager\bin\$Configuration\mod-manager.dll" -Destination $BprDirectory
Copy-Item -Path ".\vendor\imgui\bin\$Configuration\imgui.dll" -Destination $BprDirectory

foreach ($ModDirectory in $Mods)
{
    Copy-Item -Path (Join-Path -Path $ModDirectory.FullName -ChildPath "bin\$Configuration\$($ModDirectory.Name).dll") -Destination $BprModsDirectory

    # Non-DLL assets (textures, fonts) go into a sibling "<mod-name>-assets\" folder so the
    # mod can find them relative to its own DLL at runtime.
    $RscDirectory = Join-Path -Path $ModDirectory.FullName -ChildPath "rsc"
    if (Test-Path -Path $RscDirectory)
    {
        $AssetsDestination = Join-Path -Path $BprModsDirectory -ChildPath "$($ModDirectory.Name)-assets"
        New-Item -ItemType Directory -Path $AssetsDestination -Force | Out-Null
        Copy-Item -Path "$RscDirectory\*" -Destination $AssetsDestination -Recurse -Force
    }

    Write-Host -Object "Installed $($ModDirectory.Name)."
}

Write-Host -Object "Installation finished. To remove a mod, delete its DLL (and its -assets folder) from '$BprModsDirectory'."
