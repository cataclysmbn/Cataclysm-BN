# 개발자 도구

## 코드 스타일 (astyle)

소스 코드의 자동 포매팅은 [Artistic Style](http://astyle.sourceforge.net/) 또는 줄여서 `astyle`로 수행됩니다.

시스템이나 개인 선호도에 따라 코드베이스에서 호출하는 여러 방법이 있습니다.

### astyle 직접 호출

`astyle`만 설치되어 있는 경우:

```sh
astyle --options=.astylerc --recursive src/*.cpp,*.h tests/*.cpp,*.h tools/*.cpp,*.h
```

### make를 통해 astyle 호출

`make`와 `astyle`이 모두 설치되어 있는 경우:

```sh
make astyle
```

### pre-commit 훅을 통해 astyle 호출

관련 도구가 모두 설치되어 있는 경우 git pre-commit 훅 (일반적으로 `.git/hooks/pre-commit`)에 다음 명령을 추가하여 git이 코드 및 json 스타일을 자동으로 확인하도록 할 수 있습니다:

```sh
git diff --cached --name-only -z HEAD | grep -z 'data/.*\.json' | \
    xargs -r -0 -L 1 ./tools/format/json_formatter.[ce]* || exit 1

make astyle-check || exit 1
```

### Visual Studio용 Astyle 확장

Visual Studio Marketplace에 astyle 확장이 있지만 VS2019 또는 VS2022에서 우리 목적으로 올바르게 작동하는 것으로 (아직) 확인된 것은 없습니다.

#### Visual Studio 2022

<https://github.com/olanti-p/BN_Astyle>로 이동하여 [README.md](https://github.com/olanti-p/BN_Astyle/blob/master/README.md)의 지침을 따르세요. 소스에서 확장을 컴파일하고 설치하거나 [릴리스 섹션](https://github.com/olanti-p/BN_Astyle/releases)의 미리 빌드된 버전을 활용할 수 있습니다.

#### Visual Studio 2019

확장의 소스 코드는 https://github.com/lukamicoder/astyle-extension에 있습니다. 설치하고 컴파일하려면:

1. Visual Studio 설치 프로그램을 통해 VS2019에 `Visual Studio extension development` 워크로드를 추가합니다.
2. 소스 코드를 다운로드하고 압축을 풀거나 저장소를 클론합니다 (간단한 `git clone --depth 1 https://github.com/lukamicoder/astyle-extension.git`로 충분함).
3. 루트 폴더에서 `astyle-extension/AStyleExtension2017.sln`을 엽니다.
4. `Release` 빌드 구성을 선택합니다 (VS가 기본적으로 `Debug` 구성을 선택할 가능성이 높음).
5. 솔루션을 빌드합니다.
6. 빌드가 성공하면 `AStyleExtension\bin\Release`에서 컴파일된 확장을 볼 수 있습니다. 더블 클릭하여 설치합니다.
7. [구성 지침 (Visual Studio 2019 또는 이전)](#configuration-instructions-visual-studio-2019-or-older) 섹션에 따라 확장을 구성합니다.

#### Visual Studio 2017 또는 이전

VS2019의 단계를 따라 소스에서 컴파일할 수 있지만 Visual Studio Marketplace에서 [사용 가능한](https://marketplace.visualstudio.com/items?itemName=Lukamicoder.AStyleExtension2017) 미리 빌드된 버전이 있으며 VS의 확장 관리자를 통해 확장을 설치한 다음 동일한 방식으로 구성할 수 있어야 합니다.

#### 구성 지침 (Visual Studio 2019 또는 이전):

1. `Tools` - `Options` - `AStyle Formatter` - `General`로 이동합니다.

2. `Export/Import` 탭에서 `Import` 버튼을 사용하여 `https://github.com/cataclysmbn/Cataclysm-BN/blob/main/msvc-full-features/AStyleExtension-Cataclysm-BN.cfg`를 가져옵니다:

![image](./img/VS_Astyle_Step_1.png)

3. 가져오기가 성공하면 `C/C++` 탭에서 가져온 규칙을 볼 수 있습니다:

![image](./img/VS_Astyle_Step_2.png)

4. `Options` 메뉴를 닫고 astyle을 적용할 파일을 열고 `Edit` - `Advanced` 메뉴에서 `Format Document (Astyle)` 또는 `Format Selection (Astyle)` 명령을 사용합니다.

![image](./img/VS_Astyle_Step_3.png)

_참고:_ `Tools` - `Options` - `Environment` - `Keybindings` 메뉴에서 언급된 명령에 대한 키바인딩을 구성할 수도 있습니다:

![image](./img/VS_Astyle_Step_4.png)

## JSON 스타일

[JSON 스타일 가이드](../../mod/json/explanation/json_style)를 참조하세요.

## ctags

예를 들어 [`ctags`](http://ctags.sourceforge.net/)를 통해 `tags` 파일을 만드는 일반적인 수단 외에도 CDDA JSON 데이터에서 가져온 정의 위치로 `tags` 파일을 보강하기 위해 `tools/json_tools/cddatags.py`를 제공합니다. `cddatags.py`는 소스 코드 태그를 포함하는 tags 파일을 안전하게 업데이트하도록 설계되었으므로 `tags` 파일에 두 가지 유형의 태그를 모두 원하면 `ctags -R . && tools/json_tools/cddatags.py`를 실행할 수 있습니다. 또는 이것을 수행하는 `Makefile`의 규칙이 있습니다. `make ctags` 또는 `make etags`를 실행하면 됩니다.

## clang-tidy

Cataclysm에는 [clang-tidy 구성 파일](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/.clang-tidy)이 있으며 `clang-tidy`가 사용 가능하면 실행하여 코드베이스의 정적 분석을 수행할 수 있습니다. CI로 LLVM 18의 `clang-tidy`로 테스트하므로 가장 일관된 결과를 위해 해당 버전을 사용할 수 있습니다.

실행하려면 몇 가지 옵션이 있습니다.

- `clang-tidy`는 래퍼 스크립트 `run-clang-tidy.py`와 함께 제공됩니다.

- `-DCMAKE_CXX_CLANG_TIDY=clang-tidy` 또는 유사한 것을 추가하여 CMake의 내장 지원을 사용하여 선택한 clang-tidy 버전을 가리킵니다.

- `clang-tidy`를 직접 실행하려면 다음과 같이 시도하세요:

```sh
grep '"file": "' build/compile_commands.json | \
    sed "s+.*$PWD/++;s+\"$++" | \
    egrep '.' | \
    xargs -P 9 -n 1 clang-tidy -quiet
```

파일의 하위 집합에 집중하려면 명령줄 중간의 `egrep` 정규식에 이름을 추가합니다.

## 커스텀 clang-tidy 플러그인

커스텀 플러그인에 자체 clang-tidy 검사를 작성했습니다. Ubuntu 24.04에서 플러그인을 빌드하는 정확한 단계는 [clang-tidy.yml](https://github.com/cataclysmbn/Cataclysm-BN/blob/main/.github/workflows/clang-tidy.yml)을 참조하세요.

### Ubuntu 24.04에서 플러그인 빌드

다음 변경 사항과 함께 [cmake 가이드](../../dev/guides/building/cmake.md)의 정확한 단계를 따르세요:

이러한 추가 의존성도 설치합니다:

```sh
sudo apt-get install \
  clang-18 libclang-18-dev llvm-18 llvm-18-dev clang-tidy-18
```

빌드를 구성할 때 cmake 플래그에 `CATA_CLANG_TIDY_PLUGIN=ON`을 추가합니다.

### Fedora 40에서 플러그인 빌드

다음 변경 사항과 함께 [atomic cmake 가이드](../../dev/guides/building/atomic_cmake.md)의 정확한 단계를 따르세요:

이러한 추가 의존성도 설치합니다:

```sh
sudo dnf install clang-devel llvm-devel clang-tools-extra-devel
```

빌드를 구성할 때 cmake 플래그에 `CATA_CLANG_TIDY_PLUGIN=ON`을 추가합니다.

### 플러그인 사용

단일 파일에서 플러그인을 실행하려면 프로젝트 루트에서 다음 명령을 실행합니다:

```sh
$ ./build-scripts/clang-tidy-wrapper.sh -fix src/achievement.cpp
```

여러 파일에서 플러그인을 실행하려면 [GNU parallel](https://www.gnu.org/software/parallel/)을 사용합니다:

```sh
$ parallel ./build-scripts/clang-tidy-wrapper.sh -fix ::: src/*.cpp
```

## include-what-you-use

[include-what-you-use](https://github.com/include-what-you-use/include-what-you-use) (IWYU)는 include를 최적화하기 위한 프로젝트입니다. 필요한 헤더를 계산하고 적절하게 include를 추가 및 제거합니다.

이 코드베이스에서 실행하면 몇 가지 문제가 드러났습니다. 다음 PR이 병합된 IWYU 버전이 필요합니다 (작성 시점에 아직 발생하지 않았지만 운이 좋으면 IWYU의 clang-10 릴리스에 포함될 수 있음):

- https://github.com/include-what-you-use/include-what-you-use/pull/775

IWYU를 빌드한 후 `CMAKE_EXPORT_COMPILE_COMMANDS=ON`으로 cmake를 사용하여 코드베이스를 빌드하여 컴파일 데이터베이스를 만듭니다 (빌드 디렉토리에서 `compile_commands.json`을 찾아 작동했는지 확인).

그런 다음 실행:

```sh
iwyu_tool.py -p $CMAKE_BUILD_DIR/compile_commands.json -- -Xiwyu --mapping_file=$PWD/tools/iwyu/cata.imp | fix_includes.py --nosafe_headers --reorder
```

IWYU는 때때로 clang-tidy가 좋아하지 않는 C 스타일 라이브러리 헤더를 추가하므로 clang-tidy를 실행한 다음 (위에 설명된 대로) IWYU를 두 번째로 다시 실행해야 할 수 있습니다.

`tools/iwyu`에 IWYU가 올바른 헤더를 선택하는 데 도움이 되는 매핑 파일이 있습니다. 대부분은 상당히 명확해야 하지만 SDL 매핑은 추가 설명이 필요할 수 있습니다. `sdl_wrappers.h`를 통해 대부분의 SDL include가 이루어지도록 강제하려고 합니다. 플랫폼 종속 문제를 처리하기 때문입니다 (include 경로가 Windows에서 다릅니다). 몇 가지 예외 (`SDL_version.h` 및 `SDL_mixer.h`)가 있습니다. 전자는 `main.cpp`가 모든 SDL 헤더를 포함할 수 없기 때문입니다. `#define WinMain`을 하기 때문입니다. `sdl.imp`의 모든 매핑은 이것이 발생하도록 설계되었습니다.

일부 상황에서는 IWYU pragma를 사용해야 합니다. 이유 중 일부는 다음과 같습니다:

- IWYU에는 [연관된 헤더](https://github.com/include-what-you-use/include-what-you-use/blob/master/docs/IWYUPragmas.md#iwyu-pragma-associated) 개념이 있으며 각 cpp 파일은 이러한 헤더의 일부를 가질 수 있습니다. cpp 파일은 해당 헤더에 선언된 것을 정의할 것으로 예상됩니다. Cata에서 헤더와 cpp 파일 간의 매핑은 그렇게 간단하지 않으므로 여러 연관된 헤더가 있는 파일과 없는 파일이 있습니다. 어떤 cpp 파일의 연관된 헤더도 아닌 헤더는 include가 업데이트되지 않아 빌드가 깨질 수 있으므로 모든 헤더가 일부 cpp 파일과 연관되는 것이 이상적입니다. 다음 명령을 사용하여 현재 cpp 파일과 연관되지 않은 헤더 목록을 얻을 수 있습니다 (GNU sed 필요):

```sh
diff <(ls src/*.h | sed 's!.*/!!') <(for i in src/*.cpp; do echo $i; sed -n '/^#include/{p; :loop n; p; /^$/q; b loop}' $i; done | grep 'e "' | grep -o '"[^"]*"' | sort -u | tr -d '"')
```

- [clang 버그](https://bugs.llvm.org/show_bug.cgi?id=20666)로 인해 명시적 인스턴스화에 대한 템플릿 인수의 사용이 계산되지 않아 일부 `IWYU pragma: keep`가 필요합니다.

- [이러한](https://github.com/include-what-you-use/include-what-you-use/blob/4909f206b46809775e9b5381f852eda62cbf4bf7/iwyu.cc#L1617) [누락된](https://github.com/include-what-you-use/include-what-you-use/blob/4909f206b46809775e9b5381f852eda62cbf4bf7/iwyu.cc#L1629) IWYU 기능으로 인해 반환 타입에 대한 템플릿 인수의 사용을 계산하지 않아 `IWYU pragma: keep`에 대한 다른 요구 사항이 발생합니다.

- IWYU는 맵에 사용되는 타입에 특히 문제가 있는 것 같습니다. 자세히 살펴보지 않았지만 pragma로 해결했습니다.
