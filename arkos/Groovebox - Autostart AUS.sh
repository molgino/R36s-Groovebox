#!/bin/bash
# Stellt den normalen ArkOS-Start (EmulationStation) wieder her.
ES_UNIT=$(systemctl list-unit-files 2>/dev/null | awk '/^emulationstation/{print $1; exit}')
sudo systemctl disable groovebox.service 2>/dev/null
sudo rm -f /etc/systemd/system/groovebox.service
sudo systemctl daemon-reload
[ -n "$ES_UNIT" ] && sudo systemctl enable "$ES_UNIT"
echo "Autostart aus - beim naechsten Start kommt wieder EmulationStation." > /dev/tty1
sleep 3
