# 사운드팩

사운드팩은 `data/sound` 디렉토리에 설치할 수 있습니다. 최소한 `soundpack.txt`라는 이름의 파일을 포함하는 하위 디렉토리여야 합니다. sound_effect 또는 playlist를 추가하는 임의의 수의 json 파일을 포함할 수 있습니다.

## soundpack.txt 형식

`soundpack.txt` 파일 형식에는 NAME과 VIEW라는 2개의 값이 필요합니다. NAME 값은 고유해야 합니다. VIEW 값은 옵션 메뉴에 표시됩니다. `#`로 시작하는 모든 줄은 주석 줄입니다.

```
#Basic provided soundpack
#Name of the soundpack
NAME: basic
#Viewing name of the soundpack
VIEW: Basic
```

## JSON 형식

### 사운드 효과

사운드 효과는 다음과 같은 형식으로 포함할 수 있습니다:

```json
[
  {
    "type": "sound_effect",
    "id": "menu_move",
    "volume": 100,
    "files": [
      "nenadsimic_menu_selection_click.wav"
    ]
  },
  {
    "type": "sound_effect",
    "id": "fire_gun",
    "volume": 90,
    "variant": "bio_laser_gun",
    "files": [
      "guns/energy_generic/weapon_fire_laser.ogg"
    ]
  }
]
```

다양성 추가: 특정 `id`의 `variant`에 대해 여러 `files`가 정의되면 `variant`가 재생될 때 무작위로 선택됩니다.

volume 키는 0-100 범위일 수 있습니다.

Cataclysm에는 사운드에 추가로 영향을 미치는 사용자가 제어할 수 있는 자체 볼륨 세트가 있습니다. 이것들은 0-128 범위이며 기본값은 100입니다. 즉, 기본 볼륨에서 Cataclysm이 재생하는 모든 사운드는 기본적으로 최대값의 약 78%로 재생됩니다. 외부 오디오 편집기에서 사운드를 작업하는 경우, 기본 볼륨 설정의 Cataclysm이 편집기보다 더 조용하게 해당 사운드 파일을 재생할 것으로 예상하세요.

### SFX 미리 로드

사운드 효과는 다음과 같은 형식으로 미리 로드에 포함할 수 있습니다:

```json
[
  {
    "type": "sound_effect_preload",
    "preload": [
      { "id": "fire_gun", "variant": "all" },
      { "id": "environment", "variant": "daytime" },
      { "id": "environment" }
    ]
  }
]
```

`"variant": "all"`은 특별하게 처리되며 주어진 id의 모든 변형을 로드합니다.

> [!WARNING]
>
> `"variant": "all"`은 최적이 아닌 알고리즘을 사용하며(개발자가 멍청하고 게으르며 핵을 사용했기 때문에) 게임 로딩 시간을 느리게 만듭니다.

`"variant"`가 생략되면 기본값은 `"default"`입니다.

### 재생 목록

재생 목록은 다음과 같은 형식으로 포함할 수 있습니다:

```json
[
  {
    "type": "playlist",
    "playlists": [
      {
        "id": "title",
        "shuffle": false,
        "files": [
          {
            "file": "Dark_Days_Ahead_demo_2.wav",
            "volume": 100
          },
          {
            "file": "cataclysmthemeREV6.wav",
            "volume": 90
          }
        ]
      }
    ]
  }
]
```

각 사운드 효과는 id와 variant로 식별됩니다. 사운드 효과가 json 파일에 존재하지 않는 variant로 재생되지만 "default" variant가 존재하면 대신 "default" variant가 재생됩니다. 사운드 효과의 파일 이름은 사운드팩 디렉토리를 기준으로 하므로 파일 이름이 "sfx.wav"로 설정되고 사운드팩이 `data/sound/mypack`에 있으면 파일은 `data/sound/mypack/sfx.wav`에 배치되어야 합니다.

## 사운드 효과 목록

사운드 효과 id와 variant의 전체 목록은 다음과 같습니다. 목록의 각 줄은 다음 형식을 갖습니다:

`id variant1|variant2`

여기서 id는 사운드 효과의 id를 설명하고, |로 구분된 variant 목록이 뒤따릅니다. variant가 생략되면 "default" variant가 가정됩니다. variant가 리터럴 문자열이 아니라 변수를 나타내는 경우 `<` `>`로 묶입니다. 예를 들어, `<furniture>`는 유효한 가구 ID(가구 정의 JSON에서와 같이)에 대한 자리 표시자입니다.

    # 문 열기/닫기

- `open_door default|<furniture>|<terrain>`
- `close_door default|<furniture>|<terrain>`

  # 부수기 시도 및 결과, 몇 가지 특수 항목 및 가구/지형 특정
- `bash default`
- `smash wall|door|door_boarded|glass|swing|web|paper_torn|metal`
- `smash_success hit_vehicle|smash_glass_contents|smash_cloth|<furniture>|<terrain>`
- `smash_fail default|<furniture>|<terrain>`

  # 근접 사운드
- `melee_swing default|small_bash|small_cutting|small_stabbing|big_bash|big_cutting|big_stabbing`
- `melee_hit_flesh default|small_bash|small_cutting|small_stabbing|big_bash|big_cutting|big_stabbing|<weapon>`
- `melee_hit_metal default|small_bash|small_cutting|small_stabbing|big_bash|big_cutting|big_stabbing!<weapon>`
- `melee_hit <weapon>` # 주의: 맨손 공격에는 무기 id "null" 사용

  # 총기/원거리 무기 사운드
- `fire_gun <weapon>|brass_eject|empty`
- `fire_gun_distant <weapon>`
- `reload <weapon>`
- `bullet_hit hit_flesh|hit_wall|hit_metal|hit_glass|hit_water`

  # 환경 sfx, 명확성을 위해 섹션으로 구분됨
- `environment thunder_near|thunder_far`
- `environment daytime|nighttime`
- `environment indoors|indoors_rain|underground`
- `environment <weather_type>` # 예시:
  `WEATHER_DRIZZLE|WEATHER_RAINY|WEATHER_THUNDER|WEATHER_FLURRIES|WEATHER_SNOW|WEATHER_SNOWSTORM`
- `environment alarm|church_bells|police_siren`
- `environment deafness_shock|deafness_tone_start|deafness_tone_light|deafness_tone_medium|deafness_tone_heavy`

  # 기타 환경 사운드
- `footstep default|light|clumsy|bionic`
- `explosion default|small|huge`

  # 많은 수의 좀비를 볼 때의 주변 위험 테마
- `danger_low`
- `danger_medium`
- `danger_high`
- `danger_extreme`

  # 전기톱 팩
- `chainsaw_cord     chainsaw_on`
- `chainsaw_start    chainsaw_on`
- `chainsaw_start    chainsaw_on`
- `chainsaw_stop     chainsaw_on`
- `chainsaw_idle     chainsaw_on`
- `melee_swing_start chainsaw_on`
- `melee_swing_end   chainsaw_on`
- `melee_swing       chainsaw_on`
- `melee_hit_flesh   chainsaw_on`
- `melee_hit_metal   chainsaw_on`
- `weapon_theme      chainsaw`

  # 몬스터 죽음과 물기 공격
- `mon_death zombie_death|zombie_gibbed`
- `mon_bite bite_miss|bite_hit`

- `melee_attack monster_melee_hit`

- `player_laugh laugh_f|laugh_m`

  # 플레이어 이동 sfx
  중요: `plmove <terrain>`은 기본 `plmove|walk_<what>` (`|barefoot` 제외)보다 우선순위가 높습니다. 예시: `plmove|t_grass_long`이 정의되어 있으면 모든 잔디 지형에 대한 기본 `plmove|walk_grass`보다 먼저 재생됩니다.

- `plmove <terrain>|<vehicle_part>`
- `plmove walk_grass|walk_dirt|walk_metal|walk_water|walk_tarmac|walk_barefoot|clear_obstacle`

  # 피로
- `plmove fatigue_m_low|fatigue_m_med|fatigue_m_high|fatigue_f_low|fatigue_f_med|fatigue_f_high`

  # 플레이어 부상 사운드
- `deal_damage hurt_f|hurt_m`

  # 플레이어 죽음 및 게임 종료 사운드
- `clean_up_at_end game_over|death_m|death_f`

  # 다양한 생체공학 사운드
- `bionic elec_discharge|elec_crackle_low|elec_crackle_med|elec_crackle_high|elec_blast|elec_blast_muffled|acid_discharge|pixelated`
- `bionic bio_resonator|bio_hydraulics|`

  # 다양한 도구/함정 사용(일부 관련 지형/가구 포함)
- `tool alarm_clock|jackhammer|pickaxe|oxytorch|hacksaw|axe|shovel|crowbar|boltcutters|compactor|gaspump|noise_emitter|repair_kit|camera_shutter|handcuffs`
- `tool geiger_low|geiger_medium|geiger_high`
- `trap bubble_wrap|bear_trap|snare|teleport|dissector|glass_caltrop|glass`

  # 다양한 활동
- `activity burrow`

  # 악기, `_bad`는 연주를 잘 못했을 때 사용됨
- `musical_instrument <instrument>`
- `musical_instrument_bad <instrument>`

  # 다양한 함성과 비명
- `shout default|scream|scream_tortured|roar|squeak|shriek|wail|howl`

  # 말하기, 현재 아이템 또는 몬스터 id와 연결되거나 특수한 `NPC` 또는 `NPC_loud`임
  # TODO: speech.json의 완전한 음성화
- `speech <item_id>` # 예시: talking_doll, creepy_doll, Granade,
- `speech <monster_id>` # 예시: eyebot, minitank, mi-go, 많은 로봇
- `speech NPC_m|NPC_f|NPC_m_loud|NPC_f_loud` # NPC용 특수
- `speech robot` # 기계 등에서 나오는 로봇 음성용 특수

  # 라디오 잡음
- `radio static|inaudible_chatter`

  # 다양한 출처의 윙윙거리는 소리
- `humming electric|machinery`

  # (타는) 불과 관련된 소리
- `fire ignition`

  # 차량 사운드 - 엔진 및 기타 부품의 작동
  # 참고: 기본값은 특정 옵션이 정의되지 않았을 때 실행됩니다
- `engine_start <vehicle_part>` # 참고: 특정 엔진 시작(모든 engine/motor/steam_engine/paddle/oar/sail/등의 id)
- `engine_start combustion|electric|muscle|wind` # 기본 엔진 시작 그룹
- `engine_stop <vehicle_part>` # 참고: 특정 엔진 정지(모든 engine/motor/steam_engine/paddle/oar/sail/등의 id)
- `engine_stop combustion|electric|muscle|wind` # 기본 엔진 정지 그룹

  # 참고: 내부 엔진 소리는 차량 속도에 따라 동적으로 피치가 변경됩니다
  # 전용 채널을 가진 주변 루프 사운드입니다
- `engine_working_internal <vehicle_part>` # 참고: 차량 내부에서 들리는 엔진 작동 소리
- `engine_working_internal combustion|electric|muscle|wind` # 기본 엔진 작동(내부) 그룹

  # 참고: 외부 엔진 소리 볼륨과 팬은 차량까지의 거리와 각도에 따라 동적으로 변경됩니다
  # 주어진 거리에서 들리는 볼륨은 엔진의 `noise_factor`와 엔진에 대한 스트레스와 연결됩니다(`vehicle::noise_and_smoke()` 참조)
  # 전용 채널을 가진 주변 루프 사운드입니다
  # 이것은 단일 채널 솔루션입니다(TODO: 들리는 모든 차량에 대한 다중 채널); 가장 큰 소리가 나는 차량을 선택합니다
  # 여기에는 피치 변경이 없습니다(필요할 때 도입될 수 있음)
- `engine_working_external <vehicle_part>` # 참고: 차량 외부에서 들리는 엔진 작동 소리
- `engine_working_external combustion|electric|muscle|wind` # 기본 엔진 작동(외부)
  그룹

  # 참고: gear_up/gear_down은 피치 조작에 의해 자동으로 수행됩니다
  # 기어 변속은 최대 안전 속도에 의존하며, 전진 기어가 6개, 기어 0 = 중립, 기어 -1 = 후진이라는 가정하에 작동합니다
- `vehicle gear_shift`

- `vehicle engine_backfire|engine_bangs_start|fault_immobiliser_beep|engine_single_click_fail|engine_multi_click_fail|engine_stutter_fail|engine_clanking_fail`
- `vehicle horn_loud|horn_medium|horn_low|rear_beeper|chimes|car_alarm`
- `vehicle reaper|scoop|scoop_thump`

- `vehicle_open <vehicle_part>` # 참고: 문, 트렁크, 해치 등의 id
- `vehicle_close <vehicle part>`

  # 기타 사운드
- `misc flashbang|flash|shockwave|earthquake|stairs_movement|stones_grinding|bomb_ticking|lit_fuse|cow_bell|bell|timber`
- `misc rc_car_hits_obstacle|rc_car_drives`
- `misc default|whistle|airhorn|horn_bicycle|servomotor`
- `misc beep|ding|`
- `misc rattling|spitting|coughing|heartbeat|puff|inhale|exhale|insect_wings|snake_hiss` # 주로
  유기체 소음
