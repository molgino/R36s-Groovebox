#!/bin/bash
# Macht die Groovebox zur Startoberflaeche des R36S (statt EmulationStation).
# Beenden der Groovebox (SELECT+START 2s halten) startet EmulationStation wieder.
GBDIR="/roms/ports/groovebox"
[ -d "/roms2/ports/groovebox" ] && GBDIR="/roms2/ports/groovebox"
ES_UNIT=$(systemctl list-unit-files 2>/dev/null | awk '/^emulationstation/{print $1; exit}')
if [ -z "$ES_UNIT" ]; then
  echo "emulationstation-Dienst nicht gefunden - Autostart nicht moeglich" > /dev/tty1
  sleep 4; exit 1
fi
sed -e "s|@GBDIR@|$GBDIR|g" -e "s|@ESUNIT@|$ES_UNIT|g" "$GBDIR/groovebox.service" | sudo tee /etc/systemd/system/groovebox.service > /dev/null
sudo systemctl daemon-reload
sudo systemctl enable groovebox.service
sudo systemctl disable "$ES_UNIT"
echo "Groovebox-Autostart aktiv. Neustart ..." > /dev/tty1
sleep 2
sudo reboot
