# 로딩 순서

이 문서는 Cataclysm: Bright Nights가 JSON 파일을 로드하는 순서를 설명합니다.

## 로딩 단계

게임은 다음 순서로 JSON을 로드합니다:

### 1. 코어 데이터

```
data/json/
├── damage_type.json      # 먼저 로드되어야 함
├── materials.json        # 기본 재질
├── flags.json            # 플래그 정의
└── ...
```

### 2. 기본 정의

```
data/json/
├── items/                # 아이템 정의
├── monsters/             # 몬스터
├── furniture_terrain/    # 가구와 지형
└── ...
```

### 3. 복잡한 시스템

```
data/json/
├── mapgen/              # 맵 생성
├── recipes/             # 제작법
├── requirements/        # 제작 요구사항
└── ...
```

### 4. 모드

```
data/mods/
├── mod1/
├── mod2/
└── ...
```

모드는 `modinfo.json`의 종속성 순서에 따라 로드됩니다.

## 종속성

### 타입 종속성

일부 타입은 다른 타입에 의존합니다:

```
materials → items → recipes
damage_types → monsters
terrain → mapgen
```

### 필드 종속성

일부 필드는 다른 정의를 참조합니다:

```json
{
  "type": "recipe",
  "result": "item_id", // items에서 존재해야 함
  "using": ["requirement_id"] // requirements에서 존재해야 함
}
```

## 모드 종속성

모드는 `modinfo.json`에서 종속성을 선언할 수 있습니다:

```json
{
  "type": "MOD_INFO",
  "id": "my_mod",
  "name": "My Mod",
  "dependencies": ["dda", "other_mod"]
}
```

로딩 순서:

1. 기본 게임 (`dda`)
2. `other_mod`
3. `my_mod`

## 오버라이드

나중에 로드된 정의가 이전 정의를 오버라이드합니다:

### 복사-출처 (Copy-From)

```json
{
  "type": "item",
  "id": "my_variant",
  "copy-from": "base_item",
  "name": "My Variant"
}
```

### 확장 (Extend)

```json
{
  "type": "item",
  "id": "existing_item",
  "extend": { "flags": ["NEW_FLAG"] }
}
```

### 삭제 (Delete)

```json
{
  "type": "item",
  "id": "existing_item",
  "delete": { "flags": ["OLD_FLAG"] }
}
```

## 로딩 오류

### 일반적인 문제

1. **누락된 종속성**

```
ERROR: item "my_item" references unknown material "nonexistent"
```

**수정**: 재질을 먼저 정의하거나 올바른 ID 사용

2. **순환 종속성**

```
ERROR: circular dependency detected: item_a -> item_b -> item_a
```

**수정**: 종속성 체인 재구성

3. **모드 순서 문제**

```
ERROR: mod "my_mod" depends on "other_mod" which is not loaded
```

**수정**: `modinfo.json`에 종속성 추가

## 디버깅

로딩 문제를 디버깅하려면:

### 디버그 모드 활성화

```bash
./cataclysm-tiles --debug
```

### 로그 확인

```
config/debug.log
```

### 특정 타입 검사

게임 내 디버그 메뉴 사용:

- `~` - 디버그 메뉴 열기
- "Show JSON errors" 선택

## 모범 사례

1. **올바른 순서로 정의**: 종속성을 먼저 정의
2. **명시적 종속성**: 모드에서 명확한 종속성 선언
3. **상위 호환성 테스트**: 모드를 다양한 순서로 테스트
4. **로그 확인**: 항상 로딩 오류 확인
5. **문서화**: 복잡한 종속성 문서화

## 관련 문서

- [MOD_INFO](../reference/mod/modinfo.md)
- [JSON 스타일](json_style.md)
- [파일 설명](file_description.md)
