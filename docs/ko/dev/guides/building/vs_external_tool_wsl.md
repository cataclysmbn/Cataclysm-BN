# Visual Studio 외부 도구 자동화 (Windows + WSL)

이 문서는 Visual Studio 외부 도구 워크플로를 사용해 Cataclysm: BN CMake 빌드 자동화를
프롬프트 기반 `cmd` 인터페이스로 실행하는 방법을 설명합니다.

## 개요

- 이 워크플로는 Visual Studio의 **External Tools** 메뉴에서 실행합니다.
- 도구는 `cmd` 창을 열고 프롬프트로 진행을 안내하므로, 수동 셸 명령 입력이 필요하지 않습니다.
- 동일한 흐름으로 Windows 빌드와 WSL 기반 Linux 빌드를 모두 처리할 수 있습니다.

## Visual Studio에 외부 도구 추가

**Tools -> External Tools...**를 열고 빌드 도구 항목을 구성합니다.

![Visual Studio External Tools menu](https://github.com/user-attachments/assets/a7b5d4b8-2cd3-41be-98ae-e75997619a2c)

![External tool configuration example](https://github.com/user-attachments/assets/197c59df-ac2e-4e2a-99f4-8a5dab860367)

## 실행 인터페이스

실행하면 자동화 도구가 메뉴형 프롬프트 흐름을 가진 `cmd` 세션을 엽니다.

![Prompt-driven cmd interface](https://github.com/user-attachments/assets/934ce9eb-37db-482d-b100-afd7fa215ed8)

`cmd` 창에서 실행되지만, 사용자는 옵션을 선택하고 질문에 답하면 되므로 수동 명령 작성이
필요하지 않습니다.

짧은 데모 영상:

<video controls src="https://github.com/user-attachments/assets/54968297-3b4c-462f-9bae-bd5c1e190b75"></video>

## 빌드 완료

워크플로는 같은 명령 창에서 완료 상태를 출력합니다.

![Build completion output](https://github.com/user-attachments/assets/b43f3130-77c0-4beb-91a0-3b98cb5915f8)

완료 후에는 빌드된 게임을 정상적으로 실행할 수 있습니다.

![Cataclysm: BN running after build](https://github.com/user-attachments/assets/3eaf7f95-5653-4c7d-acdd-7977c81ca0ee)
