---
title: Cygwin
---

> [!CAUTION]
>
> Cygwin 빌드는 오랫동안 테스트되지 않았으며 이 문서는 참고용으로만 보관되고 있습니다.

Windows의 Cygwin에서 Cataclysm-BN을 컴파일하는 가이드입니다.

> [!NOTE]
>
> 이 지침은 CBN의 재배포 가능한 복사본을 만들기 위한 것이 _아닙니다_. 재배포가 목적이라면 웹사이트에서 공식 빌드를 다운로드하거나 [Linux에서 Windows로 크로스 컴파일](./makefile.md#cross-compile-to-windows-from-linux)하세요.

이 지침은 64비트 Windows 7과 64비트 버전의 Cygwin을 사용하여 작성되었습니다. 다른 버전의 Windows에서도 단계는 동일해야 합니다.

느린 환경 설정 및 결과 바이너리 실행으로 인해 MSYS2를 사용한 컴파일이 선호됩니다.

## 전제 조건:

- 64비트 버전의 Windows 7, 8, 8.1 또는 10
- 약 10GB 여유 공간이 있는 NTFS 파티션 (Cygwin 설치에 약 2GB, 저장소에 약 3GB, ccache에 약 5GB)
- 64비트 버전의 Cygwin

## 설치:

1. [Cygwin 홈페이지](https://cygwin.com/)로 가서 64비트 인스톨러를 다운로드합니다 (예: [setup-x86_64.exe](https://cygwin.com/setup-x86_64.exe)).

2. 다운로드한 파일을 실행하여 Cygwin을 설치합니다. `Install from Internet`을 선택하고 원하는 설치 디렉토리를 선택합니다. 개발 전용 디렉토리 (예: `C:\dev\cygwin64`)에 설치하는 것이 좋지만 필수는 아닙니다. 모든 사용자를 위해 설치합니다.

3. `\downloads\` 폴더를 로컬 패키지 디렉토리로 지정합니다 (예: `C:\dev\cygwin64\downloads`).

4. 특별한 이유가 없으면 시스템 프록시 설정을 사용합니다.

5. 다음 화면에서 원하는 미러를 선택합니다.

6. 검색 상자에 `wget`을 입력하고 "Web" 카테고리를 확장하여 드롭다운에서 최신 버전을 선택합니다 (이 가이드 작성 시점에 1.21.1-1).

7. wget이 다음 화면에서 하단으로 스크롤하여 표시되는지 확인합니다. Cygwin이 다운로드하고 설치할 패키지의 전체 목록입니다.

8. 오류가 발생하는 패키지를 재시도합니다.

9. 시작 메뉴 및/또는 데스크톱에 바로 가기를 추가하는 체크박스를 선택한 다음 `Finish` 버튼을 누릅니다.

## 구성:

1. 데스크톱에서 Cygwin64 터미널을 시작합니다.

2. 더 쉬운 패키지 설치를 위해 `apt-cyg`를 설치합니다:

```bash
wget https://raw.githubusercontent.com/transcode-open/apt-cyg/master/apt-cyg -O /bin/apt-cyg
chmod 755 /bin/apt-cyg
```

3. 컴파일에 필요한 패키지를 설치합니다:

```bash
apt-cyg install astyle ccache gcc-g++ intltool git libSDL2_image-devel libSDL2_mixer-devel libSDL2_ttf-devel make xinit
```

이미 설치된 패키지라는 메시지와 요청하지 않은 패키지를 Cygwin이 설치하는 것을 볼 수 있습니다. 이것은 Cygwin의 패키지 관리자가 자동으로 의존성을 해결한 결과입니다.

## 클론 및 컴파일:

1. 다음 명령으로 Cataclysm-BN 저장소를 클론합니다:

**참고:** 전체 CBN 저장소와 모든 히스토리 (3GB)를 다운로드합니다. 테스트만 하는 경우 `--depth=1` (~350MB)을 추가하세요.

```bash
cd /cygdrive/c/dev
git clone https://github.com/cataclysmbn/Cataclysm-BN.git
cd Cataclysm-BN
```

2. 컴파일:

```bash
make -j$((`nproc`+0)) CCACHE=1 RELEASE=1 CYGWIN=1 DYNAMIC_LINKING=1 SDL=1 TILES=1 SOUND=1 LANGUAGES=all LINTJSON=0 ASTYLE=0 BACKTRACE=0 RUNTESTS=0
```

종료되지 않은 문자 상수에 대한 경고를 받을 것입니다. 제가 아는 한 컴파일에는 영향을 주지 않습니다.

**참고**: Sound와 Tiles 지원 및 모든 현지화 언어가 포함된 릴리스 버전을 컴파일하며 검사 및 테스트를 건너뛰고 더 빠른 빌드를 위해 ccache를 사용합니다. 다른 스위치를 사용할 수 있지만 `CYGWIN=1`, `DYNAMIC_LINKING=1`, `BACKTRACE=0`은 문제 없이 컴파일하는 데 필요합니다.

## 실행:

1. 시작 메뉴에서 XWin Server를 실행합니다.

2. 아이콘이 시스템 트레이에 나타나면 검은색 C처럼 보이는 아이콘을 마우스 오른쪽 버튼으로 클릭합니다 (X Applications Menu).

3. System Tools를 가리킨 다음 UXTerm을 클릭합니다.

```bash
cd /cygdrive/c/dev/Cataclysm-BN
./cataclysm-bn-tiles
```

UXTerm 외부에서 Cygwin으로 컴파일된 CBN을 실행할 수 있는 기능은 없습니다.
