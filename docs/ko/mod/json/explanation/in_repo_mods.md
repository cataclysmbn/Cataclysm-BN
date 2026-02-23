# 저장소 내 모드

Cataclysm: Bright Nights는 기본 게임과 함께 몇 가지 공식 모드를 포함하고 있습니다.

## 모드 위치

저장소 내 모드는 다음 위치에 있습니다:

```
data/mods/
├── Aftershock/          # Aftershock 모드
├── No_Hope/             # No Hope 모드  
├── Graphical_Overmap/   # 그래픽 오버맵
└── ...
```

## 공식 모드

### Aftershock

고급 공상과학 콘텐츠가 포함된 대형 콘텐츠 모드:

- 새로운 변이
- 고급 기술
- 추가 적과 생물군계

### No Hope

더 어려운 생존 경험:

- 더 강한 좀비
- 희귀한 자원
- 증가된 난이도

### Graphical Overmap

오버맵 표시 개선:

- 더 나은 시각 효과
- 개선된 아이콘
- 명확한 지형 표시

## 모드 구조

각 모드는 다음을 포함합니다:

```
mod_name/
├── modinfo.json    # 모드 메타데이터
├── items/          # 아이템 정의
├── monsters/       # 몬스터 정의
├── mapgen/         # 맵 생성
└── ...
```

### modinfo.json

```json
[
  {
    "type": "MOD_INFO",
    "id": "mod_id",
    "name": "Mod Name",
    "description": "Mod description",
    "category": "content",
    "dependencies": []
  }
]
```

## 모드 기여

저장소 내 모드에 기여하려면:

1. **모드 디렉토리에서 작업**: 관련 모드 폴더에서 변경
2. **모드 테마 유지**: 변경사항이 모드의 주제에 맞는지 확인
3. **종속성 확인**: 필요한 모든 종속성이 나열되어 있는지 확인
4. **테스트**: 모드를 활성화하여 게임 내에서 테스트

## 새 모드 추가

새로운 저장소 내 모드를 추가하려면:

1. `data/mods/`에 새 디렉토리 생성
2. 필수 `modinfo.json` 추가
3. 모드 콘텐츠 구성
4. 문서 업데이트
5. Pull Request 제출

## 모범 사례

- **일관성 유지**: 기본 게임 스타일 따르기
- **호환성**: 다른 모드와의 충돌 피하기
- **문서화**: 모드 기능 명확히 설명
- **테스트**: 모드를 활성화/비활성화하여 철저히 테스트

## 관련 문서

- [MOD_INFO](../reference/mod/modinfo.md)
- [로딩 순서](loading_order.md)
