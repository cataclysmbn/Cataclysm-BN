# 사용자 인터페이스

Cataclysm: Bright Nights는 사용자 인터페이스를 위해 ncurses를 사용하며, 타일 빌드의 경우 ncurses 포트를 사용합니다. 윈도우 관리는 `ui_adaptor`를 통해 이루어지며, 각 UI가 크기 조정 및 재그리기를 처리하기 위해 크기 조정 콜백과 재그리기 콜백이 필요합니다. `ui_adaptor` 사용 방법에 대한 자세한 내용은 `ui_manager.h`에서 찾을 수 있습니다.

`ui_adaptor` 사용의 좋은 예는 다음 파일에서 찾을 수 있습니다:

- `popup.h/cpp`의 `query_popup` 및 `static_popup`
- `messages.cpp`의 `Messages::dialog`
