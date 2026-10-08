# 게임 설치 방법

## 기본 게임 설치

- https://github.com/cataclysmbn/Cataclysm-BN 로 이동합니다.
- `Executables`에서 원하는 기본 게임 버전을 선택합니다.

1. `Stable`은 버그가 더 적지만 최신 콘텐츠는 포함되지 않습니다. 안정판은 몇 달마다 한 번씩 나옵니다.
2. `Latest release`는 Nightly 빌드가 게시되는 곳입니다(매일 밤 한 번 릴리스). 최신 콘텐츠를 받을 수 있지만 불안정할 수 있습니다.
   <img width="834" height="170" alt="image" src="https://github.com/user-attachments/assets/c23f8629-dc4f-46c7-8f09-28f4d0f2cbfb" />

- 어떤 경우든 **게임을 업데이트하기 전에 항상 저장 데이터를 백업하세요**. 가능하면 게임 폴더 전체를 백업하는 것이 좋습니다.
- 사용 중인 OS에 맞는 게임 버전을 선택합니다.
  <img width="1231" height="639" alt="image" src="https://github.com/user-attachments/assets/9f82e154-1034-477b-b8ad-5c83c746dc16" />

- 게임을 설치할 위치에 새 폴더를 만듭니다(여기서는 "CBN_TUTO_INSTALL"이라고 부릅니다).
- 게임 zip 파일을 더블 클릭하고 모든 파일을 선택한 다음 새로 만든 폴더로 끌어다 놓습니다.
  <img width="1917" height="1077" alt="image" src="https://github.com/user-attachments/assets/ded02968-52ac-48a5-813c-1883885951cf" />

- 축하합니다! 게임은 이미 설치되었습니다. `cataclysm-bn-tiles`라는 파일을 더블 클릭하면 게임을 시작할 수 있습니다.

## 외부 모드 받기

- Bright Nights에는 첫 플레이에 충분한 모드가 이미 포함되어 있지만, 어쩌면 _더 많은_ 모드를 원할 수도 있습니다.
- https://mods.cataclysmbn.org/ 로 이동한 다음 `View all XXX mods`를 클릭합니다.
  <img width="1903" height="930" alt="image" src="https://github.com/user-attachments/assets/08786ffd-6cff-46a5-afb1-83b5db997d7a" />

- 흥미로워 보이는 모드를 클릭해 자세한 정보를 확인합니다.
  <img width="1919" height="932" alt="image" src="https://github.com/user-attachments/assets/3a63722b-11f3-4fdb-b903-c8a9e0affdf3" />

- 모드를 다운로드하려면 `Installation` 아래의 링크를 클릭합니다.
  <img width="1914" height="937" alt="image" src="https://github.com/user-attachments/assets/37052212-929d-4459-b383-a665d10f500e" />

- 기본 게임에서 했던 것처럼 모드 파일을 `/gamefolder/mods/`에 끌어다 놓습니다.
- _모드 폴더에는 modinfo.json 파일이 있어야 하므로, 그 파일이 들어 있는 폴더를 끌어다 놓아야 합니다._
  <img width="1917" height="1076" alt="image" src="https://github.com/user-attachments/assets/da63f3ff-40e5-44c2-82cc-15bcb3e3e9b2" />

- 모드가 설치되었습니다!

## ~~진짜~~ 사운드팩 받기

- 먼저 `/gamefolder/data/sound/` 안에 이름에 "otopack"이 들어간 폴더가 있는지 확인합니다. 있다면 이 부분은 건너뛰어도 됩니다. 없다면 다음을 진행합니다.
- https://mods.cataclysmbn.org/mods/otopack_bn_mk_2/ 로 이동합니다.
- `외부 모드 받기` 절차를 반복하되, 이번에는 파일을 `/gamefolder/sound/`에 넣습니다.
- 추신: `/gamefolder/`에 `sound` 폴더가 없다면 만드세요. _`data`에도 작동은 하지만 아무것도 설치해서는 안 됩니다. 이곳은 기본 게임 파일을 위한 공간입니다._
  <img width="1914" height="1064" alt="image" src="https://github.com/user-attachments/assets/a71770dc-a032-4ed5-93a6-bf90c60e4a99" />

- 사운드팩이 설치되었습니다!

## (선택 사항) 사운드팩용 추가 음악 받기

- 이 사운드팩의 추가 음악을 원한다면 여기 링크 https://github.com/leoCottret/cbn-leocottret-mods/tree/main/MUSICS 를 클릭해 안내를 확인하세요.

## 게임 안에서 모두 활성화하기

- 게임 안에서 사운드팩을 활성화하려면 `Settings` > `Options`(`Enter`) >\
  <img width="1924" height="1079" alt="image" src="https://github.com/user-attachments/assets/f59a43c1-c92a-4a32-b3b1-111d15e7b43e" />

- `General` > `Choose soundpack` "Otopack BN" > `ESC` 키 > `Yes` 선택
  <img width="1914" height="1079" alt="image" src="https://github.com/user-attachments/assets/54c1af2a-be56-4bc8-85dd-4013ff73b85e" />

- 새 모드로 게임을 시작하려면 월드를 만들 때 해당 모드에서 `Enter`를 누르면 됩니다.
- `World` > `Create World` > 원하는 모드로 이동 > `Enter` 누르기
  <img width="1916" height="1079" alt="image" src="https://github.com/user-attachments/assets/7da9ea69-d95e-4d87-8285-57242dc2efd0" />

- 황금률: 게임 어디에서든 `?`를 누르면 현재 상황에서 사용할 수 있는 키를 보고 변경할 수 있습니다.
