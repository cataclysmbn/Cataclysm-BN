# Mitwirken

## Möchtest du helfen?

Hilfe wird geschätzt, besonders bei:

- Fehler melden. Einschließlich solcher, die von DDA übernommen wurden.
- Identifizieren von Problemen, die keine Fehler sind. Irreführende Beschreibungen, Werte, die im Vergleich zu ähnlichen Fällen eindeutig falsch sind, Grammatikfehler, UI-Merkwürdigkeiten, die eine offensichtliche Lösung haben.
- Nutzlose Dinge nützlich machen oder auf eine schwarze Liste setzen. Hinzufügen von Zerlegungsrezepten für Dinge, die sie haben sollten, aber nicht haben; Ersetzen von völlig redundanten Gegenständen durch ihre generischen Versionen (z. B. "winzige markierte Flasche" durch einfach "winzige Flasche") in Spawn-Listen.
- Tileset-Arbeit. Ich füge gelegentlich neue Objekte hinzu, wie die neuen Stromnetzelemente, und sie könnten neue Tiles gebrauchen.
- Balance-Analyse. Diese sollten eher tiefgehend oder "offensichtlich richtig" sein. "Offensichtlich richtig" wären Dinge wie: "Waffe x hat strikt bessere Werte als y, aber y erfordert seltenere Komponenten und hat ansonsten identische Anforderungen".
- Identifizieren von Leistungsengpässen mit einem Profiler.
- Hilfe bei der Codequalität.

## Wie man anfängt

Zu Cataclysm: Bright Nights beizutragen ist einfach:

1. Forke das Repository hier auf GitHub.
2. Nimm deine Änderungen vor.
3. Sende uns einen Pull Request.

> [!NOTE]
>
> #### Lizenz
>
> Cataclysm: Bright Nights wird unter der Creative Commons Attribution ShareAlike 3.0 Lizenz veröffentlicht.
> Der Code und Inhalt des Spiels kann für jeden Zweck frei verwendet, modifiziert und weiterverbreitet werden. Siehe http://creativecommons.org/licenses/by-sa/3.0/ für Details. Das bedeutet, dass jeder Beitrag, den du zum Projekt leistest, ebenfalls unter derselben Lizenz steht, und diese Lizenz ist unwiderruflich.

## Richtlinien

Es gibt ein paar Richtlinien, an die wir uns halten sollten:

- Füge dieses Repository als `upstream` Remote hinzu.
- Halte deinen `main` Branch sauber. Das bedeutet, dass du Änderungen, die an diesem Repository vorgenommen wurden, leicht in deines ziehen kannst.
- Erstelle für jedes neue Feature oder jede Reihe von zusammenhängenden Fehlerbehebungen einen neuen Branch.
- Merge niemals von deinen lokalen Branches in deinen `main` Branch. Aktualisiere diesen nur durch Pullen von `upstream/main`.

## Code-Stil

### C++

Der Code-Stil wird in der gesamten Codebasis durch `astyle` durchgesetzt. Siehe [CODE_STYLE](./../dev/explanation/code_style.md) für Details.

### JSON

JSON-Dateien werden mit einem benutzerdefinierten Formatierer formatiert, der in `tools/format` verfügbar ist. Besuche den [JSON Style Guide](./../mod/json/explanation/json_style.md) für Details.

### Markdown

Markdown-Dateien wie `doc/` werden mit dem eingebauten Formatierer von [`deno`](https://deno.com) formatiert. Führe [`deno fmt`](https://deno.land/manual/tools/formatter) überall aus, um Markdown-Dateien zu formatieren. In VSCode kannst du die folgende Konfiguration festlegen, um Markdown-Dateien beim Speichern automatisch zu formatieren:

```json
// .vscode/settings.json
{
  "[markdown]": {
    "editor.formatOnSave": true,
    "editor.defaultFormatter": "denoland.vscode-deno"
  }
}
```

### Lua

Lua-Dateien werden mit dem eingebauten Formatierer von [`dprint`](https://dprint.dev) formatiert. Führe [`deno task dprint fmt`](https://dprint.dev/plugins/lua) überall aus, um Lua-Dateien zu formatieren. Für Details siehe [Lua Style Guide](./../mod/lua/explanation/lua_style.md).

## Übersetzungen

Die Übersetzung von Cataclysm: BN erfolgt über Transifex. Schau dir das [Übersetzungsprojekt](https://app.transifex.com/bn-team/cataclysm-bright-nights/) für eine aktuelle Liste der unterstützten Sprachen an.

Für weitere Informationen:

- [Für Übersetzer](./../i18n/tutorial/transifex)
- [Für Entwickler](./../i18n/reference/translation)
- [Für Maintainer](./../i18n/guides/maintain)

## Dokumentation

<!--
![](./img/contributing-doxy1.png)
![](./img/contributing-doxy2.png)

Automatisch generierte Dokumentation wird auf
[GitHub Pages](https://cataclysmbn.github.io/Cataclysm-BN) gehostet. -->

### Doxygen-Kommentare

Ausführliche Dokumentation von Klassen und Klassenmitgliedern macht den Code für neue Mitwirkende lesbarer. Neue Doxygen-Kommentare für bestehende Klassen sind ein willkommener Beitrag.

Verwende die folgende Vorlage zum Kommentieren von Klassen:

```cpp
/**
 * Brief description
 *
 * Lengthy description with many words. (optional)
 */
class foo {

}
```

Verwende die folgende Vorlage zum Kommentieren von Funktionen:

```cpp
/**
 * Brief description
 *
 * Lengthy description with many words. (optional)
 * @param param1 Description of param1 (optional)
 * @return Description of return (optional)
 */
int foo(int param1);
```

Verwende die folgende Vorlage zum Kommentieren von Mitgliedsvariablen:

```cpp
/** Brief description **/
int foo;
```

### Richtlinien zum Hinzufügen von Dokumentation

- Doxygen-Kommentare sollten das Verhalten nach außen beschreiben, nicht die Implementierung, aber da viele Klassen in Cataclysm miteinander verflochten sind, ist es oft notwendig, die Implementierung zu beschreiben.
- Beschreibe Dinge, die für Neulinge nicht allein aus dem Namen ersichtlich sind.
- Beschreibe nicht redundant: `/** Map **/; map* map;` ist kein hilfreicher Kommentar.
- Wenn du X dokumentierst, beschreibe, wie X mit anderen Komponenten interagiert, nicht nur, was X selbst tut.

### Erstellen der Dokumentation für die lokale Ansicht

- Installiere doxygen
- `doxygen doxygen_doc/doxygen_conf.txt`
- `firefox doxygen_doc/html/index.html` (ersetze firefox durch deinen bevorzugten Browser)

## Beispiel-Workflow

### Einrichten deiner Umgebung

_(Dies muss nur einmal durchgeführt werden.)_

1. Forke dieses Repository hier auf GitHub.

1. Klone deinen Fork lokal.

```sh
$ git clone https://github.com/DEIN_BENUTZERNAME/Cataclysm-BN.git
# Klont deinen Fork des Repositorys in das aktuelle Verzeichnis im Terminal
```

3. Lege die Commit-Nachrichtenvorlage fest.

```sh
$ git config --local commit.template .gitmessage
```

4. Füge dieses Repository als Remote hinzu.

```sh
$ cd Cataclysm-BN
# Wechselt das aktive Verzeichnis im Prompt in das neu geklonte "Cataclysm-BN" Verzeichnis
$ git remote add -f upstream https://github.com/cataclysmbn/Cataclysm-BN.git
# Weist das ursprüngliche Repository einem Remote namens "upstream" zu
```

Weitere Details zu den Richtlinien für Commit-Nachrichten findest du unter:

- [codeinthehole.com](https://codeinthehole.com/tips/a-useful-template-for-commit-messages/)
- [chris.beams.io](https://chris.beams.io/posts/git-commit/)
- [help.github.com](https://help.github.com/articles/closing-issues-using-keywords/)

### Aktualisiere deinen `main` Branch

0. **Stelle sicher, dass dein `main` Branch den `main` von upstream trackt, nicht den `main` von origin, indem du folgendes eingibst:**

```sh
$ git branch --set-upstream-to upstream/main main
# lässt deinen Branch 'main' den main von upstream tracken.
```

Um zu überprüfen, ob dein `main` korrekt auf upstream zeigt, gib `git remote -v` ein und du solltest etwas sehen wie

```sh
$ git remote -v

origin  git@github.com:DEIN_BENUTZERNAME/Cataclysm-BN.git (fetch)
origin  git@github.com:DEIN_BENUTZERNAME/Cataclysm-BN.git (push)
upstream        https://github.com/cataclysmbn/Cataclysm-BN.git (fetch)
upstream        https://github.com/cataclysmbn/Cataclysm-BN.git (push)
```

und wenn du `git branch -vv` eingibst, solltest du `[upstream/main]` neben dem `main` Branch sehen, wie:

```sh
$ git branch -vv

# ...
* main   ed11439ee61 [upstream/main] feat: add more vending machines to marina (#7001)
# ...
```

1. Stelle sicher, dass du dich auf deinem `main` Branch befindest.

```sh
$ git switch main
```

2. Pulle die Änderungen vom `upstream/main` Branch.

```sh
$ git pull --ff-only upstream main
# holt Änderungen vom "main" Branch auf dem "upstream" Remote
```

> **Hinweis** Wenn dies einen Fehler ergibt, bedeutet das, dass du direkt in deinen lokalen `main` Branch committed hast.
> [Klicke hier für Anweisungen, wie du dieses Problem beheben kannst](#why-does-git-pull---ff-only-result-in-an-error).

### Nimm deine Änderungen vor

0. Aktualisiere deinen `main` Branch, falls du das noch nicht getan hast.

1. Erstelle für jedes neue Feature oder jede Fehlerbehebung einen neuen Branch.

```sh
$ git switch --create new_feature
# Erstellt einen neuen Branch namens "new_feature" und macht ihn zum aktiven Branch
```

2. Sobald du einige Änderungen lokal committed hast, musst du sie zu deinem Fork hier auf GitHub pushen.

```sh
$ git push origin new_feature
# origin wurde beim Klonen automatisch so eingestellt, dass es auf deinen Fork zeigt
```

3. Sobald du mit der Arbeit an deinem Branch fertig bist und alle deine Änderungen committed und gepusht hast, sende einen Pull Request von deinem `new_feature` Branch zum `main` Branch dieses Repositorys.

> **Hinweis** alle neuen Commits im `new_feature` Branch auf GitHub werden automatisch in den Pull Request aufgenommen, also stelle sicher, dass du nur zusammengehörige Änderungen in denselben Branch committest.

## Hinweise zu Pull Requests

Wenn du einen PR einreichst, aber noch daran arbeitest, markiere ihn bitte als
[Entwurf (draft)](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests#draft-pull-requests).
Dies kann helfen, unseren Überprüfungsprozess zu beschleunigen, indem es uns erlaubt, nur die Dinge zu überprüfen, die dafür bereit sind, und verhindert, dass etwas gemerged wird, das noch nicht vollständig fertig ist.

Es ist nicht erforderlich, ein offenes Issue zu lösen oder darauf zu verweisen, um einen PR einzureichen, aber wenn du dies tust, musst du das Problem, das dein PR löst, im Detail erklären.

### Issues mit Schlüsselwörtern schließen

Noch eine Sache: Wenn du deinen PR als schließend, behebend oder lösend für Issues markierst, füge dies bitte irgendwo in der Beschreibung ein:

```md
- {keyword} #{issue}
```

zum Beispiel: `- fixed #12345`

### Schlüsselwort

`{keyword}` muss eines der folgenden sein:

- `close`, `closes`, `closed`
- `fix`, `fixes`, `fixed`
- `resolve`, `resolves`, `resolved`

### Issue

und `{issue}` ist die Nummer des Issues, das du schließt, nachdem der PR gemerged wurde.

Dies würde das Issue automatisch schließen, wenn der PR hereingezogen wird, und ermöglicht es, dass Merges etwas schneller funktionieren.

### Mehrere Issues auf einmal schließen

```md
- {keyword} #{issue}, {keyword} #{issue}
```

Siehe https://help.github.com/articles/closing-issues-using-keywords für mehr.

## Tool-Unterstützung

Es stehen verschiedene Tools zur Verfügung, die dir helfen, deine Beiträge im entsprechenden Stil zu halten.
Siehe [DEVELOPER_TOOLING](./../dev/reference/tooling) für weitere Details.

## Fortgeschrittene Techniken

Diese Richtlinien sind nicht wesentlich, aber sie können es viel einfacher machen, Ordnung zu halten.

### Verwendung von Remote-Tracking-Branches

Remote-Tracking-Branches ermöglichen es dir, leicht mit dem `main` Branch dieses Repositorys in Kontakt zu bleiben, da sie automatisch wissen, von welchem Remote-Branch sie Änderungen erhalten sollen.

```sh
$ git branch -vv
* main        xxxx [origin/main] ....
  new_feature xxxx ....
```

Hier siehst du, dass wir zwei Branches haben; `main`, der `origin/main` trackt, und `new_feature`, der keinen Branch trackt. In der Praxis bedeutet dies, dass git nicht weiß, woher es Änderungen erhalten soll.

```sh
$ git checkout new_feature
Switched to branch 'new_feature'
$ git pull
There is no tracking information for the current branch.
Please specify which branch you want to merge with.
```

Um Änderungen von `upstream/main` einfach in den `new_feature` Branch zu pullen, können wir git sagen, welchen Branch es tracken soll. (Du kannst dies sogar für deinen lokalen main Branch tun.)

```sh
$ git branch -u upstream/main new_feature
Branch new_feature set up to track remote branch main from upstream.
$ git pull
Updating xxxx..xxxx
....
```

Du kannst die Tracking-Informationen auch gleichzeitig mit dem Erstellen des Branches festlegen.

```sh
$ git branch new_feature_2 --track upstream/main
Branch new_feature_2 set up to track remote branch main from upstream.
```

> **Hinweis**: Obwohl dies das Pullen von `upstream/main` erleichtert, ändert es nichts in Bezug auf das Pushen. `git push` schlägt fehl, weil du keine Berechtigung hast, zu `upstream/main` zu pushen.

```sh
$ git push
error: The requested URL returned error: 403 while accessing https://github.com/cataclysmbn/Cataclysm-BN.git
fatal: HTTP request failed
$ git push origin
....
To https://github.com/DEIN_BENUTZERNAME/Cataclysm-BN.git
xxxx..xxxx  new_feature -> new_feature
```

## Unit-Tests

Es gibt eine Testsuite, die im Quellbaum unter tests/ eingebaut ist. Du solltest die Testsuite nach **JEDER** Änderung am Spielquellcode ausführen. Ein gewöhnlicher Aufruf von `make` baut die Test-Executable unter tests/cata_test, und sie kann wie jede gewöhnliche Executable oder über `make check` aufgerufen werden. Ohne Argumente führt sie die gesamte Testsuite aus. Mit `--help` gibt sie eine Reihe von Aufrufoptionen aus, die du verwenden kannst, um ihre Arbeitsweise anzupassen.

```sh
$ make
... compilation details ...
$ tests/cata_test
Starting the actual test at Fri Nov  9 04:37:03 2018
===============================================================================
All tests passed (1324684 assertions in 94 test cases)
Ended test at Fri Nov  9 04:37:45 2018
The test took 41.772 seconds
```

Ich empfehle, make gewohnheitsmäßig wie `make DEINE BUILD OPTIONEN && make check` aufzurufen.

## Testen im Spiel, Testumgebung und das Debug-Menü

Egal, ob du ein neues Feature implementierst oder einen Fehler behebst, es ist immer eine gute Praxis, deine Änderungen im Spiel zu testen. Es kann eine schwierige Aufgabe sein, die genauen Bedingungen durch Spielen eines normalen Spiels zu schaffen, um deine Änderungen testen zu können, weshalb es ein Debug-Menü gibt. Es gibt keine Standardtaste, um das Menü aufzurufen, also musst du zuerst eine zuweisen.

Rufe das Tastenbelegungsmenü auf (drücke `Escape` dann `1`), scrolle fast bis ganz nach unten und drücke `+`, um eine neue Tastenbelegung hinzuzufügen. Drücke den Buchstaben, der dem _Debug menu_ Eintrag entspricht, und drücke dann die Taste, die du verwenden möchtest, um das Debug-Menü aufzurufen. Um deine Änderungen zu testen, erstelle eine neue Welt mit einem neuen Charakter. Sobald du in dieser Welt bist, drücke die Taste, die du gerade für das Debug-Menü zugewiesen hast, und du solltest so etwas sehen:

```
┌─────────────────────────────────────────────────────┐
│ Debug Functions - Manipulate the fabric of reality! │
├─────────────────────────────────────────────────────┤
│ i Info                                              │
│ Q Quit to main menu                                 │
│ s Spawning...                                       │
│ p Player...                                         │
│ t Teleport...                                       │
│ m Map...                                            │
└─────────────────────────────────────────────────────┘
```

Mit diesen Befehlen solltest du in der Lage sein, die richtigen Bedingungen zu schaffen, um deine Änderungen zu testen. Das [BN Wiki](https://cataclysmbn-guide.com/) kann nützliche Informationen zum Debug-Menü enthalten.

## Häufig gestellte Fragen

### Warum führt `git pull --ff-only` zu einem Fehler?

Wenn `git pull --ff-only` einen Fehler anzeigt, bedeutet das, dass du direkt in deinen lokalen `main` Branch committed hast. Um dies zu beheben, erstellen wir einen neuen Branch mit diesen Commits, finden den Punkt, an dem wir von `upstream/main` abgewichen sind, und setzen dann `main` auf diesen Punkt zurück.

```sh
$ git pull --ff-only upstream main
From https://github.com/cataclysmbn/Cataclysm-BN
 * branch            main     -> FETCH_HEAD
fatal: Not possible to fast-forward, aborting.
$ git branch new_branch main          # markiere den aktuellen Commit mit einem tmp Branch
$ git merge-base main upstream/main
cc31d0... # der letzte Commit, bevor wir direkt in main committed haben
$ git reset --hard cc31d0....
HEAD is now at cc31d0... ...
```

Jetzt, da `main` bereinigt wurde, können wir einfach von `upstream/main` pullen und dann weiter an `new_branch` arbeiten.

```sh
$ git pull --ff-only upstream main
# holt Änderungen vom "upstream" Remote für den passenden Branch, in diesem Fall "main"
$ git checkout new_branch
```

Für weitere häufig gestellte Fragen siehe die [Entwickler-FAQ](./../dev/reference/faq.md).
