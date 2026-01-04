# 자주 묻는 질문

# 추가하기

## 몬스터 추가

1. `data/json/monsters.json`을 편집하거나 새 json 파일을 만들고 새 몬스터의 정의를 삽입합니다 (아마도 기존 항목을 복사).
2. id 값이 다른 모든 몬스터 타입 중에서 고유한지 확인합니다.
3. 몬스터 타입이 이제 유효하지만 생성되지 않습니다. 유사한 몬스터들 사이에서 생성되도록 하려면 monstergroups.json을 편집합니다. 적절한 배열을 찾아 몬스터의 식별자 (예: `mon_zombie`)를 삽입합니다. `cost_multiplier`는 생성 비용을 더 비싸게 만듭니다. 비용이 높을수록 더 많은 '슬롯'을 차지하고 freq는 생성 빈도입니다. `mongroupdef.cpp` 참조
4. 몬스터가 아이템을 드롭하도록 하려면 `monster_drops.json`을 편집합니다. 몬스터 타입에 대한 새 배열을 만들고 가질 수 있는 모든 맵 아이템 그룹과 각각의 확률 값을 포함합니다.
5. 몬스터가 특수 공격, `monattack::function` 참조를 가질 수 있습니다. `monattack.h`를 편집하고 클래스 정의에 함수를 포함합니다. `monstergenerator.cpp`를 편집하고 번역을 추가한 다음 `monattack.cpp`를 편집하고 함수를 정의합니다. 함수는 다른 몬스터 타입 간에 공유될 수 있습니다. 함수에 몬스터가 공격 사용 여부를 결정하는 문이 포함되어야 하며, 사용하는 경우 몬스터의 공격 타이머를 재설정해야 합니다.
6. 공격과 마찬가지로 일부 몬스터는 죽을 때 호출되는 특수 함수를 가질 수 있습니다. 이것은 공격과 동일하게 작동하지만 관련 파일은 `mondeath.h` 및 `mondeath.cpp`입니다.
7. 플래그를 추가하는 경우 `json_flags.md` 및 `mtype.h`에 문서화하세요. 제발. 아니면 밤에 당신의 피를 산으로 바꿀 것입니다.

## 맵에 구조물 추가

대부분의 "일반" 건물은 도시 (서로 상당히 가까운 건물의 큰 클러스터)에서 생성됩니다.

`omdata.h` 파일의 `oter_id` 구조 enum에서 건물의 이름 (코드 식별자)을 정의합니다.

건물을 오버맵에서 다른 방향으로 표시하려면 각 방향 (`south`, `east`, `west`, `north` 순서)에 대해 4개의 식별자를 추가해야 합니다.

동일한 파일의 `const oter_t oterlist[num_ter_types]` 구조에서 이러한 건물이 표시되는 방법, 시야를 얼마나 가리는지, 어떤 엑스트라 세트를 가지는지 정의해야 합니다. 예:

```cpp
{"mil. surplus", '^', c_white, 5, build_extras, false, false},
{"mil. surplus", '>', c_white, 5, build_extras, false, false},
{"mil. surplus", 'v', c_white, 5, build_extras, false, false},
{"mil. surplus", '<', c_white, 5, build_extras, false, false}
```

이 구조 시작 부분의 주석이 상당히 유용합니다. `mapgen.cpp` 파일에서 `draw_map(...);`라는 서브루틴을 찾으면 거대한 variant 연산자 ("switch")를 찾아야 합니다. 여기에 새 case-statement를 추가하여 새 건물을 정의하는 코드를 넣어야 합니다.

대부분의 건물은 `SEEX*2 x SEEY*2` 타일 사각형에 빌드됩니다.

건물을 도시 경계뿐만 아니라 생성하려면 `omdata.h` 파일의 구조 (`#define OMSPEC_FREQ 7` 줄부터 시작)를 참조해야 합니다.

이러한 구조는 소스 코드에서도 주석이 달려 있습니다. `NUM_OMSPECS` 전에 `omspec_id` 구조 enum에 새 식별자를 추가한 다음 `const overmap_special overmap_specials[NUM_OMSPECS]` 배열에 레코드를 추가합니다. 예:

```cpp
{ot_toxic_dump,   0,  5, 15, -1, mcat_null, 0, 0, 0, 0, &omspec_place::wilderness,0}
```

소스 코드의 `struct overmap_special` 구조에 제공된 주석은 위 예제의 이러한 상수의 의미를 설명합니다.

## 바이오닉 추가

1. `data/json/bionics.json`을 편집하고 유사한 타입 근처에 바이오닉을 추가합니다. 개별 필드에 대한 더 심층적인 검토는 `JSON_INFO.md`를 참조하세요.
2. 바이오닉을 게임 세계의 아이템으로 사용할 수 있도록 하려면 `item_groups.json`에 추가하고 `data/json/items/bionics.json`에 바이오닉 아이템을 추가합니다.
3. 적절한 파일에 효과를 수동으로 코딩합니다. 활성화된 바이오닉의 경우 `bionics.cpp`의 `player::activate_bionic` 함수를 편집합니다.
4. 바이오닉 원거리 무기의 경우 바이오닉 무기 대응물을 `ranged.json`에 추가하고 `BIONIC_WEAPON` 플래그를 부여합니다.
5. 바이오닉 근접 전투 무기의 경우 바이오닉 무기를 `data/json/items/melee.json`에 추가하고 최소한 `NON_STUCK`, `NO_UNWIELD`를 부여합니다.

## 갑옷 보호가 계산되는 방법

1. 플레이어가 특정 신체 부위에 맞을 때 갑옷 커버리지가 갑옷이 맞았는지 플레이어의 커버되지 않은 부분이 맞았는지 결정합니다 (커버리지에 대해 `1d100` 굴림).
2. 위의 굴림이 실패하면 (즉, 굴림 값이 커버리지보다 높음) 갑옷은 타격으로부터 어떤 데미지도 흡수하지 않으며 손상되지도 않습니다.
3. 위의 굴림이 성공하면 갑옷이 맞았으며 일부 데미지를 흡수하고 프로세스에서 손상될 수 있습니다.
4. 위의 단계는 신체 부위의 각 갑옷 레이어에 대해 반복됩니다.
5. 갑옷은 bash 및 cut 데미지를 방어합니다. 이것은 갑옷 두께에 `materials.json`에 제공된 재료 bash/cut 저항 계수를 각각 곱하여 결정됩니다.
6. 갑옷이 2가지 재료 타입으로 만들어진 경우 주 재료 (`66%`)와 부 재료 (`33%`)의 가중 평균을 사용합니다.
7. 재료 저항 계수는 재료로 `PAPER`에 상대적으로 제공됩니다 (균형을 위한 미세 조정이 필요할 것 같습니다).

## iuse 함수 추가

1. 새 아이템 사용 코드를 `iuse.cpp` 및 `iuse.h`에 추가합니다.
2. 새 json_flag를 아이템에 추가합니다. `item_factory.cpp`에서 iuse 함수에 연결합니다.
3. 새 플래그를 `json_flags.md`에 문서화합니다.

## 산 저항

아이템이 산 필드에 어떻게 반응하는지 결정합니다. 아이템 산 저항은 재료의 산 저항의 가중 평균입니다 (`item::acid_resist` 참조):

- 아이템 산 저항이 0이면 매 턴 부식됩니다.
- 아이템 산 저항이 0보다 크면 매 턴 부식될 확률이 있습니다.
- 아이템 산 저항이 9보다 크면 완전히 산에 내성이 있습니다.

산 저항 값은 `materials.json`에 있으며 다음과 같이 정의됩니다:

- 0 - 산에 대한 저항 없음 (금속)
- 1 - 산에 부분적으로 저항
- 2 - 산에 매우 저항
- 3 - 완전한 산 저항

# FAQ

**Q: NPC 인벤토리에 아이템이 나타나지 않도록 하려면 어떻게 하나요?**

A: `TRADER_AVOID` 플래그를 추가합니다.

**Q: 맵 객체가 도대체 뭔가요?**

A: 관련 맵 객체는 submap, mapbuffer, map, overmap입니다. submap은 SEEXxSEEY 청크로 실제 맵 데이터를 포함합니다. 차량 및 스폰은 상대적으로 희소하기 때문에 벡터에 저장됩니다. Submap은 단일 전역 mapbuffer인 MAPBUFFER에 있습니다. Map은 현재 활성화된 플레이어 주변 영역을 캡슐화합니다. grid라는 MAPSIZE * MAPSIZE submap 포인터 배열을 포함합니다. 이것은 2D 배열이지만 (의심스러운) 인덱싱 목적으로 1D 배열에 매핑됩니다. 플레이어가 한 submap에서 다른 submap으로 이동하면 `map::shift()`가 호출됩니다. 플레이어를 중앙에 유지하기 위해 grid의 포인터를 이동합니다. grid의 선두 엣지는 `map::load()`에 의해 채워집니다. submap이 이전에 방문된 경우 MAPBUFFER에서 로드됩니다. 그렇지 않으면 해당 오버맵 사각형 타입을 기반으로 2x2 청크의 submap이 생성됩니다. overmap은 맵의 대규모 구조 (도시, 숲, 강, 도로 등의 위치 및 레이아웃)를 캡슐화합니다. 임의의 수의 overmap이 있습니다. 플레이어가 새 영역에 들어가면 추가 overmap이 생성됩니다.

**Q: NPC/몬스터와 맵의 관계는 무엇인가요? 어떻게 저장되나요, 어디에 저장되나요? 저장되나요?**

A: 모든 npc는 이제 overmap에 저장됩니다. Active npc는 현재 활성화된 npc만 포함합니다. npc 활성 npc 좌표와 overmap 좌표 사이에 차이가 있습니다. 따라서 npc를 저장할 때 변경되어야 합니다. 그렇지 않으면 npc가 잘못된 위치에 저장됩니다. 그리고 예, 저장됩니다.
