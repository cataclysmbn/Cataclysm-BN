# Das Spiel aktualisieren

# Empfohlen

0. Lade das Spiel von https://github.com/cataclysmbn/Cataclysm-BN/releases herunter
1. Entpacke das Spiel
2. Kopiere `save` und `config` aus dem alten Spielverzeichnis in das neue Verzeichnis
3. (Optional) Kopiere `mods` aus dem alten Spielverzeichnis

# Nicht empfohlen

### Das neue Spiel über das alte Verzeichnis entpacken

Das Entpacken des Spiels über das alte Verzeichnis kann zu Fehlern durch doppelte JSON-Einträge führen. Wenn du dies unbedingt tun möchtest, stelle sicher, dass du das alte `data`-Verzeichnis löschst, bevor du die neue Version entpackst.

Manchmal werden Datendateien im Core gelöscht. Die neue Version des Spiels wird diese Dateien nicht haben, aber das Entpacken eines Archivs löscht die alten Dateien nicht. Alte Dateien werden weiterhin gelesen und geladen, was neue Einträge überschreiben kann.

### Eigene Mods im `data/mods`-Verzeichnis haben

Lege sie direkt in das `mods`-Verzeichnis, auf der gleichen Ebene wie `data`. Dieses Verzeichnis existiert standardmäßig nicht, aber das Spiel wird es lesen, wenn es vorhanden ist.

Dies ermöglicht es dir, die Mods zwischen Spielversionen zu kopieren, während die "Core-Mods" weiterhin korrekt aktualisiert werden können.
