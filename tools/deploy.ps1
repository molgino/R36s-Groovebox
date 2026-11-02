<#
  R36S Groovebox - Aufspielen per USB-Kabel (PC-Modus) oder WLAN, ohne SD-Karte.

  Voraussetzungen:
    * R36S: in ArkOS "Options > Enable Remote Services" an, in der Groovebox SYS-Seite USB = PC
    * Windows 10/11 (OpenSSH-Client ist dabei: ssh/scp)

  Beispiele:
    .\deploy.ps1                     # neueste Version von GitHub (nightly) aufspielen
    .\deploy.ps1 -Zip .\r36s-groovebox-arkos.zip
    .\deploy.ps1 -SetupKey           # einmalig: SSH-Schluessel hinterlegen (danach kein Passwort mehr)
    .\deploy.ps1 -Log                # nur das Log live anzeigen
    .\deploy.ps1 -Shell              # SSH-Konsole oeffnen
#>
param(
    [string]$Ip = "192.168.77.1",
    [string]$User = "ark",
    [string]$Repo = "OWNER/R36s-Groovebox",
    [string]$Zip = "",
    [switch]$SetupKey,
    [switch]$Log,
    [switch]$Shell,
    [switch]$NoRestart
)
$ErrorActionPreference = "Stop"
$Target = "$User@$Ip"
$SshOpts = @("-o", "StrictHostKeyChecking=accept-new", "-o", "ConnectTimeout=5")

function Remote([string]$cmd) { & ssh @SshOpts $Target $cmd; if ($LASTEXITCODE -ne 0) { throw "SSH-Befehl fehlgeschlagen: $cmd" } }

# R36S erreichbar? (USB-Netz: erst 192.168.77.1, sonst 169.254.77.1)
if (-not (Test-Connection -Count 1 -Quiet $Ip)) {
    if ($Ip -eq "192.168.77.1" -and (Test-Connection -Count 1 -Quiet "169.254.77.1")) { $Ip = "169.254.77.1"; $Target = "$User@$Ip" }
    else { Write-Host "R36S unter $Ip nicht erreichbar. PC-Modus aktiv? Kabel steckt? (Windows braucht nach dem Einstecken bis zu 1 Minute)" -ForegroundColor Red; exit 1 }
}
Write-Host "R36S erreichbar: $Ip" -ForegroundColor Green

if ($SetupKey) {
    $key = Join-Path $env:USERPROFILE ".ssh\id_ed25519"
    if (-not (Test-Path $key)) { & ssh-keygen -t ed25519 -N '""' -f $key | Out-Null }
    $pub = Get-Content "$key.pub"
    & ssh @SshOpts $Target "mkdir -p ~/.ssh && chmod 700 ~/.ssh && grep -qxF '$pub' ~/.ssh/authorized_keys 2>/dev/null || echo '$pub' >> ~/.ssh/authorized_keys; chmod 600 ~/.ssh/authorized_keys"
    Write-Host "SSH-Schluessel hinterlegt - ab jetzt ohne Passwort." -ForegroundColor Green
    exit 0
}
if ($Shell) { & ssh @SshOpts $Target; exit 0 }

$GbDir = "/roms/ports/groovebox"
$Ports = "/roms/ports"
$probe = & ssh @SshOpts $Target "[ -d /roms2/ports/groovebox ] && echo roms2 || echo roms"
if ($probe -match "roms2") { $GbDir = "/roms2/ports/groovebox"; $Ports = "/roms2/ports" }

if ($Log) { & ssh @SshOpts $Target "tail -n 50 -f $GbDir/groovebox.log"; exit 0 }

# Paket besorgen
$tmp = Join-Path $env:TEMP "r36s-groovebox-deploy"
if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
New-Item -ItemType Directory $tmp | Out-Null
if (-not $Zip) {
    if ($Repo -like "OWNER/*") { Write-Host "Bitte -Repo <github-name>/R36s-Groovebox angeben oder im Skript eintragen." -ForegroundColor Red; exit 1 }
    $Zip = Join-Path $tmp "pkg.zip"
    $url = "https://github.com/$Repo/releases/download/nightly/r36s-groovebox-arkos.zip"
    Write-Host "Lade $url"
    Invoke-WebRequest -Uri $url -OutFile $Zip -UseBasicParsing
}
$ex = Join-Path $tmp "pkg"
Expand-Archive -Path $Zip -DestinationPath $ex

# Groovebox beenden, Dateien kopieren (Projekte, Samples, Einstellungen bleiben unberuehrt)
Write-Host "Kopiere auf den R36S ..."
Remote "pkill -TERM -x groovebox; sleep 1; true"
& scp @SshOpts (Join-Path $ex "groovebox\groovebox") (Join-Path $ex "groovebox\pc-modus.sh") (Join-Path $ex "groovebox\groovebox.service") (Join-Path $ex "groovebox\LIESMICH.md") (Join-Path $ex "groovebox\VERSION") "${Target}:$GbDir/"
if ($LASTEXITCODE -ne 0) { throw "scp fehlgeschlagen" }
$scripts = Get-ChildItem $ex -Filter "*.sh" | ForEach-Object { $_.FullName }
& scp @SshOpts $scripts "${Target}:$Ports/"
Remote "chmod +x $GbDir/groovebox $GbDir/pc-modus.sh '$Ports'/Groovebox*.sh; sync"
$ver = Get-Content (Join-Path $ex "groovebox\VERSION") -ErrorAction SilentlyContinue
Write-Host "Aufgespielt: Version $ver" -ForegroundColor Green

if (-not $NoRestart) {
    $svc = & ssh @SshOpts $Target "systemctl is-enabled groovebox 2>/dev/null"
    if ($svc -match "enabled") { Remote "sudo systemctl restart groovebox"; Write-Host "Groovebox neu gestartet (Autostart-Dienst)." }
    else { Write-Host "Groovebox beendet - am R36S unter Ports neu starten." -ForegroundColor Yellow }
    Write-Host "Log (Strg+C beendet):"
    & ssh @SshOpts $Target "sleep 2; tail -n 30 -f $GbDir/groovebox.log"
}
