# 외부 타일셋

`data/json/external_tileset`은 UDP나 Ultica 같은 주요 타일셋에 아직 포함되지 않은 Bright Nights 전용 콘텐츠 중, `looks_like`만으로는 콘텐츠를 만족스럽게 표현할 수 없는 것에 사용합니다. `mod_tileset` 기능을 이용하여 지정한 타일셋에 원하는 새 스프라이트를 적용하거나 기존 스프라이트를 덮어씁니다.

이 방식의 주된 장점은 스프라이트를 별도로 관리하여 타일셋 자체의 업데이트와 충돌하지 않게 하는 것입니다. 타일셋 원본은 각 타일셋 제작자의 저장소에 있으므로, BN에서 스프라이트를 수정한 뒤 타일셋 업데이트를 반영하더라도 BN 전용 콘텐츠가 실수로 지워지지 않습니다. 또한 콘텐츠가 서로 달라져 별도의 스프라이트가 더 적합한 경우도 처리할 수 있습니다.

아래에서 다루는 콘텐츠와 관련된 풀 리퀘스트 링크입니다.

- DDA에서 Patchwork Skin 이식: [#1298](https://github.com/cataclysmbn/Cataclysm-BN/pull/1298)
- 탄환 애니메이션: [#1861](https://github.com/cataclysmbn/Cataclysm-BN/pull/1681)
- 증기 터빈: [#2815](https://github.com/cataclysmbn/Cataclysm-BN/pull/2815)
- 나무 방패, 진압 방패, 방탄 방패:
  [#2851](https://github.com/cataclysmbn/Cataclysm-BN/pull/2851)
- 납 슬링 탄환: [#2709](https://github.com/cataclysmbn/Cataclysm-BN/pull/2709)
- 가죽 방패와 띠 두른 방패: [#2856](https://github.com/cataclysmbn/Cataclysm-BN/pull/2856)
- 블루베리 덤불, 딸기 덤불, 벚나무 수정:
  [#2861](https://github.com/cataclysmbn/Cataclysm-BN/pull/2861)
- 폐기 상태였던 나무 대궁 복원, 컴파운드/컴포지트 대궁 설정 변경:
  [#2862](https://github.com/cataclysmbn/Cataclysm-BN/pull/2862)
- 카타르와 사이: [#2715](https://github.com/cataclysmbn/Cataclysm-BN/pull/2715)
- 다용도 조명 끄기 기능:
  [#1003](https://github.com/cataclysmbn/Cataclysm-BN/pull/1003)
- 미고 신경 다발: [#1962](https://github.com/cataclysmbn/Cataclysm-BN/pull/1962)
- 비늘곰: [#1371](https://github.com/cataclysmbn/Cataclysm-BN/pull/1371)
- 버클러와 용접 방패: [#2878](https://github.com/cataclysmbn/Cataclysm-BN/pull/2878)
- 전투 가면과 청동 팔 보호대:
  [#3221](https://github.com/cataclysmbn/Cataclysm-BN/pull/3221)
- 재배선된 가로등: [#3273](https://github.com/cataclysmbn/Cataclysm-BN/pull/3273)
- 귀/꼬리 돌연변이 대체 스프라이트: [#3340](https://github.com/cataclysmbn/Cataclysm-BN/pull/3340)
- 늪철로 만든 철광석: [#3506](https://github.com/cataclysmbn/Cataclysm-BN/pull/3506)
- 새 나무: [#3626](https://github.com/cataclysmbn/Cataclysm-BN/pull/3626)
- 표지판 대체 스프라이트: [#3670](https://github.com/cataclysmbn/Cataclysm-BN/pull/3670)
- M1874 개틀링 건: [#3815](https://github.com/cataclysmbn/Cataclysm-BN/pull/3815)
- 새 함정: [#3939](https://github.com/cataclysmbn/Cataclysm-BN/pull/3939)
- 새 몬스터: [#4182](https://github.com/cataclysmbn/Cataclysm-BN/pull/4182)
- 가구 형태의 다용도 조명:
  [#4780](https://github.com/cataclysmbn/Cataclysm-BN/pull/4780)
- 쓰러진 강철 표적:
  [#5361](https://github.com/cataclysmbn/Cataclysm-BN/pull/5361)
- 깃대: [#5363](https://github.com/cataclysmbn/Cataclysm-BN/pull/5363)
- 차량 장착 깃발: [#5372](https://github.com/cataclysmbn/Cataclysm-BN/pull/5372)
- 해적기: [#5375](https://github.com/cataclysmbn/Cataclysm-BN/pull/5375)
- 급조 대포와 산탄:
  [#5398](https://github.com/cataclysmbn/Cataclysm-BN/pull/5398)
- 수확한 부들: [#5445](https://github.com/cataclysmbn/Cataclysm-BN/pull/5445)
- 초석층: [#5446](https://github.com/cataclysmbn/Cataclysm-BN/pull/5446)
- 비늘가죽 갑옷, 이빨과 뼈 무기:
  [#5466](https://github.com/cataclysmbn/Cataclysm-BN/pull/5466)
- 뼈 갑옷 재구현: [#5646](https://github.com/cataclysmbn/Cataclysm-BN/pull/5646)
- 추가 이빨·뼈 무기와 트리피드 무기:
  [#5712](https://github.com/cataclysmbn/Cataclysm-BN/pull/5712)
- DDA에서 나무 꼭대기 이식: [#5167](https://github.com/cataclysmbn/Cataclysm-BN/pull/5167)
- DDA에서 스케이트보드 이식: [#5849](hhttps://github.com/cataclysmbn/Cataclysm-BN/pull/5849)
- 문 용접 봉쇄 기능: [#6182](https://github.com/cataclysmbn/Cataclysm-BN/pull/6182)
- 정찰 바이저: [#6687](https://github.com/cataclysmbn/Cataclysm-BN/pull/6687)
- DDA에서 해골 리치와 해골 마스터 이식: [#6831](https://github.com/cataclysmbn/Cataclysm-BN/pull/6831)
- 새 엘리트 좀비: [#6854](https://github.com/cataclysmbn/Cataclysm-BN/pull/6854)
- 엄폐용 탁자/벤치 뒤집기: [#6857](https://github.com/cataclysmbn/Cataclysm-BN/pull/6857)
- 화살 구멍/요새화: [#6864](https://github.com/cataclysmbn/Cataclysm-BN/pull/6864)
- 어도비 바닥: [#6864](https://github.com/cataclysmbn/Cataclysm-BN/pull/6885)
- 호수 바닥 콘텐츠: [#6903](https://github.com/cataclysmbn/Cataclysm-BN/pull/6903)

## Undead People

다음은 이 폴더가 UDP 타일셋에 추가하는 현재 스프라이트 목록으로, 각 스프라이트가 들어 있는 파일과 용도를 설명합니다. 현재 `external_tileset`이 지원하는 타일셋은 이것뿐이지만, 향후 Ultica용 스프라이트도 추가할 예정입니다.

### External_Tileset_DP_Normal.png

- 고급 증기 엔진으로 분해할 수 있는 가구 형태의 증기 터빈. BN 전용 콘텐츠입니다.
- 비행 중인 탄환의 애니메이션 효과. BN 전용 기능입니다.
- 착용 및 손에 든 상태를 포함한 나무 방패. BN 전용 아이템입니다.
- 착용 및 손에 든 상태를 포함한 진압 방패. BN 전용 아이템입니다.
- 착용 및 손에 든 상태를 포함한 방탄 방패. BN 전용 아이템입니다.
- 납 슬링 탄환. BN 전용 아이템이며 `rock`을 `looks_like`로 사용하기에는 적합하지 않았습니다.
- 착용 및 손에 든 상태를 포함한 가죽 보강 방패. BN 전용 아이템입니다.
- 착용 및 손에 든 상태를 포함한 대형 가죽 보강 방패. BN 전용 아이템입니다.
- 착용 및 손에 든 상태를 포함한 띠 두른 방패. BN 전용 아이템입니다.
- 착용 및 손에 든 상태를 포함한 대형 띠 두른 방패. BN 전용 아이템입니다.
- UDP 버전에 스프라이트 오류가 있었던 `woodgreatbow`의 착용 스프라이트 수정.
  `참고: 원본 저장소에 수정이 반영되었으므로 BN의 UDP가 업데이트되면 제거할 수 있습니다.`
- BN의 `compgreatbow`는 컴파운드 보우가 아니라 컴포지트 보우를 본떴으므로 기존 스프라이트를 덮어씁니다.
- DDA의 컴파운드 대궁은 착용할 수 없어 UDP에 없던 `compgreatbow` 착용 스프라이트. 어차피 컴포지트 보우처럼 보이도록 수정해야 했을 것입니다.
- 손에 든 상태를 포함한 카타르. BN 전용 아이템입니다.
- 통신 시설이 아니라 무기인 사이와 손에 든 상태 스프라이트. BN 전용 아이템입니다.
- 착용 및 손에 든 상태를 포함한 용접 방패. BN 전용 아이템입니다.
- 착용 및 손에 든 상태를 포함한 버클러. BN 전용 아이템입니다.
- 착용 스프라이트를 포함한 철제·청동 전투 가면. BN 전용 아이템입니다.
- 착용 스프라이트를 포함한 청동 팔 보호대. BN 전용 아이템입니다.
- 카카오 꼬투리. BN 전용 아이템입니다.
- DDA 전용 앞면 글자를 제거한 표지판 덮어쓰기 스프라이트.
- 급조 경계 경보기. BN 전용 함정입니다.
- 나비인간. BN 전용 새 몬스터입니다.
- 가구 형태의 다용도 조명. BN 전용 가구입니다.
- 쓰러진 강철 표적. BN 전용 가구입니다.
- 아이템 및 착용 스프라이트가 있는 졸리 로저. BN 전용 아이템입니다.
- 아이템 및 차량 부품 스프라이트가 있는 급조 대포. BN 전용 아이템입니다.
- 폭발성 포탄과 발사 준비된 대포 탄약 스프라이트. BN 전용 아이템입니다.
- 겨울 변형을 포함한 수확된 부들. BN 전용 가구입니다.
- 파충류 비늘로 만든 갑옷과 무거운 뼈, 이빨, 곤충 독침으로 만든 무기. BN 전용 아이템입니다.
- 뼈 흉갑과 정강이받이 스프라이트. 이전에 폐기된 뼈 갑옷을 BN 전용으로 재구현했습니다.
- 트리피드 독침 무기를 포함한 추가 이빨·독침 무기. BN 전용 아이템입니다.
- 정찰 바이저. BN 전용 아이템입니다.
- 시체 조립자, 좀비 보급 하사관, 밴시, 폭풍인도자, 장막 직조자, 충격 소용돌이. BN 전용 몬스터입니다.
- 뒤집힌 벤치와 탁자 변형. BN 전용 가구입니다.
- DDA에서 이식했지만 적절한 스프라이트가 없었던 철광석.
- 손에 든 스프라이트와 차량 포탑을 포함한 M1874 개틀링 건. BN 전용 아이템입니다.
- DDA에서 이식했지만 BN의 UDP 버전에 스프라이트가 없었던 Patchwork Skin.
- 아이템 및 차량 부품 스프라이트가 있는 스케이트보드. UDP에 스프라이트가 없는 DDA 이식 콘텐츠입니다.
- 아이템 및 차량 부품 스프라이트가 있는 수중 스쿠터. BN 전용 아이템입니다.
- UDP에 스프라이트가 없는 DDA 이식 콘텐츠인 해골 리치와 해골 마스터.
- AR-10.

### External_Tileset_DP_terrain_normal.png

- 논. BN 전용 지형입니다.
- 초석층. BN 전용 지형입니다.
- BN에서 수확철이 변경되었으므로 블루베리 덤불의 봄·여름 스프라이트를 서로 바꿉니다. 원래 여름 스프라이트에는 어린 열매가 묘사되지 않았습니다.
- 들여다보는 구멍이 있는 문, 격벽문, 연구소 문, 창살문을 포함한 용접 봉쇄 금속문. BN 전용 지형입니다.
- 나무, 통나무, 돌, 벽돌 벽에 파낸 요새화 구조. BN 전용 지형입니다.
- 어도비 바닥. BN 전용 지형입니다.
- 호수 바닥 표면의 이끼와 나무줄기. BN 전용 지형입니다.

### External_Tileset_DP_Tall.png

- 꺼진 다용도 조명. 켜고 끄는 기능은 BN 전용입니다.
- BN의 미고 장소에 추가된 가구인 외계인 신경 다발.
  `참고: 이 스프라이트는 원본 저장소에 반영되었으므로 BN의 UDP가 업데이트되면 제거할 수 있습니다.`
- 시체를 포함한 비늘곰. BN 전용 몬스터입니다.
- BN에서 수확철이 변경되었으므로 벚나무에 여름 스프라이트와 벚꽃 색상을 사용합니다. 여름 스프라이트에는 열매가 묘사되지 않았습니다.
- 활성 상태를 포함한 재배선된 가로등. BN 전용 가구입니다.
- 카카오나무. BN 전용 지형입니다.
- 코카나무. BN 전용 지형입니다.
- 이제 분해할 수 있는 다용도 조명을 바탕으로 BN에 추가한 격자 투광등 가구.
- 깃발을 올린 상태를 포함한 금속 및 나무 깃대. BN에 추가된 가구입니다.
- 차량 부품으로 표시되는 성조기. BN 전용 콘텐츠입니다.
- 졸리 로저용 깃대 가구와 차량 부품. BN 전용 콘텐츠입니다.
- DDA에서 이식했지만 현재 UDP에 스프라이트가 없는 나무 꼭대기 지형.

### alternative_mutation_tileset.png

<details><summary>변경 전</summary>

![](./img/alternative_external_mutation_before.png)

</details>

<details><summary>변경 후</summary>

![](./img/alternative_external_mutation_after.png)

</details>

다음 돌연변이의 대체 스프라이트를 포함합니다.

- `FELINE_EARS`
- `LUPINE_EARS`
- `MOUSE_EARS`
- `CANINE_EARS`
- `TAIL_FLUFFY`
- `TAIL_STUB`
