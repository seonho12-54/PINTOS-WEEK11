$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Resolve-Path (Join-Path $ScriptDir "..")
$SourceDir = Join-Path $RepoRoot ".codex\skills"

if (-not (Test-Path -Path $SourceDir -PathType Container)) {
    throw "Skill source directory not found: $SourceDir"
}

if ($env:CODEX_HOME) {
    $TargetRoot = $env:CODEX_HOME
} else {
    $TargetRoot = Join-Path $HOME ".codex"
}

$TargetDir = Join-Path $TargetRoot "skills"
New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null

function Install-Skill {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SkillName
    )

    $SourceSkill = Join-Path $SourceDir $SkillName
    $TargetSkill = Join-Path $TargetDir $SkillName
    $SkillFile = Join-Path $SourceSkill "SKILL.md"

    if (-not (Test-Path -Path $SkillFile -PathType Leaf)) {
        Write-Warning "Skipping ${SkillName}: missing SKILL.md"
        return
    }

    if (Test-Path -Path $TargetSkill) {
        Remove-Item -Path $TargetSkill -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $TargetSkill | Out-Null
    Copy-Item -Path (Join-Path $SourceSkill "*") -Destination $TargetSkill -Recurse -Force

    Get-ChildItem -Path $TargetSkill -Directory -Filter "__pycache__" -Recurse -ErrorAction SilentlyContinue |
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Get-ChildItem -Path $TargetSkill -File -Filter "*.pyc" -Recurse -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue

    Write-Host "Installed ${SkillName} -> $TargetSkill"
}

Install-Skill "gh-create-issue-branch"
Install-Skill "gh-create-pr"
Install-Skill "gh-review-pr"

Write-Host "Done. Restart Codex if the skills are not visible yet."
