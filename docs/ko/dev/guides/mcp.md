---
title: MCP로 플레이하기
---

`cataclysm-bn-mcp` 실행 파일은 stdio로 통신하는 MCP 서버입니다. 에이전트는 게임의 화면과 조작 방식을 그대로 사용해 플레이합니다.

```sh
cmake --preset linux-full -B out/build/linux-full-mcp -DMCP=ON -DTILES=ON -DCURSES=OFF
cmake --build out/build/linux-full-mcp --target cataclysm-bn-mcp
```

MCP 호스트에 이 서버를 등록합니다. `--userdir`와 `--configdir`를 항상 함께 지정하세요. 지정하지 않으면 XDG 빌드가 평소 쓰는 설정을 읽고 덮어쓸 수 있습니다. `--world NAME`을 주면 시작할 때 해당 월드를 불러옵니다. `--replay-record PATH`를 주면 세션을 기록하고, 기록은 [리플레이 안내](replay.md)에 따라 다시 재생합니다.

```json
{
  "mcpServers": {
    "bn": {
      "command": "/path/to/Cataclysm-BN/out/build/linux-full-mcp/src/cataclysm-bn-mcp",
      "args": [
        "--datadir",
        "/path/to/Cataclysm-BN/data/",
        "--userdir",
        "/path/to/bn-agent-profile/",
        "--configdir",
        "/path/to/bn-agent-profile/config/"
      ]
    }
  }
}
```

호스트가 `notifications/initialized`를 보내기 전에는 `ping`을 뺀 모든 호출이 실패합니다. 서버는 `bn://screen`, `bn://state` 리소스도 제공합니다.

| 도구             | 결과                                                                               |
| ---------------- | ---------------------------------------------------------------------------------- |
| `bn.observe`     | 합성된 화면: `width`, `height`, `cursor`, `rows`, `styles`.                        |
| `bn.state`       | 월드 헤더. `part`를 주면 맵 파트 하나를 읽습니다.                                  |
| `bn.press`       | `keys`를 보내고, 다음 입력 지점의 화면을 돌려줍니다.                               |
| `bn.actions`     | `input_id`, `category`, `timeout_ms`, 그리고 바인딩이 붙은 `actions`.              |
| `bn.interaction` | 컨트랙트 스택 맨 위 화면: `screen_id`와 affordance 목록.                           |
| `bn.interact`    | affordance 하나(`affordance_id`, `args`)를 실행하고, 다음 맨 위 화면을 돌려줍니다. |

`bn.interact`는 `bn.command.submit`과 같은 경로를 탑니다. 입력은 한 번에 하나씩 보내세요. 결과를 읽은 뒤에 다음 입력을 정합니다. 입력 하나로 다른 메뉴가 열릴 수 있습니다.

```json
{ "name": "bn.press", "arguments": { "keys": [{ "action": "inventory" }] } }
```

`bn.actions`가 알려 준 `input_id`를 이벤트에 넣으면, 그 사이 화면이 바뀐 경우 서버가 이벤트를 거부합니다.

## 제한

- `bn.hello`를 보낸 클라이언트가 입력을 소유합니다. 이때 `bn.press`와 `bn.interact`는 실패합니다.
- 1 MiB를 넘는 결과는 도구 오류가 됩니다.
- 키 이벤트 하나에는 `action`, `key`(`text` 선택), `text`, `mouse` 중 정확히 하나만 씁니다.
- `modifiers`는 `key`와 함께 써야 합니다.
- `keys`에는 이벤트를 1개에서 256개까지 담을 수 있습니다.
