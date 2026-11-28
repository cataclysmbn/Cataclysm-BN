# 터미널 캡처 녹화하기

PR에서 버그나 수정 결과를 보여 주려면 curses 빌드를 asciinema로 녹화합니다. 녹화본은 `just term-capture`로 짧은 움직이는 WebP로 만듭니다. [PR #11041](https://github.com/cataclysmbn/Cataclysm-BN/pull/11041)에 수정 전후 캡처가 있습니다.

## 필요한 것

- curses 빌드: `cmake --preset linux-curses`와 `cmake --build --preset linux-curses --target cataclysm-bn` ([CMake](./building/cmake.md) 참고)
- 녹화용 [asciinema](https://docs.asciinema.org/)
- 녹화본 렌더링용 [agg](https://github.com/asciinema/agg)
- Pillow로 스크립트를 실행할 [uv](https://docs.astral.sh/uv/)

## 녹화

1. 터미널을 작게 만듭니다. 예를 들어 120열 40행입니다. 터미널이 작을수록 파일이 작아집니다.
2. 저장소 루트에서 시간 제한과 별도의 사용자, 설정 디렉터리를 주고 asciinema 안에서 게임을 시작합니다:

   ```sh
   asciinema rec -c 'timeout -k 10s 600 out/build/linux-curses/src/cataclysm-bn --userdir capture/ --configdir capture/config/' before.cast
   ```

   `--userdir`와 `--configdir`를 주면 게임이 새 세이브와 설정을 쓰고 기존 것은 건드리지 않습니다. `timeout`은 600초 뒤 게임을 끝냅니다. 없으면 터미널 창을 닫아도 게임이 계속 실행될 수 있습니다.
3. 디버그 메뉴로 장면을 준비하고 버그가 나오는 조작을 합니다.
4. 게임 메뉴에서 게임을 끝냅니다.
5. 수정된 빌드로 같은 방법으로 `after.cast`를 녹화합니다.

## WebP 만들기

1. 녹화본을 GIF로 렌더링합니다:

   ```sh
   just term-capture render before.cast before.gif
   ```

   GIF는 2배속으로 재생됩니다. 1초보다 긴 정지는 0.5초가 됩니다.
2. GIF의 각 프레임을 PNG 파일로 저장합니다:

   ```sh
   just term-capture frames before.gif before-frames/
   ```

3. 플레이어 조작마다 첫 프레임을 찾아 `scenes.json`에 장면 하나씩 적습니다. `start`는 PNG 파일 이름의 번호입니다. 자막에는 게임 화면에 나오는 말을 씁니다. 마지막 장면은 버그면 `red`, 수정이면 `green`으로 합니다.

   ```json
   [
     { "start": 40, "caption": "enable UPS charging" },
     { "start": 67, "caption": "next turn: invalid ammo error", "color": "red" }
   ]
   ```

4. WebP를 만듭니다. 남길 마지막 프레임 번호는 `--last`로 줍니다:

   ```sh
   just term-capture encode before.gif before.webp --scenes scenes.json --last 80
   ```

   WebP는 첫 장면에서 시작해 `--last` 프레임에서 끝납니다. 게임 시작 화면과 메뉴처럼 첫 장면 앞의 프레임은 잘립니다. 각 장면의 마지막 프레임은 2초, 캡처의 마지막 프레임은 6초 동안 멈춥니다. 인코딩에 몇 분 걸릴 수 있습니다.
5. 브라우저로 WebP를 열어 장면을 하나씩 확인합니다.

WebP는 무손실입니다. 손실 WebP는 터미널 글자가 흐려지고 파일도 더 커지는 경우가 많습니다.

## PR에 캡처 넣기

WebP 파일을 포크의 브랜치에 푸시합니다. 예를 들어 `assets/<이슈 번호>`입니다. PR의 Testing 항목에 raw URL을 넣습니다:

```md
![before](https://raw.githubusercontent.com/<you>/Cataclysm-BN/assets/9853/before.webp)
```

캡처를 바꾸려면 같은 경로에 새 파일을 푸시합니다. PR 본문은 고치지 않아도 됩니다. 새 파일이 보이기까지 몇 분 걸릴 수 있습니다.
