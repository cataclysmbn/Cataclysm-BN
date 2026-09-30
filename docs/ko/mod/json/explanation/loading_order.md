# 로딩 순서

`data/json`에 있는 모든 파일은 결국 읽히지만, 다른 종류의 객체에 의존하는 객체(예: 제작법은
스킬에 의존함)의 경우에는 읽는 순서가 중요할 수 있습니다. 올바른 로딩 순서를 보장하면 대개
세그멘테이션 오류로 게임이 강제 종료되는 형태로 나타나는 예상치 못한 문제(매우 나쁜 문제)를
방지할 수 있습니다.

Cataclysm은 `data/json/` 파일 트리를 너비 우선 탐색하여 JSON 파일을 찾아 로드합니다. 즉,
`data/json/whatever.json`은 **항상** `data/json/subdir/whatever.json`보다 먼저 읽힙니다. 이를
이용하면 의존성이 올바른 순서로 로드되도록 할 수 있습니다.

예를 들어 시나리오가 직업에 의존하고 직업이 스킬에 의존한다면, 다음과 같은 디렉터리 구조를
사용해야 합니다.

```
data/json/
  skills.json
  professions/
    professions.json
    scenarios/
      scenarios.json
```

이 구조에서는 `skills.json`, `professions.json`, `scenarios.json` 순으로 로드됩니다.

## 같은 깊이의 로딩 순서

파일(또는 디렉터리)이 같은 깊이에 있을 때(예: 모두 `data/json/`에 있을 때)는 사전식 순서로
읽힙니다. ASCII 문자만 사용하는 파일 이름이라면 이는 알파벳순과 거의 같습니다. UTF-8 또는
그 밖의 비 ASCII 파일 이름은 코드 포인트 순서로 정렬됩니다.
