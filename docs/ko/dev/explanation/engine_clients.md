---
title: 엔진과 클라이언트
---

실행 파일 하나에 활성화된 클라이언트가 모두 링크되며, 시작할 때 `--client=tiles`, `--client=curses`, `--client=mcp` 중 하나를 선택합니다. `TILES`, `CURSES`, `MCP`가 빌드할 클라이언트를 정합니다. 기본값은 tiles, curses, MCP 순으로 우선합니다.

모든 클라이언트는 같은 게임 루프와 `input_context` 로직을 실행하고, 같은 `catacurses` 셀 창에 그립니다. 네이티브 클라이언트는 이 셀을 그대로 보여 줍니다. 외부 클라이언트는 입력 경계마다 [프로토콜](../reference/engine_client_protocol.md)로 셀을 받습니다.

클라이언트를 추가하려면 장치 인터페이스를 구현하고, 팩토리를 등록하고, 네이티브 소스를 `CataClientSources.cmake`에 추가하세요.
