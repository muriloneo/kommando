Param(
    [string]$ProjectDir = ".",
    [string]$BaseUrl = "",
    [string]$GitHubRepo = "",
    [string]$GitHubTag = "",
    [switch]$Configure,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$projectPath = (Resolve-Path $ProjectDir).Path
Set-Location $projectPath

$pythonCandidates = @(
    "C:\Espressif\tools\python\v6.0-beta2\venv\Scripts\python.exe",
    "python"
)

$pythonExe = $null
foreach ($candidate in $pythonCandidates) {
    try {
        if ($candidate -eq "python") {
            $null = & $candidate --version 2>$null
            if ($LASTEXITCODE -eq 0) {
                $pythonExe = $candidate
                break
            }
        } elseif (Test-Path $candidate) {
            $pythonExe = $candidate
            break
        }
    } catch {
        continue
    }
}

if (-not $pythonExe) {
    Write-Error "Python not found. Activate ESP-IDF environment or install Python."
    exit 2
}

$scriptPath = Join-Path $projectPath "tools\build_and_package_ota.py"
if (-not (Test-Path $scriptPath)) {
    Write-Error "Script not found: $scriptPath"
    exit 3
}

$args = @($scriptPath, "--project-dir", $projectPath)

if ($GitHubRepo -and $GitHubTag) {
    $args += @("--github-repo", $GitHubRepo, "--github-tag", $GitHubTag)
} elseif ($BaseUrl) {
    $args += @("--base-url", $BaseUrl)
} else {
    Write-Error "Provide either -BaseUrl OR both -GitHubRepo and -GitHubTag."
    exit 4
}

if ($Configure) { $args += "--configure" }
if ($SkipBuild) { $args += "--skip-build" }

Write-Host "[INFO] Running OTA build/publish pipeline..."
& $pythonExe @args
$code = $LASTEXITCODE

if ($code -ne 0) {
    Write-Warning "OTA pipeline finished with non-zero exit code: $code"
    exit $code
}

Write-Host "[OK] OTA pipeline finished."
