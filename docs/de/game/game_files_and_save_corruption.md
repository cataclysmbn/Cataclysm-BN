# Beschädigung von Spieldateien und Spielständen

Die Beschädigung von Spieldateien und Spielständen fällt in folgende Kategorien:

1. Verursacht durch dein System
2. Verursacht durch deinen Launcher
3. Verursacht durch das Spiel
4. Verursacht durch unsachgemäße Upgrade-Verfahren
5. Verursacht durch den Tod des Charakters

## Beschädigung durch dein System

### Häufige Symptome

- Unsinnige Fehlermeldungen beim Versuch, einen Spielstand zu laden (z. B. "expected comma" oder "expected array")
- Problematische Dateien sind, wenn sie mit einem anständigen Texteditor (Notepad++ für euch Windows-Benutzer) geöffnet werden, teilweise mit Null-Bytes oder anderem Kauderwelsch gefüllt
  ![image](https://user-images.githubusercontent.com/60584843/186019683-36a59d3b-31ce-408c-96ee-42390975171c.png)

### Häufige Ursachen und Möglichkeiten zur Vermeidung

1. Unzureichender Speicherplatz (kaufe eine größere Festplatte)
2. Fehlerhafte Festplatte (kaufe eine neue Festplatte)
3. Speichern auf einem USB-Stick / einer externen Festplatte und Herausziehen ohne "Sicheres Entfernen" (tu das nicht)
4. Stromausfall (kaufe eine USV für deinen Computer)
5. Stromausfall aufgrund von Hardwarefehlern (rufe einen PC-Reparaturdienst)
6. Stromausfall durch Benutzeraktionen (ziehe nicht den Netzstecker deines PCs, fahre den Laptop nicht durch langes Drücken des Netzschalters herunter)
7. Bluescreen oder ein ähnlicher nicht behebbarer Betriebssystemfehler (rufe wieder den PC-Reparaturdienst)

### Behebung der Beschädigung

Wir können nichts dagegen tun, aber du kannst versuchen, die Dateisicherungsfunktion deines Betriebssystems (oder die Sicherungsfunktion des Spiel-Launchers, falls dessen Backups nicht ebenfalls beschädigt wurden) zu nutzen.

## Beschädigung durch deinen Launcher

### Häufige Symptome

1. Alle (oder einige) Spielstände sind plötzlich weg
2. Alle (oder einige) deiner persönlichen Anpassungen in `[Spielstammverzeichnis]/data/` sind plötzlich rückgängig gemacht
3. Alle (oder einige) deiner Mods/Soundpacks sind plötzlich weg
4. Alle (oder einige) Einstellungen sind auf Standardwerte zurückgesetzt

### Häufige Ursachen und Möglichkeiten zur Vermeidung

1. Der Launcher ist fehlerhaft. Nerv die Autor(en)/Maintainer, es zu beheben, oder finde einen anderen Launcher - vielleicht einen, der [versucht, Benutzerdaten nicht anzutasten](https://github.com/qrrk/Catapult).
2. Deine Mods/Soundpacks/persönlichen Anpassungen befanden sich im Ordner `[Spielstammverzeichnis]/data/`. Dieser Ordner ist nur für Basisspiel-Sachen gedacht, also wundere dich nicht, wenn er nach einem Update überschrieben/gelöscht/beschädigt wird. Speziell um diese Situation zu vermeiden, erlaubt das Spiel, diese Dateien in sogenannte "Benutzer"-Ordner zu legen - unter Windows wären das Ordner namens `[Spielstammverzeichnis]/mods/` und `[Spielstammverzeichnis]/sound/`. Alle persönlichen Anpassungen sollten in eine dedizierte Mod kommen, die im selben `[Spielstammverzeichnis]/mods/` Ordner platziert wird, und wenn das aufgrund von Einschränkungen des Modding-Systems nicht möglich ist - in irgendeiner Backup-Form aufbewahrt werden, vorzugsweise weg von `[Spielstammverzeichnis]/`, um zu vermeiden, dass der Launcher es als Müll behandelt.
3. Der Launcher kann einige Macken haben. Zum Beispiel hatte eines der letzten Updates eines beliebten Launchers eine große Warnung, die einige Leute ignorierten. Der Rat hier ist ähnlich: Nerv die Autor(en)/Maintainer, diese zu lösen (Migration hinzufügen?) oder sei einfach aufmerksamer.
   ![image](https://user-images.githubusercontent.com/60584843/186022055-0015f2cc-2549-4721-8a0d-8b7047b3d2b1.png)

### Behebung der Beschädigung

Auch hier können wir nicht viel tun. Versuche, die Launcher-Leute um eine Lösung zu bitten oder dein Betriebssystem um die Backups.

## Beschädigung durch das Spiel

### Häufige Symptome

1. Wann immer du einen Spielstand lädst, passiert etwas Seltsames, aber nicht Spielbrechendes (du stirbst, oder verlierst 10kg, oder dein NPC-Begleiter teleportiert sich herum)
2. Wann immer du einen Spielstand lädst, schreit dich das Spiel wegen verlorener Gegenstandsorte an

### Häufige Ursachen und Möglichkeiten zur Vermeidung

Dies sind Spielfehler und sollten behoben, nicht vermieden werden. Wir können nicht beheben, was wir nicht wissen, also müssen Fehler gemeldet werden - entweder auf GitHub durch Erstellen eines [Fehlerberichts](https://github.com/cataclysmbn/Cataclysm-BN/issues/new/choose) (du brauchst nur eine funktionierende E-Mail-Adresse, um ein GitHub-Konto zu erstellen, es ist nicht _so_ schwer) oder indem du unserem [Discord-Server](https://discord.gg/XW7XhXuZ89) beitrittst und dich im `#development` Kanal beschwerst - dort wirst du höchstwahrscheinlich eine schnellere Antwort erhalten. Wenn du weißt, wie man es behebt (oder es versuchen willst) - Hilfe wird immer geschätzt.

### Behebung der Beschädigung

Spielstände können in diesen Fällen normalerweise repariert werden, obwohl die genaue Methode von Fall zu Fall variiert.

Wenn das Spiel rot-auf-schwarz Nachrichten anzeigt, die etwas über `ACT_XXX lost target item` oder `item_location lost target during save/load cycle` sagen, versuche, ein Backup des Spielstands zu machen, dann öffne die Speicherdatei deines Charakters (normalerweise `[Spielstammverzeichnis]/save/[Weltname]/#[Kauderwelsch].sav`) mit einem anständigen Texteditor (Notepad++ für Windows ist ok), finde alle Vorkommen von Sequenzen, die mit `ACT_` beginnen (z. B. `ACT_DROP` oder `ACT_WASH`) und ersetze sie durch `ACT_NULL`.

## Beschädigung durch unsachgemäße Upgrade-Verfahren

### Häufige Symptome

1. Nach einem manuellen Update zeigt das Spiel seltsame Fehler an, die niemand kennt oder erlebt, den du fragst, selbst wenn du keine Mods verwendest.
2. Die Fehler verschwinden nach einer sauberen Neuinstallation des Spiels.

### Häufige Ursachen und Möglichkeiten zur Vermeidung

Du aktualisierst das Spiel höchstwahrscheinlich, indem du eine frische Version herunterlädst und sie über die bestehende Installation entpackst. **Dies wird nicht unterstützt und wurde nie unterstützt.** Es ist im Allgemeinen eine schlechte Idee, Programme auf diese Weise zu aktualisieren, es sei denn, sie geben ausdrücklich an, dass sie damit umgehen können und es erlaubt ist. BN kann damit nicht umgehen. Die Mods, die du heruntergeladen und auf die gleiche Weise aktualisiert hast, können damit höchstwahrscheinlich auch nicht umgehen.

### Behebung der Beschädigung

Mache einfach eine saubere Neuinstallation oder verwende einen Launcher.

## Beschädigung durch den Tod des Charakters

### Häufige Symptome

Dein Charakter stirbt - es ist eine Tragödie, und du wünschst, du könntest die Zeit in die guten alten Tage zurückdrehen.
Und dann erkennst du, dass du genau das tun kannst, indem du Alt+F4 drückst. Aber wenn du den Spielstand das nächste Mal lädst, siehst du deine Leiche in der Nähe mit allen Gegenständen dupliziert und dein Fahrzeug ist nirgends zu sehen.

### Häufige Ursachen und Möglichkeiten zur Vermeidung

Du hast versucht, den Tod zu betrügen, hast aber versagt und wurdest letztendlich unter den Milliarden vergessen, die im Kataklysmus verloren gingen.

### Behebung der Beschädigung

Du wirst mit der Erinnerung an deine Fehler leben müssen. Versuche das nächste Mal, den Spielstand zu sichern.
Technisch gesehen ist es ein Fehler, aber es ist auch ein einfacher Ausweg, also kümmert sich niemand wirklich darum, ihn zu beheben.
