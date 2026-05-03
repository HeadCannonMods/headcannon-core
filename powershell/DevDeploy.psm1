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

    # -------- Resolve game path --------
    if ($GivenPath) {
        $gamePath = $GivenPath
        Write-Host "Using launcher-provided game path: $gamePath" -ForegroundColor Green
    } else {
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
    }

    $managedPath = Join-Path $gamePath $ManagedSubfolder
    if (-not (Test-Path $managedPath)) {
        throw "Managed folder not found at: $managedPath"
    }
    $assemblyPath = Join-Path $managedPath $AssemblyDll
    if (-not (Test-Path $assemblyPath)) {
        throw "$AssemblyDll not found at: $assemblyPath"
    }

    # -------- Locate build output --------
    $buildOutput = Get-BuildOutputPath -ProjectRoot $ProjectRoot -ProjectName $ProjectName -Configuration $Configuration
    $sourceDll = Join-Path $buildOutput $ModDllName
    if (-not (Test-Path $sourceDll)) {
        throw "Built DLL not found at: $sourceDll. Run 'pixi run build' first."
    }

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

Export-ModuleMember -Function @(
    'Invoke-DevDeployCecil'
)
