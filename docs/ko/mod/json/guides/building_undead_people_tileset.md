# Undead People 타일셋 빌드

이 가이드는 Cataclysm: Bright Nights용 Undead People 타일셋을 빌드하는 방법을 설명합니다.

## 전제 조건

시작하기 전에 다음이 필요합니다:

- Python 3.6+
- PIL/Pillow 라이브러리
- Undead People 타일셋 소스 파일
- 기본 이미지 편집 지식

## 디렉토리 구조

```
gfx/UltimateCataclysm/
├── tile_config.json       # 타일 설정
├── tiles/                 # 개별 타일 이미지
├── pngs_tiles_32x32/      # 32x32 타일
├── pngs_tiles_64x64/      # 64x64 타일
└── ...
```

## 빌드 프로세스

### 1. 종속성 설치

```bash
pip install Pillow
```

### 2. 타일셋 복제

```bash
git clone https://github.com/SomeDeadGuy/UndeadPeopleTileset
cd UndeadPeopleTileset
```

### 3. 타일 빌드

```bash
python build_tileset.py
```

### 4. 출력 확인

빌드된 타일셋은 다음 위치에 있습니다:

```
output/
├── tile_config.json
├── tiles.png
└── ...
```

## 타일 설정

### tile_config.json

```json
{
  "tile_info": [
    {
      "width": 32,
      "height": 32,
      "pixelscale": 1
    }
  ],
  "tiles-new": [
    {
      "file": "tiles.png",
      "tiles": [
        {
          "id": "mon_zombie",
          "fg": 0,
          "bg": 1
        }
      ]
    }
  ]
}
```

## 새 타일 추가

### 1. 타일 이미지 생성

- 크기: 32x32 또는 64x64 픽셀
- 형식: PNG (투명도 지원)
- 스타일: 기존 타일과 일치

### 2. 타일 추가

이미지를 적절한 디렉토리에 배치:

```
tiles/monsters/zombie_new.png
```

### 3. 설정 업데이트

`tile_config.json`에 항목 추가:

```json
{
  "id": "mon_zombie_new",
  "fg": "zombie_new",
  "bg": "default_bg"
}
```

### 4. 재빌드

```bash
python build_tileset.py
```

## 타일 ID

타일 ID는 게임 JSON ID와 일치해야 합니다:

| 타입      | 접두사 | 예시           |
| --------- | ------ | -------------- |
| 몬스터    | `mon_` | `mon_zombie`   |
| 아이템    | 없음   | `knife_combat` |
| 지형      | `t_`   | `t_wall`       |
| 가구      | `f_`   | `f_chair`      |
| 차량 부품 | `vp_`  | `vp_wheel`     |

## 고급 기능

### 다중 프레임 애니메이션

```json
{
  "id": "mon_zombie_animated",
  "fg": [
    "zombie_frame1",
    "zombie_frame2",
    "zombie_frame3"
  ],
  "animated": true
}
```

### 계절별 변형

```json
{
  "id": "t_tree",
  "fg": "tree_spring",
  "additional_tiles": [
    {
      "season": "summer",
      "fg": "tree_summer"
    },
    {
      "season": "autumn",
      "fg": "tree_autumn"
    },
    {
      "season": "winter",
      "fg": "tree_winter"
    }
  ]
}
```

### 방향별 타일

```json
{
  "id": "t_door",
  "multitile": true,
  "additional_tiles": [
    {
      "id": "center",
      "fg": "door_center"
    },
    {
      "id": "edge",
      "fg": ["door_edge_ns", "door_edge_ew"]
    }
  ]
}
```

## 테스트

### 게임 내 테스트

1. 빌드된 타일셋을 `gfx/` 폴더에 복사
2. 게임 실행
3. 설정 > 그래픽 > 타일셋 선택
4. 새 타일이 올바르게 나타나는지 확인

### 일반적인 문제

**타일이 표시되지 않음**

- ID가 일치하는지 확인
- 이미지 경로 확인
- JSON 구문 검증

**타일이 잘못 정렬됨**

- 이미지 크기 확인 (정확히 32x32 또는 64x64)
- 타일 설정에서 픽셀스케일 확인

**타일이 깨짐**

- PNG 형식 확인
- 투명도 확인
- 색상 모드 (RGBA) 확인

## 도구

### 이미지 편집기

- GIMP (무료, 오픈소스)
- Aseprite (픽셀 아트 특화)
- Photoshop

### 타일 도구

- `tile_editor.py` - 타일 빠르게 편집
- `sprite_sheet_gen.py` - 스프라이트 시트 생성
- `tile_validator.py` - 타일 설정 검증

## 모범 사례

1. **일관된 스타일**: 기존 타일과 일치
2. **적절한 크기**: 올바른 픽셀 치수 사용
3. **설명적 이름**: 명확한 파일명 사용
4. **레이어 구성**: 소스 파일을 레이어별로 유지
5. **버전 관리**: 타일 변경사항 추적

## 기여

Undead People 타일셋에 기여하려면:

1. 저장소 포크
2. 새 브랜치 생성
3. 타일 추가/수정
4. 변경사항 테스트
5. Pull Request 제출

## 참고 자료

- [타일셋 튜토리얼](https://github.com/CleverRaven/Cataclysm-DDA/wiki/Tileset-creation)
- [타일 설정 참조](https://github.com/CleverRaven/Cataclysm-DDA/blob/master/doc/TILESET.md)
- [Undead People 저장소](https://github.com/SomeDeadGuy/UndeadPeopleTileset)

## 관련 문서

- [타일셋 JSON](../../reference/tileset.md)
