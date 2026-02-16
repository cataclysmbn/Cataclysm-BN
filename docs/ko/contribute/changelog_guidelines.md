# 변경 내역 작성법

PR 제목은 변경 내역 생성기가 읽을 수 있는 형식인 [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) 규칙을 따라야 합니다. 둘 중 하나를 고르면 됩니다:

```
<타입>: <PR 제목>
<타입>(<범위>, <범위>, ...): <PR 제목>
```

예를 들어:

```
feat: add new mutation
feat(port): port mutation description from DDA
```

제목은 플레이어가 한눈에 이해하기 쉬워야 합니다. 이렇게 명확한 제목(`<동사> <명사>`)을 사용하는 것이 좋습니다:

```diff
- feat: rebalancing some rifles
+ feat: nerf jam chance of m16 and m4
```

변경 전: 버프인지 너프인지, 어떤 소총이 변경되었는지 전체 PR 설명을 읽지 않으면 알기 어렵습니다.
변경 후: 제목만으로 무엇이 변경되었는지 정확히 이해하기 쉽습니다.

## 타입

타입은 제목에서 `:` 앞에 오는 단어로, PR의 종류를 정합니다. 무엇을 골라야 할 지 확실치 않은 경우엔 `feat` (기능 추가) 아니면 `fix`(버그 수정)를 사용하세요.

### `feat`: 기능 추가 및 변경

새로운 기능, 추가 사항 또는 밸런스 변경.

### `fix`: 버그 수정

버그를 고치거나 게임이 문제 없이 돌아가도록 안정성을 높이는 작업.

### `refactor`: 인프라 개선

위 둘에 해당하지는 않지만 개발이 더 편해지도록 프로젝트를 개선하는 작업입니다. 예를 들어:

- `C++` 리팩토링 및 개편
- `Json` 재구성
- `docs/`, `.github/` 및 저장소 변경
- 기타 개발 도구 관련 변경들

### `build`: 빌드

빌드 프로세스를 개선할 때 씁니다.

- 빌드 안정성 높이기
- 초보도 빌드하기 더 쉽게 만들기
- 컴파일 시간 개선

### 기타

- `docs`: 문서 변경
- `style`: 코드 스타일 변경 (공백, 서식 등), 보통은 JSON 서식 수정에 쓰입니다.
- `perf`: 성능 개선
- `test`: 테스트 추가 및 수정
- `ci`: 배포 프로세스 변경
- `chore`: 위의 카테고리에 맞지 않는 기타 변경 사항
- `revert`: 이전 커밋 되돌리기

## 범위

1. 카테고리 뒤 괄호 안에 사용하여 PR의 범위를 더 구체적으로 지정할 수 있습니다.
2. 범위의 수에는 제한이 없으며, 선택 사항이지만 있으면 더 좋습니다.

### `<없음>`: 기본값

예를 들어,

플레이어 관련 변경 사항:

- 플레이어가 새로운 것을 할 수 있음 (예: 변이, 스킬)
- 플레이어에게 새로운 일이 일어날 수 있음 (예: 새로운 질병)

새로운 콘텐츠:

- 새로운 몬스터
- 새로운 지형
- 새로운 아이템
- 새로운 차량
- 새로운 장치

PR 제목 예시:

```
feat: strength training activity
feat: mutation system overhaul
feat: semi-plausible smokeless gunpowder recipe
feat(port): game store
```

### `lua`: Lua API 변경

Lua API 변경 사항, 예:

- [새로운 바인딩 추가](../mod/lua/guides/binding.md)
- lua 문서/API 생성 개선
- [하드코딩된 C++ 기능을 lua로 마이그레이션](https://github.com/cataclysmbn/Cataclysm-BN/pull/6901)

PR 제목 예시:

```
feat(lua): add dialogue bindings
```

### `UI`: 인터페이스

UI/UX 변경 사항, 예시:

- 마우스 지원 추가
- 메뉴 추가/조정
- 단축키 변경
- 워크플로우 간소화
- 편의성 개선

PR 제목 예시:

```
feat(UI): More info about items dropped with bags
feat(UI): overhaul encumbrance UI
```

### `i18n`: 국제화

번역 및 기타 언어 지원 개선.

```
fix(UI,i18n): recipe names not translated unless learned
```

### `mods` 또는 `mods/<MOD_ID>`: 모드

- 모드 내에 포함된 변경 사항
- 모드 내에서 가능한 기능 추가

PR 제목 예시:

```
feat(mods/Magical_Nights): add missing owlbear pelts recipe
fix(mods): No Hope doesn't make the world freezing
```

### `balance`: 밸런스 변경

게임 밸런스 변경.

PR 제목 예시:

```
feat(balance): Give moose pelts instead of hides
```

### `port`: DDA 또는 다른 포크에서 포팅

PR 제목 예시:

```
feat(port): game shop
```
