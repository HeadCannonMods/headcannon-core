#!/usr/bin/env pwsh
# DevDeploy.psm1 - Orchestrators for `pixi run install` (dev-deploy) flows
# Part of CameraUnlock-Core shared utilities.
#
# Each per-strategy `Invoke-DevDeploy*` runs the canonical dev-install
# pipeline for that loader (resolve game, copy DLLs from build output,
# run the patch / loader-setup, cleanup). Per-mod scripts/deploy.ps1
# files reduce to a thin wrapper that sets the mod-specific config and
# delegates here. Source of truth for the orchestration is here - bumping
# the cameraunlock-core submodule + re-running `pixi run install` ships
# any change without per-mod re-templating.

$ErrorActionPreference = "Stop"

Import-Module (Join-Path $PSScriptRoot 'GamePathDetection.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'ModDeployment.psm1')     -Force
Import-Module (Join-Path $PSScriptRoot 'ModLoaderSetup.psm1')    -Force

# Shared resolver: prefer caller-supplied -GivenPath (the launcher always
# passes one), else fall back to Find-GamePath against games.json. Throws
# with a clear diagnostic on miss so every orchestrator's "not found"
# error matches the install.cmd template's wording.
function Resolve-DevGamePath {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$GameId,
        [Parameter(Mandatory)][string]$GameDisplayName,
        [string]$GivenPath
    )
    if ($GivenPath) {
        Write-Host "Using launcher-provided game path: $GivenPath" -ForegroundColor Green
        return $GivenPath
    }
    $gamePath = Find-GamePath -GameId $GameId
    if (-not $gamePath) {
        $config = Get-GameConfig -GameId $GameId
        Write-GameNotFoundError `
            -GameName $GameDisplayName `
            -EnvVar $config.EnvVar `
            -SteamFolder $config.SteamFolder
        throw "Game not found: $GameDisplayName"
    }
    Write-Host "Found game installation at: $gamePath" -ForegroundColor Green
    return $gamePath
}

# Shared resolver: locate the build output directory and verify the
# primary mod DLL exists. Throws the canonical "run pixi run build" hint
# on miss so every orchestrator surfaces the same dev-flow error.
function Resolve-DevBuildOutput {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$ProjectRoot,
        [Parameter(Mandatory)][string]$ProjectName,
        [Parameter(Mandatory)][string]$ModDllName,
        [Parameter(Mandatory)][string]$Configuration
    )
    $buildOutput = Get-BuildOutputPath -ProjectRoot $ProjectRoot -ProjectName $ProjectName -Configuration $Configuration
    $sourceDll = Join-Path $buildOutput $ModDllName
    if (-not (Test-Path $sourceDll)) {
        throw "Built DLL not found at: $sourceDll. Run 'pixi run build' first."
    }
    return $buildOutput
}

<#
.SYNOPSIS
    Dev-deploy a Mono.Cecil-patched mod into the game's Managed folder.
.DESCRIPTION
    The Cecil flow: build output already produced by `pixi run build`,
    DLLs copied into Managed, then a per-mod patcher script-block is
    invoked to mutate Assembly-CSharp.dll. Backup/restore around the
    patch keeps re-runs clean (always patches a known-good baseline).
    Mods on the SHARED screen-center patcher pass a script block that
    calls Invoke-HeadTrackingPatch from AssemblyPatching.psm1; mods
    with a custom BootstrapPatcher.cs pass a block that compiles +
    runs that source against Mono.Cecil.
.PARAMETER GameId
    games.json id used by Find-GamePath when GivenPath is empty.
.PARAMETER GameDisplayName
    Friendly name for the "not found" diagnostic. Only used on miss.
.PARAMETER ProjectRoot
    Mod repo root. Build output is resolved as
    <ProjectRoot>\src\<ProjectName>\bin\<Configuration>\net48\.
.PARAMETER ProjectName
    csproj folder name under src\. Often (but not always) equal to
    ModDllName without the .dll suffix.
.PARAMETER ModDllName
    Final DLL filename produced by the build (e.g. HeadTracking.dll).
    Some mods rename their assembly during build; others leave it.
.PARAMETER Configuration
    Debug | Release - selects the build output subfolder.
.PARAMETER ManagedSubfolder
    Relative path under GamePath that contains Assembly-CSharp.dll
    (e.g. GoneHome_Data\Managed).
.PARAMETER AssemblyDll
    Filename of the assembly to patch. Defaults to Assembly-CSharp.dll.
.PARAMETER ExtraDlls
    Other DLLs from the build output to copy into Managed alongside
    ModDllName (e.g. CameraUnlock.Core.dll). Missing entries are a
    deploy error - if you mean "copy if present", use a build step to
    guarantee it.
.PARAMETER GivenPath
    Optional explicit game path. When supplied (the launcher always
    does), Find-GamePath is bypassed. Empty / null -> auto-detect.
.PARAMETER Patcher
    [scriptblock] called with one positional arg ($assemblyPath). Must
    throw on failure. The orchestrator calls it AFTER backup-restore so
    every invocation patches the original assembly.
.OUTPUTS
    Hashtable: @{ GamePath; ManagedPath; DeployedDllPath }
#>
function Invoke-DevDeployCecil {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$GameId,
        [Parameter(Mandatory)][string]$GameDisplayName,
        [Parameter(Mandatory)][string]$ProjectRoot,
        [Parameter(Mandatory)][string]$ProjectName,
        [Parameter(Mandatory)][string]$ModDllName,
        [Parameter(Mandatory)][ValidateSet('Debug','Release')][string]$Configuration,
        [Parameter(Mandatory)][string]$ManagedSubfolder,
        [string]$AssemblyDll = 'Assembly-CSharp.dll',
        [string[]]$ExtraDlls = @(),
        [string]$GivenPath,
        [Parameter(Mandatory)][scriptblock]$Patcher
    )

    $gamePath = Resolve-DevGamePath -GameId $GameId -GameDisplayName $GameDisplayName -GivenPath $GivenPath
    $managedPath = Join-Path $gamePath $ManagedSubfolder
    if (-not (Test-Path $managedPath)) {
        throw "Managed folder not found at: $managedPath"
    }
    $assemblyPath = Join-Path $managedPath $AssemblyDll
    if (-not (Test-Path $assemblyPath)) {
        throw "$AssemblyDll not found at: $assemblyPath"
    }
    $buildOutput = Resolve-DevBuildOutput -ProjectRoot $ProjectRoot -ProjectName $ProjectName -ModDllName $ModDllName -Configuration $Configuration

    # -------- Copy mod files --------
    Write-Host ""
    Write-Host "Deploying mod files..." -ForegroundColor Yellow
    $copyResult = Copy-ModFiles `
        -SourceDir $buildOutput `
        -TargetDir $managedPath `
        -ModDllName $ModDllName `
        -Dependencies $ExtraDlls
    if (-not $copyResult.Success) {
        Write-DeploymentError -Errors $copyResult.Errors
        throw "Copy-ModFiles failed"
    }

    # -------- Backup + clean-restore Assembly DLL --------
    Write-Host ""
    Write-Host "Patching $AssemblyDll..." -ForegroundColor Yellow
    $backupPath = New-FileBackup -FilePath $assemblyPath
    if ($backupPath -and (Test-Path $backupPath)) {
        Restore-FileFromBackup -FilePath $assemblyPath | Out-Null
    }

    # -------- Run the per-mod patcher --------
    & $Patcher $assemblyPath

    # -------- Cleanup stale doorstop files (Cecil mods don't use them) --------
    $removedFiles = @(Remove-OldDoorstopFiles -GamePath $gamePath)
    if ($removedFiles.Count -gt 0) {
        Write-Host "  Cleaned up $($removedFiles.Count) old doorstop file(s)" -ForegroundColor Gray
    }

    return @{
        GamePath        = $gamePath
        ManagedPath     = $managedPath
        DeployedDllPath = (Join-Path $managedPath $ModDllName)
    }
}

<#
.SYNOPSIS
    Dev-deploy a BepInEx mod to <game>/BepInEx/plugins/.
.DESCRIPTION
    Ensures BepInEx is installed (via ModLoaderSetup.Install-BepInEx) if
    -EnsureLoader is set. Copies the freshly-built mod DLL + extras from
    bin/<Configuration>/net48/ into BepInEx/plugins/. No assembly patch.
.PARAMETER EnsureLoader
    When set, calls Install-BepInEx if BepInEx isn't already detected
    in the game folder. Pass $false to fail loud on missing loader
    (useful in CI / strict dev flows).
.PARAMETER MajorVersion
    BepInEx major version to install when EnsureLoader fires (5 or 6).
.PARAMETER Architecture
    BepInEx architecture (x64 or x86) for the install.
#>
function Invoke-DevDeployBepInEx {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$GameId,
        [Parameter(Mandatory)][string]$GameDisplayName,
        [Parameter(Mandatory)][string]$ProjectRoot,
        [Parameter(Mandatory)][string]$ProjectName,
        [Parameter(Mandatory)][string]$ModDllName,
        [Parameter(Mandatory)][ValidateSet('Debug','Release')][string]$Configuration,
        [string[]]$ExtraDlls = @(),
        [string]$GivenPath,
        [switch]$EnsureLoader,
        [ValidateSet(5,6)][int]$MajorVersion = 5,
        [ValidateSet('x64','x86')][string]$Architecture = 'x64'
    )

    $gamePath    = Resolve-DevGamePath -GameId $GameId -GameDisplayName $GameDisplayName -GivenPath $GivenPath
    $buildOutput = Resolve-DevBuildOutput -ProjectRoot $ProjectRoot -ProjectName $ProjectName -ModDllName $ModDllName -Configuration $Configuration

    if (-not (Test-BepInExInstalled -GamePath $gamePath)) {
        if ($EnsureLoader) {
            Install-BepInEx -GamePath $gamePath -MajorVersion $MajorVersion -Architecture $Architecture | Out-Null
        } else {
            throw "BepInEx not detected at $gamePath. Pass -EnsureLoader to auto-install, or install BepInEx by hand."
        }
    }

    $pluginsPath = Get-BepInExPluginsPath -GamePath $gamePath
    if (-not (Test-Path $pluginsPath)) { New-Item -ItemType Directory -Path $pluginsPath -Force | Out-Null }

    Write-Host ""
    Write-Host "Deploying mod files..." -ForegroundColor Yellow
    $copyResult = Copy-ModFiles `
        -SourceDir $buildOutput `
        -TargetDir $pluginsPath `
        -ModDllName $ModDllName `
        -Dependencies $ExtraDlls
    if (-not $copyResult.Success) {
        Write-DeploymentError -Errors $copyResult.Errors
        throw "Copy-ModFiles failed"
    }

    return @{
        GamePath        = $gamePath
        PluginsPath     = $pluginsPath
        DeployedDllPath = (Join-Path $pluginsPath $ModDllName)
    }
}

<#
.SYNOPSIS
    Dev-deploy a MelonLoader mod to <game>/Mods/.
.DESCRIPTION
    Ensures MelonLoader is installed (via Install-MelonLoader) if
    -EnsureLoader is set. Copies mod DLL + extras into Mods/. Some mods
    pin a specific MelonLoader version (Firewatch needs 0.5.7 to avoid
    the RegexOptions crash on Unity 2017 Mono) - pass it via -Version.
#>
function Invoke-DevDeployMelonLoader {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$GameId,
        [Parameter(Mandatory)][string]$GameDisplayName,
        [Parameter(Mandatory)][string]$ProjectRoot,
        [Parameter(Mandatory)][string]$ProjectName,
        [Parameter(Mandatory)][string]$ModDllName,
        [Parameter(Mandatory)][ValidateSet('Debug','Release')][string]$Configuration,
        [string[]]$ExtraDlls = @(),
        [string]$GivenPath,
        [switch]$EnsureLoader,
        [ValidateSet('x64','x86')][string]$Architecture = 'x64',
        [string]$Version
    )

    $gamePath    = Resolve-DevGamePath -GameId $GameId -GameDisplayName $GameDisplayName -GivenPath $GivenPath
    $buildOutput = Resolve-DevBuildOutput -ProjectRoot $ProjectRoot -ProjectName $ProjectName -ModDllName $ModDllName -Configuration $Configuration

    if (-not (Test-MelonLoaderInstalled -GamePath $gamePath)) {
        if ($EnsureLoader) {
            $installArgs = @{ GamePath = $gamePath; Architecture = $Architecture }
            if ($Version) { $installArgs['Version'] = $Version }
            Install-MelonLoader @installArgs | Out-Null
        } else {
            throw "MelonLoader not detected at $gamePath. Pass -EnsureLoader to auto-install, or install MelonLoader by hand."
        }
    }

    $modsPath = Get-MelonLoaderModsPath -GamePath $gamePath
    if (-not (Test-Path $modsPath)) { New-Item -ItemType Directory -Path $modsPath -Force | Out-Null }

    Write-Host ""
    Write-Host "Deploying mod files..." -ForegroundColor Yellow
    $copyResult = Copy-ModFiles `
        -SourceDir $buildOutput `
        -TargetDir $modsPath `
        -ModDllName $ModDllName `
        -Dependencies $ExtraDlls
    if (-not $copyResult.Success) {
        Write-DeploymentError -Errors $copyResult.Errors
        throw "Copy-ModFiles failed"
    }

    return @{
        GamePath        = $gamePath
        ModsPath        = $modsPath
        DeployedDllPath = (Join-Path $modsPath $ModDllName)
    }
}

<#
.SYNOPSIS
    Dev-deploy an Ultimate ASI Loader mod to the game's exe directory.
.DESCRIPTION
    Copies the .asi (and any sibling INI / config files) from the build
    output to the directory containing the game executable, derived from
    GameExeRelpath. Loader install is NOT auto-handled here - dev
    iteration assumes the loader (winmm.dll / dinput8.dll) is already
    in place from a prior install.cmd run; if missing the deploy fails
    loudly so the dev knows to run install.cmd first.
.PARAMETER GameExeRelpath
    Relative path under GamePath to the game's main exe. Used to derive
    the exe directory (where ASI plugins land for nested-exe games).
.PARAMETER AsiLoaderName
    Filename the ASI DLL was renamed to (winmm.dll, dinput8.dll, etc.).
    Used only for the loader-presence check.
#>
function Invoke-DevDeployASILoader {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$GameId,
        [Parameter(Mandatory)][string]$GameDisplayName,
        [Parameter(Mandatory)][string]$ProjectRoot,
        [Parameter(Mandatory)][string]$ProjectName,
        [Parameter(Mandatory)][string]$ModDllName,
        [Parameter(Mandatory)][ValidateSet('Debug','Release')][string]$Configuration,
        [Parameter(Mandatory)][string]$GameExeRelpath,
        [string]$AsiLoaderName = 'winmm.dll',
        [string[]]$ExtraDlls = @(),
        [string]$GivenPath
    )

    $gamePath    = Resolve-DevGamePath -GameId $GameId -GameDisplayName $GameDisplayName -GivenPath $GivenPath
    $buildOutput = Resolve-DevBuildOutput -ProjectRoot $ProjectRoot -ProjectName $ProjectName -ModDllName $ModDllName -Configuration $Configuration

    $exePath = Join-Path $gamePath $GameExeRelpath
    $exeDir = Split-Path -Parent $exePath
    if (-not (Test-Path $exeDir)) {
        throw "Exe directory not found: $exeDir (derived from $GameExeRelpath)"
    }
    if (-not (Test-Path (Join-Path $exeDir $AsiLoaderName))) {
        throw "ASI loader $AsiLoaderName not present at $exeDir. Run install.cmd to install the loader first."
    }

    Write-Host ""
    Write-Host "Deploying mod files to: $exeDir" -ForegroundColor Yellow
    $copyResult = Copy-ModFiles `
        -SourceDir $buildOutput `
        -TargetDir $exeDir `
        -ModDllName $ModDllName `
        -Dependencies $ExtraDlls
    if (-not $copyResult.Success) {
        Write-DeploymentError -Errors $copyResult.Errors
        throw "Copy-ModFiles failed"
    }

    return @{
        GamePath        = $gamePath
        ExeDir          = $exeDir
        DeployedDllPath = (Join-Path $exeDir $ModDllName)
    }
}

<#
.SYNOPSIS
    Dev-deploy a REFramework mod to <game>/reframework/plugins/.
.DESCRIPTION
    Copies the mod DLL + extras into reframework/plugins/. Loader
    install is NOT auto-handled (REFramework's per-game zip is
    vendored, easier to install via install.cmd than by-hand here);
    the deploy fails loudly if dinput8.dll / reframework/ are missing.
#>
function Invoke-DevDeployREFramework {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$GameId,
        [Parameter(Mandatory)][string]$GameDisplayName,
        [Parameter(Mandatory)][string]$ProjectRoot,
        [Parameter(Mandatory)][string]$ProjectName,
        [Parameter(Mandatory)][string]$ModDllName,
        [Parameter(Mandatory)][ValidateSet('Debug','Release')][string]$Configuration,
        [string[]]$ExtraDlls = @(),
        [string]$GivenPath
    )

    $gamePath    = Resolve-DevGamePath -GameId $GameId -GameDisplayName $GameDisplayName -GivenPath $GivenPath
    $buildOutput = Resolve-DevBuildOutput -ProjectRoot $ProjectRoot -ProjectName $ProjectName -ModDllName $ModDllName -Configuration $Configuration

    if (-not (Test-Path (Join-Path $gamePath 'dinput8.dll'))) {
        throw "REFramework loader (dinput8.dll) not present at $gamePath. Run install.cmd to install the loader first."
    }
    $pluginsPath = Join-Path $gamePath 'reframework\plugins'
    if (-not (Test-Path $pluginsPath)) { New-Item -ItemType Directory -Path $pluginsPath -Force | Out-Null }

    Write-Host ""
    Write-Host "Deploying mod files to: $pluginsPath" -ForegroundColor Yellow
    $copyResult = Copy-ModFiles `
        -SourceDir $buildOutput `
        -TargetDir $pluginsPath `
        -ModDllName $ModDllName `
        -Dependencies $ExtraDlls
    if (-not $copyResult.Success) {
        Write-DeploymentError -Errors $copyResult.Errors
        throw "Copy-ModFiles failed"
    }

    return @{
        GamePath        = $gamePath
        PluginsPath     = $pluginsPath
        DeployedDllPath = (Join-Path $pluginsPath $ModDllName)
    }
}

<#
.SYNOPSIS
    Dev-deploy a shim-only mod (system-DLL replacement) to the game's
    exe directory, with first-install backup of any pre-existing file.
.DESCRIPTION
    Shim mods replace a system DLL the game loads at startup
    (xinput1_3.dll / dxgi.dll / winmm.dll / etc.). On first deploy, if
    the target name already exists in the exe dir, back it up to
    <name>.backup so uninstall can restore it. Subsequent deploys leave
    the .backup intact - the user's pre-mod state must survive
    re-deploys.
#>
function Invoke-DevDeployShim {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string]$GameId,
        [Parameter(Mandatory)][string]$GameDisplayName,
        [Parameter(Mandatory)][string]$ProjectRoot,
        [Parameter(Mandatory)][string]$ProjectName,
        [Parameter(Mandatory)][string]$ModDllName,
        [Parameter(Mandatory)][ValidateSet('Debug','Release')][string]$Configuration,
        [Parameter(Mandatory)][string]$GameExeRelpath,
        [string[]]$ExtraDlls = @(),
        [string]$GivenPath
    )

    $gamePath    = Resolve-DevGamePath -GameId $GameId -GameDisplayName $GameDisplayName -GivenPath $GivenPath
    $buildOutput = Resolve-DevBuildOutput -ProjectRoot $ProjectRoot -ProjectName $ProjectName -ModDllName $ModDllName -Configuration $Configuration

    $exePath = Join-Path $gamePath $GameExeRelpath
    $exeDir = Split-Path -Parent $exePath
    if (-not (Test-Path $exeDir)) {
        throw "Exe directory not found: $exeDir (derived from $GameExeRelpath)"
    }

    # Backup-on-first-deploy. Same semantics as the install body's shim
    # logic - .backup is the user's pre-mod state and never gets clobbered.
    $allFiles = @($ModDllName) + $ExtraDlls
    foreach ($f in $allFiles) {
        $target = Join-Path $exeDir $f
        $backup = "$target.backup"
        if ((Test-Path $target) -and -not (Test-Path $backup)) {
            Copy-Item -Path $target -Destination $backup -Force
            Write-Host "  Backed up original $f to $f.backup" -ForegroundColor Gray
        }
    }

    Write-Host ""
    Write-Host "Deploying shim files to: $exeDir" -ForegroundColor Yellow
    $copyResult = Copy-ModFiles `
        -SourceDir $buildOutput `
        -TargetDir $exeDir `
        -ModDllName $ModDllName `
        -Dependencies $ExtraDlls
    if (-not $copyResult.Success) {
        Write-DeploymentError -Errors $copyResult.Errors
        throw "Copy-ModFiles failed"
    }

    return @{
        GamePath        = $gamePath
        ExeDir          = $exeDir
        DeployedDllPath = (Join-Path $exeDir $ModDllName)
    }
}

Export-ModuleMember -Function @(
    'Invoke-DevDeployCecil',
    'Invoke-DevDeployBepInEx',
    'Invoke-DevDeployMelonLoader',
    'Invoke-DevDeployASILoader',
    'Invoke-DevDeployREFramework',
    'Invoke-DevDeployShim',
    'Resolve-DevGamePath',
    'Resolve-DevBuildOutput'
)
