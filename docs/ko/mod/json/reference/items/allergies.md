# 알레르기

## 기본 정보

알레르기는 유전 가능성을 구현하고 여러 아이템 변형을 만들거나 식품 재료에 대해 어려운 결정을 내릴 필요를 줄이기 위해 비타민 시스템을 기반으로 합니다.

초기 조치로 회귀를 막고 비타민을 일일이 추가할 필요가 없도록 재료에 따라 이 비타민을 적용했습니다. 그러나 아이템에 `NUTRIENT_OVERRIDE`가 없다면 요리는 실제로 들어간 재료를 정확히 반영해야 합니다. 자연적으로 생성되는 아이템은 재료가 나타내는 알레르겐을 갖습니다.

식품에 채식 알레르겐이 있으면 육식동물은 먹을 수 없습니다. 반대로 초식동물은 고기 알레르겐이 있는 식품을 먹을 수 없습니다.

## 현재 알레르겐

- `egg_allergen`
- `fruit_allergen`
- `human_flesh_vitamin` (특수한 경우로, 식인과 관련된 사기 보너스/불이익에 사용됩니다.)
- `junk_allergen`
- `meat_allergen`
- `milk_allergen`
- `nut_allergen`
- `veggy_allergen`
- `wheat_allergen` (실제로는 일반적인 빵/구운 음식 알레르겐입니다.)

## 현재 식이 제한

완전 금지:

- 육식동물: `veggy_allergen`, `fruit_allergen`, `wheat_allergen`, `nut_allergen`
- 초식동물/반추동물: `meat_allergen`, `egg_allergen`

싫어함:

- 채식주의자
- 반식물성
- 유당불내증
- 과일 불내증
- 정크푸드 불내증
- 곡물 불내증
