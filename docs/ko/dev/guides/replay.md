---
title: 입력 기록과 재생
---

`--replay-record PATH`로 게임 입력을 기록하고, `--replay-play PATH`로 재생합니다.

```sh
./cataclysm-bn-tiles --userdir out/record/ --configdir out/record/config/ \
  --seed example --replay-record out/example.jsonl
./cataclysm-bn-tiles --userdir out/play/ --configdir out/play/config/ \
  --replay-play out/example.jsonl
```

두 실행은 같은 사용자 디렉터리 사본에서 시작해야 합니다. 리플레이는 입력만 기록하므로 월드, 설정, 키 배치를 복원하지 않습니다.

빌드, 데이터, 모드, 옵션, 창 크기를 같게 맞추세요. RNG 시드는 헤더에 들어 있습니다. 재생할 때 `--seed`를 주면 헤더의 값과 같아야 합니다.

입력 지점 불일치, 알 수 없는 형식 버전, 잘못된 레코드, 끝 레코드 누락이 있으면 파일과 줄 번호를 알리고 재생을 멈춥니다.

## 두 실행 비교

두 실행에 모두 `--state-hash-log PATH`를 추가합니다. 로그에는 턴마다 한 줄씩 RNG, 아바타, 몬스터, NPC, 아바타 주변 맵의 해시가 기록됩니다. 두 파일을 비교하면 처음 달라진 턴을 찾을 수 있습니다.
