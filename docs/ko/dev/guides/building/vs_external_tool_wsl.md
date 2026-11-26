# Visual Studio 외부 도구 자동화 (Windows + WSL)

이 페이지는 `cmd`/PowerShell 인터페이스에서 Cataclysm: BN CMake 빌드 자동화를 실행하는 Visual Studio 외부 도구 워크플로를 설명합니다.

## 개요

- 워크플로는 Visual Studio의 **External Tools** 메뉴에서 실행합니다.
- 동일한 흐름으로 Windows(MSVC) 빌드와 WSL 기반 Linux 빌드를 처리합니다.
- 모든 선택은 메뉴에서 이루어지므로 수동 셸 명령이 필요하지 않습니다.
- 완전한 디버거 지원: Windows는 VS 디버거를 자동으로 연결하고 Linux는 VS SSH 연결을 사용합니다.

## Visual Studio에 외부 도구 추가

**Tools -> External Tools...**를 열고 항목을 추가합니다. 선택한 인터페이스 모드와 관계없이 **Arguments** 필드는 같습니다:

| 필드              | 값                                   |
| ----------------- | ------------------------------------ |
| Title             | BN Build                             |
| Command           | `cmd.exe`                            |
| Arguments         | `/c "$(SolutionDir)cmake-build.bat"` |
| Initial directory | `$(SolutionDir)`                     |

**Use Output Window** 체크박스가 인터페이스 모드를 결정합니다:

| Use Output Window | 인터페이스                                                |
| ----------------- | --------------------------------------------------------- |
| **해제**          | 별도의 `cmd` 창을 열고 번호가 매겨진 텍스트 메뉴를 표시   |
| **선택**          | VS Output 창에서 실행하고 WinForms GUI 목록 선택기를 표시 |

GUI 선택기(선택)는 VS 안에서 모든 작업을 수행하지만 여러 팝업 창을 클릭해야 합니다. 텍스트 메뉴(해제)는 별도의 콘솔 창에서 숫자를 입력합니다. 두 모드의 결과는 같으며 선택은 취향의 문제입니다.

![Visual Studio External Tools 메뉴](https://github.com/user-attachments/assets/a7b5d4b8-2cd3-41be-98ae-e75997619a2c)

![외부 도구 구성 예](https://github.com/user-attachments/assets/197c59df-ac2e-4e2a-99f4-8a5dab860367)

전용 디버그 바로 가기(예: "BN Debug")를 만들려면 같은 설정으로 두 번째 항목을 추가하고 Arguments 필드에 `-Action debug`를 붙입니다:

```
/c "$(SolutionDir)cmake-build.bat" -Platform win -Preset 1 -BuildType 2 -Action debug
```

## 인터페이스 모드

**텍스트 메뉴**("Use Output Window" 해제): 번호가 매겨진 프롬프트가 `cmd` 창에 표시됩니다. 선택할 번호를 입력하세요.

![프롬프트 기반 cmd 인터페이스](https://github.com/user-attachments/assets/934ce9eb-37db-482d-b100-afd7fa215ed8)

**WinForms GUI 선택기**("Use Output Window" 선택): 각 선택마다 팝업 목록 상자가 표시됩니다. 항목을 클릭하고 OK를 누르거나 더블 클릭해 확정합니다.

> **Linux/WSL 참고:** GUI 선택기는 Linux 빌드의 초기 플랫폼 선택에만 적용됩니다. 이후 스크립트가 WSL 작업에 필요한 관리자 콘솔에서 다시 실행되며, 그 창의 나머지 메뉴는 텍스트 프롬프트를 사용합니다.

## 워크플로

실행할 때 스크립트가 다음을 묻습니다:

1. **플랫폼** — Windows(MSVC) 또는 Linux(WSL)
2. **구성 preset** — `CMakePresets.json`(있으면 `CMakeUserPresets.json`도 포함)에서 읽음
3. **빌드 유형** — Debug / RelWithDebInfo / Release (Windows만, Linux 유형은 preset에서 설정)
4. **대상** — preset의 `TILES`/`TESTS` 캐시 변수에서 유도하거나 사용자 지정 이름 입력
5. **동작** — Build, Run, Rebuild, Delete 또는 Debug

빌드가 성공하면 기본 메뉴로 돌아가지 않고 즉시 Run 또는 Debug를 선택할 수 있습니다.

각 세션 끝에는 매번 프롬프트에 답하지 않고 같은 구성을 다시 실행하는 "Repeat last" 옵션이 표시됩니다.

![빌드 완료 출력](https://github.com/user-attachments/assets/b43f3130-77c0-4beb-91a0-3b98cb5915f8)

![빌드 후 실행 중인 Cataclysm: BN](https://github.com/user-attachments/assets/3eaf7f95-5653-4c7d-acdd-7977c81ca0ee)

## 권한 상승

**Windows 빌드**는 일반(권한 상승하지 않은) 무결성으로 실행됩니다. 디버거 자동 연결이 작동하려면 `cmake-build.bat` 또는 Visual Studio를 관리자 권한으로 실행하지 마세요.

**Linux/WSL 빌드**는 WSL 파일 시스템 및 네트워크 작업에 관리자 권한이 필요합니다. 스크립트가 권한 상승 상태가 아님을 감지하면 선택한 옵션을 모두 전달하면서 관리자 PowerShell 창을 자동으로 엽니다.

## 디버깅

### Windows

**Debug** 동작을 선택하면:

1. `Start-Process`로 일반 무결성에서 게임 실행 파일을 시작합니다.
2. COM 자동화(DTE 객체, Running Object Table)를 통해 실행 중인 Visual Studio 인스턴스에 연결합니다.
3. 게임 프로세스에서 `Debugger.Attach()`를 호출해 VS 네이티브 디버거를 자동으로 연결합니다.

스크립트는 VS가 프로세스를 등록할 때까지 최대 5초간 확인한 뒤 성공을 보고하거나 대체 지침을 출력합니다.

**요구 사항:**

- Visual Studio는 일반(관리자 권한이 아닌) 무결성으로 열려 있어야 합니다. VS를 관리자로 실행하면 COM ROT가 무결성 수준별로 분리되어 자동 연결이 실패하고 "no running VS instance found" 메시지가 표시됩니다. 관리자 권한 없이 VS를 다시 시작하세요.
- `cmake-build.bat`도 권한 상승 없이 실행해야 합니다(Windows 빌드에서는 launcher가 권한을 상승시키지 않습니다).

**자동 연결이 실패하면** 스크립트가 프로세스 PID를 출력합니다. **Debug → Attach to Process**(`Ctrl+Alt+P`)를 통해 이름 또는 PID로 수동 연결하세요.

### Linux (SSH 연결)

Linux WSL 빌드 디버깅은 Visual Studio의 SSH 원격 연결 기능을 사용합니다. 스크립트는 각 디버그 실행에서 다음을 자동 구성합니다:

1. WSL에 없으면 `openssh-server` 설치
2. `/etc/ssh/sshd_config`에서 `PasswordAuthentication yes` 활성화
3. 누락된 호스트 키를 만들기 위해 `ssh-keygen -A` 실행
4. SSH 서비스 시작 또는 재시작
5. VS(GDB)가 디버거의 직접 자식이 아닌 프로세스에 연결할 수 있도록 `/proc/sys/kernel/yama/ptrace_scope`를 `0`으로 설정
6. 현재 WSL IP 주소 확인(WSL2는 시작할 때마다 새 IP를 할당)
7. 오래된 `netsh` 포트 프록시 제거 후 새로 생성: `Windows localhost:2222 → WSL <ip>:22`
8. `localhost:2222` 연결 가능 여부 확인
9. WSL 내부 `/tmp/`에 작은 실행 스크립트를 작성하고 새 WSL 창에서 열기(게임이 실제 TTY와 WSLg 디스플레이 환경을 사용하도록 `DISPLAY`/`WAYLAND_DISPLAY` 설정)
10. 단계별 연결 지침 출력

**게임 창이 열린 뒤 Visual Studio에서 연결하려면:**

1. **Debug → Attach to Process**(`Ctrl+Alt+P`)를 엽니다.
2. **Connection type**을 `SSH`로 설정합니다.
3. **Connection target**을 `localhost:2222`로 설정합니다.
4. Enter 또는 연결 버튼을 누르고 WSL 사용자 이름과 암호를 입력합니다.
5. 프로세스 목록에서 게임 프로세스(예: `cataclysm-bn-tiles`)를 찾습니다.
6. **Attach**를 클릭합니다.

Visual Studio는 첫 사용 후 SSH 연결을 저장합니다. 다음 디버그 실행부터는 "Attach to Process"를 열고 저장된 `localhost:2222` 연결을 선택해 연결하면 됩니다.

> 포트 프록시는 `127.0.0.1`(루프백 전용)을 사용하므로 Windows 방화벽의 인바운드 규칙이 필요하지 않습니다.

> `ptrace_scope=0`은 일시적으로 설정되며 WSL을 다시 시작하면 초기화됩니다. TSan의 `vm.mmap_rnd_bits` 수정과 달리 영구 sysctl 파일에는 기록되지 않습니다.

## 비대화형 단축키

`cmake-build.bat`에 매개변수를 직접 전달하면 모든 프롬프트를 건너뛸 수 있습니다. 특정 동작으로 바로 가는 추가 External Tool 항목에 유용합니다.

```bat
rem Windows MSVC 빌드 (preset 1, RelWithDebInfo)
cmake-build.bat -Platform win -Preset 1 -BuildType 2 -Action build

rem Windows 디버그 (게임 실행 및 VS 디버거 자동 연결)
cmake-build.bat -Platform win -Preset 1 -BuildType 1 -Action debug

rem Linux WSL 빌드 (이름으로 preset)
cmake-build.bat -Platform linux -Preset linux-slim -Target cataclysm-bn-tiles -Action build

rem 테스트 필터와 함께 Linux 실행
cmake-build.bat -Platform linux -Preset 2 -Action run -RunArgs "[map]"
```

**매개변수 참조:**

| 매개변수      | 값                                       | 설명                                   |
| ------------- | ---------------------------------------- | -------------------------------------- |
| `-Platform`   | `win` / `linux`                          |                                        |
| `-Preset`     | preset 이름 또는 1부터 시작하는 인덱스   | 인덱스는 CMakePresets.json 순서와 일치 |
| `-BuildType`  | `1`=Debug `2`=RelWithDebInfo `3`=Release | Windows만                              |
| `-Target`     | cmake 대상 이름                          | 생략하면 preset에서 유도               |
| `-Action`     | `build` `run` `rebuild` `delete` `debug` |                                        |
| `-RunArgs`    | 바이너리에 전달할 문자열                 | 예: 테스트 필터의 `[map]`              |
| `-ExtraFlags` | 추가 cmake 구성 플래그                   | 예: `-DFOO=ON`                         |
