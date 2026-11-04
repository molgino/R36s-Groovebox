<#
  Lokal bauen und sofort auf den R36S (Groovebox-OS) spielen - ohne Commit. Baut in WSL-Debian (tools/dev.sh).
    .\tools\dev.ps1            # arm64-Build + App-Update aufspielen (Groovebox startet neu)
    .\tools\dev.ps1 -NoDeploy  # nur bauen -> dist\groovebox-os-update.tgz
    .\tools\dev.ps1 -Test      # Unit-Tests + ThreadSanitizer + Selbsttest (wie die CI, mit Test-Samples)
    .\tools\dev.ps1 -Quick     # nur Unit-Tests
  Einmalig: WSL + Debian 13 mit den Paketen aus dem Kopf von tools/dev.sh.
#>
param([switch]$NoDeploy, [switch]$Test, [switch]$Quick, [string]$Ip = "192.168.77.1", [string]$Distro = "Debian")
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot
$wslRepo = (& wsl -d $Distro wslpath -a ($repo -replace '\\', '/')).Trim()
$mode = if ($Quick) { "quick" } elseif ($Test) { "test" } else { "os" }
$t0 = Get-Date
$ErrorActionPreference = "Continue"   # Compiler-Warnungen auf stderr sind kein Abbruch (PowerShell 5.1)
& wsl -d $Distro -- bash "$wslRepo/tools/dev.sh" $mode 2>&1 | ForEach-Object { "$_" }
$rc = $LASTEXITCODE
$ErrorActionPreference = "Stop"
if ($rc -ne 0) { throw "Build/Test fehlgeschlagen ($mode)" }
Write-Host ("fertig in {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds)
if ($mode -ne "os" -or $NoDeploy) { exit 0 }
& (Join-Path $PSScriptRoot "deploy.ps1") -Update (Join-Path $repo "dist\groovebox-os-update.tgz") -Ip $Ip
