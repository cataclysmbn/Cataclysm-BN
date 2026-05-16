---
title: Formatting & Linting
---

This guide explains how to format and lint code in Cataclysm: Bright Nights.

## Quick Reference

| File Type       | Tool                | Command                                            |
| --------------- | ------------------- | -------------------------------------------------- |
| C++ (`.cpp/.h`) | astyle/clang-format | `cmake --build build --target format`              |
| JSON            | json_formatter      | `cmake --build build --target style-json-parallel` |
| Markdown        | deno fmt            | `deno fmt`                                         |
| TypeScript      | deno fmt            | `deno fmt`                                         |
| Lua             | dprint              | `deno task dprint fmt`                             |

## Automated Formatting

Pull requests are automatically formatted by [autofix.ci](https://autofix.ci). If your code has
style violations, a commit will be pushed to fix them.

> [!TIP]
> To avoid merge conflicts after autofix commits, either:
>
> 1. Run `git pull` to merge the autofix commit, then continue working
> 2. Format locally before pushing, then `git push --force` if needed

## C++ Formatting

Top-level C++ files are formatted with [astyle](http://astyle.sourceforge.net/). C++ files in source subdirectories are formatted with [clang-format](https://clang.llvm.org/docs/ClangFormat.html).

```sh
# Install formatters (Ubuntu/Debian)
sudo apt install astyle clang-format

# Install formatters (Fedora)
sudo dnf install astyle clang-tools-extra

# Install formatters (macOS)
brew install astyle clang-format
```

### Using CMake

```sh
# Configure (only needed once, or use an existing build)
cmake --preset lint

# Format all C++ files
cmake --build build --target format
```

The style configurations are in `.astylerc` and `.clang-format` at the repository root.

## JSON Formatting

JSON files are formatted with `json_formatter`, a custom tool built from the project source.

### Using CMake

```sh
# Configure (only needed once, or use an existing build)
cmake --preset lint

# Format all JSON files in parallel
cmake --build build --target style-json-parallel

# Format all JSON files sequentially (slower, but useful for debugging)
cmake --build build --target style-json
```

> [!NOTE]
> The `data/names/` directory is excluded from formatting because name files have special formatting
> requirements.

### JSON Syntax Validation

Before formatting, you can validate JSON syntax:

```sh
build-scripts/lint-json.sh
```

This runs Python's `json.tool` on all JSON files to catch syntax errors.

## Markdown & TypeScript Formatting

Markdown and TypeScript files are formatted with [Deno](https://deno.land/).

```sh
# Install Deno
curl -fsSL https://deno.land/install.sh | sh

# Format Markdown and TypeScript
deno fmt
```

## Lua Formatting

Lua files are formatted with [dprint](https://dprint.dev/) via Deno.

```sh
# Format Lua files
deno task dprint fmt
```

## Dialogue Validation

NPC dialogue files have additional validation:

```sh
tools/dialogue_validator.py data/json/npcs/* data/json/npcs/*/* data/json/npcs/*/*/*
```

## Pre-commit Workflow

Before committing, run these checks:

```sh
# Configure once (creates build directory with formatting tools)
cmake --preset lint

# Format all code
cmake --build build --target format           # C++
cmake --build build --target style-json-parallel  # JSON
deno fmt                                       # Markdown/TypeScript
deno task dprint fmt                           # Lua
```

## CI Integration

The CI pipeline runs these checks automatically:

1. **JSON syntax validation** - `build-scripts/lint-json.sh`
2. **JSON formatting** - `cmake --build build --target style-json-parallel`
3. **Dialogue validation** - `tools/dialogue_validator.py`

If any check fails, the build will fail. Use the commands above to fix issues locally before
pushing.

## Editor Integration

### VS Code

Install these extensions for automatic formatting:

- **C++**: [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) with
  astyle integration
- **Deno**: [Deno](https://marketplace.visualstudio.com/items?itemName=denoland.vscode-deno) for
  Markdown/TypeScript

### Visual Studio

Install the formatters once from PowerShell. LLVM provides `clang-format` and `clang-tidy`.

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
irm get.scoop.sh | iex
scoop install llvm astyle
clang-format --version
clang-tidy --version
astyle --version
```

If Scoop is already installed, run only `scoop install llvm astyle`. If any version command is not
found, close and reopen PowerShell and Visual Studio so they reload `PATH`.

After installing the tools:

1. Open the repository folder in Visual Studio as a CMake project.
2. Configure CMake. If Visual Studio already configured the project before installing the formatters,
   reconfigure the CMake cache.
3. Open **View > Other Windows > CMake Targets View**.
4. Build the `format` target before committing.

Visual Studio's normal **Format Document** command does not run this target. Use the `format` target
for repository style: it runs `astyle` for top-level C++ files and `clang-format` for C++ files in
`src/` subdirectories.

### Vim/Neovim

Add to your config:

```vim
" Format C++ with astyle on save
autocmd BufWritePre *.cpp,*.h !astyle --options=.astylerc %

" Format with deno
autocmd BufWritePre *.md,*.ts !deno fmt %
```

## Troubleshooting

### "json_formatter not found" or "style-json-parallel target not found"

Make sure you configured CMake with the `lint` preset or with `-DJSON_FORMAT=ON`:

```sh
cmake --preset lint
cmake --build build --target json_formatter
```

### "format target not found"

Make sure `astyle` and `clang-format` are installed and in your PATH:

```sh
# Check if formatters are available
which astyle
which clang-format

# Install if missing (Ubuntu/Debian)
sudo apt install astyle clang-format
```

Then reconfigure CMake:

```sh
cmake --preset lint
```

### C++ formatters produce different results

Make sure you're using the formatter configs from the repository root:

```sh
astyle --options=.astylerc src/*.cpp
clang-format -i src/utils/*.cpp src/utils/*.h
```
