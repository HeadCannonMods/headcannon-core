#!/usr/bin/env pwsh
# Shared release notes generator for CameraUnlock mods
# Generates changelog from commits that touched source files
#
# Usage: generate-release-notes.ps1 -Version <version> -ArtifactPaths <paths> [-ProjectName <name>]

param(
    [Parameter(Mandatory=$true)]
    [string]$Version,

    [Parameter(Mandatory=$true)]
    [string[]]$ArtifactPaths,

    [string]$ProjectName = "CameraUnlock Mod",

    [string]$OutputFile = "release-notes.txt"
)

$ErrorActionPreference = "Stop"

# Manual override takes priority
if (Test-Path "RELEASE_NOTES.md") {
    Write-Host "Using RELEASE_NOTES.md override" -ForegroundColor Cyan
    Copy-Item "RELEASE_NOTES.md" $OutputFile
    Get-Content $OutputFile
    exit 0
}

# Check for previous tag
$previousTag = git describe --tags --abbrev=0 HEAD^ 2>$null

if (-not $previousTag -or $LASTEXITCODE -ne 0) {
    # First release - use default feature description
    Write-Host "First release, using default notes" -ForegroundColor Cyan
    $notes = @"
## $ProjectName v$Version

Initial release with head tracking support via OpenTrack.

### Features
- Head tracking via OpenTrack UDP protocol
- Configurable sensitivity and smoothing
- Hotkeys for recenter and toggle

### Installation
See INSTALL.md for setup instructions.
"@
    $notes | Out-File -FilePath $OutputFile -Encoding utf8
    Get-Content $OutputFile
    exit 0
}

# Get commits that touched artifact-affecting paths
Write-Host "Generating changelog from $previousTag to HEAD" -ForegroundColor Cyan
Write-Host "Artifact paths: $($ArtifactPaths -join ', ')" -ForegroundColor Gray

$commits = git log "$previousTag..HEAD" --pretty=format:"- %s" --no-merges -- $ArtifactPaths

if (-not $commits) {
    Write-Host "No artifact-affecting commits found" -ForegroundColor Yellow
    "## v$Version`n`nBug fixes and improvements." | Out-File -FilePath $OutputFile -Encoding utf8
    Get-Content $OutputFile
    exit 0
}

# Filter out internal/noise commits
$filtered = $commits | Where-Object {
    $_ -notmatch "^- (chore|refactor|internal|clean ?up|wip|fixup|squash):" -and
    $_ -notmatch "^- (Update (cameraunlock|submodule)|Merge )" -and
    $_ -notmatch "^- (bump|release|version)" -and
    $_ -notmatch "^- Release v\d+"
}

if ($filtered) {
    if ($filtered -is [array]) {
        $commitList = $filtered -join "`n"
    } else {
        $commitList = $filtered
    }
    $notes = "## What's Changed in v$Version`n`n$commitList"
} else {
    $notes = "## v$Version`n`nBug fixes and improvements."
}

$notes | Out-File -FilePath $OutputFile -Encoding utf8
Write-Host "`nRelease notes:" -ForegroundColor Green
Get-Content $OutputFile
