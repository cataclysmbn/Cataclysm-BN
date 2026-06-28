---
title: Formatting & Linting
---

This guide explains how to format and lint code in Cataclysm: Bright Nights.

## Quick Reference

| Scope           | Tool                | Command          |
| --------------- | ------------------- | ---------------- |
| Staged files    | all formatters      | `just fmt`       |
| All files       | all formatters      | `just fmt --all` |
| C++ (`.cpp/.h`) | astyle/clang-format | `just fmt-cpp`   |
| JSON            | json_formatter      | `just fmt-json`  |
| Markdown/TS     | deno fmt            | `just fmt-docs`  |
| Lua             | dprint              | `just fmt-lua`   |

## Automated Formatting

Pull requests are automatically formatted by [autofix.ci](https://autofix.ci). If your code has
style violations, a commit will be pushed to fix them.

> [!TIP]
> To avoid merge conflicts after autofix commits, either:
>
> 1. Run `git pull` to merge the autofix commit, then continue working
> 2. Format locally before pushing, then `git push --force` if needed

## C++ Formatting

Top-level `src/*.cpp` and `src/*.h` files use [astyle](http://astyle.sourceforge.net/). Most other C++ files use [clang-format](https://clang.llvm.org/docs/ClangFormat.html). Formatter-sensitive fixtures such as `tools/clang-tidy-plugin/test/` are left unchanged.

```sh
# Install formatters (Ubuntu/Debian)
sudo apt install astyle clang-format

# Install formatters (Fedora)
sudo dnf install astyle clang-tools-extra

# Install formatters (macOS)
brew install astyle clang-format
```

### Using the helper

```sh
just fmt-cpp
# or, for all staged file types
just fmt
```

If you already have a CMake build tree with `bash` available, `cmake --build <build-dir> --target format` calls the same C++ helper. The style configurations are in `.astylerc` and `.clang-format` at the repository root.

## JSON Formatting

JSON files are formatted with `json_formatter`, a custom tool built from the project source.

### Using the script

```sh
just fmt-json
# or
build-scripts/format-json.sh
```

The script builds `json_formatter` in `out/build/json-format` and formats JSON files. It does not
require a configured game build or a CMake preset. Override the helper build directory with
`CATA_JSON_FORMAT_BUILD_DIR` if needed.

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

Install the optional pre-commit hook with [`prek`](https://github.com/j178/prek):

```sh
prek install
# or
just hooks-setup
```

When you commit, the hook runs `just fmt`: Deno and dprint run normally, while C++ and JSON formatters
run only on staged created/updated files. Formatted staged files are re-added so the commit includes
the formatted result.

To run the same formatting manually against staged files:

```sh
just fmt
```

Before committing without the hook, run these checks:

```sh
# Format staged files
just fmt

# Format all formattable files
just fmt --all
```

## CI Integration

The CI pipeline runs these checks automatically:

1. **JSON syntax validation** - `build-scripts/lint-json.sh`
2. **JSON formatting** - `build-scripts/format-json.sh`
3. **Dialogue validation** - `tools/dialogue_validator.py`

If any check fails, the build will fail. Use the commands above to fix issues locally before
pushing.

## Editor Integration

### VS Code

Use the repository helper for C++ and the Deno extension for Markdown/TypeScript:

- **C++**: run `just fmt-cpp`
- **Deno**: [Deno](https://marketplace.visualstudio.com/items?itemName=denoland.vscode-deno)

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

Visual Studio's normal **Format Document** command does not run repository style. Run `just fmt-cpp`
in a terminal; the CMake `format` target is a wrapper for environments with `bash` available.

### Vim/Neovim

Use the helper instead of calling astyle or clang-format directly:

```vim
autocmd BufWritePre *.cpp,*.h !just fmt-cpp %
autocmd BufWritePre *.md,*.ts !deno fmt %
```

## Troubleshooting

### "json_formatter not found"

Run the JSON formatter script. It configures and builds the formatter helper automatically:

```sh
build-scripts/format-json.sh
```

### C++ formatter not found

Make sure `astyle` and `clang-format` are installed and in your PATH:

```sh
# Check if formatters are available
which astyle
which clang-format

# Install if missing (Ubuntu/Debian)
sudo apt install astyle clang-format
```

Then rerun:

```sh
build-scripts/format-cpp.sh
```

### C++ formatters produce different results

Use the repository helper from the repository root so each file goes through the right formatter:

```sh
just fmt-cpp
```
