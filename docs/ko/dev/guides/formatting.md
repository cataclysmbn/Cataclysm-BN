---
title: 포매팅 및 린팅
---

Cataclysm: Bright Nights에서 코드를 포맷하고 린트하는 방법을 설명합니다.

## 빠른 참조

| 범위            | 도구                | 명령어           |
| --------------- | ------------------- | ---------------- |
| staged 파일     | 모든 포매터         | `just fmt`       |
| 모든 파일       | 모든 포매터         | `just fmt --all` |
| C++ (`.cpp/.h`) | astyle/clang-format | `just fmt-cpp`   |
| JSON            | json_formatter      | `just fmt-json`  |
| Markdown/TS     | deno fmt            | `just fmt-docs`  |
| Lua             | dprint              | `just fmt-lua`   |

## 자동 포매팅

풀 리퀘스트는 [autofix.ci](https://autofix.ci)에 의해 자동으로 포맷됩니다. 코드에 스타일 위반이 있으면 수정 커밋이 푸시됩니다.

> [!TIP]
> autofix 커밋 후 머지 충돌을 피하려면:
>
> 1. `git pull`을 실행하여 autofix 커밋을 머지한 후 작업을 계속하거나
> 2. 푸시하기 전에 로컬에서 포맷한 다음 필요시 `git push --force`

## C++ 포매팅

Top-level `src/*.cpp`와 `src/*.h` 파일은 [astyle](http://astyle.sourceforge.net/)을 사용합니다. 대부분의 다른 C++ 파일은 [clang-format](https://clang.llvm.org/docs/ClangFormat.html)을 사용합니다. `tools/clang-tidy-plugin/test/` 같은 포매터에 민감한 fixture는 변경하지 않습니다.

```sh
# 포매터 설치 (Ubuntu/Debian)
sudo apt install astyle clang-format

# 포매터 설치 (Fedora)
sudo dnf install astyle clang-tools-extra

# 포매터 설치 (macOS)
brew install astyle clang-format
```

### Helper 사용

```sh
just fmt-cpp
# 또는 staged 파일 전체
just fmt
```

이미 CMake 빌드 트리가 있고 `bash`를 사용할 수 있다면 `cmake --build <build-dir> --target format`도 같은 C++ helper를 호출합니다. 스타일 설정은 저장소 루트의 `.astylerc`와 `.clang-format`에 있습니다.

## JSON 포매팅

JSON 파일은 프로젝트 소스에서 빌드된 커스텀 도구 `json_formatter`로 포맷됩니다.

### 스크립트 사용

```sh
just fmt-json
# 또는
build-scripts/format-json.sh
```

이 스크립트는 `out/build/json-format`에 `json_formatter`를 빌드하고 JSON 파일을 포맷합니다. 구성된 게임 빌드나 CMake 프리셋은 필요하지 않습니다. 필요하면 `CATA_JSON_FORMAT_BUILD_DIR`로 보조 빌드 디렉터리를 바꿀 수 있습니다.

> [!NOTE]
> `data/names/` 디렉토리는 이름 파일이 특별한 포맷 요구사항을 가지고 있어 포맷에서 제외됩니다.

### JSON 구문 검증

포맷하기 전에 JSON 구문을 검증할 수 있습니다:

```sh
build-scripts/lint-json.sh
```

모든 JSON 파일에 Python의 `json.tool`을 실행하여 구문 오류를 찾습니다.

## Markdown 및 TypeScript 포매팅

Markdown과 TypeScript 파일은 [Deno](https://deno.land/)로 포맷됩니다.

```sh
# Deno 설치
curl -fsSL https://deno.land/install.sh | sh

# Markdown와 TypeScript 포맷
deno fmt
```

## Lua 포매팅

Lua 파일은 Deno를 통해 [dprint](https://dprint.dev/)로 포맷됩니다.

```sh
# Lua 파일 포맷
deno task dprint fmt
```

## 대화 검증

NPC 대화 파일에는 추가 검증이 있습니다:

```sh
tools/dialogue_validator.py data/json/npcs/* data/json/npcs/*/* data/json/npcs/*/*/*
```

## 커밋 전 워크플로우

[`prek`](https://github.com/j178/prek)를 사용하는 선택적 pre-commit 훅을 설치할 수 있습니다:

```sh
prek install
# 또는
just hooks-setup
```

커밋할 때 훅은 `just fmt`를 실행합니다. Deno와 dprint는 일반적으로 실행되고, C++ 및 JSON 포매터는 staged된 생성/수정 파일만 처리합니다. 포맷된 staged 파일은 같은 커밋에 포함되도록 다시 `git add`됩니다.

staged 파일에 같은 포매팅을 수동으로 실행하려면:

```sh
just fmt
```

훅 없이 커밋하기 전에 실행할 명령:

```sh
# staged 파일 포맷
just fmt

# 포맷 가능한 모든 파일 포맷
just fmt --all
```

## CI 통합

CI 파이프라인은 이러한 검사를 자동으로 실행합니다:

1. **JSON 구문 검증** - `build-scripts/lint-json.sh`
2. **JSON 포매팅** - `build-scripts/format-json.sh`
3. **대화 검증** - `tools/dialogue_validator.py`

검사가 실패하면 빌드가 실패합니다. 푸시하기 전에 위 명령어로 로컬에서 문제를 수정하세요.

## 에디터 통합

### VS Code

C++에는 저장소 helper를 사용하고 Markdown/TypeScript에는 Deno 확장을 사용하세요:

- **C++**: `just fmt-cpp` 실행
- **Deno**: [Deno](https://marketplace.visualstudio.com/items?itemName=denoland.vscode-deno)

### Visual Studio

PowerShell에서 포매터를 한 번 설치합니다. LLVM은 `clang-format`과 `clang-tidy`를 제공합니다.

```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
irm get.scoop.sh | iex
scoop install llvm astyle
clang-format --version
clang-tidy --version
astyle --version
```

Scoop을 이미 설치했다면 `scoop install llvm astyle`만 실행하세요. 버전 확인 명령을 찾지 못하면 PowerShell과
Visual Studio를 닫았다가 다시 열어 `PATH`를 다시 읽게 하세요.

도구를 설치한 뒤:

1. 저장소 폴더를 Visual Studio에서 CMake 프로젝트로 엽니다.
2. CMake를 구성합니다. 포매터를 설치하기 전에 Visual Studio가 프로젝트를 이미 구성했다면 CMake 캐시를 재구성합니다.
3. **View > Other Windows > CMake Targets View**를 엽니다.
4. 커밋하기 전에 `format` 타겟을 빌드합니다.

Visual Studio의 일반 **Format Document** 명령은 저장소 스타일을 실행하지 않습니다. 터미널에서 `just fmt-cpp`를
실행하세요. CMake `format` 타겟은 `bash`가 있는 환경용 wrapper입니다.

### Vim/Neovim

astyle이나 clang-format을 직접 호출하지 말고 helper를 사용하세요:

```vim
autocmd BufWritePre *.cpp,*.h !just fmt-cpp %
autocmd BufWritePre *.md,*.ts !deno fmt %
```

## 문제 해결

### "json_formatter not found"

JSON 포매터 스크립트를 실행하세요. 보조 포매터를 자동으로 구성하고 빌드합니다:

```sh
build-scripts/format-json.sh
```

### C++ 포매터를 찾을 수 없음

`astyle`과 `clang-format`이 설치되어 있고 PATH에 있는지 확인하세요:

```sh
# 포매터가 사용 가능한지 확인
which astyle
which clang-format

# 없으면 설치 (Ubuntu/Debian)
sudo apt install astyle clang-format
```

그런 다음 다시 실행하세요:

```sh
build-scripts/format-cpp.sh
```

### C++ 포매터가 다른 결과를 생성

저장소 루트에서 helper를 사용해 파일마다 올바른 포매터를 적용하세요:

```sh
just fmt-cpp
```
