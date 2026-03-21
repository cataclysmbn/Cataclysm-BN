---
edit: false
---

# CLI 옵션

> [!NOTE]
>
> 이 페이지는 `tools/gen_cli_docs.ts`에서 자동 생성되며 직접 편집해서는 안 됩니다.

게임 실행 파일은 좋아하는 roguelike를 실행할 뿐만 아니라 모더와 개발자를 돕는 여러 명령줄 옵션을 제공합니다.

---

## 정보

### `--help`

이 메시지를 출력하고 종료합니다.

### `--version`

버전을 출력하고 종료합니다.

### `--paths`

게임에서 사용하는 경로를 출력하고 종료합니다.

## 명령줄 매개변수

### `--seed <문자 및/또는 숫자 문자열>`

난수 생성기의 시드 값을 설정합니다.

### `--jsonverify`

BN json 파일을 확인합니다.

### `--check-mods [mods…]`

BN 모드에 속한 json 파일을 확인합니다.

### `--dump-stats <what> [mode = TSV] [opts…]`

아이템 통계를 덤프합니다.

### `--world <name>`

월드를 로드합니다.

### `--basepath <path>`

모든 게임 데이터 하위 디렉토리의 기본 경로입니다.

### `--dont-debugmsg`

설정하면 디버그 메시지가 출력되지 않습니다.

### `--lua-doc <output path>`

주어진 경로에 Lua 문서를 생성하고 종료합니다.

### `--lua-types <output path>`

주어진 경로에 Lua 타입을 생성하고 종료합니다.

### `--datadir <directory name>`

게임 데이터가 로드되는 하위 디렉토리입니다.

### `--autopickupfile <filename>`

configdir 내의 autopickup 옵션 파일 이름입니다.

### `--motdfile <filename>`

motd 디렉토리 내의 오늘의 메시지 파일 이름입니다.

## 맵 공유

### `--shared`

맵 공유 모드를 활성화합니다.

### `--username <name>`

맵 공유 코드에 이 이름을 캐릭터에 사용하도록 지시합니다.

### `--addadmin <username>`

맵 공유 코드에 이 이름을 캐릭터에 사용하고 치트 함수에 대한 액세스 권한을 부여하도록 지시합니다.

### `--adddebugger <username>`

디버거 내에서 실행 중임을 맵 공유 코드에 알립니다.

### `--competitive`

맵 공유 코드에 게임 내 치트 함수에 대한 액세스를 비활성화하도록 지시합니다.

### `--worldmenu`

맵 공유 코드에서 월드 메뉴를 활성화합니다.

## 사용자 디렉토리

### `--userdir <path>`

./data 디렉토리의 파일 및 아래 이름이 지정된 파일에 대한 사용자 재정의의 기본 경로입니다.

### `--savedir <directory name>`

게임 저장의 하위 디렉토리입니다.

### `--configdir <directory name>`

게임 구성의 하위 디렉토리입니다.

### `--memorialdir <directory name>`

추모의 하위 디렉토리입니다.

### `--optionfile <filename>`

configdir 내의 옵션 파일 이름입니다.
