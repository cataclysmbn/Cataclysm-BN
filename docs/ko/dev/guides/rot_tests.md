# 부패 회귀 테스트와 커버리지

저장소 루트에서 부패 테스트를 실행합니다.

```sh
cmake --preset linux-full
cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  out/build/linux-full/tests/cata_test-tiles --filenames-as-tags \
  '[rot],[#rot_test],[messages]' --rng-seed 10278
```

테스트는 `data/mods/TEST_DATA/rot.json`의 고정 픽스처와 범위가 제한된 Catch2 제너레이터를 사용합니다. 보관 온도, 시간과 부패 경계, 중첩 용기, 시체 회수, 스폰, 콜백 소유권을 다룹니다. 요리 회귀 테스트는 21일 뒤에 실제 기름 조리기와 RV 주방 활동도 실행합니다. 수제 옥수수가루와 라드도 포함하며, 재료와 연료가 실제로 소비되는지 검증합니다.

## 부패 경계

- `src/rot/rot_calculation.h`는 검증된 양수 유통기한과 순수 산술을 정의합니다. 부패하지 않는 아이템에는 유통기한 프로필이 없으며 상대 부패도는 항상 0입니다. 유한하지 않은 값과 범위를 벗어난 상대 부패도 편집은 거부됩니다.
- `src/rot/item_rot.cpp`는 이 산술을 아이템 상태, 날씨, 기존 처리 주기에 연결합니다. 훈제와 제분 중에는 고유 유통기한을 0으로 바꾸지 않고 부패 진행만 멈춥니다. 일반 라이브 아이템 getter는 기존의 지연 실제화 동작을 유지합니다.
- 완성된 구성요소의 소유권으로 비활성 재료 기록을 식별합니다. `COMPONENT` 플래그가 없는 예전 세이브도 같습니다. 제작 중인 재료는 계속 라이브 상태입니다. 기록을 제거하거나 복사하면 기록된 기간만큼 부패시키지 않고 라이브 시계를 다시 시작합니다. 안에 든 아이템은 함께 다시 시작하고, 별도로 중첩된 기록은 비활성 상태로 남습니다.
- 개수 스택 콜백의 복사본은 콜백이 끝날 때까지 원본의 기록/라이브 역할을 빌려 씁니다. 실제 배치가 우선하며, 임시 역할은 복사되거나 저장되지 않습니다.
- 기존 세이브 필드 `rot`, `last_rot_check`, `components`는 그대로 유지합니다. 단독으로 붙은 예전 `COMPONENT` 플래그는 getter에서만 동작하는 기존 방식을 유지하며, 명시적 처리를 끄지 않습니다.

원래 이슈의 회귀 태그는 `[issue_10181]`입니다. `[lifecycle]`은 기록 읽기, 회수, 복사, 처리 중단, 요리 워크플로를 다룹니다. `[calculation]`은 순수 산술과 직접 진입점의 경계를 다룹니다. 콜백 테스트는 앞선 테스트가 여유 용량을 남겼더라도 벡터 재할당을 강제합니다.

## 커버리지 100% 요구

[커버리지 검사기](../../../../tools/check_rot_coverage.ts)는 `src/rot.cpp`, `src/rot/item_rot.cpp`, `src/rot/rot_calculation.cpp`의 모든 함수와 중첩 콜백을 자동으로 포함합니다. 명시적 매니페스트는 더 큰 소스 파일에 남아 있는 위치/실제화, 제작 신선도, 표시, 시체/스폰, 순회 어댑터도 다룹니다. 새 부패 구현은 `item.h`에 측정되지 않는 인라인 본문으로 추가하지 말고, 파일 전체가 측정 범위에 들어가는 위치에 두세요. 이 값은 부패 서브시스템의 분모이며 게임 전체 커버리지가 아닙니다.

측정할 때마다 새 프로필 디렉토리를 사용하세요. 오래된 프로필이나 실패한 테스트의 프로필은 병합하지 마세요.

```sh
cmake --preset linux-full \
  -DCMAKE_PROJECT_INCLUDE="$PWD/build-scripts/rot-coverage.cmake"
mkdir -p out/rot-coverage/build
LLVM_PROFILE_FILE="$PWD/out/rot-coverage/build/%p.profraw" \
  cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles
profiles=$(mktemp -d "$PWD/out/rot-coverage/run-XXXXXX")
LLVM_PROFILE_FILE="$profiles/%p.profraw" \
  SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  out/build/linux-full/tests/cata_test-tiles --filenames-as-tags \
  '[rot],[#rot_test],[messages]' --rng-seed 10278 && \
  llvm-profdata merge -sparse "$profiles/"*.profraw -o "$profiles/rot.profdata" && \
  tools/check_rot_coverage.ts out/build/linux-full/tests/cata_test-tiles "$profiles/rot.profdata"
```

검사기는 LLVM의 네이티브 함수 요약을 사용하며, 반올림한 `100.00%`가 아니라 누락된 리전, 라인, 분기 결과가 0일 것을 요구합니다. 함수나 요약이 없어도 검사에 실패합니다. `llvm-cov`, `llvm-profdata`, `llvm-cxxfilt`의 버전은 Clang과 맞추세요. 검사기에는 Deno가 필요하며 저장소 루트에서 실행합니다.

`-DROT_SANITIZE=ON`을 지정하면 [rot-coverage.cmake](../../../../build-scripts/rot-coverage.cmake)에 나열된 소스 파일에 AddressSanitizer도 적용됩니다. 프로그램 전체에 적용되는 새니타이저 빌드는 아닙니다. 커버리지 프로필은 새니타이저 진단과 별개이므로, 커버리지가 통과해도 새니타이저 실패는 허용되지 않습니다.

다음 명령으로 일반 빌드로 돌아갑니다.

```sh
cmake --preset linux-full -DCMAKE_PROJECT_INCLUDE= -DROT_SANITIZE=OFF
cmake --build --preset linux-full --target cataclysm-bn-tiles cata_test-tiles
```

커버리지 매니페스트나 검사기를 확장할 때는 검사기 테스트도 실행하세요.

```sh
deno test tools/check_rot_coverage_test.ts
deno check tools/check_rot_coverage.ts
deno lint tools/check_rot_coverage.ts tools/check_rot_coverage_test.ts
```

테스트 실행 파일은 보통 게임 메시지를 버립니다. 화면에 표시되는 텍스트를 검증해야 하는 테스트는 `tests/message_helpers.h`의 `capture_messages_during`을 사용할 수 있습니다. 캡처는 스레드 로컬이며 범위가 있고 중첩할 수 있습니다.
