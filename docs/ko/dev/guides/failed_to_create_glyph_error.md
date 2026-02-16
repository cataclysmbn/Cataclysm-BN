# Failed to create glyph 오류

MSYS Windows 빌드와 일부 Mac 빌드에서 BN과 DDA에 유사한 이슈가 보고되었습니다.
https://github.com/CleverRaven/Cataclysm-DDA/issues/50115

최신 버전의 SDL2/freetype이 기본 폰트와 호환되지 않는 문제입니다. vcpkg에서 각각 20일, 9일 전에 업데이트되었습니다 (https://github.com/microsoft/vcpkg/pull/19509, https://github.com/microsoft/vcpkg/pull/19284). 한 달 전 버전의 vcpkg를 사용하면 글리프 문제가 없으므로 이것이 원인인 것으로 보입니다.

안타깝게도 Microsoft의 공식 C++ 라이브러리 관리자인 vcpkg는 특정 버전의 라이브러리 설치를 지원하지 않습니다. 따라서 두 가지 옵션이 있습니다:

1. 업데이트 이전의 구버전 vcpkg를 사용합니다. 이 버전을 사용하면 작동합니다: https://github.com/microsoft/vcpkg/tree/6bc4362fb49e53f1fff7f51e4e27e1946755ecc6

2. config/fonts.json을 열고 Terminus.ttf 항목을 제거합니다. 왜 기본적으로 unifont를 사용하지 않는지는 기억나지 않지만, i18n/폰트 코드를 작업할 계획이 아니라면 시각적 차이 외에는 영향이 없을 것입니다.
