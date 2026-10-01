# 파일 설명

다음은 폴더별 JSON 파일의 내용을 간단히 정리한 목록입니다. 모든 내용을 다루지는 않지만 큰 범위는 설명합니다.

## `data/json/`

| 파일명                     | 설명                                            |
| -------------------------- | ----------------------------------------------- |
| achievements.json          | 업적                                            |
| anatomy.json               | 플레이어 신체 부위 목록(편집하지 말 것)         |
| ascii_arts.json            | 아이템 설명에 사용하는 ASCII 아트               |
| bionics.json               | 바이오닉(바이오닉 효과는 포함하지 않음)         |
| body_parts.json            | anatomy.json의 확장(편집하지 말 것)             |
| clothing_mods.json         | 의류 개조 정의                                  |
| construction.json          | 건설 메뉴 작업 정의                             |
| default_blacklist.json     | 농담성 몬스터의 기본 블랙리스트                 |
| doll_speech.json           | 말하는 인형의 대사                              |
| dreams.json                | 꿈 문구와 연결된 변이 범주                      |
| disease.json               | 질병 정의                                       |
| effects.json               | 공통 효과와 효과 설명                           |
| emit.json                  | 연기와 기체 방출                                |
| flags.json                 | 공통 플래그와 설명                              |
| furniture.json             | 가구 및 가구처럼 취급되는 기능                  |
| game_balance.json          | 게임 밸런스를 조정하는 여러 옵션                |
| gates.json                 | 문 지형 정의                                    |
| harvest.json               | 시체 도축 시 나오는 아이템                      |
| health_msgs.json           | 플레이어가 깨어날 때 표시되는 메시지            |
| item_actions.json          | 표준 아이템 행동 설명                           |
| item_category.json         | 아이템 범주와 기본 정렬 순서                    |
| item_groups.json           | 아이템 생성 그룹                                |
| lab_notes.json             | 연구소 컴퓨터 메시지                            |
| martialarts.json           | 무술 스타일과 버프                              |
| materials.json             | 재질 유형                                       |
| monster_attacks.json       | 몬스터 공격                                     |
| monster_drops.json         | 몬스터 사망 시 아이템 드롭                      |
| monster_factions.json      | 몬스터 세력                                     |
| monstergroups.json         | 몬스터 생성 그룹                                |
| monstergroups_egg.json     | 알에서 나오는 몬스터 생성 그룹                  |
| monsters.json              | 몬스터 설명(주로 좀비)                          |
| morale_types.json          | 사기 수정 메시지                                |
| mutation_category.json     | 변이 범주 메시지                                |
| mutation_ordering.json     | 타일 모드에서 변이와 CBM 오버레이를 그리는 순서 |
| mutations.json             | 특성/변이                                       |
| names.json                 | NPC/플레이어 이름 생성에 사용하는 이름          |
| overmap_connections.json   | 오버맵의 도로와 터널 연결                       |
| overmap_terrain.json       | 오버맵 지형                                     |
| player_activities.json     | 플레이어 활동                                   |
| professions.json           | 직업 정의                                       |
| recipes.json               | 제작/분해 레시피                                |
| regional_map_settings.json | 전체 맵 생성 설정                               |
| road_vehicles.json         | 도로 차량 생성 정보                             |
| rotatable_symbols.json     | 회전 가능한 기호(편집하지 말 것)                |
| scent_types.json           | 사용할 수 있는 냄새 유형                        |
| scores.json                | 점수                                            |
| skills.json                | 스킬 설명과 ID                                  |
| snippets.json              | 전단지/포스터 설명                              |
| species.json               | 몬스터 종                                       |
| speech.json                | 몬스터 발성                                     |
| statistics.json            | 점수와 업적을 정의하는 통계 및 변환             |
| start_locations.json       | 시나리오 시작 위치                              |
| techniques.json            | 아이템과 무술에 공통으로 사용하는 기술          |
| terrain.json               | 지형 유형과 정의                                |
| test_regions.json          | 테스트 지역                                     |
| tips.json                  | 오늘의 팁                                       |
| tool_qualities.json        | 표준 도구 품질과 행동                           |
| traps.json                 | 표준 함정                                       |
| tutorial.json              | 튜토리얼 메시지(오래됨)                         |
| vehicle_groups.json        | 차량 생성 그룹                                  |
| vehicle_parts.json         | 차량 부품(플래그 효과에는 영향을 주지 않음)     |
| vitamin.json               | 비타민과 결핍                                   |

선택한 하위 폴더

## `data/json/items/`

아이템 파일의 자세한 내용은 다음과 같습니다.

| 파일명                       | 설명                                                    |
| ---------------------------- | ------------------------------------------------------- |
| ammo.json                    | 배터리와 구슬 같은 공통 기본 구성요소                   |
| ammo_types.json              | 총별 표준 탄약 유형                                     |
| archery.json                 | 활과 화살                                               |
| armor.json                   | 갑옷과 의류                                             |
| bionics.json                 | 소형 바이오닉 모듈(CBM)                                 |
| biosignatures.json           | 동물 배설물                                             |
| books.json                   | 책                                                      |
| chemicals_and_resources.json | 화학 전구체                                             |
| comestibles.json             | 음식과 음료                                             |
| containers.json              | 용기                                                    |
| crossbows.json               | 석궁과 볼트                                             |
| fake.json                    | 바이오닉이나 변이에 사용하는 가짜 아이템                |
| fuel.json                    | 액체 연료                                               |
| grenades.json                | 수류탄과 투척 폭발물                                    |
| handloaded_bullets.json      | 무작위 탄약                                             |
| melee.json                   | 다른 아이템 JSON에 속하지 않는 아이템과 근접 무기       |
| migration.json               | 저장 게임의 존재하지 않는 아이템을 현재 아이템으로 변환 |
| newspaper.json               | 전단지, 신문, 생존자 기록(snippets.json은 메시지용)     |
| obsolete.json                | 게임에서 제거 중인 아이템                               |
| ranged.json                  | 총기                                                    |
| software.json                | SD 카드와 USB 스틱용 소프트웨어                         |
| tool_armor.json              | 활성화할 수 있는 옷과 갑옷                              |
| toolmod.json                 | 도구 개조                                               |
| tools.json                   | 활성화할 수 있는 도구와 아이템                          |
| vehicle_parts.json           | 차량에 장착되지 않은 차량 구성요소                      |

### `data/json/items/comestibles`

## `data/json/requirements/`

제작에 사용하는 표준 부품과 도구입니다.

| 파일명                    | 설명                       |
| ------------------------- | -------------------------- |
| ammo.json                 | 탄약 구성요소              |
| cooking_components.json   | 공통 재료 묶음             |
| cooking_requirements.json | 조리 도구와 열원           |
| materials.json            | 실, 천 및 기타 기본 재료   |
| toolsets.json             | 함께 사용하는 도구 세트    |
| uncraft.json              | 분해 시 나오는 공통 결과물 |
| vehicle.json              | 차량 작업용 도구           |

## `data/json/vehicles/`

차량 정의 그룹입니다. 파일명만 봐도 용도를 알 수 있습니다.

| 파일명               |
| -------------------- |
| bikes.json           |
| boats.json           |
| cars.json            |
| carts.json           |
| custom_vehicles.json |
| emergency.json       |
| farm.json            |
| helicopters.json     |
| military.json        |
| trains.json          |
| trucks.json          |
| utility.json         |
| vans_busses.json     |
| vehicles.json        |
