---
title: 포매팅 및 린팅
---

Cataclysm: Bright Nights에서 코드를 포맷하고 린트하는 방법을 설명합니다.

## 빠른 참조

| 파일 타입       | 도구           | 명령어                                             |
| --------------- | -------------- | -------------------------------------------------- |
| C++ (`.cpp/.h`) | astyle         | `cmake --build build --target astyle`              |
| JSON            | json_formatter | `cmake --build build --target style-json-parallel` |
| Markdown        | deno fmt       | `deno fmt`                                         |
| TypeScript      | deno fmt       | `deno fmt`                                         |
| Lua             | dprint         | `deno task dprint fmt`                             |

## 자동 포매팅

풀 리퀘스트는 [autofix.ci](https://autofix.ci)에 의해 자동으로 포맷됩니다. 코드에 스타일 위반이 있으면 수정 커밋이 푸시됩니다.

> [!TIP]
> autofix 커밋 후 머지 충돌을 피하려면:
>
> 1. `git pull`을 실행하여 autofix 커밋을 머지한 후 작업을 계속하거나
> 2. 푸시하기 전에 로컬에서 포맷한 다음 필요시 `git push --force`

## C++ 포매팅

C++ 파일은 [astyle](http://astyle.sourceforge.net/)로 포맷됩니다.

```sh
# astyle 설치 (Ubuntu/Debian)
sudo apt install astyle

# astyle 설치 (Fedora)
sudo dnf install astyle

# astyle 설치 (macOS)
brew install astyle
```

### CMake 사용

```sh
# 구성 (한 번만 필요하거나 기존 빌드 사용)
cmake --preset lint

# 모든 C++ 파일 포맷
cmake --build build --target astyle
```

스타일 설정은 저장소 루트의 `.astylerc`에 있습니다.

## JSON 포매팅

JSON 파일은 프로젝트 소스에서 빌드된 커스텀 도구 `json_formatter`로 포맷됩니다.

### CMake 사용

```sh
# 구성 (한 번만 필요하거나 기존 빌드 사용)
cmake --preset lint

# 모든 JSON 파일을 병렬로 포맷
cmake --build build --target style-json-parallel

# 모든 JSON 파일을 순차적으로 포맷 (느리지만 디버깅에 유용)
cmake --build build --target style-json
```

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

커밋하기 전에 다음 검사를 실행하세요:

```sh
# 한 번만 구성 (포매팅 도구가 포함된 빌드 디렉토리 생성)
cmake --preset lint

# 모든 코드 포맷
cmake --build build --target astyle           # C++
cmake --build build --target style-json-parallel  # JSON
deno fmt                                       # Markdown/TypeScript
deno task dprint fmt                           # Lua
```

## CI 통합

CI 파이프라인은 이러한 검사를 자동으로 실행합니다:

1. **JSON 구문 검증** - `build-scripts/lint-json.sh`
2. **JSON 포매팅** - `cmake --build build --target style-json-parallel`
3. **대화 검증** - `tools/dialogue_validator.py`

검사가 실패하면 빌드가 실패합니다. 푸시하기 전에 위 명령어로 로컬에서 문제를 수정하세요.

## 에디터 통합

### VS Code

자동 포매팅을 위해 다음 확장을 설치하세요:

- **C++**: [C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) (astyle 통합 포함)
- **Deno**: [Deno](https://marketplace.visualstudio.com/items?itemName=denoland.vscode-deno) (Markdown/TypeScript용)

### Vim/Neovim

설정에 추가:

```vim
" 저장시 astyle로 C++ 포맷
autocmd BufWritePre *.cpp,*.h !astyle --options=.astylerc %

" deno로 포맷
autocmd BufWritePre *.md,*.ts !deno fmt %
```

## 문제 해결

### "json_formatter not found" 또는 "style-json-parallel target not found"

`lint` 프리셋으로 CMake를 구성했거나 `-DJSON_FORMAT=ON`을 사용했는지 확인하세요:

```sh
cmake --preset lint
cmake --build build --target json_formatter
```

### "astyle target not found"

`astyle`이 설치되어 있고 PATH에 있는지 확인하세요:

```sh
# astyle이 사용 가능한지 확인
which astyle

# 없으면 설치 (Ubuntu/Debian)
sudo apt install astyle
```

그런 다음 CMake를 재구성:

```sh
cmake --preset lint
```

### astyle이 다른 결과를 생성

저장소 루트의 `.astylerc`를 사용하고 있는지 확인하세요:

```sh
astyle --options=.astylerc src/*.cpp
```
