# JSON Flags

## Notes

- 일부 플래그(아이템, 효과, 차량 부품)는 `flags.json` 또는 `vp_flags.json`에 정의되어야 합니다.
  (유형: `json_flag`)이 올바르게 작동합니다.
- 하나의 카테고리 또는 항목 유형을 위한 많은 플래그가 다른 카테고리 또는 항목에 사용될 수 있습니다.
  유형. else 플래그를 사용할 수 있는 곳을 실험해 보세요.
- 공격 및 방어 깃발은 착용할 수 있는 모든 아이템 유형에 사용할 수 있습니다.

## Inheritance

아이템을 제작할 때 해당 아이템을 제작하는 데 사용된 구성 요소로부터 플래그를 상속받을 수 있습니다. 이것
상속할 플래그에 `"craft_inherit": true` 항목이 있어야 합니다. 당신이 원하지 않는 경우
특정 항목을 제작할 때 플래그를 상속하려면 다음의 배열인 delete_flags 멤버를 지정하세요.
문자열. 여기에 지정된 플래그는 제작 시 결과 아이템에서 제거됩니다. 이것은
플래그 상속을 재정의하지만 항목 유형 자체의 일부인 플래그는 삭제하지 않습니다.

## TODO

- `Monsters` 아래의 `Special attacks`에 대한 설명이 더 정확할 수 있습니다.
  공격이 무엇을 하는지.
- `Ammo` 아래의 `Ammo effects`에는 더 자세한 설명이 필요하며 일부는 다시 확인해야 합니다.
  정확성.

## Ammo

### Ammo type

이는 `ammo_types.json`을 통해 처리됩니다. 무기에 태그를 달아 챔버에 넣을 수 있습니다.
기존 탄약을 보유하거나 그곳에서 자신만의 탄약을 만드세요. 이 목록의 첫 번째 열은 태그의 "id"입니다.
내부 식별자 BN은 태그를 추적하는 데 사용하며 두 번째는 탄약에 대한 간략한 설명입니다.
태그됨. ID는 BN 코드 전체에서 일정하므로 탄약 목록을 검색하려면 ID를 사용하십시오. 행복하다
챔버링! :-)

- `120mm` 120mm 열
- `12mm` 12mm
- `20x66mm` 20x66mm 샷(및 친척)
- `223` .223 레밍턴(및 5.56 NATO)
- `22` .22LR(및 친척)
- `3006` 30.06
- `300` .300 윈마그
- `308` .308 윈체스터(및 친척)
- `32` .32 ACP
- `36paper` .36 캡 & 볼
- `38` .38 스페셜
- `40` 10mm
- `40mm` 40mm 수류탄
- `44` .44 매그넘
- `44paper` .44 캡 & 볼
- `45` .45 ACP(및 친척)
- `50` .50 BMG
- `5x50` 5x50 다트
- `66mm` 66mm 히트
- `762R` 7.62x54mm
- `762` 7.62x39mm
- `84x246mm` 84x246mm HE
- `8x40mm` 8mm 케이스 없음
- `9mm` 9x19mm 루거(및 친척)
- `BB`BB
- `RPG-7` RPG-7
- `UPS` UPS 요금
- `ammo_flintlock` 플린트록 탄약
- `ampoule` 앰플
- `arrow` 화살
- `battery` 배터리
- `blunderbuss` 블런더버스
- `bolt` 볼트
- `charcoal` 숯
- `components` 구성품
- `dart` 다트
- `diesel` 디젤
- `fish_bait` 물고기 미끼
- `fishspear` 작살총 창
- `fusion` 레이저 팩
- `gasoline` 휘발유
- `homebrew_rocket` 자가제 로켓
- `lamp_oil` 등유
- `laser_capacitor` 충전
- `m235` M235 TPA (66mm 소이 로켓)
- `metal_rail` 철근 레일
- `money` 센트
- `muscle` 근육
- `nail` 네일
- `pebble` 페블
- `plasma` 플라즈마
- `plutonium` 플루토늄 셀
- `rebreather_filter` 재호흡기 필터
- `shot` 쇼셸
- `signal_flare` 신호탄
- `tape` 덕테이프
- `thread` 스레드
- `thrown` 던져짐
- `unfinished_char` 반탄 연료
- `water` 물

다음 탄약 유형은 저장소 내 모드에서 사용할 수 있거나 곧 사용할 수 있습니다.
`exotic_ammo`:

- `454` .454 카술
- `500` .500 매그넘
- `57` 5.7mm
- `46` 4.6mm
- `762x25` 7.62x25mm (토카레프)
- `9x18` 9x18mm
- `700nx` .700 니트로 익스프레스
- `38super` .38슈퍼
- `460` .460 로랜드
- `545x39` 5.45x39mm
- `270win` .270 윈체스터

### Effects

- `ACIDBOMB` 폭발 시 산성 웅덩이를 남깁니다.
- `BEANBAG` 대상을 기절시킵니다.
- `BLACKPOWDER` 흑색 가루로 인해 총이 막힐 수 있으며 이로 인해 녹이 발생할 수도 있습니다.
- `BLINDS_EYES` 머리에 맞으면 눈이 멀게 됩니다. (원거리 투사체는 실제로 머리에 맞을 수 없습니다.)
  지금의 눈).
- `BOUNCE` `bounced` 효과로 대상을 공격하고 이 효과 없이 근처 대상에게 반동합니다.
  효과.
- `COOKOFF` 불이 붙으면 폭발합니다.
- `CUSTOM_EXPLOSION` 사용된 탄약의 `"explosion"` 필드에 지정된 폭발입니다. `JSON_INFO.md`을 참조하세요.
- `DRAW_AS_LINE` 일반적인 총알 애니메이션을 거치지 않고 대신 선을 그어 총알을 붙입니다.
  한 프레임 동안 끝납니다.
- `EXPLOSIVE_BIG` 파편 하나 없이 큰 폭발.
- `EXPLOSIVE_HUGE` 파편 하나 없이 엄청난 폭발이 일어납니다.
- `EXPLOSIVE` 파편 없이 폭발합니다.
- `FLAME` 불이 붙는 아주 작은 폭발.
- `FLARE` 대상에게 불을 붙입니다.
- `FLASHBANG` 근처 대상의 눈을 멀게 하고 귀를 멀게 합니다.
- `FRAG` 파편이 퍼지는 작은 폭발.
- `INCENDIARY` 대상에 불을 붙입니다.
- `LARGE_BEANBAG` 대상을 강하게 기절시킵니다.
- `LASER` 레이저의 흔적을 생성합니다(필드형).
- `LIGHTNING` 번개의 흔적을 생성합니다.
- `MININUKE_MOD` 방사성 낙진을 남기는 작은 열핵 폭발입니다.
- `MUZZLE_SMOKE` 소스에 작은 연기 구름을 생성합니다.
- `NAPALM` 불을 퍼뜨리는 폭발.
- `NEVER_MISFIRES` 이 플래그 없이 탄약을 발사하면 오발이 발생할 수 있습니다. 이는
  무기 플래그.
- `NOGIB` 대상에 대한 과도한 피해를 방지합니다(대상은 폭발하여 토막이 나지 않습니다.
  몬스터 플래그 NO_GIBS).
- `NO_PENETRATE_OBSTACLES` 발사체가 장애물이 있는 타일을 통과하는 것을 방지합니다.
  체인 링크 울타리 또는 옷장.
- `TANGLE` 이 발사체가 목표물에 닿으면 일정 확률로 목표물을 뒤엉키고 움직이지 못하게 만듭니다.
  그들을.
- `NO_EMBED` 아이템이 발사체에서 생성되면 항상 발사체에서 생성됩니다.
  몬스터의 인벤토리가 아닌 지상에 있습니다. 활성 상태로 던진 아이템에 대해 암시됩니다. 아무것도 하지 않습니다
  아이템을 떨어뜨리지 않는 발사체.
- `NO_ITEM_DAMAGE` 지도에 있는 항목을 손상시키려고 해도 손상시키지 않습니다.
- `PLASMA` 과열 플라즈마의 흔적을 생성합니다.
- `RECOVER_[X]` 사용된 탄약을 1회 충전할 수 있는 (X-1/X) 확률이 있습니다.
  영향.
- `RECYCLED` (수제 탄약의 경우) 무기와 관계없이 총이 가끔 불발되는 경우가 있습니다.
  플래그.
- `SHOT` 여러 개의 작은 알갱이; 갑옷에 대해서는 덜 효과적이지만 명중 확률이 증가하고
  빈칸 페널티
- `SMOKE_BIG` 대상에게 큰 연기 구름을 생성합니다.
- `SMOKE` 대상에게 연기구름을 생성합니다.
- `STREAM_BIG` 강렬한 불길의 흔적을 남깁니다.
- `STREAM` 불길의 흔적을 남깁니다.
- `TRAIL` 연기의 흔적을 만듭니다.
- `WIDE` `HARDTOSHOOT` 몬스터 플래그가 효과를 발휘하지 못하도록 방지합니다. `SHOT` 또는 liquid로 암시됨
  탄약.

## Armor

### Covers

- `ARMS` ... 동일 `ARM_L` 및 `ARM_R`
- `ARM_L`
- `ARM_R`
- `EYES`
- `FEET` ... 동일 `FOOT_L` 및 `FOOT_R`
- `FOOT_L`
- `FOOT_R`
- `HANDS` ... 동일 `HAND_L` 및 `HAND_R`
- `HAND_L`
- `HAND_R`
- `HEAD`
- `LEGS` ... 동일 `LEG_L` 및 `LEG_R`
- `LEG_L`
- `LEG_R`
- `MOUTH`
- `TORSO`

### Flags

`WATCH` 및 `ALARMCLOCK`과 같은 일부 방어구 플래그는 다른 아이템 유형과 호환됩니다. 실험
어떤 플래그가 다른 곳에서 작동하는지 찾으려면

- `ACTIVE_CLOAKING` 활성화된 동안 UPS를 소모하여 보이지 않게 합니다.
- `ALARMCLOCK` 알람시계 기능이 있습니다.
- `ALLOWS_NATURAL_ATTACKS` 자연적인 공격이나 돌연변이로 인한 유사한 이점을 방지하지 않습니다.
  손가락 끝 면도기 등 관련 신체 부위를 덮는 대부분의 품목과 같습니다.
- `AURA` 형이상학적 효과를 목적으로 하는 외부 오라층에 들어가는 아이템입니다.
- `BAROMETER` 이 장비에는 정확한 기압계(대기를 측정하는 데 사용되는 장치)가 장착되어 있습니다.
  압력).
- `BELTED` 배낭이나 겉옷 위에 입는 물건을 위한 레이어입니다.
- `BLIND` 착용하는 동안 착용자의 눈을 멀게 하며 섬광탄 대비 공칭 보호 기능을 제공합니다.
- `BLOCK_WHILE_WORN` 착용한 갑옷이나 방패를 사용하여 공격을 차단할 수 있습니다. 또한 참조하십시오
  `Techniques` 섹션.
- `BULLET_IMMNUE` 이 깃발이 달린 아이템을 착용하면 총탄 피해에 면역이 됩니다.
- `CLIMATE_CONTROL` 이 옷은 일종의 온도 조절 기능이 있어 몸을 따뜻하게 유지하거나
  주변 온도와 체온에 따라 더 시원해집니다.
- `COLLAR` 칼라가 넓어 입을 따뜻하게 해줄 수 있는 옷입니다.
- `DEAF` 플레이어의 청각 장애를 유발합니다.
- `ELECTRIC_IMMUNE` 이 장비는 방전으로부터 당신을 완벽하게 보호해 줍니다.
- `FANCY` 이 옷을 입으면 플레이어에게 사기 보너스가 주어집니다.
  `Fashion Deficient` 특성.
- `FIX_FARSIGHT` 원시를 교정해주는 장비입니다.
- `FIX_NEARSIGHT` 근시를 교정해주는 장비입니다.
- `FLOTATION` 플레이어가 깊은 물에 빠지는 것을 방지합니다. 또한 수중 다이빙을 방지합니다.
- `FRAGILE` 이 장비는 일반 장비보다 손상에 대한 저항력이 약합니다.
- `HELMET_COMPAT` SKINTIGHT나 OVERSIZE는 아니지만 헬멧과 함께 착용 가능한 아이템.
- `HOOD` 추가적인 보온이나 물 공급을 위해 이 옷이 머리를 조건부로 덮도록 허용합니다.
  보호. 플레이어의 머리가 방해받지 않는 경우
- `HYGROMETER` 이 장비에는 정확한 습도계가 장착되어 있습니다.
  습기).
- `NO_TAKEOFF` 해당 플래그가 붙은 아이템은 떼어낼 수 없습니다.
- `NO_QUICKDRAW` 발사 키를 누른 상태에서 이 권총집에서 아이템을 꺼낼 것을 제안하지 마십시오.
  플레이어의 손이 비어 있습니다
- `ONLY_ONE` 하나만 착용하셔도 됩니다.
- `OUTER` 겉옷층.
- `OVERSIZE` 부담/돌연변이/생체공학/등에 관계없이 항상 착용할 수 있지만 다른 것을 방지합니다.
  그 위에 입는 옷.
- `PARTIAL_DEAF` 소리의 볼륨을 안전한 수준으로 줄입니다.
- `PERSONAL` 개인 아우라 레이어에 들어가는 형이상학적인 효과를 의도한 아이템입니다.
- `POCKETS` 플레이어의 손이 차갑고 플레이어가 휘두르는 경우 손의 따뜻함이 증가합니다.
  아무것도 아님.
- `POWERARMOR_EXO` 아이템을 파워 아머의 주요 외골격으로 표시합니다.
- `POWERARMOR_EXTERNAL` 신체 부위와 외골격을 덮는 외부 부품으로 항목을 표시합니다.
  그렇지 않습니다.
- `POWERARMOR_MOD` 아이템을 외골격/외부 장치에 착용하는 파워 아머 모드로 표시합니다.
  조각.
- `POWERARMOR_COMPATIBLE` 다른 매개변수에도 불구하고 아이템이 파워 아머와 호환되도록 만듭니다.
  실패.
- `PSYSHIELD_PARTIAL` 착용시 25% 확률로 Fear_paralyze 몬스터의 공격으로부터 보호됩니다.
- `RAD_PROOF` 이 옷은 방사선으로부터 당신을 완벽하게 보호합니다.
- `RAD_RESIST` 이 옷은 방사선으로부터 부분적으로 보호해 줍니다.
- `RAINPROOF` 비에 덮힌 신체 부위가 젖는 것을 방지합니다.
- `REQUIRES_BALANCE` 안정감이 있기 위해서는 어느 정도의 밸런스가 필요한 장비입니다. 플레이어가 타격을 받은 경우
  착용하는 동안 쓰러질 가능성이 있습니다.
- `RESTRICT_HANDS` 플레이어가 무기를 양손으로 휘두르는 것을 방지하고, 다음과 같은 경우 한손으로 사용하도록 강제합니다.
  무기가 그것을 허용합니다.
- `ROLLER_ONE` ROLLER_QUAD의 덜 안정적이고 느린 버전이지만 여전히 플레이어가 움직일 수 있습니다.
  걷는 속도보다 빠르다.
- `ROLLER_QUAD`ROLLER_INLINE과 ROLLER_ONE 사이의 중간 선택이지만 더 안정적이며
  더 빠르게 움직이며 ROLLER_ONE보다 평평하지 않은 지형 페널티가 더 가혹합니다.
- `ROLLER_INLINE` 더 빠르지만 전체적으로 안정성이 떨어지므로 평평하지 않은 지형에 대한 페널티는 더욱 가혹합니다.
- `SEMITANGIBLE` 착용 시 아이템이 부담 시스템에 참여하는 것을 방지합니다.
- `COMPACT` 착용 시 아이템이 레이어링 부담 시스템에 참여하는 것을 방지합니다.
- `SKINTIGHT` 속옷층.
- `SLOWS_MOVEMENT` 이 옷은 이동 비용을 1.1배로 늘려줍니다.
- `SLOWS_THIRST` 이 옷은 플레이어의 목마름 속도를 몇 배로 증가시킵니다.
  0.70.
- `STURDY` 이 옷은 일반 옷보다 손상에 훨씬 강합니다.
- `SUN_GLASSES` 햇빛 아래 있을 때 눈부심을 방지합니다.
- `SUPER_FANCY` 플레이어가 `FANCY`에 비해 추가 도덕적 보너스를 제공합니다.
  `Fashion Deficient` 특성.
- `SWIM_GOGGLES` 물속에서 훨씬 더 멀리 볼 수 있습니다.
- `THERMOMETER` 이 장비에는 정확한 온도계(측정에 사용되는)가 장착되어 있습니다.
  온도).
- `VARSIZE` 맞춤 제작 가능합니다.
- `WAIST` 허리에 착용하는 벨트 등의 레이어입니다.
- `WATCH` 시계 역할을 하며 플레이어가 실제 시간을 볼 수 있도록 합니다.
- `WATERPROOF` 어떤 상황에서도 가려진 신체 부위가 젖는 것을 방지합니다.
- `WATER_FRIENDLY` 신체 부위가 물에 좋지 않은 것으로 간주되는 것을 방지합니다.
  젖어서 사기가 저하됩니다.
- `ALLOWS_FLIGHT` 활성화된 동안 UPS를 방전시켜 비행을 제공합니다.
- `ALWAYS_ALLOWS_FLIGHT` 항상 비행을 허용하세요.

## Bionics

- `BIONIC_ARMOR_INTERFACE` 이 생체공학은 동력 방어구에 전력을 공급할 수 있습니다.
- `BIONIC_FAULTY` 이 생체공학은 "faulty" 생체공학입니다.
- `BIONIC_GUN` 이 생체 공학은 총 생체 공학이며 활성화하면 발사됩니다. 다른 모든 것을 방지합니다.
  활성화 효과.
- `BIONIC_NPC_USABLE` NPC AI는 이 CBM의 사용법을 알고 있어 NPC에 설치할 수 있습니다.
- `BIONIC_POWER_SOURCE` 이 생체공학은 동력원 생체공학입니다.
- `BIONIC_SLEEP_FRIENDLY` 이 생체 공학은 사용자가 잠을 자려고 할 때 끄라는 메시지를 표시하지 않습니다.
  활동하는 동안.
- `BIONIC_TOGGLED` 이 생체공학은 활성화되었을 때만 기능을 갖고, 그렇지 않으면 매번 효과를 발생시킵니다.
  회전하다.
- `BIONIC_WEAPON` 이 생체공학은 무기 생체공학이며 활성화하면 생체공학을 생성(또는 파괴)합니다.
  fake_item이 사용자 손에 있습니다. 다른 모든 활성화 효과를 방지합니다.
- `BIONIC_SHOCKPROOF` 이 생체 공학은 전기 공격으로 무력화될 수 없습니다.
- `BIONIC_FLIGHT` 이 생체공학 장치를 사용하면 비행이 가능합니다.
- `MULTIINSTALL` 이 생체공학은 여러 번 설치할 수 있습니다.
- `INITIALLY_ACTIVE` 이 생체공학은 설치 시점에 활성화됩니다.
- `BIONIC_TOOLS` 이 생체공학에는 도구가 있습니다. 활성화하면 제작에 사용할 수 있는 것들을 다시 로드합니다.

## Books

- `BOOK_CANNIBAL` 이 책을 읽으면 부정적인 사기가 긍정적인 사기로 전환됩니다.
  CANNIBAL, PSYCHOPATH 또는 SAPIVORE 특성 플래그.
- `INSPIRATIONAL` 이 책을 읽으면 SPIRITUAL 특성 플래그가 있는 캐릭터에게 보너스 사기가 부여됩니다.
- `MORBID` 이 책을 읽으면 캐릭터의 부정적인 사기가 긍정적인 사기로 전환됩니다.
  PSYCHOPATH 특성 플래그.

### Use actions

- `ACIDBOMB_ACT` 없애지 않으면 로보캅에 나오는 그 남자처럼 될 것입니다.
- `ACIDBOMB` 산성폭탄의 핀을 뽑으세요.
- `AUTOCLAVE` 하나의 CBM을 고압멸균하여 멸균합니다.
- `ARROW_FLAMABLE` 화살에 불을 붙여 날아가 보세요.
- `BELL` 벨을 울려보세요.
- `BOLTCUTTERS` 타운 키를 사용하여 어디든 접근할 수 있습니다.
- `BREAK_STICK` 긴 막대를 두 개로 나눕니다.
- `C4` C4를 무장하세요.
- `CABLE_ATTACH` 케이블 스풀입니다. 차량에 부착하려고 할 때 사용합니다.
- `CAN_GOO` 작은 덩어리 친구를 풀어주세요.
- `CAPTURE_MONSTER_ACT` 몬스터를 캡쳐해서 캡슐화합니다. 관련 작업은 다음 용도로도 사용됩니다.
  그것을 공개합니다.
- `CROWBAR` 문, 창문, 맨홀 뚜껑 및 기타 비집고 들어야 할 많은 것들을 열어서 들어보세요.
- `DIG` 잔해가 깨끗해졌습니다.
- `DIRECTIONAL_ANTENNA` 라디오로 신호 소스를 찾으세요.
- `DIVE_TANK` 압축 공기 탱크를 사용하여 호흡하십시오.
- `DOG_WHISTLE` 개들은 이것을 싫어합니다. 그래도 네 개는 꽤 괜찮아 보이는데.
- `DOLLCHAT` 저 으스스한 인형이 계속 말을 하네요.
- `ELEC_CHAINSAW_OFF` 전기톱을 켜세요.
- `ELEC_CHAINSAW_ON` 전기톱을 끄세요.
- `EXTINGUISHER` 불을 끄세요.
- `FIRECRACKER_ACT` 가장 슬픈 7월 4일.
- `FIRECRACKER_PACK_ACT` 잔돈은 계속 가져가라 더러운 동물아.
- `FIRECRACKER_PACK` 폭죽을 통째로 켜세요.
- `FIRECRACKER` 특이한 폭죽을 켜보세요.
- `FLASHBANG` 섬광탄 핀을 뽑아보세요.
- `GEIGER` 국소 방사선 수준을 감지합니다.
- `GRANADE_ACT` 소스 코드 수정으로 적을 공격합니까?
- `GRANADE` 그라나데의 핀을 당겨보세요.
- `GRENADE` 수류탄의 핀을 뽑으세요.
- `HACKSAW` 금속을 덩어리로 자릅니다.
- `HAMMER` 창문, 문, 울타리에서 판자를 떼어냅니다.
- `HEATPACK` 히트팩을 켜고 몸을 따뜻하게 해주세요.
- `HEAT_FOOD` 불 주변에서 음식을 가열하세요.
- `HOTPLATE` 핫플레이트를 사용하세요.
- `JACKHAMMER` 벽과 기타 구조물을 무너뜨립니다.
- `JET_INJECTOR` 제트 약물을 정맥에 직접 주입하십시오.
- `LAW` 발사를 위해 LAW의 포장을 풀어주세요.
- `LIGHTSTRIP` 라이트스트립을 활성화합니다.
- `LUMBER` 통나무를 판자로 자릅니다.
- `MAKEMOUND` 흙더미를 만들어 보세요.
- `MANHACK` 맨해킹을 활성화합니다.
- `MATCHBOMB` 성냥갑을 켜세요.
- `MILITARYMAP` 지역 군사시설에 대해 알아보고 도로를 보여주세요.
- `MININUKE` 타이머를 설정하고 실행하세요. 아니면 망치로 때리세요(꼭 그렇지는 않습니다).
- `MOLOTOV_LIT` 던지고, 떨어뜨리지 마세요.
- `MOLOTOV` 화염병에 불을 붙입니다.
- `MOP` 어지러운 것을 정리하세요.
- `MP3_ON` MP3 플레이어를 꺼주세요.
- `MP3` MP3 플레이어를 켜세요.
- `NOISE_EMITTER_OFF` 노이즈 발생기를 켜세요.
- `NOISE_EMITTER_ON` 노이즈 발생기를 꺼주세요.
- `NONE` 아무것도 하지 마세요.
- `PACK_CBM` CBM을 특수 오토클레이브 파우치에 넣어 멸균 후 멸균 상태를 유지합니다.
- `PHEROMONE` 좀비가 당신을 무시하게 만듭니다.
- `PICK_LOCK` 문에 있는 자물쇠를 선택하세요. 속도와 성공 확률은 'LOCKPICK' 스킬에 따라 결정됩니다.
  아이템 품질 및 'PERFECT_LOCKPICK' 아이템 플래그
- `PICKAXE` 가지고 있다고 질책만 할 뿐이다(진지하다).
- `PLACE_RANDOMLY` 이것은 맨해킹 용도의 플래그와 매우 흡사합니다.
  플레이어에게 몬스터를 내릴 위치를 묻고 대신 무작위로 선택합니다.
- `PORTABLE_GAME` 게임을 해보세요.
- `PORTAL` 포탈 트랩을 생성합니다.
- `RADIO_OFF` 라디오를 켜세요.
- `RADIO_ON` 라디오를 꺼주세요.
- `RAG` 출혈을 멈추세요.
- `RESTAURANTMAP` 현지 식당에 대해 알아보고 길을 보여주세요.
- `ROADMAP` 지역 공통 관심 지점에 대해 알아보고 도로를 보여주세요.
- `SCISSORS` 옷을 자르세요.
- `SEED` 정말 씨앗을 먹고 싶은지 물어봅니다. 씨앗을 심는 것이 더 낫기 때문입니다.
- `SEW` 옷을 꿰매세요.
- `SHELTER` 본격적인 쉼터를 마련해 보세요.
- `SHOCKTONFA_OFF` 쇼크톤파를 켜세요.
- `SHOCKTONFA_ON` 쇼크톤파를 꺼주세요.
- `SIPHON` 차량에서 액체를 빼냅니다.
- `SMOKEBOMB_ACT` 흡연자로서 숨는 좋은 방법이 될 수도 있습니다.
- `SMOKEBOMB` 연막탄의 핀을 뽑으세요.
- `SOLARPACK_OFF` 접이식 태양열 배낭 배열.
- `SOLARPACK` 태양광 배낭 어레이를 펼칩니다.
- `SOLDER_WELD` 물품을 납땜 또는 용접하거나 상처를 소작합니다.
- `SPRAY_CAN` 마을에 낙서를 한다.
- `SURVIVORMAP` 생존에 도움이 될 수 있는 지역 관심 지점에 대해 알아보고 도로를 보여주세요.
- `TAZER` 누군가 또는 무언가에 충격을 줍니다.
- `TELEPORT` 텔레포트.
- `TOGGLE_HEATS_FOOD` 항목에 HEATS_FOOD 플래그가 없으면 해당 항목에 플래그를 부여하고 그 반대의 경우도 마찬가지입니다. 예전에는
  식사 시 해당 항목으로 음식을 자동으로 재가열하는 기능을 활성화/비활성화합니다.
- `TOGGLE_UPS_CHARGING` 항목에 USE_UPS 플래그가 없으면 해당 항목에 플래그를 부여하고 그 반대의 경우도 마찬가지입니다. 예전에는
  UPS, 고급 UPS, 통합 전력 시스템 CBM 등에서 해당 항목 충전을 활성화/비활성화합니다.
- `TELEPORT` 텔레포트.
- `TORCH` 횃불을 켜주세요.
- `TOURISTMAP` 관광객이 방문하고 싶은 지역 명소를 알아보고 도로를 보여줍니다.
- `TOWEL` 아이템을 수건처럼 사용해 캐릭터의 물기를 말려주세요.
- `TOW_ATTACH` 견인 케이블입니다. 활성화하여 차량에 연결하세요.
- `TURRET` 포탑을 활성화합니다.
- `WATER_PURIFIER` 물을 정화한다.

## Comestibles

### Comestible type

- `DRINK`
- `FOOD`
- `MED`

### Addiction type

- `alcohol`
- `amphetamine`
- `caffeine`
- `cocaine`
- `crack`
- `nicotine`
- `opiate`
- `sleeping pill`

### Use action

- `ALCOHOL_STRONG` 만취도가 대폭 증가한다. 질병 `drunk`을 추가합니다.
- `ALCOHOL_WEAK` 만취도가 약간 증가한다. 질병 추가 `drunk`
- `ALCOHOL` 음주량이 증가합니다. 질병 `drunk`을 추가합니다.
- `ANTIBIOTIC` 감염 퇴치에 도움이 됩니다. `infected` 질병을 제거하고 `recover` 질병을 추가합니다.
- `BANDAGE` 출혈을 멈추십시오.
- `BIRDFOOD` 작은 새를 친근하게 만들어줍니다.
- `BLECH` 구토를 유발합니다.
- `BLECH_BECAUSE_UNCLEAN` 경고가 발생합니다.
- `CATFOOD` 고양이를 친근하게 만들어줍니다.
- `CATTLEFODDER` 대형 초식동물을 친화적으로 만듭니다.
- `CHEW` "%s을(를) 씹으셨습니다"라는 메시지를 표시하지만 그 외에는 아무 작업도 수행하지 않습니다.
- `CIG` 니코틴 갈망을 완화합니다. 질병 `cig`을 추가합니다.
- `COKE` 배고픔을 줄여준다. 질병 `high`을 추가합니다.
- `CRACK` 배고픔을 줄여줍니다. 질병 `high`을 추가합니다.
- `DISINFECTANT` 감염을 예방합니다.
- `DOGFOOD` 개를 친근하게 만들어줍니다.
- `FIRSTAID` 힐링하세요.
- `FLUMED` 질병 `took_flumed`을 추가합니다.
- `FLUSLEEP` 질병을 추가하고 `took_flumed` 피로를 증가시킵니다.
- `FUNGICIDE` 곰팡이와 포자를 죽입니다. `fungus` 및 `spores` 질병을 제거합니다.
- `HALLU` 질병 `hallu`을 추가합니다.
- `HONEYCOMB` 왁스를 생성합니다.
- `INHALER` 질병 `asthma`을 제거합니다.
- `IODINE` 질병 `iodine`을 추가합니다.
- `MARLOSS` "베리를 먹으면서 거의 종교적인 경험을 하게 되고 자신과 하나가 되는 느낌을 받게 됩니다.
  주위..."
- `METH` 질병 추가 `meth`
- `NONE` "`[x]`으로는 흥미로운 일을 할 수 없습니다."
- `PKILL` 통증을 줄여줍니다. `pkill[n]` 질병을 추가합니다. 여기서 `[n]`는 사용된 플래그 `PKILL_[n]`의 수준입니다.
  이 식료품.
- `PLANTBLECH` 플레이어가 식물 돌연변이를 포함하지 않으면 구토를 유발합니다.
- `POISON` 질병 `poison` 및 `foodpoison`을 추가합니다.
- `PROZAC` 현재 존재하지 않는 경우 `took_prozac` 질병을 추가하고, 그렇지 않으면 경미한 자극제로 작용합니다.
  `took_prozac_bad` 부작용이 거의 발생하지 않습니다.
- `PURIFIER` 음성 돌연변이를 제거합니다.
- `ROYAL_JELLY` 많은 부정적인 상태와 질병을 완화합니다.
- `SEWAGE` 구토를 유발하고 돌연변이가 발생할 가능성이 있습니다.
- `SLEEP` 피로도가 대폭 증가한다.
- `THORAZINE` 질병을 제거합니다 `hallu`, `visuals`, `high`. 추가적으로 질병을 제거합니다.
  `formication` 질병 `dermatik`도 존재하지 않는 경우. 부정적인 반응을 보일 가능성이 있음
  피로도를 증가시킵니다.
- `VACCINE` 체력이 크게 증가합니다.
- `VITAMINS` 건강도 증가 (HP와 혼동하지 말 것)
- `WEED` Cheech & Chong과 함께라면 기분이 좋아질 것입니다. 질병 `weed_high`을 추가합니다.
- `XANAX` 불안감을 완화시켜줍니다. 질병 `took_xanax`을 추가합니다.

### Flags

- `ACID` BLECH 기능을 이용하여 섭취시 내산성일 경우 페널티가 감소됩니다.
- `CARNIVORE_OK` 육식변이 캐릭터가 먹을 수 있습니다.
- `CAN_PLANT_UNDERGROUND` 씨앗이라면 상관없이 z레벨 0 이하에서 심을 수 있습니다.
  주변 온도의.
- `CANT_HEAL_EVERYONE` 이 약은 모든 사람이 사용할 수는 없으며 특별한 돌연변이가 필요합니다. 보다
  `can_heal_with` 돌연변이가 발생했습니다.
- `EATEN_COLD` 차갑게 먹으면 사기 보너스.
- `EATEN_HOT` 뜨겁게 먹으면 사기 보너스.
- `NO_COOKING_BUFF`은 플레이어가 요리할 때 요리 스킬로 인해 해당 음식의 칼로리가 증가하지 않도록 방지합니다.
- `INEDIBLE` 기본적으로 먹을 수 없으며 (돌연변이 임계값)과 결합하면 먹을 수 있습니다.
  플래그: BIRD, CATTLE.
- `FERTILIZER` PLANTBLECH 기능으로 섭취할 경우 농사에 비료로 사용됩니다.
  식물에 대해서는 페널티가 취소됩니다.
- `FUNGAL_VECTOR` 섭취 시 곰팡이 감염이 발생합니다.
- `HIDDEN_HALLU` ... 음식은 특정 생존 스킬 레벨에서만 볼 수 있는 환각을 유발합니다.
- `HIDDEN_POISON` ... 특정 생존 스킬 레벨에서는 음식이 유독한 것으로 표시됩니다. 참고하세요
  아이템 자체가 독성을 띠게 만들지는 않습니다. `"use_action": "POISON"`도 추가해 보세요.
  대신 `FORAGE_POISON`을 사용하세요.
- `IS_BLOOD` 원심분리기, 병원, 연구실 등에 배치하면 스캔됩니다.
- `MELTS` 얼지 않는 이상 재미는 절반입니다. 냉동시 식용 가능합니다.
- `MILLABLE` 방앗간 안에 넣으면 밀가루로 변할 수 있습니다.
- `MYCUS_OK` 임계값 이후 마이커스 캐릭터가 먹을 수 있습니다. 마이커스 과일에만 적용됩니다.
  기본.
- `NEGATIVE_MONOTONY_OK` `negative_monotony` 속성이 재미있는 재미를 부정적으로 낮출 수 있도록 허용합니다.
  가치.
- `NO_INGEST` 경구 섭취 이외의 방법으로 투여됩니다.
- `PKILL_1` 가벼운 진통제.
- `PKILL_2` 중간 정도의 진통제.
- `PKILL_3` 진통제.
- `PKILL_4` "You shoot up."
- `PKILL_L` 천천히 방출되는 진통제.
- `RAW` 조리될 때까지(즉, 열원이 필요한 레시피에 사용됨) kcal을 25% 줄입니다.
  해당 음식의 칼로리가 50% 이상인 경우를 제외하고 _모든_ 조리되지 않은 음식에 추가해야 합니다.
  설탕(예: 많은 과일, 일부 채소) 또는 지방(예: 도살된 지방, 코코넛)에서 추출됩니다. TODO: 만들기
  지방/단백질/탄수화물을 추가한 후 이러한 기준에 대한 단위 테스트를 수행합니다.
- `SMOKABLE` 흡연대에서 허용됩니다.
- `SMOKED` 스모킹랙(스모킹 제품)에는 허용되지 않습니다.
- `USE_EAT_VERB` "%s을(를) 마십니다." or "%s(을)를 먹습니다."
- `USE_ON_NPC` NPC에게 사용할 수 있습니다(반드시 NPC가 사용할 필요는 없음).
- `ZOOM` 확대/축소 항목은 오버맵 시야 범위를 늘릴 수 있습니다.

## Furniture and Terrain

`terrain.json` 및 `furniture.json` 모두에 사용되는 알려진 플래그 목록입니다.

### Flags

- `ALARMED` 파손되면 알람이 울립니다.
- `ALIGN_WORKBENCH`(가구에만 해당) 타일에 대한 힌트는 이 스프라이트가
  가구는 작업대 품질의 인접한 타일을 향해야 합니다.-
- `ALLOW_FIELD_EFFECT` `SEALED` 지형/가구 내부 항목에 전계 효과를 적용합니다.
- `AUTO_WALL_SYMBOL` (지형에만 해당) 이 지형의 기호는 선화 중 하나가 됩니다.
  (모서리, T자 교차점, 직선 등)은 인접한 지형에 따라 달라집니다.

예: `-` 및 `|`은 둘 다 `CONNECT_TO_WALL` 플래그가 있는 지형입니다. `O`에는 플래그가 없습니다.
`X` 및 `Y`에는 `AUTO_WALL_SYMBOL` 플래그가 있습니다.

`X` 지형은 T자형 교차로(서쪽, 남쪽, 동쪽으로 연결됨)로 그려지고, `Y`은
수평선으로 그려집니다(서쪽에서 동쪽으로 가고 남쪽으로는 연결되지 않음).

```
-X-    -Y-
 |      O
```

- `ADV_DECONSTRUCT` 이는 일반적인 분해를 사용하여 분해할 수 없습니다. 특별히 정의된
  건설 조치가 필요하다. 이들 중 대부분은 "`advanced_object_deconstruction`"에 속합니다.
  그룹.
- `BARRICADABLE_DOOR_DAMAGED`
- `BARRICADABLE_DOOR_REINFORCED_DAMAGED`
- `BARRICADABLE_DOOR_REINFORCED`
- `BARRICADABLE_DOOR` 바리케이드를 쳐둘 수 있는 문.
- `BARRICADABLE_WINDOW_CURTAINS`
- `BARRICADABLE_WINDOW` 바리케이드가 가능한 창문.
- `BASHABLE` 플레이어 + 몬스터는 이것을 강타할 수 있습니다.
- `BLOCK_WIND` 이 지형은 바람의 영향을 차단합니다.
- `BURROWABLE` 굴을 파는 몬스터는 이 지형 아래로 이동할 수 있지만 대부분의 몬스터는 이동할 수 없습니다(예:
  그래보이드는 체인 링크 울타리 아래로 이동하지만 일반 좀비는 체인 링크 울타리 아래로 이동합니다.
- `BUTCHER_EQ` 정육점의 장비 - 시체를 완전히 도살하려면 필요합니다.
- `CAN_SIT` 플레이어가 앉을 수 있는 가구. "FLAT_SURF" 태그가 있는 가구 근처에 앉아 있는 플레이어
  식사에 대한 기분 보너스를 받게됩니다.
- `CHIP` 건설 메뉴에서 벽의 페인트가 벗겨졌는지 확인하는 데 사용됩니다.
- `COLLAPSES` 무너질 수 있는 지붕이 있습니다.
- `CONNECT_TO_WALL`(지형에만 해당) 이 플래그는 JSON 항목으로 대체되었습니다.
  `connects_to`, 이전 버전과의 호환성을 위해 유지됩니다.
- `CONSOLE` 컴퓨터로 사용됩니다.
- `CONTAINER` 이 사각형의 아이템은 플레이어가 약탈할 때까지 숨겨져 있습니다.
- `DECONSTRUCT` 분해될 수 있습니다.
- `DEEP_WATER` 물에 잠길 만큼 깊은 곳
- `WATER_CUBE` 온전한 물로 이루어진 워터타일
- `DESTROY_ITEM` 여기에 떨어진 아이템은 파괴됩니다. `NOITEM`도 참조하세요.
- `DIFFICULT_Z` 대부분의 좀비는 이 지형(예: 사다리)까지 따라갈 수 없습니다.
- `DIGGABLE_CAN_DEEPEN` 파기 가능한 위치를 다시 파서 더 깊게 만들 수 있습니다(예: 얕은 구덩이에서 깊은 곳까지)
  피트).
- `DIGGABLE` 몬스터 파기, 몬스터 파종, 삽으로 파기 등
- `DOOR` 열 수 있습니다(NPC 경로 찾기에 사용).
- `EASY_DECONSTRUCT` 플레이어는 도구 없이 이것을 분해할 수 있습니다.
- `ELEVATOR` 이 플래그가 있는 지형은 다음과 같은 경우 플레이어, NPC, 몬스터 및 아이템을 위아래로 이동시킵니다.
  플레이어가 `elevator controls` 근처에서 활성화됩니다.
- `EMITTER` 이 가구는 방출 항목에 정의된 대로 자동으로 필드를 방출합니다.
- `FIRE_CONTAINER` 화재 확산을 방지합니다(화로, 장작난로 등).
- `FLAMMABLE_ASH` 불타서 잔해가 아닌 재로 변합니다.
- `FLAMMABLE_HARD` 불을 붙이기가 더 어렵지만 여전히 가능합니다.
- `FLAMMABLE` 불이 붙을 수 있습니다.
- `FLAT_SURF` 평평하고 단단한 표면이 있는 가구 또는 지형(예: 의자가 아닌 테이블, 나무 그루터기,
  등.).
- `FLAT` 플레이어는 가구를 만들고 이동할 수 있습니다.
- `FORAGE_HALLU` 채집을 통해 발견하면 `HIDDEN_HALLU` 플래그로 찾을 수 있는 아이템입니다.
- `FORAGE_POISION` 채집을 통해 발견하면 `HIDDEN_POISON` 플래그로 찾을 수 있는 아이템입니다.
- `FRIDGE` 내용물을 냉장보관하는 상품입니다. 그들을 보존합니다. 자동차 부품과 달리 어떤 가구도
  이 플래그를 사용하면 지속적으로 작동합니다.
- `FREEZER` 내부의 아이템을 얼리는 아이템입니다. 그것들을 매우 잘 보존하고 있습니다. 자동차 부품과 달리
  이 플래그가 있는 모든 가구는 지속적으로 작동합니다.
- `GOES_DOWN` <kbd>></kbd>을 사용하여 한 단계 아래로 내려갈 수 있습니다.
- `GOES_UP` <kbd><</kbd>을 사용하면 한 단계 더 올라갈 수 있습니다.
- `GROWTH_SEED` 이 식물은 종자 성장 단계에 있습니다.
- `GROWTH_SEEDLING` 이 식물은 묘목 성장 단계에 있습니다.
- `GROWTH_MATURE` 이 식물은 성장의 성숙한 단계에 있습니다.
- `GROWTH_HARVEST` 이 식물은 수확할 준비가 되었습니다.
- `HARVESTED` 지형 유형의 수확된 버전을 표시합니다(예: 사과나무를 수확하면 지형 유형이 변경됩니다).
  수확된 나무가 되고, 나중에 다시 사과나무가 됩니다.)
- `HIDE_PLACE` 이 타일에 있는 생물은 인접한 타일에 서 있지 않은 생물에게 보이지 않습니다.
- `INDOORS` 지붕이 있습니다. 비, 햇빛 등을 차단합니다.
- `LADDER` 쉽게 오르내릴 수 있게 해주는 가구입니다(z레벨 모드에서만 작동).
- `LIQUIDCONT` 액체가 들어 있는 가구, 일부 검사를 통해 내용물에 접근 가능
  `SEALED`인 경우에도 마찬가지입니다.
- `LIQUID` 움직임을 차단하지만 벽은 아닙니다(용암, 물 등).
- `MINEABLE` 곡괭이/착암기로 채굴할 수 있습니다.
- `MOUNTABLE` `MOUNTED_GUN` 플래그가 있는 총에 적합합니다.
- `NOCOLLIDE` 차량과 전혀 충돌하지 않는 기능입니다.
- `NOITEM` 여기에는 항목을 추가할 수 없지만 인접한 타일로 넘칠 수 있습니다. `DESTROY_ITEM`도 참조하세요.
- `NO_FLOOR` 이 타일에 물건을 놓으면 물건이 떨어져야 합니다.
- `NO_SIGHT` 이 타일에 있는 생물의 시야가 타일 1개로 줄어듭니다.
- `NO_SCENT` 이 타일은 향기 값을 가질 수 없으므로 이 타일을 통한 향기 확산을 방지합니다.
- `OPENCLOSE_INSIDE` 문('열림' 또는 '닫기' 필드 포함)인 경우 열리거나 닫히기만 할 수 있습니다.
  안에 있으면 닫혀 있어요.
- `PAINFUL` 약간의 통증이 있을 수 있습니다.
- `PERMEABLE` 가스 투과성.
- `PLACE_ITEM` `place_item()`이 아이템을 장착할 수 있는 유효한 지형입니다.
- `PLANT` 자라서 열매를 맺는 '가구'.
- `PLANTABLE` 이 지형이나 가구에는 씨앗이 심어질 수 있습니다.
- `PLOWABLE` 지형을 경작할 수 있습니다.
- `RAMP_END`
- `RAMP` z 수준 위로 이동하는 데 사용할 수 있습니다.
- `REDUCE_SCENT` 향기 확산을 줄입니다(지역 내 전체 향기의 양이 아님). 다음과 같은 경우에만 작동합니다.
  당황할 수 있는.
- `ROAD` 운전이나 스케이트(롤러블레이드 사용)를 할 수 있을 만큼 평평하고 단단합니다.
- `ROUGH` 선수의 발을 다칠 수 있습니다.
- `RUG` `Remove Carpet` 구성 항목을 활성화합니다.
- `SALT_WATER` 바닷물 소스("water_source" 조사 작업이 있는 지형에서 작동).
- `SEALED` <kbd>e</kbd>을 사용하여 항목을 검색할 수 없습니다. 먼저 열어야 합니다.
- `SEEN_FROM_ABOVE` 더 높은 층에서 볼 수 있음(위 타일에 바닥이 없는 경우)
- `SHARP` 통과하는 플레이어/몬스터에게 약간의 피해를 줄 수 있습니다.
- `SHOOT_ME` 플레이어는 이 깃발을 사용하여 지형이나 가구를 겨냥할 수 있습니다.
  `tr_practice_target` 사격술을 훈련합니다.
- `SHORT` 차량 돌출부와 충돌하기에는 기능이 너무 짧습니다. (거울, 블레이드).
- `SIGN` 검사 시 작성된 메시지를 표시합니다.
- `SMALL_PASSAGE` 이 지형이나 가구는 크거나 거대한 생물이 지나갈 수 없을 정도로 작습니다.
  을 통해.
- `SOURCE_CLAY` `Extract Clay` 구성 항목을 활성화합니다.
- `SOURCE_IRON` `Extract Iron` 구성 항목을 활성화합니다.
- `SOURCE_SAND` `Extract Sand` 구성 항목을 활성화합니다.
- `SUN_ROOF_ABOVE` 이 가구(현재 지형은 지원되지 않음) 위에 "fake roof"이 있습니다.
  햇빛을 차단합니다. #44421에 대한 특별 해킹입니다. 나중에 제거됩니다.
- `SUPPORTS_ROOF` 지붕공사의 경계로 사용됩니다.
- `SUPPRESS_SMOKE` 화재로 인한 연기를 방지합니다. 통풍이 되는 장작 난로 등에 사용됩니다.
- `SWIMMABLE` 플레이어와 몬스터가 헤엄쳐 통과할 수 있습니다.
- `THIN_OBSTACLE` 플레이어와 몬스터가 지나갈 수 있습니다. 차량은 그것을 파괴합니다.
- `TINY` 기능이 너무 짧아 차량 하부 구조와 충돌할 수 없습니다. 차량은 아무 것도 없이 그 위를 지나간다.
  바퀴가 부딪히지 않는 한 손상됩니다.
- `TRANSPARENT` 플레이어와 몬스터는 그것을 꿰뚫어 볼 수 있습니다. 또한 ter_t.transparent를 설정합니다.
- `UNSTABLE` 여기로 걸어가면 캐릭터에게 바위 효과가 발생합니다.
- `USABLE_FIRE` 이 지형이나 가구는 제작 시 근처의 불로 간주됩니다.
- `VEH_TREAT_AS_BASH_BELOW` 험한 지형이라 하더라도 차량은 이것과 충돌하지 않으며,
  bash_below가 있는 바닥처럼요. 다음과 같은 경우 다른 지형으로 전환될 지형에 사용됩니다.
  아래 타일을 파괴하는 대신 박살냅니다.
- `WALL` 이 지형은 직립 장애물입니다. 곰팡이 전환에 사용되며 다음을 의미합니다.
  `CONNECT_TO_WALL`.
- `WELDABLE_DOOR` "weld shut metal door" 구성은 이 지형을 `t_door_metal_welded`으로 변환합니다.
- `WELDABLE_BARS` 위와 동일하지만 대신 `t_door_metal_welded_bars`으로 변환됩니다. 변환하고 싶다면
  대신 특정 지형에 `weld_shut_metal_door` 구성에 별도의 구성 항목을 추가하세요.
  `pre_flags` 대신 `pre_terrain`을 사용하는 그룹입니다.
- `WINDOW` 이 지형은 창이지만 닫혀 있거나 부서지거나 가려져 있을 수 있습니다. teh 님이 사용함
  가구 스프라이트를 창에서 멀리 정렬하는 타일 코드입니다.

### Examine Actions

- `aggie_plant` 식물을 수확합니다.
- `autodoc` autodoc 콘솔 메뉴를 가져옵니다. 제대로 작동하려면 `AUTODOC` 플래그가 필요하며
  `AUTODOC_COUCH` 플래그가 있는 인접한 가구.
- `autoclave_empty` 결함이 있는 CBM이 포함되어 있고 플레이어에 충분한 양이 있는 경우 오토클레이브 주기를 시작하십시오.
  물.
- `autoclave_full` 주기 진행 상황을 확인하고, 주기가 끝나면 멸균된 CBM을 수집합니다.
  완전한.
- `bars` AMORPHOUS를 활용하고 바를 통과하세요.
- `cardreader` 유효한 카드로 카드리더를 사용하거나 해킹을 시도해보세요.
- `chainfence` 쇠사슬 울타리를 뛰어넘으세요.
- `controls_gate` 연결된 게이트를 제어합니다.
- `dirtmound` 씨앗과 식물을 심습니다.
- `elevator` 층을 바꾸려면 엘리베이터를 이용하세요.
- `fault` 설명 메시지를 표시하지만 그 외에는 사용되지 않습니다.
- `flower_poppy` 돌연변이된 양귀비를 골라보세요.
- `fswitch` 스위치를 뒤집으면 암석이 이동합니다.
- `fungus` 지형이 무너지면 포자를 방출합니다.
- `gaspump` 가스펌프를 사용하세요.
- `none` 없음
- `pedestal_temple` 눈이 석화된 경우 관자놀이를 엽니다.
- `pedestal_wyrm` 지룡을 생성합니다.
- `pit_covered` 구덩이를 발견하세요.
- `pit` 나무판이 있으면 구덩이를 덮어주세요.
- `portable_structure` 텐트나 유사한 이동식 구조물을 철거하세요.
- `recycle_compactor` 순금속 물체를 기본 모양으로 압축합니다.
- `rubble` 삽이 있으면 잔해를 치우세요.
- `safe` 금고를 깨려고 시도합니다.
- `shelter` 대피소를 무너뜨려라.
- `shrub_marloss` 말로스 덤불을 선택하세요.
- `shrub_wildveggies` 야생 채소 관목을 선택하세요.
- `slot_machine` 도박.
- `toilet` 마시거나 화장실 물을 빼내세요.
- `trap` 트랩과 상호작용합니다.
- `water_source` 수원에서 물을 마시거나 섭취하세요.

### Fungal Conversions Only

- `FLOWER` 이 가구는 꽃이에요.
- `FUNGUS` 곰팡이가 덮였습니다.
- `ORGANIC` 이 가구는 부분적으로 유기농입니다.
- `SHRUB` 이 지형은 관목입니다.
- `TREE` 이 지형은 나무입니다.
- `YOUNG` 이 지형은 어린 나무입니다.

### Furniture Only

- `AUTODOC` 이 가구는 자동 문서 콘솔이 될 수 있으며 `autodoc` 검사 작업도 필요합니다.
- `AUTODOC_COUCH` 이 가구는 `autodoc` 조사 작업을 통해 가구용 소파가 될 수 있습니다.
- `BLOCKSDOOR` `str_*_blocked`이 설정된 경우 지도 지형의 강타 저항력이 향상됩니다(참조
  `map_bash_info`)

## Generic

### Flags

- `BIONIC_NPC_USABLE` ... 광범위한 NPC 재작성 없이 NPC가 사용할 수 있는 안전한 CBM
  CBM을 전환합니다.
- `BIONIC_TOGGLED` ... 이 생체공학은 효과를 유발하는 것이 아니라 활성화되었을 때만 기능을 갖습니다.
  매 턴마다.
- `BIONIC_POWER_SOURCE` ... 이 생체공학은 생체공학적 힘의 원천입니다.
- `BIONIC_SHOCKPROOF` ... 이 생체 공학은 전기 공격으로 무력화될 수 없습니다.
- `BIONIC_FAULTY` ... 이 생체공학은 "faulty" 생체공학입니다.
- `BIONIC_WEAPON` ... 이 생체공학은 무기 생체공학이며 활성화하면 생성(또는 파괴)됩니다.
  Bionic의 fake_item이 사용자 손에 있습니다. 다른 모든 활성화 효과를 방지합니다.
- `BIONIC_ARMOR_INTERFACE` ... 이 생체공학은 동력 방어구에 전력을 공급할 수 있습니다.
- `BIONIC_SLEEP_FRIENDLY` ... 이 생체 공학은 플레이어가 잠을 자려고 해도 경고를 제공하지 않습니다.
  그것은 활동적입니다.
- `BIONIC_GUN` ... 이 생체공학은 총 생체공학이며 활성화하면 발사됩니다. 다른 모든 것을 방지합니다.
  활성화 효과.
- `COMBAT_NPC_ON` ... NPC가 전투에서 사용할 아이템에 대한 "on" 상태
- `COMBAT_NPC_USE` ... 전투 시 NPC가 활성화하는 아이템 및 CBM
- `CORPSE` ... 맵 생성 중에 다양한 인간 시체를 생성하는 데 사용되는 플래그입니다.
- `DANGEROUS` ... NPC는 이 아이템을 받지 않습니다. 폭발 사용 행위자는 이 플래그를 의미합니다. 암시하다
  "NPC_THROW_NOW".
- `DESTROY_ON_DECHARGE` ... 이 아이템은 요금이 손실되면 폐기되어야 합니다.
- `DURABLE_MELEE`... 쳐맞을려고 만든 아이템인데 잘 쳐줘서 많이 쳐준다고 생각함
  같은 재료로 만든 다른 무기보다 더 튼튼합니다.
- `FAKE_MILL` ... 아이템은 @ref로 부분 가공된 제품을 나타내는 가짜 아이템입니다.
  항목::process_fake_mill, 제거 조건이 설정됩니다.
- `FAKE_SMOKE` ... 아이템은 @ref로 인식할 수 있는 연기를 발생시키는 가짜 아이템입니다.
  item::process_fake_smoke, 제거 조건이 설정됩니다.
- `FIREWOOD` ... 장작 역할을 할 수 있는 아이템입니다. 이 플래그가 있는 항목은 "전리품:
  우드' 존
- `FLY_STRAIGHT` ... 던진 아이템은 공중에서 회전하지 않고 직선으로 날아갑니다.
  공기 역학적 방향. 일반적으로 창, 창, 다트에 사용됩니다.
- `FRAGILE_MELEE` ... 불량으로 인해 무기로 사용시 쉽게 부서지는 깨지기 쉬운 물건
  건축 품질이 우수하며 파손 시 부품이 파손될 수 있습니다.
- `GAS_DISCOUNT` ... 자동주유소 할인카드입니다.
- `IS_PET_ARMOR`... 사람용 방어구가 아닌 애완몬스터용 방어구입니다
- `LEAK_ALWAYS` ... 누출("RADIOACTIVE"과 결합될 수 있음).
- `LEAK_DAM` ... 손상 시 누출됩니다("RADIOACTIVE"과 결합될 수 있음).
- `NEEDS_UNFOLD` ... 휘두르면 추가 시간 패널티가 있습니다. 근접 무기와 총의 경우
  해당 스킬로 상쇄됩니다. "SLOW_WIELD"으로 스택됩니다.
- `MISSION_ITEM` ... 아이템 생성 속도 설정에 관계없이 항상 전리품으로 생성됩니다.
- `NO_PACKED` ... 이 품목은 오염으로부터 보호되지 않으며 멸균 상태를 유지하지 않습니다. 오직
  CBM에 적용됩니다.
- `NO_REPAIR` ... 적합한 도구가 있더라도 이 항목의 수리를 방지합니다.
- `NO_SALVAGE` ... 회수 과정을 통해 아이템을 분해할 수 없습니다. 어떤 일이 있을 때 가장 잘 사용됩니다.
  분해될 수 없어야 합니다(예: 가죽 패치와 같은 기본 구성 요소).
- `NO_STERILE` ... 이 제품은 무균 제품이 아닙니다. CBM에만 적용됩니다.
- `NPC_ACTIVATE` ... NPC는 이 아이템을 대체 공격으로 활성화할 수 있습니다. 현재는 던지면서
  활성화 직후. "BOMB"에 의해 암시됩니다.
- `NPC_ALT_ATTACK` ... 직접 설정하면 안 됩니다. "NPC_ACTIVATE" 및 "NPC_THROWN"에 의해 암시됩니다.
- `NPC_THROWN` ... NPC는 대체 공격으로 이 아이템을 (먼저 활성화하지 않고) 던집니다.
- `NPC_THROW_NOW` ... NPC는 이 아이템을 가급적이면 적에게 버리려고 할 것입니다. 암시하다
  "TRADER_AVOID" 및 "NPC_THROWN".
- `PERFECT_LOCKPICK` ... 완벽한 자물쇠 따기 아이템입니다. 자물쇠를 따는 데 단 5초밖에 걸리지 않으며 결코 자물쇠를 따지 못합니다.
  실패하지만 이를 사용하면 잠금 선택 XP가 약간만 부여됩니다. 항목에는 "LOCKPICK"이 있어야 합니다.
  품질이 1 이상입니다.
- `PSEUDO` ... 내부적으로는 제작 인벤토리에 언급되어 있지만
  실제로 항목이 아닙니다. 도구로 사용할 수 있지만 구성요소로는 사용할 수 없습니다. "TRADER_AVOID"을 의미합니다.
- `BIONIC_TOOLS` ... 유사 생체공학 도구에서 생체공학적 힘을 사용해야 한다는 점을 명확히 하기 위해 사용됩니다.
- `RADIOACTIVE` ... 방사성입니다(LEAK_*와 함께 사용할 수 있음).
- `RAIN_PROTECT` ... 휘두를 때 햇빛과 비로부터 보호합니다.
- `REDUCED_BASHING` ... Gunmod 플래그; 아이템의 강타 데미지를 50% 감소시킵니다.
- `REDUCED_WEIGHT` ... Gunmod 플래그; 아이템의 기본 무게가 25% 감소합니다.
- `REQUIRES_TINDER` ... 이 항목이 불을 붙이려고 하는 타일에 틴더가 있어야 합니다.
  에.
- `SHATTERS`... 이 아이템은 무기로 사용시 유리처럼 깨질 가능성이 있으며,
  던지고, 때리고, 등등.
- `SLEEP_AID` ... 숙면에 도움이 되는 아이템입니다.
- `SLEEP_IGNORE` ... 이 항목은 잠들기 전 경고로 표시되지 않습니다.
- `SLOW_WIELD` ... 휘두르면 추가 시간 패널티가 있습니다. 근접 무기와 총의 경우 다음과 같습니다.
  해당 스킬로 상쇄됩니다. "NEEDS_UNFOLD"으로 스택됩니다.
- `TACK` ... 마운트용 압정으로 사용할 수 있는 아이템입니다.
- `TIE_UP` ... 아이템을 사용하면 생물을 묶을 수 있습니다.
- `TINDER` ... 이 항목은 REQUIRES_TINDER 플래그가 지정된 불을 피우는 데 사용할 수 있습니다.
  방화 장치.
- `TRADER_AVOID` ... NPC는 이 아이템을 가지고 시작할 수 없습니다. 활성 항목(예: 손전등)에 사용하세요.
  (on)), 위험한 아이템(예: 활성 폭탄), 가짜 아이템 또는 특이한 아이템(예: 고유 퀘스트 아이템).
- `UNBREAKABLE` ... 이 깃발이 달린 방어구는 착용자가 타격을 받을 때 절대 피해를 입지 않습니다.
- `UNBREAKABLE_MELEE` ... 근접무기로 사용해도 절대 데미지를 입지 않습니다.
- `UNRECOVERABLE` ... 분해하여 복구할 수 없습니다.
- `TOBACCO` ... 불을 붙일 때마다 담배를 피우는 듯한 효과를 주는 아이템입니다.
- `MARIJUANA` ... 불을 붙일 때마다 마리화나 효과를 주는 아이템입니다.

## Guns

- `BACKBLAST` 무기를 발사하는 사람 뒤에서 작은 폭발을 일으킵니다. 현재는 아님
  구현?
- `BIPOD` 핸들링 보너스는 MOUNTABLE 지도/차량 타일에만 적용됩니다. 사용 시간은 포함되지 않습니다.
  페널티(SLOW_WIELD 참조)
- `CHARGE` 발사하려면 충전이 필요합니다. 충전량이 높을수록 피해가 더 커집니다.
- `COLLAPSIBLE_STOCK` 총의 기본 크기에 비례하여 무기 볼륨을 줄입니다.
  모드). 사용 시간 페널티는 포함되지 않습니다(NEEDS_UNFOLD 참조).
- `CONSUMABLE` 발사된 탄약에 따라 총구가 손상될 확률을 지정하고 정의할 수 있습니다.
  필드 'consume_chance' 및 'consume_divisor'.
- `CROSSBOW` gunmod 호환성을 위해 석궁으로 간주됩니다. 기본 동작은 다음과 같습니다.
  해당 무기에 사용되는 스킬과 일치합니다.
- `DISABLE_SIGHTS` 기본 무기 조준경 사용을 방지합니다.
- `FIRE_20` 한 번 발사하면 20발을 사용합니다.
- `FIRE_50` 1발당 50발을 사용합니다.
- `FIRE_100` 1발당 100발을 사용합니다. 금액을 지정하려면 `ammo_to_fire` 속성도 참조하세요.
  원하는 샷당 탄약 사용량. 이러한 플래그는 `ammo_to_fire`이 있는 경우 재정의됩니다.
- `HEAVY_WEAPON_SUPPORT` 이것을 착용하면 지형 없이도 중화기를 발사할 수 있습니다.
  크거나 거대한 돌연변이처럼 지원이 가능합니다.
- `FIRE_TWOHAND` 총은 플레이어에게 두 손이 없는 경우에만 발사할 수 있습니다.
- `IRREMOVABLE` 건모드를 제거할 수 없도록 만듭니다.
- `MECH_BAT` 이것은 군용 기계에 전력을 공급하도록 설계된 이국적인 배터리입니다.
- `MOUNTED_GUN` 총은 "MOUNTABLE" 플래그가 있는 지형/가구에서만 사용할 수 있습니다.
  평범한 인간. 당신이 특대 돌연변이(불편할 정도로 크다, 크다, 괴상할 정도로 거대하다, 거대하다)라면,
  분산 및 반동 페널티를 대가로 정기적으로 발사할 수 있습니다. 뭔가를 입고
  `HEAVY_WEAPON_SUPPORT` 플래그도 작동합니다.
- `NEVER_JAMS` 절대로 오작동하지 않습니다.
- `NO_UNLOAD` 언로드할 수 없습니다.
- `PRIMITIVE_RANGED_WEAPON` 총제작자가 아닌 도구를 사용하여 수리할 수 있습니다(강화하지는 않음).
- `PUMP_ACTION` 총의 펌프 동작에 레일이 있어 다음과 같은 모드만 설치할 수 있습니다.
  언더배럴 슬롯의 PUMP_RAIL_COMPATIBLE 플래그.
- `PUMP_RAIL_COMPATIBLE` Mod는 펌프 레일이 있는 총신 아래 슬롯에 설치할 수 있습니다.
  행동.
- `RELOAD_AND_SHOOT` 발사하면 자동으로 재장전한 후 발사됩니다.
- `RELOAD_EJECT` 발사할 때가 아니라 재장전할 때 총에서 포탄을 꺼냅니다.
- `RELOAD_ONE` 한 번에 한 라운드만 재장전합니다.
- `STR_DRAW` 무기에 강도 요구 사항이 있는 경우 요구 사항이 부족하면 페널티가 적용됩니다.
  나열된 강도의 50% 미만인 경우 발사할 수 없을 때까지 피해, 범위 및 분산,
  평소처럼 엄격한 제한을 두는 대신.
- `STR_RELOAD` 재장전 속도는 힘의 영향을 받습니다.
- `UNDERWATER_GUN` 총은 수중 사용에 최적화되어 있지만 물 밖에서는 성능이 좋지 않습니다.
- `USE_PARENT_GUN` `gun_data`이 있는 총기 개조의 경우 총에 있는 추가 탄창을 나타냅니다. KSG.
  총 자체가 아닌 설치된 총에 오염이 적용되도록 합니다. 소음기가 설치되어 있는지 확인하세요.
  상위 항목에 적용하고, 상위 항목에 있는 경우 황동 수집기 동작을 적용합니다.
- `WATERPROOF_GUN` 총은 녹슬지 않으며 수중에서도 사용할 수 있습니다.

### Firing modes

- `MELEE` 총기나 보조포의 속성을 이용한 근접 공격
- `NPC_AVOID` NPC는 이 모드를 사용하려고 시도하지 않습니다.
- `SIMULTANEOUS` 모든 탄환은 동시에(순차적이지 않음) 발사되었으며 반동은 한 번만 추가되었습니다.
  끝)

### Faults

#### Flags

- `SILENT` 일반 UI의 항목 옆에 "faulty " 텍스트가 표시되지 않도록 합니다. 그렇지 않으면 결함이 발생합니다.
  동일합니다.

#### Parameters

- `turns_into` 방금 수선한 항목에 이 결함이 적용됩니다.
- `also_mends` 해당 오류가 해결되면 이 오류가 수정됩니다(선택된 오류에 추가로).
  고치는.

## Magazines

- `MAG_BULKY` 적절한 특대 탄약 주머니에 보관할 수 있습니다(부피가 크거나 어색하게 사용할 수 있는 용도)
  모양의 잡지)
- `MAG_COMPACT` 적절한 탄약 주머니에 보관할 수 있습니다(소형 탄창용).
- `MAG_DESTROY` 탄창은 마지막 라운드가 소모되면 파괴됩니다(탄약 벨트용). 가지다
  MAG_EJECT보다 우선순위가 높습니다.
- `MAG_EJECT` 마지막 라운드가 소모되면 탄창이 총/도구에서 배출됩니다.
- `SPEEDLOADER` 탄창처럼 작동하지만 총알을 대상 총에 전달하는 대신
  그것에 삽입되었습니다.

## MAP SPECIALS

- `mx_bandits_block` ... 도적들이 나무 통나무로 만든 도로 블록입니다.
- `mx_burned_ground`... 화재가 이곳을 휩쓸었습니다.
- `mx_point_burned_ground`... 화재가 이곳을 휩쓸었습니다. (일부적용)
- `mx_casings` ... 여러 유형의 사용한 케이스(단독, 그룹, 전체 오버맵 타일)
- `mx_clay_deposit` ... 작은 표면 점토 퇴적물.
- `mx_clearcut` ... 모든 나무는 그루터기가 된다.
- `mx_collegekids` ... 시체와 아이템.
- `mx_corpses` ... 매일 전리품으로 시체 최대 5개.
- `mx_crater` ... 잔해와 방사능이 있는 분화구.
- `mx_drugdeal` ... 시체와 일부 약물.
- `mx_dead_vegetation` ... 모든 식물을 죽입니다. (산성비 등의 여파)
- `mx_point_dead_vegetation` ... 모든 식물을 죽입니다. (산성비 등의 여파) (일부
  애플리케이션)
- `mx_grove` ... 모든 나무와 관목은 하나의 나무가 됩니다.
- `mx_grave` ... 시체와 일상 전리품이 있는 열린 들판의 무덤입니다.
- `mx_helicopter` ... 금속 잔해 및 일부 품목.
- `mx_jabberwock` ... jabberwock의 _기회_입니다.
- `mx_looters` ... 건물에는 도적 최대 5마리가 스폰됩니다.
- `mx_marloss_pilgrimage` 곰팡이류를 숭배하는 종파입니다.
- `mx_mayhem` ... 여러 유형의 도로 대혼란(총격전, 차량 충돌 등).
- `mx_military` ... 시체와 일부 군용품.
- `mx_minefield` ... 교량 입구에 지뢰가 산재해 있는 군용 장애물
  그 앞.
- `mx_null` ... 전혀 특별하지 않습니다.
- `mx_pond` ... 작은 연못.
- `mx_portal_in` ... 두 공간 모두에 대한 또 다른 포털입니다.
- `mx_portal` ... 여러 유형의 주변 환경을 갖춘 두 공간 모두에 대한 포털입니다.
- `mx_roadblock` ... 포탑이 있는 장애물 가구와 일부 자동차.
- `mx_roadworks` ... 작업 장비 및 유틸리티의 가능성이 있는 부분 폐쇄된 손상된 도로
  차량.
- `mx_science` ... 시체와 일부 과학자 아이템.
- `mx_shia` ... Crazy Catalcysm이 활성화된 경우 시아파의 _기회_.
- `mx_shrubbery` ... 모든 나무와 관목은 하나의 관목이 됩니다.
- `mx_spider` ... 거미와 알로 가득한 커다란 거미줄.
- `mx_supplydrop` ... 군용품이 담긴 상자입니다.

## Material Phases

- `GAS`
- `LIQUID`
- `NULL`
- `PLASMA`
- `SOLID`

## Melee

### Flags

- `ALWAYS_TWOHAND` 아이템은 항상 두 손으로 사용합니다. 이것이 없으면 품목의 부피와 무게가
  이를 계산하는 데 사용됩니다.
- `DIAMOND` 다이아몬드 코팅으로 베기, 찌르기 데미지 30% 추가
- `MESSY` 펄프화할 때 더 혼란스러워집니다.
- `NO_CVD` CVD 기계에서는 절대 사용할 수 없는 아이템입니다
- `NO_RELOAD` 아이템을 다시 로드할 수 없습니다(유효한 탄약 유형이 있더라도).
- `NO_UNWIELD` 이 아이템은 풀 수 없습니다.
- `POLEARM` 아이템은 가까이서 보면 어색하고 인접한 대상에게 일반 피해의 70%를 줍니다. 해야 한다
  REACH_ATTACK과 쌍을 이룹니다. 창과 같은 단순 관통 무기에는 이 플래그가 적용되지 않습니다.
- `REACH_ATTACK` 도달 공격을 수행할 수 있도록 허용합니다.
- `SHEATH_KNIFE` 칼집에 칼집을 넣을 수 있으며, 소형/중형 칼에 적용 가능합니다.
  볼륨은 2보다 크지 않음)
- `SHEATH_SWORD` 아이템을 칼집에 넣을 수 있습니다.
- `SPEAR` 도달 공격을 할 때 THIN_OBSTACLE 지형에 개입하는 것은 장애물이 아닙니다. 되어야 한다
  `REACH_ATTACK`과 쌍을 이룹니다.
- `STAB` 무기의 피해 유형을 관통으로 변환하는 레거시 플래그입니다. 이것은 해킹입니다.
  근접 무기의 피해 유형을 지정하기 위해 탄약에 사용되는 피해 개체를 사용할 수 없습니다.
- `UNARMED_WEAPON` 이 아이템을 휘두르는 것은 여전히 ​​비무장 전투로 간주됩니다.
- `WHIP` 상대를 무장해제시킬 확률이 있습니다.
- `FLAMING` 적중 시 무작위 양의 화염 피해를 입힙니다.
- `SHOCKING` 적중 시 무작위 양의 전기 피해를 입힙니다.
- `ACIDIC` 적중 시 무작위 양의 산성 피해를 입힙니다.

## Monster Groups

조건 플래그는 몬스터가 생성될 수 있는 시기를 제한합니다.

### Seasons

여러 시즌 조건이 함께 결합되어 해당 조건 중 하나가 유효해집니다.
올해의 산란 시간.

- `AUTUMN`
- `SPRING`
- `SUMMER`
- `WINTER`

### Time of day

하루 중 여러 시간 조건이 함께 결합되어 해당 조건 중 하나가 다음과 같이 됩니다.
유효한 시간 생성 시간.

- `DAWN`
- `DAY`
- `DUSK`
- `NIGHT`

## Monsters

몬스터를 설명하고 속성과 능력을 정의하는 데 사용되는 플래그입니다.

### Anger, Fear and Placation Triggers

- `FIRE` 근처에 불이 났어요.
- `FRIEND_ATTACKED` 같은 종류의 몬스터가 공격을 받았습니다. 항상 캐릭터 어그로를 유발합니다.
- `FRIEND_DIED` 같은 종류의 몬스터가 사망했습니다. 항상 캐릭터 어그로를 유발합니다.
- `HURT` 몬스터가 다쳤어요. 항상 캐릭터 어그로를 유발합니다.
- `MEAT` 근처에 고기나 시체가 있습니다.
- `NULL` 소스만 사용하시겠습니까?
- `PLAYER_CLOSE` 플레이어는 몇 타일 거리 내에 도달합니다. `<anger>%`의 캐릭터 어그로를 유발합니다.
  시간.
- `PLAYER_WEAK` 선수가 다쳤어요.
- `SOUND` 소리가 들렸다.
- `STALK` 플레이어를 따라갈 때 증가합니다.

### Categories

- `CLASSIC` 고전 좀비영화에서나 볼 수 있는 괴물들만 나옵니다.
- `NULL` 카테고리가 없습니다.
- `WILDLIFE` 자연동물.

### Death Functions

여러 사망 기능을 사용할 수 있습니다. 모든 조합이 의미가 있는 것은 아닙니다.

- `ACID` 몸 대신 산성. ACID_BLOOD 플래그와 동일하지 않습니다. 대부분의 경우 둘 다 원합니다.
- `AMIGARA` 마지막 최면인 경우 최면을 제거합니다.
- `BLOBSPLIT` 더 많은 Blob을 생성합니다.
- `BOOMER_GLOW` 뜨거운 토사물 속에서 폭발합니다.
- `BOOMER` 토사물로 폭발함.
- `BRAINBLOB` 2개의 얼룩을 생성합니다.
- `BROKEN_AMMO` 탄약을 파괴하라는 메시지를 보낸 후 "BROKEN"을 호출합니다.
- `BROKEN` 손상된 로봇 항목을 생성합니다. 해당 ID는 다음과 같이 계산됩니다. 접두사 "mon_"이 제거됩니다.
  몬스터 ID에서 "broken_" 접두사가 추가됩니다. 예: mon_eyebot -> broken_eyebot.
- `CONFLAGRATION` 거대한 불덩이로 폭발한다.
- `DARKMAN` 시력이 정상으로 돌아왔습니다.
- `DETONATE` 자폭합니다.
- `DISAPPEAR` 환각이 사라진다.
- `DISINTEGRATE` 무너진다.
- `EXPLODE` 파괴적인 폭발.
- `FIREBALL` 불덩어리로 폭발할 확률은 10%입니다.
- `FOCUSEDBEAM` 눈부신 광선.
- `FUNGALBURST` 곰팡이 안개 구름으로 폭발합니다.
- `FUNGUS` 포자 속에서 폭발합니다.
- `GAMEOVER` 게임 끝! 게임 끝! 방어 모드.
- `GAS` 유독가스 속에서 폭발함.
- `GUILT` 도덕적 처벌. 비슷한 효과를 내는 플래그도 있습니다.
- `JABBERWOCKY` 스니커간식!
- `JACKSON` 댄서를 복귀시킵니다.
- `KILL_BREATHERS` 숨 쉬는 사람은 다 죽는다.
- `KILL_VINES` 근처의 덩굴을 모두 죽입니다.
- `MELT` 정상적인 죽음이지만 녹는다.
- `NORMAL` 시체를 버리고, 토막을 남겨주세요.
- `PREG_ROACH` 바퀴벌레 약충을 생성하세요.
- `RATKING` 해충염을 치료합니다.
- `SMOKEBURST` 거대한 연막탄처럼 터진다.
- `SPLATTER` 덩어리로 폭발합니다.
- `THING` 완전하게 변신하세요.
- `TRIFFID_HEART` 모든 루트를 파괴합니다.
- `VINE_CUT` 인접한 덩굴이 잘리면 죽입니다.
- `WORM` 반벌레 2마리를 생성합니다.

### Flags

- `ABSORBS_SPLITS` 이동하는 물체를 소비하며, 충분히 흡수하면 여러 개의 개체로 분할됩니다.
  복사.
- `ABSORBS` 이동한 개체를 소비합니다. (모더는 이것을 사용합니다).
- `ACIDPROOF` 산성에 면역입니다.
- `ACIDTRAIL` 산성의 흔적을 남깁니다.
- `ACID_BLOOD` 몬스터의 출혈을 산성으로 만듭니다. 재미있는 것들! 풀에 자동으로 용해되지 않습니다.
  사망 시 산성.
- `ANIMAL` `Animal Empathy` 특성의 목적상 _동물_입니다.
- `AQUATIC` 물에만 국한됨.
- `ARTHROPOD_BLOOD` 몬스터에게 혈림프 출혈을 강제로 발생시킵니다.
- `ATTACKMON` 다른 몬스터를 공격합니다.
- `BADVENOM` 공격은 플레이어를 **심각하게** 중독시킬 수 있습니다.
- `BASHES` 문을 강타합니다.
- `BILE_BLOOD` 몬스터가 담즙을 흘리게 만듭니다.
- `BIOPROOF` 몬스터가 바이오 피해에 면역이 되도록 합니다(주로 마법 모드에서 사용되는 피해 유형).
- `COLDPROOF` 몬스터가 콜드 데미지에 면역이 되도록 합니다. (마법 모드에서 주로 사용되는 데미지 유형)
- `DARKPROOF` 몬스터가 암흑 피해에 면역이 되도록 합니다. (마법 모드에서 완전히 사용되는 피해 유형)
- `LIGHTPROOF` 몬스터가 빛 피해에 면역이 되도록 합니다. (마법 모드에서 완전히 사용되는 피해 유형)
- `PSIPROOF` 몬스터가 사이오닉 피해에 면역이 되도록 만듭니다(주로 마법 모드에서 사용되는 피해 유형).
- `BIRDFOOD` 새 먹이와 친해지거나 길들여집니다.
- `BLEED` 플레이어에게 출혈을 유발합니다.
- `BONES` 도살하면 뼈와 힘줄이 생성될 수 있습니다.
- `BORES` 거의 모든 것을 통과하는 터널링(15x 강타 승수: dark wyrms의 강타 스킬 12->180)
- `CAN_BE_ORDERED` 이 생물은 우호적인 경우 적을 공격하지 않도록 지시할 수 있습니다.
- `CAN_DIG` 땅을 파고_걸을 수 있습니다.
- `CAN_OPEN_DOORS` 경로에 있는 문을 열 수 있습니다.
- `CANPLAY` 이 생물은 애완동물이라면 가지고 놀 수 있습니다.
- `CATFOOD` 고양이 사료와 친해지거나 길들여집니다.
- `CATTLEFODDER` 가축 사료와 친해지거나 길들여집니다.
- `CBM_CIV` 도축 시 일반 CBM과 파워 CBM을 생산할 수 있습니다.
- `CBM_OP` 도살 시 'bionics_op' 아이템 그룹에서 CBM 한두 개를 생산할 수 있습니다.
- `CBM_POWER` 도살 시 CBM과 별개로 강력한 CBM을 생산할 수 있습니다.
- `CBM_SCI` 도살 시 'bionics_sci' 아이템 그룹에서 CBM을 생산할 수 있습니다.
- `CBM_SUBS` bionics_subs에서 CBM 한두 개를 생산할 수 있으며 도살 시 파워 CBM을 생산할 수 있습니다.
- `CBM_TECH` 'bionics_tech' 아이템 그룹에서 CBM 한두 개를 생산할 수 있으며 도살 시 파워 CBM을 생산할 수 있습니다.
- `CHITIN` 도살하면 키틴이 생성될 수 있습니다.
- `CLIMBS` 올라갈 수 있습니다.
- `CURRENT` 물이 흐르고 있어요.
- `DESTROYS` 벽 등을 강타합니다. (2.5x bash 승수, 여기서 기본은 동물의 최대값입니다.
  근접 강타)
- `DIGS` 땅을 파헤칩니다.
- `DOGFOOD` 개밥과 친해지거나 길들여집니다.
- `DRIPS_GASOLINE` 이동 중에 가끔 휘발유가 뚝뚝 떨어집니다.
- `DRIPS_NAPALM` 이동 중에 가끔 네이팜탄이 떨어집니다.
- `ELECTRIC` 비무장 공격자에게 충격을 줍니다.
- `ELECTRONIC` 예: 로봇; EMP 폭발 및 기타 것들의 영향을 받습니다.
- `FAT` 도살하면 지방이 생성될 수 있습니다.
- `FEATHER` 도축 시 깃털이 생성될 수 있습니다.
- `FIREPROOF` 발사에 면역입니다.
- `FIREY` 물건을 태우고 불에 면역됩니다.
- `FISHABLE` 낚시가 가능합니다.
- `FLAMMABLE` 몬스터는 불이 붙어 화상을 입고 주변 물체에 불이 퍼집니다.
- `FLIES` 날 수 있다(물 위 등)
- `FUR` 도살하면 모피가 나올 수 있습니다.
- `GOODHEARING` 대부분의 몬스터보다 더 많은 소리를 추구합니다.
- `GRABS` 공격에 사로잡힐 수도 있습니다!
- `GROUP_BASH` 강타 시 주변 몬스터의 도움을 받습니다.
- `GROUP_MORALE` 친구와 함께 있을 때 더 용기가 난다.
- `GUILT` 죽인 것에 대해 죄책감을 느낀다.
- `HARDTOSHOOT` 원거리 공격의 경우 MS_TINY보다 한 사이즈 더 작습니다.
- `HEARS` 당신의 말을 들을 수 있습니다.
- `HIT_AND_RUN` 근접 공격 후 몇 턴 동안 도망칩니다.
- `HUMAN` 살아 있는 한 살아있는 인간이다.
- `MF_CARD_OVERRIDE` 기계는 아니지만 같은 방법으로 신분증을 이용해 아군으로 전환 가능
  그 기계는 할 수 있습니다.
- `CONSOLE_DESPAWN` 근처 콘솔이 제대로 해킹되면 소멸됩니다.
- `IMMOBILE` 움직이지 않고 비특수 공격(예: 포탑)을 사용하지 않습니다.
- `STATIONARY` 고정되어 있지만 반격할 것입니다(예: 훈련용 인형 )
- `ID_CARD_DESPAWN` 근처 콘솔에서 과학 ID 카드를 사용하면 사라집니다.
- `INTERIOR_AMMO` 몬스터 내부에는 탄약이 포함되어 있어 발사 시 장전할 필요가 없습니다. 탄약을 방지합니다.
  비활성화 시 삭제됩니다.
- `KEENNOSE` 후각이 예민해요.
- `LARVA` 생물은 유충이다. 현재는 토막 및 혈액 취급에 사용됩니다.
- `LEATHER` 도살하면 가죽이 생산될 수 있습니다.
- `LOUDMOVES` Mkes는 날아가더라도 소음을 ~2배 더 크게 움직입니다.
- `MECH_RECON_VISION` 이 기계는 다음과 같은 경우 야간 시야와 향상된 지도 시야 반경을 제공합니다.
  조종.
- `MECH_DEFENSIVE` 이 기계는 조종할 때 당신을 철저히 보호할 수 있습니다.
- `MILITARY_MECH` 산업용 신분증 대신 군인 신분증을 요구하겠습니다. 실제로는 없습니다
  기계가 되는 것.
- `MILKABLE` 착유하면 우유가 생성됩니다.
- `NIGHT_INVISIBILITY` 몬스터가 한 타일 이상 떨어져 있고 조명이 켜져 있으면 몬스터가 보이지 않게 됩니다.
  해당 타일은 LL_LOW 이하입니다. 가시성은 야간 시력의 영향을 받지 않습니다.
- `NOGIB` 큰 데미지로 죽였을 때 토막/고기 덩어리가 남지 않습니다.
- `NOHEAD` 얼굴 사진 금지!
- `NO_BREATHE` 생물은 익사할 수 없으며 가스, 연기 또는 독에 의해 피해를 입지 않습니다.
- `NO_BREED` 생물은 번식 데이터가 있음에도 불구하고 번식하지 않습니다. - 사용시 유용합니다.
  성인 생물의 어린이 버전을 만들기 위한 복사
- `NO_FUNG_DMG` 생물은 곰팡이 포자에 면역이며 곰팡이가 생길 수 없습니다.
- `PAY_BOT` 생물은 제한된 시간 동안 전자화폐로 애완동물로 변신할 수 있습니다.
- `PET_MOUNTABLE` 생물은 타거나 하네스에 부착할 수 있습니다.
- `PET_HARNESSABLE`생물은 하네스에 부착할 수 있습니다.
- `NULL` 소스 전용입니다.
- `PACIFIST` 저 괴물은 절대로 근접 공격을 하지 않습니다.
- `PARALYZE` 공격은 독으로 플레이어를 마비시킬 수 있습니다.
- `PLASTIC` 강타로 받는 피해가 50%, 66%, 75% 감소합니다. 히트할 때마다 무작위로 선택됩니다.
- `POISON` 먹으면 독하다.
- `PUSH_MON` 생물을 밀어낼 수 있습니다.
- `QUEEN` 죽으면 지역 주민들도 죽기 시작합니다.
- `REVIVES` 몬스터의 시체는 잠시 후 부활합니다.
- `RIDEABLE_MECH` 이 몬스터는 조종이 가능한 메카슈트입니다.
- `SEES` 당신을 볼 수 있습니다(달리거나 따라갈 것입니다).
- `SHEARABLE` 이 몬스터는 양털을 깎을 수 있습니다.
- `SLUDGEPROOF` 슬러지 트레일의 영향을 무시합니다.
- `SLUDGETRAIL` 몬스터가 이동할 때 슬러지 트랩 흔적을 남기게 합니다.
- `SMELLS` 냄새가 날 수 있어요.
- `STUMBLES` 움직이다가 비틀거린다.
- `SUNDEATH` 햇빛을 받으면 죽습니다.
- `SWARMS` 그룹화되어 느슨한 무리를 형성합니다.
- `SWIMS` 물을 이동점 50 지형으로 취급합니다.
- `VENOM` 공격하면 플레이어가 중독될 수 있습니다.
- `VERMIN` 중요하지 않은 몬스터에 대한 더 이상 사용되지 않는 플래그로 인해 이제 로드가 불가능합니다.
- `VOLATILE` 화염 피해를 입으면 항상 발화되며, 높은 확률로 막대한 피해를 입힙니다.
  총알과 전기 피해를 입으면 피해량이 증가하고 이 괴물에 불이 붙을 수도 있습니다.
- `FACTION_MEMORY` 팩션에 대한 분노를 별도로 계산하여 한 팩션(예: 좀비)에 대한 분노를 계산합니다.
  다른 사람(예: 플레이어)에게 유출되지 않습니다. 즉, 공격을 받을 때까지 플레이어에 대해 진정으로 중립적입니다. 사용 사례: 초보자 플레이어를 펄프화하는 것을 방지하기 위해 거리에 생성되는 매우 위험한 몬스터를 만듭니다.
- `WARM` 온혈.
- `WEBWALK` 웹을 파괴하지 않습니다.
- `WOOL` 도축하면 양모가 생산될 수 있습니다.

### Monster Defense and Attacks

- `ACIDSPLASH` 공격자에게 산을 뿌리세요.
- `NONE` 특별한 공격백은 없습니다.
- `RETURN_FIRE` 보이지 않는 공격자에게 맹목적으로 사격합니다.
- `REVENGE_AGGRO` 목표물에 아군을 어그로로 만듭니다.
- `ZAPBACK` 타격 시 충격 공격자.

### Sizes

- `HUGE` 탱크
- `LARGE` 소
- `MEDIUM` 인간
- `SMALL` 개
- `TINY` 다람쥐

### Special attacks

일부 특수 공격은 도구 및 무기에 대한 유효한 사용 동작이기도 합니다. `monsters.json`을 참조하세요.
이러한 공격을 사용하는 방법에 대한 예입니다. 더 많은 특수 공격에 대해서는 `monster_attacks.json`을 참조하세요.
예를 들어, 찔리고 긁히는 것입니다.

- `ACID_ACCURATE` 장거리에서는 정확도가 높지만 가까이에서는 정확도가 떨어지는 산을 발사합니다.
- `ACID_BARF` Barfs는 부식되고 눈을 멀게 하는 산입니다.
- `ACID` 산성을 뱉어라.
- `ANTQUEEN` 부화/성장: `egg > ant > soldier`.
- `BIO_OP_BIOJUTSU` 무작위 특수 무술 동작으로 공격하세요.
- `BIO_OP_TAKEDOWN` 특수 무술 테이크다운 동작으로 공격하세요.
- `BIO_OP_DISARM` 특별한 무술 무장해제 동작으로 공격하세요.
- `BIO_OP_IMPALE` 강력한 무술기법으로 공격한다.
- `BITE` 깊은 감염 상처를 유발할 수 있는 물기 공격입니다.
- `BMG_TUR` Barrett .50BMG 소총이 발사됩니다.
- `BOOMER_GLOW` 빛나는 담즙을 뱉어보세요.
- `BOOMER` 담즙을 뱉어보세요.
- `BRANDISH` 플레이어에게 칼을 휘두르세요.
- `BREATHE` `breather`을 생성합니다.
- `CALLBLOBS` 이 몬스터를 방어하기 위해 근처 블롭의 2/3를 호출하고 이후에는 근처 블롭의 1/3을 보냅니다.
  플레이어.
- `CHICKENBOT` LEGACY - 로봇은 거리에 따라 테이저, M4, MGL로 공격할 수 있습니다.
- `COPBOT` 경찰봇(Cop-bot)이 경고를 하고 플레이어에게 태즈(taze)를 가합니다.
- `DANCE` 몬스터가 춤을 춘다.
- `DARKMAN` 어둠과 망령이 스폰될 수 있습니다.
- `DERMATIK_GROWTH` 더마틱 유충이 성충으로 성장합니다.
- `DERMATIK` 플레이어에게 피부약 알을 낳으려고 시도합니다.
- `DISAPPEAR` 환각이 사라진다.
- `DOGTHING` 개 _thing_이 촉수개로 스폰됩니다.
- `FEAR_PARALYZE` 플레이어를 공포로 마비시킵니다.
- `FLAMETHROWER` 불의 흐름을 발사합니다.
- `FLESH_GOLEM` 발톱으로 플레이어를 공격하고, 공격이 연결되면 `downed` 질병에 걸립니다.
- `FORMBLOB` 얼룩을 생성합니까?
- `FRAG_TUR` MGL이 파편탄을 발사합니다.
- `FUNGUS_BIG_BLOSSOM` 화재 진압 곰팡이 연무를 확산시킵니다.
- `FUNGUS_BRISTLE` 곰팡이 감염을 일으킬 수 있는 가시덩굴 공격을 수행합니다.
- `FUNGUS_CORPORATE` Crazy Cataclysm에서만 사용됩니다. 없이 사용하면 런타임 오류가 발생합니다.
  생물 위에 SpOreos를 생성합니다.
- `FUNGUS_FORTIFY` 곰팡이 울타리를 키우고 플레이어를 마이커스 임계값 경로로 전진시킵니다.
- `FUNGUS_GROWTH` 어린 곰팡이를 성체로 성장시킵니다.
- `FUNGUS_HAZE` 곰팡이 밭을 생성합니다.
- `FUNGUS_INJECT` 곰팡이 감염을 일으킬 수 있는 바늘 공격을 수행합니다.
- `FUNGUS_SPROUT` 곰팡이 벽이 자랍니다.
- `FUNGUS` 곰팡이 포자를 방출하고 플레이어를 감염시키려고 합니다.
- `GENERATOR` 체력을 회복시킨다.
- `GENE_STING` 연결되면 돌연변이를 일으키는 플레이어에게 다트를 발사합니다.
- `GRAB` 대상을 잡고 드래그하세요.
- `GRAB` 플레이어를 붙잡아 적중 시 속도를 늦추고 이동 및 회피를 불가능하게 만들고 차단합니다.
  더 열심히.
- `GROWPLANTS` 덤불을 생성하거나 `> young tree > tree`으로 승격합니다.
- `GROW_VINE` 덩굴식물이 자란다.
- `HOWL` "귀청이 터질 듯한 울부짖음!"
- `JACKSON` 좀비를 좀비 댄서로 변환합니다.
- `LASER` 레이저 터렛이 발사됩니다.
- `LEAP` 장애물 없는 타일로 도약하세요.
- `LONGSWIPE` 데미지가 높은 발톱 공격을 하며, 멀리 있는 곳도 타격할 수 있다.
- `LUNGE` 일정 거리에서 점프 공격을 하여 대상을 쓰러뜨릴 수 있습니다.
- `MULTI_ROBOT` 로봇은 상황에 따라 테이저, 화염방사기, M4, MGL 또는 120mm 대포로 공격할 수 있습니다.
  거리.
- `NONE` 특별한 공격은 없습니다.
- `PARA_STING` 플레이어에게 마비 다트를 발사합니다.
- `PARROT` `speech.json`에 정의된 음성을 앵무새로 만들고 행 중 하나를 무작위로 선택합니다. "speaker"
  몬스터 ID를 가리킨다.
- `PARROT_AT_DANGER` PARROT과 동일한 기능을 수행하지만 생물이 화난 모습을 본 경우에만 가능합니다.
  적대 세력의 괴물.
- `PAID_BOT` PAY_BOT 플래그가 있는 생물의 경우 애완동물 효과가 소진되면 동맹 상태를 제거합니다.
- `PHOTOGRAPH` 선수 사진을 찍으세요. 로봇 공격을 유발합니까?
- `PLANT` 곰팡이 포자는 씨앗을 받아 곰팡이로 성장합니다.
- `PULL_METAL_WEAPON` 플레이어의 손에서 철이나 강철로 만들어진 무기를 꺼냅니다.
- `RANGED_PULL` 대상을 공격자 쪽으로 끌어 당깁니다.
- `RATKING` 질병을 일으킨다 `rat`
- `RATTLE` "치찰하는 덜거덕거리는 소리!"
- `RESURRECT` 죽은자를 다시 살리십니다.
- `RIFLE_TUR` 소총 포탑이 발사됩니다.
- `RIOTBOT` 최루가스나 이완가스를 뿌리고, 선수들에게 수갑을 채우고, 눈부신 섬광을 사용할 수 있습니다.
- `SCIENCE` 다양한 과학/기술 관련 공격(예: 맨해킹, 방사성빔 등)
- `SEARCHLIGHT` 탐조등으로 표적을 추적합니다.
- `SHOCKING_REVEAL` 번개를 쏘고, 충격적인 사실을 밝힙니다! 네 번째 벽
  파괴. Crazy Cataclysm에서만 사용됩니다.
- `SHOCKSTORM` 번개를 발사합니다.
- `SHRIEK_ALERT` "아주 끔찍한 비명소리야!"
- `SHRIEK_STUN` "굉장한 비명!"은 작은 충격을 일으키고 기절을 일으킬 수 있습니다.
- `SHRIEK` "끔찍한 비명소리!"
- `SLIMESPRING` 플레이어의 사기를 높여주고 물림 및 출혈 효과를 치료할 수 있습니다.
- `SMASH` 대상을 강타하여 막대한 피해를 입히고, 다음과 같은 타일 수만큼 날아갑니다.
  `("melee_dice" * "melee_dice_sides" * 3) / 10`.
- `SMG` SMG 포탑이 발사되었습니다.
- `SPIT_SAP` 수액을 뱉어라.
- `STARE` 플레이어를 응시하고 텔레글로우를 가합니다.
- `STRETCH_ATTACK` 원거리 관통 공격.
- `STRETCH_BITE` 원거리 물어뜯기 공격.
- `SUICIDE` 공격 후 사망.
- `TAZER` 플레이어에게 충격을 주세요.
- `TENTACLE` 플레이어에게 촉수를 휘두릅니다.
- `TRIFFID_GROWTH` 어린 트리피드는 어른으로 성장한다.
- `TRIFFID_HEARTBEAT` 플레이어 주변의 뿌리 벽을 성장시키고 무너뜨려 더 많은 몬스터를 생성합니다.
- `UPGRADE` 일반 좀비를 특수 좀비로 업그레이드합니다.
- `VINE` 덩굴로 공격한다.
- `VORTEX` 피해를 입히고 주변 생물을 던지는 소용돌이/토네이도를 형성합니다.

## Mutations

#### Mutation Flags

Mutation 플래그는 다른 플래그와 다른 JSON 유형을 사용합니다. json/flags_mutation.json을 참조하세요. 주요한
차이점은 `conflicts` 및 `requires`이 추가할 수 있는 유일한 추가 속성이라는 점입니다.
그들에게.

다음은 현재 게임 코드에서 사용되는 모든 특성 플래그를 보여줍니다. 특성 플래그도 다음과 같아야 합니다.
NPC 대화 조건에서 사용하려면 JSON에서 정의해야 합니다.

- `CANNIBAL` 인간 시체를 도살해도 사기 저하가 없으며 부정적인 책 사기를 전환합니다.
  BOOK_CANNIBAL 플래그를 양수로 설정하면 인간 고기에 대한 경고를 건너뜁니다. 참고: 이것은 건너뛰기만 합니다.
  경고에 따르면, 인간 육체를 먹는 실제 사기 효과는 여전히 관련 중 하나를 필요로 합니다.
  특성. 이 깃발이 있는 사용자 지정 특성은 경고를 건너뛰고 사기 저하를 겪습니다.
- `NEED_ACTIVE_TO_MELEE` 이 플래그가 있는 돌연변이는 비무장 보너스만 제공합니다.
  켜졌습니다.
- `NO_RADIATION` 이 돌연변이는 방사선에 대한 면역성을 부여합니다.
- `NO_THIRST` 갈증은 음식이나 음료로 해결되지 않습니다.
- `NON_THRESH` 이 플래그가 있는 돌연변이는 돌연변이 강도에 포함되지 않습니다.
  돌연변이 임계값을 위반할 수 있는 능력)에 속하는 것으로 간주되는 모든 카테고리에 적용됩니다.
- `PRED1` 좀비 시체를 황폐화시키는 사기 영향을 감소시키고, 살인의 사기 영향을 감소시킵니다.
  `GUILT` 플래그가 있는 몬스터입니다.
- `PRED2` 전투로 얻는 경험치 증가, 전투 스킬의 스킬 녹을 무효화(스킬 녹이 있는 경우)
  활성화), 좀비 시체를 황폐화시키는 사기 영향을 감소시키고, 살인의 사기 영향을 감소시킵니다.
  `GUILT` 플래그가 있는 몬스터입니다.
- `PRED3` 전투로 얻는 경험치 증가, 전투 스킬의 스킬 녹을 무효화(스킬 녹이 있는 경우)
  활성화됨), 이미 우울한 상태에서 좀비를 괴롭히는 것에 대한 내성이 증가하고 사기에 미치는 영향이 무효화됩니다.
  `GUILT` 플래그로 몬스터를 죽이는 것입니다.
- `PRED4` 전투로 획득하는 EXP가 증가하고, 전투로 획득한 EXP가 집중력에 영향을 미치지 않도록 방지하며,
  전투 스킬의 스킬 녹을 무효화하고(스킬 녹이 활성화된 경우) 엔즐라빙의 사기 영향을 무효화합니다.
  좀비 시체, 이미 우울한 상태에서 좀비를 괴롭히는 것에 대한 내성을 증가시키고 사기를 무효화합니다.
  `GUILT` 플래그로 몬스터를 죽이는 것이 미치는 영향.
- `PSYCHOPATH` 시체 도살로 인한 사기 저하 없음, 부정적인 책 사기로 전환
  BOOK_CANNIBAL 및 MORBID 플래그를 양수로 설정합니다.
- `SAPIVORE` 인간의 시체를 도살해도 사기 저하가 없으며 부정적인 책 사기를 전환합니다.
  BOOK_CANNIBAL 플래그를 양수로 설정합니다.
- `SILENT_SPELL` `VERBAL` 플래그를 사용하여 주문에 대한 입의 부담의 부정적인 영향을 무효화합니다.
- `SPIRITUAL` INSPIRATIONAL 플래그가 있는 책을 읽으면 보너스 사기를 부여합니다.
  부정적이라면 긍정적이다.
- `SUBTLE_SPELL` `SOMATIC` 플래그를 사용하면 주문에 대한 팔 부담의 부정적인 영향을 무효화합니다.
- `UNARMED_BONUS` 비무장 강타에 보너스를 받고 unarmed_skill/2만큼 피해를 최대 4까지 삭감합니다.
- `MUTATION_FLIGHT` 지정된 비용에 대한 대가로 사용자에게 비행권을 부여합니다.
- `FLIGHT_ALWAYS_ACTIVE` 사용자에게 항상 비행 권한을 부여합니다.

### Categories

이러한 분기는 `dreams.json`의 `dreams` 범주에 대한 유효한 항목이기도 합니다.

- `MUTCAT_ALPHA` "You feel...better. Somehow."
- `MUTCAT_BEAST` "Your heart races and you see blood for a moment."
- `MUTCAT_BIRD` "Your body lightens and you long for the sky."
- `MUTCAT_CATTLE` "Your mind and body slow down. You feel peaceful."
- `MUTCAT_CEPHALOPOD` "Your mind is overcome by images of eldritch horrors...and then they pass."
- `MUTCAT_CHIMERA` "포효하고, 몸을 녹이고, 물고, 펄럭여야 합니다. 지금."
- `MUTCAT_ELFA` "Nature is becoming one with you..."
- `MUTCAT_FISH` "You are overcome by an overwhelming longing for the ocean."
- `MUTCAT_INSECT` "윙윙거리는 소리가 들리고 몸이 굳어지는 느낌이 듭니다."
- `MUTCAT_LIZARD` "심장박동에 몸이 시원해집니다."
- `MUTCAT_MEDICAL` "혈관을 통해 혈액이 흐르는 것과 이상한 약이 공급되는 것을 느낄 수 있습니다.
  느낌이 당신의 감각을 압도합니다."
- `MUTCAT_PLANT` "You feel much closer to nature."
- `MUTCAT_RAPTOR` "Mmm...sweet bloody flavor...tastes like victory."
- `MUTCAT_RAT` "You feel a momentary nausea."
- `MUTCAT_SLIME` "Your body loses all rigidity for a moment."
- `MUTCAT_SPIDER` "You feel insidious."
- `MUTCAT_TROGLOBITE` "당신은 숨을 수 있는 시원하고 어두운 곳을 원합니다."

## Overmap

### Overmap connections

- `ORTHOGONAL` 연결은 일반적으로 직선을 선호하며 가능한 한 회전을 피합니다.

### Overmap specials

#### Flags

- `BEE` 위치는 벌과 관련이 있습니다. 위치를 분류하는 데 사용됩니다.
- `BLOB` 위치는 정의된 위치에서 "blob" 바깥쪽에 위치해야 합니다.
  인접한 위치.
- `CLASSIC` 클래식 좀비가 활성화되면 위치가 허용됩니다.
- `FUNGAL` 위치는 곰팡이와 관련이 있습니다. 위치를 분류하는 데 사용됩니다.
- `TRIFFID` 위치는 트리피드와 관련이 있습니다. 위치를 분류하는 데 사용됩니다.
- `LAKE` 위치는 호수에 배치되며 오버맵이 일치하지 않으면 배치 시 무시됩니다.
  호수 지형을 포함합니다.
- `RIVER` 위치는 호수에 배치되며 오버맵이 일치하지 않으면 배치 시 무시됩니다.
  호수 지형을 포함합니다.
- `UNIQUE` 위치는 고유하며 오버맵당 한 번만 발생합니다. `occurrences`이 다음으로 재정의되었습니다.
  확률을 정의합니다(예: `"occurrences" : [75, 100]`은 75%입니다).
- `ENDGAME` 특수 배치 중에는 위치가 가장 높은 우선순위를 가지며 다음 사항의 영향을 받지 않습니다.
  모든 발생 정규화.
- `RESTRICTED` 위치는 시작 위치로 생성되지 않습니다. 다음을 목적으로 함(그러나 이에 국한되지 않음)
  불완전하게 중첩된 특수 문자와 함께 사용합니다.

### Overmap terrains

#### Flags

- `KNOWN_DOWN` 알려진 방법이 있습니다.
- `KNOWN_UP` 알려진 방법이 있습니다.
- `LINEAR` ID_straight, ID_curved, ID_tee, ID_four_way를 사용하는 도로 등의 경우.
- `NO_ROTATE` 지형은 회전할 수 없습니다(ID_north, ID_east, ID_south 및 ID_west 인스턴스는 회전할 수 없습니다).
  생성되지 않으며 ID만 생성됩니다.
- `RIVER` 강타일입니다.
- `SIDEWALK` 도로에 인접한 측면에 보도가 있습니다.
- `IGNORE_ROTATION_FOR_ADJACENCY` 이 OMT의 mapgen이 이웃 확인을 수행할 때 방향은
  mapgen 자체의 회전을 설명하기 위해 회전하는 것이 아니라 절대값으로 처리됩니다.
  아마도 하드코딩된 mapgen에만 유용할 것입니다.
- `LAKE` 이 위치를 지도 작성 목적으로 유효한 호수 지형으로 간주하십시오.
- `LAKE_SHORE` 이 위치를 지도 작성 목적으로 유효한 호숫가 지형으로 간주하십시오.
- `SOURCE_FUEL` NPC AI의 경우 이 위치에는 약탈에 필요한 연료가 있을 수 있습니다.
- `SOURCE_FOOD` NPC AI의 경우 이 위치에 약탈용 음식이 있을 수 있습니다.
- `SOURCE_FARMING` NPC AI의 경우 이 위치에는 약탈에 유용한 농자재가 있을 수 있습니다.
- `SOURCE_FABRICATION` NPC AI의 경우 이 위치에는 제작 도구 및 구성 요소가 포함될 수 있습니다.
  약탈.
- `SOURCE_GUN` NPC AI의 경우 이 위치에 약탈용 총이 있을 수 있습니다.
- `SOURCE_AMMO` NPC AI의 경우 이 위치에 약탈용 탄약이 있을 수 있습니다.
- `SOURCE_BOOKS` NPC AI의 경우 이 위치에 약탈용 책이 있을 수 있습니다.
- `SOURCE_WEAPON` NPC AI의 경우 이 위치에는 약탈용 무기가 있을 수 있습니다.
- `SOURCE_FORAGE` NPC AI의 경우 이 위치에 채집할 식물이 있을 수 있습니다.
- `SOURCE_COOKING` NPC AI의 경우 이 위치에는 도움이 되는 유용한 도구와 재료가 포함될 수 있습니다.
  요리.
- `SOURCE_TAILORING` NPC AI의 경우 이 위치에는 재봉에 유용한 도구가 포함될 수 있습니다.
- `SOURCE_DRINK` NPC AI의 경우 이 위치에 약탈용 음료가 있을 수 있습니다.
- `SOURCE_VEHICLES` NPC AI의 경우 이 위치에는 약탈할 수 있는 차량/부품/차량 도구가 포함될 수 있습니다.
- `SOURCE_ELECTRONICS` NPC AI의 경우 이 위치에 전리품을 얻을 수 있는 유용한 전자 제품이 있을 수 있습니다.
- `SOURCE_CONSTRUCTION` NPC AI의 경우 이 위치에는 다음 작업에 유용한 도구/구성요소가 포함될 수 있습니다.
  건설.
- `SOURCE_CHEMISTRY` NPC AI의 경우 이 위치에는 유용한 화학 도구/구성요소가 포함될 수 있습니다.
- `SOURCE_CLOTHING` NPC AI의 경우 이 위치에는 약탈할 수 있는 유용한 의류가 있을 수 있습니다.
- `SOURCE_SAFETY` NPC AI의 경우 이 위치는 안전/보호되며 기지로 좋은 장소일 수 있습니다.
- `SOURCE_ANIMALS` NPC AI의 경우 이 위치에는 농사/승마에 유용한 동물이 포함될 수 있습니다.
- `SOURCE_MEDICINE` NPC AI의 경우 이 위치에는 약탈에 유용한 약이 있을 수 있습니다.
- `SOURCE_LUXURY` NPC AI의 경우 이 위치에는 판매/보관할 귀중하고 기분 좋은 품목이 포함될 수 있습니다.
- `SOURCE_PEOPLE` NPC AI의 경우 이 위치에는 다른 생존자가 있을 수 있습니다.
- `RISK_HIGH` NPC AI의 경우 이 위치는 연구실/슈퍼스토어 등 관련 위험이 높습니다.
- `RISK_LOW` NPC AI의 경우 이곳은 한적하고 외진 곳이라 안전한 것 같습니다.
- `GENERIC_LOOT` 위의 항목 중 하나가 포함될 수 있는 장소이지만 빈도는 더 낮습니다.
  보통 집.
- `IS_BRIDGE` mapgen에서 다리로 확장될 예정이며, 이 객체의 ID 뒤에 _under, _road, head_ground 및 head_ramp가 오는 지형을 정의해야 하며 _center_under도 정의할 수 있습니다.

## Recipes

### Categories

- `CC_AMMO`
- `CC_ARMOR`
- `CC_CHEM`
- `CC_DRINK`
- `CC_ELECTRONIC`
- `CC_FOOD`
- `CC_MISC`
- `CC_WEAPON`

### Flags

- `ALLOW_ROTTEN` 부패하지 않는 제품을 제작할 때 썩은 부품을 명시적으로 허용하세요.
- `BLIND_EASY` 빛이 거의 또는 전혀 없어도 쉽게 제작할 수 있습니다.
- `BLIND_HARD` 빛이 거의 또는 전혀 없이도 제작이 가능하지만 어렵습니다.
- `SECRET` 스킬레벨이 높아서 캐릭터 생성시 자동으로 학습되지 않습니다.
- `UNCRAFT_LIQUIDS_CONTAINED` 기본 컨테이너에 액체 아이템을 생성합니다.
- `FULL_MAGAZINE` 이 레시피에 탄창이 필요한 경우 꽉 찬 탄창이 필요합니다. 해체용
  레시피를 분해하면 전체 탄창이 생성됩니다.

## Scenarios

### Flags

- `ALLOW_OUTSIDE` 플레이어를 건물 외부에 배치할 수 있어 야외 시작에 유용합니다.
- `BAD_DAY` 플레이어는 술에 취해 우울하고 독감에 걸려 게임을 시작합니다.
- `BOARDED` 판자가 있는 건물에서 시작합니다(창문은 판자로 치고, 이동식 가구는
  창문과 문).
- `BORDERED` 초기 시작 위치는 거대한 암석 벽으로 둘러싸여 있습니다.
- `CHALLENGE` 게임은 무작위 게임 유형에서 이 시나리오를 선택하지 않습니다.
- `CITY_START` 시나리오는 월드 옵션의 도시 크기 값이 0 이상인 경우에만 사용할 수 있습니다.
- `FIRE_START` 플레이어는 근처에 불이 있는 상태에서 게임을 시작합니다.
- `HELI_CRASH` 플레이어는 다양한 팔다리 부상을 입은 채 게임을 시작합니다.
- `INFECTED` 플레이어가 감염되어 게임을 시작합니다.
- `LONE_START` NPC 스폰 시작 옵션이 "Scenario-based"으로 전환되면 이 시나리오는 실행되지 않습니다.
  게임 시작 시 동료 NPC를 생성합니다.
- `SCEN_ONLY` 직업은 해당 시나리오의 일부로만 선택할 수 있습니다.
- `VEH_GROUNDED` 직업 차량은 띄울 수 있음에도 불구하고 강제로 지상에 위치함

#### Season Flags

- `AUT_START` ... 가을부터 시작합니다.
- `SPR_START` ... 봄부터 시작하세요.
- `SUM_ADV_START` ... 대격변 이후 두 번째 여름을 시작하세요.
- `SUM_START` ... 여름부터 시작합니다.
- `WIN_START` ... 겨울부터 시작하세요.

## Skills

### Tags

- `combat_skill` 해당 스킬은 전투 스킬로 간주됩니다. "PACIFIST", "PRED1"의 영향을 받습니다.
  "PRED2", "PRED3" 및 "PRED4" 특성.
- `contextual_skill` 스킬은 추상적이며 상황에 따라 다릅니다(간접적인 항목).
  적용된). 플레이어나 NPC 모두 이를 소유할 수 없습니다.
- `unaffected_by_focus` 이 기술을 연습해도 집중력이 떨어지지 않으며, 반대로 집중력도 떨어지지 않습니다.
  이 스킬 레벨 업 속도에 영향을 미칩니다(긍정적 또는 부정적).
- `weapon_skill` NPC가 어떤 종류의 무기를 생성해야 하는지 결정하는 데 사용됩니다.

## Techniques

기술은 도구, 갑옷, 무기 및 기타 휘두를 수 있는 모든 것에 사용될 수 있습니다.

- `data/json/techniques.json`의 내용을 확인하세요.
- 기술은 무술 스타일에도 사용됩니다. `data/json/martialarts.json`을 참조하세요.

### WBLOCK_X

다음 무기 기술에는 몇 가지 추가 용도가 있습니다. 방어하는 기술들입니다
아이템이 근접 공격을 차단하는 데 도움이 되도록 허용하고 몇 가지 추가 특수 용도를 사용합니다.

- `WBLOCK_1` "Medium blocking ability"
- `WBLOCK_2` "High blocking ability"
- `WBLOCK_3` "Very high blocking ability"

이러한 기술 중 하나를 사용하는 아이템을 사용하여 차단으로 감소된 피해에 보너스를 제공할 수 있습니다.
비교하거나 `BLOCK_WHILE_WORN` 플래그가 있는 방어구도 이 보너스를 사용할 수 있습니다.
아이템을 착용하고 방패 역할을 합니다. 추가적으로, 조합된 아이템을 휘두르거나 착용하는 행위
이러한 기술 중 하나와 해당 플래그를 사용하면 아이템이 신체를 겨냥한 발사체를 차단할 수 있습니다.
품목이 덮지 않는 부분(또는 다음 요건을 충족하는 휘두르는 품목의 경우 덮지 않는 부분)
그 전제 조건). 이런 일이 발생할 가능성은 `coverage` 비율에 따라 결정됩니다.
일반 방어구 값으로 사용되는 아이템으로, 어떤 차단 기술에 따라 페널티가 감소합니다.
그것은 소유하고 있습니다. 다리를 타격하는 탄환을 차단할 가능성이 있습니다. (다시 말하지만, 갑옷이
이미 기본적으로 다리를 덮도록 설정되어 있으며, 이 경우 `coverage`을 정상적으로 사용합니다.)
처벌을 받았습니다. (JSON 작성자가 고안한 어떤 이유로든,
역장 아이템(예: 역장 아이템) 아이템은 이미 갑옷으로 발을 덮고 있는 방패입니다.

| 기술     | 요격 가능성(머리, 몸통, 상대 팔 등) | 요격 가능성(다리)  |
| -------- | ----------------------------------- | ------------------ |
| WBLOCK_1 | 기본 보장 값의 90%                  | 기본 보장 값의 75% |
| WBLOCK_2 | 기본 보장 값의 90%                  | 기본 보장 값의 75% |
| WBLOCK_3 | 기본 보장 값의 90%                  | 기본 보장 값의 75% |

## Tools

### Flags

근접 플래그는 도구 플래그와 완벽하게 호환되며 그 반대도 마찬가지입니다.

- `ACT_ON_RANGED_HIT` 아이템을 던지거나 발사하면 활성화된 후 즉시 처리되어야 합니다.
  땅에 스폰되는 경우.
- `ALLOWS_REMOTE_USE` 이 아이템은 선택하지 않고도 인접한 타일에서 활성화하거나 다시 로드할 수 있습니다.
  위로.
- `BELT_CLIP` 해당 품목은 적절한 크기의 벨트 고리(벨트 고리)에 클립으로 고정하거나 걸 수 있습니다.
  루프는 max_volume 및 max_weight 속성에 의해 제한됩니다.
- `BOMB` 원격 조종 폭탄일 수도 있습니다.
- `CABLE_SPOOL` 이 항목은 케이블 스풀이므로 그대로 처리해야 합니다. 내부 "state"이 있습니다.
  "attach_first" 또는 "pay_out_cable" 상태일 수 있는 변수 -- 후자의 경우
  `max_charges - dist(here, point(vars["source_x"], vars["source_y"]))`으로 청구됩니다. 만약 이
  결과가 0 또는 음수인 경우 상태를 다시 "attach_first"으로 설정합니다.
- `CANNIBALISM` 해당 아이템은 인육이 포함된 식품이며, 적용 가능한 모든 효과가 적용됩니다.
  소비될 때.
- `CHARGEDIM` 조명이 켜지면 충전량이 20% 남았을 때부터 빛의 세기가 약해집니다.
- `DIG_TOOL` 사용하면 플레이어가 들어갈 때 바위나 벽과 같은 지형을 철저하게 파냅니다. 만약에
  아이템에도 `POWERED` 플래그가 있으면 발굴 속도가 빨라지지만 활성화된 것처럼 아이템의 탄약을 소모합니다.
  그것.
- `FIRESTARTER` 약간의 어려움이 있으면 아이템이 발사됩니다.
- `FIRE` 아이템이 즉시 불을 붙입니다.
- `FISH_GOOD` 낚시에 사용할 경우 좋은 도구입니다(일치하는 use_action이 필요함).
  설정되었습니다.)
- `FISH_POOR` 낚시에 사용하면 좋지 않은 도구입니다(일치하는 use_action이 필요함).
  설정되었습니다.)
- `HAS_RECIPE` E-Ink 태블릿에서 현재 레시피를 표시하고 있음을 나타내는 데 사용됩니다.
- `IS_UPS` 품목은 통합 전원 공급 장치입니다. 활성 아이템 처리에 사용됩니다.
- `LIGHT_[X]` 광도 `[X]`으로 해당 영역을 조명합니다. 여기서 `[X]`은 광도 값입니다.
  (예: `LIGHT_4` 또는 `LIGHT_100`) 참고: 이 플래그는 `itype::light_emission` 필드를 설정한 다음
  제거됨(`has_flag`을 사용하여 찾을 수 없음)
- `MC_MOBILE`, `MC_RANDOM_STUFF`, `MC_SCIENCE_STUFF`, `MC_USED`, `MC_HAS_DATA` 메모리 카드 관련
  플래그, `iuse.cpp` 참조
- `NO_DROP` 항목은 지도 타일에 개별 항목으로 존재해서는 안 됩니다(다른 항목에 포함되어야 함).
  목)
- `NO_UNLOAD` 언로드할 수 없습니다.
- `POWERED` ON하면 사용자의 힘이 아닌 아이템 자체의 전원을 사용하게 됩니다.
- `RADIOCARITEM` 원격 조종 차량에 물건을 넣을 수 있습니다.
- `RADIOSIGNAL_1` 무선 신호 1(빨간색)에 따라 활성화됩니다.
- `RADIOSIGNAL_2` 무선 신호 2(파란색)에 따라 활성화됩니다.
- `RADIOSIGNAL_3` 무선 신호 3(녹색)에 따라 활성화됩니다.
- `RADIO_ACTIVATION` 항목은 리모콘으로 활성화할 수 있습니다(RADIOSIGNAL_*도 필요함).
- `RADIO_INVOKE_PROC` 무선 신호를 통해 활성화되면 아이템의 충전량이 제거됩니다.
  폭탄 카운트다운을 우회하는 데 사용할 수 있습니다.
- `RADIO_CONTROLLABLE` 리모콘으로 이동이 가능합니다.
- `RADIO_MODABLE` 항목을 무선 작동 항목으로 만들 수 있음을 나타냅니다.
- `RADIO_MOD` 해당 아이템이 무선 작동 아이템으로 제작되었습니다.
- `RECHARGE` 충전소가 있는 화물칸에 배치하면 요금이 부과됩니다.
- `SAFECRACK` 금고를 열 때 사용할 수 있는 아이템입니다.
- `USES_BIONIC_POWER` 이 아이템은 자체적으로 충전되지 않으며 플레이어의 생체 공학적 힘으로 작동됩니다.
- `USE_UPS` 항목은 UPS의 요금입니다. / 자체 요금 대신 UPS의 요금을 사용합니다.
- `NAT_UPS` USE_UPS에서 (UPS) 접미사를 비활성화합니다.
- `WATER_EXTINGUISH` 물 속에서나 강수 속에서도 소화될 수 있습니다. 아이템을 변환합니다(필수
  "reverts_to" 또는 use_action "transform" 설정).
- `WATER_DISABLE` 아이템이 물에 잠기면 되돌리고 비활성화됩니다.
- `WET` 물품이 젖었고 천천히 건조됩니다(예: 수건).
- `WIND_EXTINGUISH` 바람에 의해 소멸되는 상품입니다.
- `WRITE_MESSAGE` 표지판에 메시지를 쓰는 데 사용할 수 있는 아이템입니다.

### Flags that apply to items

이 플래그는 **항목 유형에는 적용되지 않습니다**.

이러한 플래그는 게임 코드에 의해 특정 항목(_모든_ 용접공이 아닌 해당 특정 용접공)에 추가됩니다.

- `COLD` 품목이 차갑습니다(EATEN_COLD 참조).
- `DIRTY` 아이템(액체)이 땅에 떨어졌고 이제 회복할 수 없을 정도로 더러워졌습니다.
- `FIELD_DRESS_FAILED` 미숙한 현장 드레싱으로 인해 시체가 손상되었습니다. 정육점 결과에 영향을 미칩니다.
- `FIELD_DRESS` 시체는 현장에서 옷을 입고 있었습니다. 정육점 결과에 영향을 미칩니다.
- `FIT` 부담을 하나 줄입니다.
- `FROZEN` 품목이 고체로 냉동되어 있습니다(냉동고에서 사용함).
- `HIDDEN_ITEM` 이 항목은 AIM에서 볼 수 없습니다.
- `HOT` 품목이 뜨겁습니다(EATEN_HOT 참조).
- `LITCIG` 불이 붙은 흡연 품목(담배, 담배 등)을 표시합니다.
- `NO_PARASITES` 식품->유형->식품->기생충에 설정된 기생충 수를 무효화합니다.
- `QUARTERED` 시체는 4등분되었습니다. 정육점 결과, 무게, 부피에 영향을 미칩니다.
- `REVIVE_SPECIAL` ... 플레이어가 근처에 있으면 시체가 부활합니다.
- `SPAWN_FRIENDLY` 애완동물이 낳은 알과 아이템으로 되돌아가는 애완동물 봇에 적용됩니다. 그 어떤 괴물이라도
  해당 알의 해치도 우호적으로 생성되며, 이 플래그가 표시된 배치 가능한 봇은 건너뜁니다.
  이미 한 번 올바르게 구성되었으므로 플레이어 기술을 확인합니다.
- `SPAWN_HOSTILE` `place_monster` 이 플래그가 있는 아이템은 대상 더미와 같이 항상 적대적인 몬스터를 배치합니다. SPAWN_FRIENDLY의 반대
- `USE_UPS` 이 도구에는 UPS 모드가 있으며 UPS에서 충전됩니다.
- `WARM` 뜨거운 곳과 뜨거운 곳 사이의 버퍼링으로 항목의 이동을 추적하는 데 사용되는 숨겨진 플래그입니다.
- `WET` 물품이 젖었고 천천히 건조됩니다(예: 수건).

## Vehicle Prototypes

### Flags

- `VEHICLE_HOTWIRE` 항상 핫와이어링 제어가 필요한 차량을 표시합니다.
- `VEHICLE_NO_HOTWIRE` 핫와이어링 제어(예: 자전거)가 필요하지 않은 차량을 표시합니다.
- `VEHICLE_UNLOCKED` 차량을 항상 잠금 해제된 상태로 생성하도록 표시하지만 핫와이어링이 필요할 수도 있습니다.
- `VEHICLE_LOCKED` 차량이 항상 잠겨 있고 핫와이어링이 필요할 수 있음을 표시합니다.
- `VEHICLE_NO_LOCKS` 차량에 잠금 장치가 자동으로 설치되지 않도록 표시합니다.

## Vehicle Parts

### Flags

- `ADVANCED_PLANTER` 이 화분은 씨앗을 흘리지 않으며, 파낼 수 없는 식물에 손상을 주지 않습니다.
  표면.
- `AISLE_LIGHT`
- `AISLE` 플레이어는 평소보다 속도 저하를 덜 받으며 이 부분을 이동할 수 있습니다.
- `ALTERNATOR` 차량에 장착된 배터리를 충전합니다. 있는 부분에만 설치 가능
  `E_ALTERNATOR` 플래그입니다.
- `ANCHOR_POINT` 안전하게 안전벨트를 부착할 수 있습니다.
- `ANIMAL_CTRL` 동물을 이용할 수 있습니다. 동물의 신체 유형을 지정하려면 HARNESS_bodytype 플래그가 필요합니다.
- `ARMOR` 충돌 시 위에 설치된 다른 차량 부품을 보호합니다.
- `ATOMIC_LIGHT`
- `AUTOCLAVE` 오토클레이브 역할을 합니다.
- `AUTOPILOT` 이 부분을 통해 차량이 간단한 자동 조종 장치를 사용할 수 있게 됩니다.
- `BALLOON` 리프팅 풍선 역할을 하며 높이 필드가 필요합니다.
- `BATTERY_MOUNT`
- `BED` 플레이어가 잠을 잘 수 있는 침대.
- `BEEPER` 차량이 후진할 때 소음이 발생합니다.
- `BELTABLE` 이 부분에 안전벨트를 부착할 수 있습니다.
- `BIKE_RACK_VEH` 인접한 단일 타일 너비의 차량을 병합하거나 단일 타일을 분할하는 데 사용할 수 있습니다.
  넓은 차량을 자기 차량으로 옮깁니다.
- `BOARDABLE` 차량이 이동하는 동안 플레이어는 안전하게 이 부분 위로 이동하거나 서 있을 수 있습니다.
- `CAMERA_CONTROL`
- `CAMERA`
- `CAPTURE_MOSNTER_VEH` 차량에 장착하여 몬스터를 포획하는 데 사용할 수 있습니다.
- `CARGO_LOCKING` 이 화물칸은 NPC가 접근할 수 없습니다. 있는 부분에만 설치 가능
  `LOCKABLE_CARGO` 플래그입니다.
- `DOOR_LOCKING` 이 부분은 전자 메뉴에서 활성화하면 비팩션 NPC와 몬스터에게 열리지 않습니다. 수 있습니다
  `OPENABLE` 플래그가 있는 부품에 설치됩니다.
- `CARGO` 화물 보관 구역.
- `CHIMES` 사용시 지속적인 소음이 발생합니다.
- `CIRCLE_LIGHT` 켜졌을 때 빛의 원형 반경을 투사합니다.
- `CONE_LIGHT` 켜졌을 때 빛의 원뿔을 투사합니다.
- `CONTROL_ANIMAL` 이 컨트롤은 동물이 끄는 차량(예:
  고삐 등).
- `CONTROLS` 차량을 제어하는 ​​데 사용할 수 있습니다.
- `COOLER` 이 부분을 토글하는 명령어가 별도로 있습니다.
- `COVERED` 화물 부품에 있는 물품이 빛을 방출하는 것을 방지합니다.
- `CRAFTER` "integrated_tools" 목록 필드 아래에 통합 도구를 정의할 수 있습니다.
- `CTRL_ELECTRONIC` 차량의 전기 및 전자 시스템을 제어합니다.
- `CONTROL_WITHOUT_HANDS` 운전 중 양손 무기를 발사할 수 있게 해줍니다. `STEERABLE` 플래그가 있는 부품에만 설치할 수 있습니다.
- `CURTAIN` `WINDOW` 표시된 부분 위에 설치할 수 있으며 블라인드와 동일한 기능을 합니다.
  건물의 창문에서 발견됩니다.
- `DIFFICULTY_REMOVE`
- `DOME_LIGHT`
- `DOOR_MOTOR` `OPENABLE` 플래그가 있는 부분에만 설치할 수 있습니다.
- `DROPPER` 인접한 화물 공간에서 화물을 떨어뜨릴 수 있음
- `E_ALTERNATOR` 교류 발전기에 전원을 공급할 수 있는 엔진입니다.
- `E_COLD_START` 추운 날씨에 훨씬 느리게 시동되는 엔진입니다.
- `E_COMBUSTION` 연료를 연소하여 손상되면 역효과를 일으키거나 폭발할 수 있는 엔진입니다.
- `E_HEATER` 엔진이며 켜져 있을 때 차량 내부 품목을 따뜻하게 해주는 히터가 있습니다.
- `E_HIGHER_SKILL` 엔진이 많이 설치될수록 설치가 더 어려워지는 엔진입니다.
- `E_NO_POWER_DECAY` 이 플래그가 있는 엔진은 총 차량 출력 감소에 영향을 미치지 않습니다.
  보고.
- `E_STARTS_INSTANTLY` 푸드페달처럼 즉시 시동이 걸리는 엔진입니다.
- `EMITTER` 활성화된 동안 방출합니다(방출은 `emissions` 항목으로 정의됨).
- `ENABLED_DRAINS_EPOWER` 활성화된 동안 `epower` 와트를 생성합니다(소모하려면 음수 사용).
  힘). 이는 원자로 전력 생산과는 별개입니다.
- `ENGINE` 엔진이며 차량의 기계적 동력에 기여합니다.
- `EVENTURN` 짝수 회전 중에만 켜집니다.
- `EXTENDABLE` 다른 확장형 돌출부에 부착할 수 있는 돌출부
- `EXTENDS_VISION` 플레이어 시야 확장(카메라, 거울 등)
- `EXTRA_DRAG` 부품이 엔진 출력 감소에 영향을 미친다는 것을 차량에 알려줍니다.
- `FAUCET`
- `FLAT_SURF` 편평하고 단단한 표면이 있는 부품(예: 테이블).
- `FLOATS` 보트에 부력을 제공
- `FLUIDTANK` 유체탱크입니다.
- `FOLDABLE`
- `FREEZER` 섭씨 0도 이하에서도 물품을 얼릴 수 있습니다.
- `FRIDGE` 냉장보관 가능합니다.
- `FUNNEL`
- `HALF_CIRCLE_LIGHT` 켜졌을 때 빛의 반원형 반경을 투사합니다.
- `HARNESS_bodytype` bodytype을 `any`으로 바꾸면 모든 유형을 허용하거나 대상 유형으로 바꿀 수 있습니다.
- `HORN` 사용시 소음이 발생합니다.
- `HOTPLATE` 핫플레이트 동작을 제공합니다.
- `INITIAL_PART` 건설 메뉴를 통해 새 차량을 시작할 때 이 차량 부품은
  차량의 초기 부품(사용된 품목이 이 부품에 필요한 품목과 일치하는 경우) 그만큼
  이 플래그가 있는 부품 항목은 자동으로 차량 시동에 구성 요소로 추가됩니다.
  건설.
- `INTERNAL` `CARGO` 플래그가 있는 부분에만 설치할 수 있습니다.
- `LADDER` 비행차량에서 내려갈 수 있는 사다리
- `LIGHT`
- `LOCKABLE_CARGO` 자물쇠를 설치할 수 있는 화물 컨테이너.
- `MOUNTABLE` 플레이어는 여기에서 장착된 무기를 발사할 수 있습니다.
- `MUFFLER` 차량이 주행하는 동안 발생하는 소음을 줄여줍니다.
- `MULTISQUARE` 이 부품과 동일한 ID를 가진 인접 부품이 단일 부품으로 작동하도록 합니다.
- `MUSCLE_ARMS` 이러한 플래그가 있는 엔진의 힘은 플레이어의 힘에 따라 달라집니다(효과가 떨어집니다)
  `MUSCLE_LEGS`보다).
- `MUSCLE_LEGS` 이러한 플래그가 붙은 엔진의 힘은 플레이어의 힘에 따라 달라집니다.
- `NAILABLE` 못으로 붙어있음
- `NEEDS_BATTERY_MOUNT`
- `NEEDS_WHEEL_MOUNT_HEAVY` `WHEEL_MOUNT_HEAVY` 플래그가 있는 부분에만 설치할 수 있습니다.
- `NEEDS_WHEEL_MOUNT_LIGHT` `WHEEL_MOUNT_LIGHT` 플래그가 있는 부분에만 설치할 수 있습니다.
- `NEEDS_WHEEL_MOUNT_MEDIUM` `WHEEL_MOUNT_MEDIUM` 플래그가 있는 부분에만 설치할 수 있습니다.
- `NEEDS_WINDOW` `WINDOW` 플래그가 있는 부분에만 설치할 수 있습니다.
- `NO_JACK`
- `NOCOLLIDE`
- `NOCOLLIDEABOVE` z 레벨로 올라갈 때 단순히 충돌하지 않거나 무언가가 그 위로 내려갈 때 충돌하지 않는 기능에는 NOCOLLIDE가 필요합니다.
- `NOCOLLIDEBELOW` z 레벨로 내려가거나 그 위로 무언가 올라올 때 단순히 충돌하지 않는 기능에는 NOCOLLIDE가 필요합니다.
- `NOSMASH`
- `NOINSTALL` 설치할 수 없습니다.
- `NOFIELDS` 필드(연기 복사 등)가 동일한 타일에 영향을 미치는 것을 방지합니다.
- `NOREMOVE_SECURITY` 차량에 보안 시스템이 작동하는 경우 제거할 수 없습니다.
- 동일한 타일에 열려 있는 `OPENABLE` 부분이 있으면 `NOREMOVE_OPEN`을 제거할 수 없습니다.
- 동일한 타일에 닫힌 `OPENABLE` 부분이 있으면 `NOREMOVE_CLOSED` 제거할 수 없습니다.
- `NOREMOVE_INSIDE` 차량 내부에서는 제거할 수 없습니다.
- `NOREMOVE_OUTSIDE` 차량 외부에서는 제거할 수 없습니다.
- `OBSTACLE` 부품이 `OPENABLE`이 아닌 경우 부품을 살펴볼 수 없습니다.
- `ODDTURN` 홀수 턴에만 켜집니다.
- `ON_CONTROLS` `CONTROLS` 플래그가 있는 부분에만 설치할 수 있습니다.
- `ON_ROOF` - ​​이 플래그가 있는 부품은 지붕에만 설치할 수 있습니다(`ROOF` 플래그가 있는 부품).
- `OPAQUE` 보이지 않습니다.
- `OPENABLE` 열거나 닫을 수 있습니다.
- `OPENCLOSE_INSIDE` 열거나 닫을 수 있지만 차량 내부에서만 가능합니다.
- `OVER` 다른 부품 위에 장착할 수 있습니다.
- `PERPETUAL` REACTOR와 결합하면 연료를 소비하지 않고 부품이 전력을 생산합니다.
- `PLANTER` 경작된 흙에 씨앗을 심고, 아래 지형이 적합하지 않을 때 씨앗을 뿌립니다.
  `DIGGABLE`이 아닌 표면 위로 주행하면 손상됩니다.
- `PLOW` 활동하는 동안 부품 아래의 흙을 경작합니다. 부적합한 지형에서 피해를 입습니다.
  차량의 속도에 비례하는 레벨.
- `POWER_TRANSFER` 연결된 사물(아마도 차량)과 전력을 전송합니다.
- `PROPELLER` 프로펠러 로터인 부품, propeller_diameter 필드 필요
- `PROTRUSION` 부품이 돌출되어 있어 그 위에 다른 부품을 설치할 수 없습니다.
- `RAIL` 차량이 레일 위를 이동할 수 있게 해주는 바퀴입니다.
- `REACTOR` 활성화되면 부품이 연료를 소비하여 전력을 생성합니다.
- `REAPER` 성숙한 작물을 잘라서 사각형에 쌓습니다.
- `RECHARGE` 동일한 플래그로 아이템을 충전하세요. (현재는 충전식 배터리 모드만 해당됩니다.)
- `REMOTE_CONTROLS`
- `REVERSIBLE` 제거 요구 사항은 설치 요구 사항과 동일하지만 속도가 두 배 더 빠릅니다.
- `ROOF` 차량의 일부분을 덮습니다. 지붕과 지붕이 있는 차량 영역
  주변 섹션은 내부로 간주됩니다. 그렇지 않으면 그들은 밖에 있습니다.
- `ROTOR` 차량이 양력을 생성할 수 있도록 합니다. 실제 양력은 모든 로터의 엔진 출력 합계에 따라 달라집니다.
  직경.
- `SCOOP` 차량 아래에 있는 물품을 부품의 화물 공간으로 끌어당깁니다. 걸레질도 하고
  액체.
- `SEAT` 플레이어가 앉거나 잠을 잘 수 있는 좌석.
- `SEATBELT` 사고 발생 시 플레이어가 차량에서 튕겨져 나가는 것을 방지합니다. 할 수 있다
  `BELTABLE` 플래그가 있는 부분에만 설치하세요.
- `SEAT_REQUIRES_BALANCE` 플레이어는 특정 상황에 부딪히면 넘어질 수 있습니다.
  근력 롤. TRAIT_DEFT 및 TRAIT_PROF_SKATER를 사용하면 차량에서 튕겨나가기가 더 어려워집니다.
- `SECURITY`
- `SHARP` 이 부분으로 몬스터를 공격하면 강타 피해가 아닌 절단 피해를 입으며,
  몬스터를 기절시키는 것을 방지합니다.
- `SOLAR_PANEL` 햇빛에 노출되면 차량 배터리를 충전합니다. 4분의 1의 확률로
  자동차 세대에서 고장났습니다.
- `SPACE_HEATER` 이 부분을 토글하는 명령어가 별도로 있습니다.
- `STABLE` `WHEEL`과 유사하지만 차량이 1x1 단면인 경우 이 단일 바퀴는
  바퀴 충분해요.
- `STEERABLE` 이 바퀴는 조종이 가능합니다.
- `STEREO`
- `TOOL_NONE` 도구 없이 제거/설치 가능
- `TOOL_SCREWDRIVER` 나사로 부착되어 있으며 드라이버로 제거/설치 가능
- `TOOL_WRENCH` 볼트로 부착되어 있으며, 렌치로 탈부착 가능
- `TOWEL` 몸을 말리는 데 사용할 수 있습니다.
- `TRACK` 설치된 차량을 지도에 표시하고 추적할 수 있습니다.
- `TRACKED` 조향 효율성에 기여하지만 설치 시 조향 축으로 간주되지 않음
  어려움이 있으며 여전히 조향 계산 중심에 대한 항력에 기여합니다.
- `TRANSFORM_TERRAIN` 지형을 변환합니다(`transform_terrain`에 정의된 규칙 사용).
- `TURRET_CONTROLS` 이 플래그가 있는 부품을 터렛 위에 설치하면 다음과 같이 설정할 수 있습니다.
  포탑의 조준 모드를 ​​완전 자동으로 설정합니다. `TURRET` 플래그가 있는 부분에만 설치할 수 있습니다.
- `TURRET_MOUNT` 이 플래그가 있는 부품은 터렛 설치에 적합합니다.
- `TURRET` 무기 포탑입니다. `TURRET_MOUNT` 플래그가 있는 부품에만 설치할 수 있습니다.
- `UNMOUNT_ON_DAMAGE` 부품이 손상되어 파손되면 차량이 파손됩니다. 품목은 새것이며
  일반적으로 손상되지 않았습니다.
- `UNMOUNT_ON_MOVE` 차량이 움직일 때 이 부분을 분리해 주세요. 당신이하지 않는 한 부품을 떨어 뜨리지 않습니다.
  특별한 처리를 해주세요.
- `VARIABLE_SIZE` 출력, 휠 반경 등 '크기'가 있습니다.
- `VISION`
- `WATER_WHEEL` 흐르는 물 속에서 차량 배터리를 충전합니다.
- `WELDRIG` 용접 수리 작업을 수행합니다.
- `WHEEL` 휠 계산에서 휠로 계산됩니다.
- `WIDE_CONE_LIGHT` 전원을 켜면 빛의 원뿔 모양을 넓게 투사합니다.
- `WIND_POWERED` 이 엔진은 바람(돛 등)으로 구동됩니다.
- `WIND_TURBINE` 바람에 노출되면 차량 배터리를 충전합니다.
- `WINDOW` 이 부분이 투과되어 그 위에 커튼을 설치할 수 있습니다.
- `WING` 항공기 날개 부분으로, Lift_coff 필드가 필요함
- `WORKBENCH` 이 부분에서 제작할 수 있으며 Workbench json 항목과 쌍을 이루어야 합니다.

### Vehicle parts requiring other vehicle parts

다른 차량 부품에 대한 요구 사항은 json 플래그에 대해 `requires_flag`을 설정하여 정의됩니다.
깃발. `requires_flag`은 이 플래그가 있는 부분에 필요한 다른 플래그입니다.

### Fuel types

- `NULL` 없음
- `battery` 짜릿하다.
- `diesel` 세련된 공룡.
- `gasoline` 세련된 공룡.
- `plasma` 과열되었습니다.
- `plutonium` 1.21기가와트!
- `water` 깨끗해요.
- `wind` 풍력으로 구동됩니다.
