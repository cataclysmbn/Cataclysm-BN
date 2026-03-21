# Dokumentation aktualisieren

Um die Dokumentationsseite zu aktualisieren, musst du:

- [Markdown in y Minuten lernen](https://learnxinyminutes.com/docs/markdown/)
- [ein GitHub-Konto erstellen](https://github.com/join), da der Quellcode für die Dokumentationsseite auf GitHub gehostet wird.

## Browser

![edit page](img/edit.webp)

1. Klicke auf die Schaltfläche `Edit page` am Ende der Seite.

![alt text](img/github-edit.webp)

2. Du wirst zur GitHub-Seite der Dokumentationsseite weitergeleitet. Hier kannst du deine Änderungen bearbeiten und eine Vorschau anzeigen.

> [!NOTE]
>
> - `.md` in `CONTRIBUTING.md` steht für Markdown-Dateien
> - `.mdx` in `docs.mdx` steht für [MarkDown eXtended](https://mdxjs.com)
>   - es ist eine Obermenge von Markdown mit Unterstützung für JavaScript und [JSX-Komponenten][jsx]
>   - sie sind etwas komplizierter, erlauben aber die Verwendung interaktiver Komponenten

[jsx]: https://www.typescriptlang.org/docs/handbook/jsx.html

![propose changes window](https://github.com/scarf005/Cataclysm-BN/assets/54838975/d4a06795-1680-4706-a84c-072346bff109)

3. Klicke auf die Schaltfläche `Commit changes...` in der oberen rechten Ecke, um [deine Änderungen zu committen](https://github.com/git-guides/git-commit). Stelle sicher, dass du:

- Eine kurze und beschreibende `Commit message` schreibst
- Das Kontrollkästchen `Create a new branch for this commit and start a pull request` aktivierst

![comparing changes page](https://github.com/scarf005/Cataclysm-BN/assets/54838975/3551797e-847b-45fe-8869-8b0b15bfb948)

4. Du wirst zur Seite `Comparing changes` weitergeleitet. Klicke auf die Schaltfläche `Create pull request`, um [einen Pull Request zu erstellen](./contributing.md#hinweise-zu-pull-requests).

![open a pull request page](https://github.com/scarf005/Cataclysm-BN/assets/54838975/2a987c19-b165-43c2-a5a2-639f22202926)

5. Klicke auf die Schaltfläche `Create pull request`, um [einen PR zu öffnen](./contributing.md#hinweise-zu-pull-requests). Es ist in Ordnung, den PR-Text für kleine Änderungen leer zu lassen.

## Lokale Entwicklung

> [!NOTE]
>
> Dieser Abschnitt setzt voraus, dass du einige Kenntnisse in [git](https://git-scm.com) und [javascript](https://developer.mozilla.org/en-US/docs/Web/JavaScript) hast. Natürlich kannst du sie auch währenddessen lernen.

Um die Dokumentationsseite lokal auszuführen, musst du:

- [deno](https://deno.com) installieren, um die Dokumentation zu formatieren und automatisch zu generieren

### Einrichten des Entwicklungsservers

```sh
(Cataclysm-BN) $ deno task docs serve

# oder wenn du dich bereits im docs-Verzeichnis befindest
(Cataclysm-BN/docs) $ deno task serve
```

Du kannst unter `http://localhost:3000` auf die Dokumentationsseite zugreifen. Der Entwicklungsserver wird automatisch neu geladen, wenn du Änderungen an der Dokumentation vornimmst.

### Automatische Seitengenerierung

Lua- und CLI-Dokumentation werden automatisch aus dem Quellcode generiert. Um sie zu generieren, gehe zum Projektstammverzeichnis und führe aus:

```sh
(Cataclysm-BN) $ deno task docs:gen
```

## Lizenz

- Durch das Beitragen zu Markdown-Dateien (einschließlich, aber nicht beschränkt auf `.md` und `.mdx` Dateien), stimmst du zu, deine Beiträge unter [CC-BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/) zu lizenzieren, derselben Lizenz wie das Spiel.

- Durch das Beitragen zum Quellcode der Dokumentationsseite (einschließlich, aber nicht beschränkt auf `.ts` Dateien), stimmst du zu, deine Beiträge unter [AGPL 3.0](https://www.gnu.org/licenses/agpl-3.0.en.html) zu lizenzieren.
