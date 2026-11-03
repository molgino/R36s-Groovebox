#!/usr/bin/env bash
# R36S Groovebox - Aufspielen per USB-Kabel (PC-Modus) oder WLAN (macOS/Linux).
#   ./deploy.sh [zip]          neueste Version (nightly) bzw. angegebenes ZIP aufspielen
#   ./deploy.sh --setup-key    SSH-Schluessel hinterlegen
#   ./deploy.sh --log          Log live anzeigen
# Umgebungsvariablen: R36S_IP (192.168.77.1), R36S_USER (ark), GB_REPO (molgino/R36s-Groovebox)
set -euo pipefail
IP=${R36S_IP:-192.168.77.1}; USER_=${R36S_USER:-ark}; REPO=${GB_REPO:-molgino/R36s-Groovebox}
OPTS=(-o StrictHostKeyChecking=accept-new -o ConnectTimeout=5)
ping -c1 -W1 "$IP" >/dev/null 2>&1 || { ping -c1 -W1 169.254.77.1 >/dev/null 2>&1 && IP=169.254.77.1; }
T="$USER_@$IP"
# Groovebox-OS? Dann App-Update ueber die Upload-Schnittstelle (kein SSH noetig)
if [ "${1:-}" != "--setup-key" ] && [ "${1:-}" != "--shell" ] && [ "${1:-}" != "--log" ] && \
   curl -fs --max-time 5 "http://$IP/api/list" | grep -q '"os":true'; then
  UPD=${1:-}
  if [ -z "$UPD" ]; then UPD=$(mktemp --suffix=.tgz); curl -fL -o "$UPD" "https://github.com/$REPO/releases/download/nightly/groovebox-os-update.tgz"; fi
  curl -fsS -T "$UPD" "http://$IP/api/update"; echo
  echo "Groovebox startet neu und installiert das Update ..."; sleep 20
  curl -fs --max-time 10 "http://$IP/api/list" | grep -o '"version":"[^"]*"' || echo "noch nicht erreichbar - Log: GROOVEBOX/logs/update.log"
  exit 0
fi
case "${1:-}" in
  --setup-key) [ -f ~/.ssh/id_ed25519 ] || ssh-keygen -t ed25519 -N "" -f ~/.ssh/id_ed25519; ssh-copy-id "${OPTS[@]}" -i ~/.ssh/id_ed25519.pub "$T"; exit ;;
  --shell) exec ssh "${OPTS[@]}" "$T" ;;
esac
GB=$(ssh "${OPTS[@]}" "$T" '[ -d /roms2/ports/groovebox ] && echo /roms2/ports/groovebox || echo /roms/ports/groovebox')
[ "${1:-}" = "--log" ] && exec ssh "${OPTS[@]}" "$T" "tail -n 50 -f $GB/groovebox.log"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
ZIP=${1:-}
if [ -z "$ZIP" ]; then
  [[ "$REPO" == OWNER/* ]] && { echo "GB_REPO=<github-name>/R36s-Groovebox setzen"; exit 1; }
  ZIP="$TMP/pkg.zip"; curl -fL -o "$ZIP" "https://github.com/$REPO/releases/download/nightly/r36s-groovebox-arkos.zip"
fi
unzip -q "$ZIP" -d "$TMP/pkg"
ssh "${OPTS[@]}" "$T" 'for p in $(pgrep -x groovebox); do grep -q -- --dhcpd /proc/$p/cmdline || kill -TERM $p; done; sleep 1; true'
scp "${OPTS[@]}" "$TMP"/pkg/groovebox/groovebox "$T:$GB/groovebox.new"
ssh "${OPTS[@]}" "$T" "mv -f $GB/groovebox.new $GB/groovebox"
scp "${OPTS[@]}" "$TMP"/pkg/groovebox/{pc-modus.sh,groovebox.service,LIESMICH.md,VERSION} "$T:$GB/"
scp "${OPTS[@]}" "$TMP"/pkg/*.sh "$T:$(dirname "$GB")/"
ssh "${OPTS[@]}" "$T" "chmod +x $GB/groovebox $GB/pc-modus.sh $(dirname "$GB")/Groovebox*.sh; sync"
echo "Aufgespielt: Version $(cat "$TMP/pkg/groovebox/VERSION")"
if ssh "${OPTS[@]}" "$T" "systemctl is-enabled groovebox" 2>/dev/null | grep -q enabled; then
  ssh "${OPTS[@]}" "$T" "sudo systemctl restart groovebox"; echo "neu gestartet"
else echo "am R36S unter Ports neu starten"; fi
exec ssh "${OPTS[@]}" "$T" "sleep 2; tail -n 30 -f $GB/groovebox.log"
