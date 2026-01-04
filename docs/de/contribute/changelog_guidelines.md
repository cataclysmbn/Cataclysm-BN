# Changelog-Richtlinien

Der PR-Titel folgt den [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) für eine einfachere Changelog-Erstellung. Das Format ist eines der folgenden:

```
<Typ>: <PR-Betreff>
<Typ>(<Bereich>, <Bereich>, ...): <PR-Betreff>
```

Zum Beispiel kann ein PR-Titel so aussehen:

```
feat: add new mutation
feat(port): port mutation description from DDA
```

Der PR-Titel sollte für Spieler auf einen Blick leicht verständlich sein. Es wird empfohlen, einen imperativen und beschreibenden Titel (`<Verb> <Nomen>`) zu verwenden, wie:

```diff
- feat: rebalancing some rifles
+ feat: nerf jam chance of m16 and m4
```

vorher: es ist schwer zu wissen, ob sie gebufft oder generft wurden und welche Gewehre geändert wurden, ohne die vollständige PR-Beschreibung zu lesen.
nachher: es ist leicht zu verstehen, was genau geändert wurde, allein aus dem Titel.

## Typ

Der Typ ist das erste Wort im PR-Titel. Er gibt die Art der vorgenommenen Änderung an. Im Zweifelsfall verwende `feat` für neue Funktionen und `fix` für Fehlerbehebungen. Hier sind einige häufig verwendete Kategorien:

### `feat`: Funktionen

Neue Funktionen, Ergänzungen oder Balance-Änderungen.

### `fix`: Fehlerbehebungen

Alles, was einen Fehler behebt oder das Spiel stabiler macht.

### `refactor`: Infrastruktur

Macht die Entwicklung einfacher, ohne das Verhalten zu ändern. Zum Beispiel:

- `C++` Refactorings und Überarbeitung
- `Json` Reorganisationen
- `docs/`, `.github/` und Repository-Änderungen
- andere Entwicklungswerkzeuge

### `build`: Build

Verbesserung des Build-Prozesses:

- robuster
- einfacher zu bedienen
- schnellere Kompilierzeit

### Andere

- `docs`: Dokumentationsänderungen
- `style`: Code-Stil-Änderungen (Leerzeichen, Formatierung usw.), meistens Korrektur der JSON-Formatierung.
- `perf`: Leistungsverbesserungen
- `test`: Hinzufügen fehlender Tests oder Korrektur bestehender Tests
- `ci`: Änderungen am CI-Prozess
- `chore`: Andere Änderungen, die in keine der oben genannten Kategorien passen
- `revert`: Macht einen vorherigen Commit rückgängig

## Bereiche

1. Verwende sie in Klammern nach der Kategorie, um den Bereich deines PRs weiter einzugrenzen.
2. Es gibt keine Begrenzung für die Anzahl der Bereiche.
3. Sie sind optional, werden aber empfohlen.
4. Dies sind nur Richtlinien, keine Regeln. Wähle frei das Beste für deinen PR!

### `<Keine>`: Allgemeine Funktionen

Zum Beispiel,

Änderungen bezüglich des Spielers:

- Spieler kann etwas Neues tun (z.B.: Mutationen, Fähigkeiten)
- etwas Neues kann dem Spieler passieren (z.B.: neue Krankheit)

Neue Inhalte wie:

- neue Monster
- neue Kartenbereiche
- neue Gegenstände
- neue Fahrzeuge
- neue Dinger

Beispiel PR-Titel:

```
feat: strength training activity
feat: mutation system overhaul
feat: semi-plausible smokeless gunpowder recipe
feat(port): game store
```

### `lua`: Änderungen an der Lua API

Änderungen an der Lua API, wie:

- [Hinzufügen neuer Bindings](../mod/lua/guides/binding.md)
- Verbesserung der Lua-Dokumentation/API-Generierung
- [Migration von hardcodierten C++-Funktionen nach Lua](https://github.com/cataclysmbn/Cataclysm-BN/pull/6901)

Beispiel PR-Titel:

```
feat(lua): add dialogue bindings
```

### `UI`: Schnittstellen

UI/UX-Änderungen wie:

- Hinzufügen von Mausunterstützung
- Hinzufügen/Anpassen von Menüs
- Ändern von Tastenkürzeln
- Straffung von Arbeitsabläufen
- Verbesserungen der Lebensqualität (QoL)

Beispiel PR-Titel:

```
feat(UI): More info about items dropped with bags
feat(UI): overhaul encumbrance UI
```

### `i18n`: Internationalisierung

Verbesserung der Übersetzung und Unterstützung anderer Sprachen.

```
fix(UI,i18n): recipe names not translated unless learned
```

### `mods` oder `mods/<MOD_ID>`: Mods

- Änderungen innerhalb einer Mod
- erweitert, was innerhalb einer Mod möglich ist

Beispiel PR-Titel:

```
feat(mods/Magical_Nights): add missing owlbear pelts recipe
fix(mods): No Hope doesn't make the world freezing
```

### `balance`: Balance-Änderungen

Änderungen an der Spielbalance.

Beispiel PR-Titel:

```
feat(balance): Give moose pelts instead of hides
```

### `port`: Ports von DDA oder anderen Forks

Beispiel PR-Titel:

```
feat(port): game shop
```
