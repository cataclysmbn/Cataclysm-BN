# 기여하기

## 도움을 주고 싶으신가요?

다음과 같은 도움을 환영합니다:

- 버그 제보. DDA에서 물려받은 버그도 포함됩니다.
- 버그는 아니지만 문제가 있는 것들을 찾아주세요. 오해의 소지가 있는 설명, 비슷한 경우와 비교했을 때
  명백히 이상한 수치, 문법 오류, 명확한 해결 방법이 있는 UI 문제 등이 있습니다.
- 쓸모없는 것들을 유용하게 만들거나 블랙리스트에 추가하기. 분해 레시피가 있어야 하는데 없는 것들에
  분해 레시피 추가하기, 스폰 목록에서 완전히 중복되는 아이템을 일반 버전으로 교체하기 (예: "작은 표시된
  병"을 "작은 병"으로) 등이 있습니다.
- 타일셋 작업. 전기 그리드 요소 같은 새 오브젝트들이 추가되고 있는데, 새 타일이 필요합니다.
- 밸런스 분석. 깊이 있는 분석이나 "명백히 올바른" 분석을 환영합니다. "명백히 올바른"은 이런 것들을
  말합니다: "무기 x는 무기 y보다 스탯이 엄격히 우수한데, y는 더 희귀한 재료가 필요하고 다른 요구사항은
  동일합니다."
- 프로파일러로 성능 병목 지점 찾기.
- 코드 품질 개선.

## 방법

카타클리즘: 밝은 밤에 기여하는 것은 쉽습니다.

1. GitHub에서 저장소를 포크해주세요.
2. 변경사항을 만들어주세요.
3. [풀 리퀘스트][pr]를 열어주세요.

[pr]: https://docs.github.com/ko/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests

> [!NOTE]
>
> #### CC-BY-SA 3.0
>
> 카타클리즘: 밝은 밤은 크리에이티브 커먼즈 저작자표시-동일조건변경허락 3.0 라이선스에 따라
> 배포됩니다. 게임의 코드와 콘텐츠는 어떠한 목적에도 사용, 수정, 재배포할 수 있습니다. 자세한 내용은
> http://creativecommons.org/licenses/by-sa/3.0/ 를 참고해주세요. 그 말은, 이 프로젝트에 기여하면, 그
> 기여물도 동일한 라이선스에 의해 보호받는다는 것이며, 이 라이선스는 취소될 수 없다는 것입니다.

## 가이드라인

몇 가지 지켰으면 하는 가이드라인이 있습니다:

- 이 저장소를 `upstream` [리모트][remote]로 추가해주세요.
- `main` 브랜치를 수정사항 없이 깨끗하게 유지해주세요. 원격 저장소의 최신 변경사항을 바로 끌어올 수
  있게 하기 위함입니다.
- 새 기능이나 버그 수정을 할 때마다 새 브랜치를 만들어주세요.
- 절대로 `main` 브랜치에 로컬 브랜치를 병합하지 마세요. `upstream/main`에서 끌어오기만 해주세요.

[remote]: https://docs.github.com/ko/pull-requests/collaborating-with-pull-requests/working-with-forks/configuring-a-remote-repository-for-a-fork

## 코드 스타일

### C++

`astyle`로 일관된 코드 스타일을 강제하고 있습니다. 자세한 내용은
[CODE_STYLE](../dev/explanation/code_style)을 참고해주세요.

### JSON 스타일

`tools/format` 경로에 있는 포매터로 일관된 JSON 스타일을 강제하고 있습니다.
[JSON Style Guide](../mod/json/explanation/json_style) 을 참고해주세요.

### 마크다운

`doc/`같은 마크다운 파일들은 [`deno`](https://deno.com)를 사용해 포매팅하고 있습니다.
[`deno fmt`](https://deno.land/manual/tools/formatter) 을 실행해 자동으로 마크다운 파일을
포매팅하세요. VsCode를 사용중이라면 다음 설정으로 저장할 때마다 자동 포매팅을 실행할 수 있습니다:

```json
// .vscode/settings.json
{
  "[markdown]": {
    "editor.formatOnSave": true,
    "editor.defaultFormatter": "denoland.vscode-deno"
  }
}
```

### Lua

Lua 파일은 [`dprint`](https://dprint.dev)의 내장 포매터를 사용해 포매팅하고 있습니다.
[`deno task dprint fmt`](https://dprint.dev/plugins/lua)를 실행해 Lua 파일을 자동으로
포매팅하세요. 자세한 내용은 [Lua Style Guide](./../mod/lua/explanation/lua_style.md)를
참고해주세요.

## 번역

카타클리즘: 밝은 밤의 번역은 Transifex에서 진행중입니다.
[번역 프로젝트](https://app.transifex.com/bn-team/cataclysm-bright-nights/)에서 지원되는 언어를
실시간으로 확인할 수 있습니다.

- [번역자를 위한 내용](../i18n/tutorial/transifex)
- [개발자를 위한 내용](../i18n/reference/translation)
- [관리자를 위한 내용](../i18n/guides/maintain)

## 공식 문서

<!-- <p align="middle">
    <img src="/docs/en/contributing/img/contributing-doxy1.png" width="48%">
    <img src="/docs/en/contributing/img/contributing-doxy2.png" width="48%">
</p> -->

자동 생성된 문서를 [깃허브 페이지](https://cataclysmbn.github.io/Cataclysm-BN)에서 읽을 수
있습니다.

### 독시젠(Doxygen) 주석

클래스와 클래스 멤버에 대한 상세한 문서가 있으면 새로운 기여자들이 코드를 읽고 이해하는데 도움이
됩니다. 기존 클래스에 독시젠 주석을 다는 것도 환영입니다.

클래스에 주석을 달 때는 다음 템플릿을 사용해주세요:

```cpp
/**
 * 간단한 설명
 *
 * 여러 단어로 된 긴 설명. (선택사항)
 */
class foo {
```

함수에 주석을 달 때는 다음 템플릿을 사용해주세요:

```cpp
/**
 * 간단한 설명
 *
 * 여러 단어로 된 긴 설명. (선택사항)
 * @param param1 param1에 대한 설명 (optional)
 * @return 반환값에 대한 설명 (optional)
 */
int foo(int param1);
```

멤버 변수에 주석을 달 때는 다음 템플릿을 사용해주세요:

```cpp
/** 간단한 설명 **/
int foo;
```

### 문서 추가 가이드라인

- 독시젠 주석은 '밖에서 보았을 때' 동작을 설명해야 합니다. 숨겨진 내부 구현에 대한 설명은 하지
  말아야 합니다. 하지만 카타클리즘의 많은 클래스들이 서로 연결되어 있어서, 구현에 대한 설명이 필요할
  때도 있을 것입니다.
- 새 기여자들이 이름만으로는 이해하기 어려운 것들에 대해서만 설명해주세요.
- 동어 반복은 피해주세요: `/** Map **/; map* map;`는 도움이 되지 않습니다.
- `X`에 대한 설명을 할 때, `X`와 다른 컴포넌트들 간의 상호작용에 대해서만 설명해주세요. `X` 자체가
  하는 일에 대해서는 설명하지 말아주세요.

### 문서를 로컬에서 빌드하기

- [독시젠 설치](https://www.doxygen.nl)
- `doxygen doxygen_doc/doxygen_conf.txt`
- `firefox doxygen_doc/html/index.html` (또는 다른 브라우저)

## 예시 워크플로우

### 작업 환경 설정하기

_(이 과정은 한 번만 하면 됩니다.)_

1. 원본 저장소를 깃허브에 포크합니다.

2. 포크한 저장소를 로컬에 클론합니다.

```sh
$ git clone https://github.com/깃허브_사용자명/Cataclysm-BN.git
# 터미널에서 현재 디렉토리에 저장소의 포크를 복제합니다.
```

3. 커밋 메시지 템플릿을 설정합니다.

```sh
$ git config --local commit.template .gitmessage
```

4. 원본 저장소를 원격 저장소로 추가합니다.

```sh
$ cd Cataclysm-BN
# 현재 작업 디렉토리를 새로 복제한 "Cataclysm-BN" 디렉토리로 변경합니다.
$ git remote add -f upstream https://github.com/cataclysmbn/Cataclysm-BN.git
# "upstream"이라는 원격 저장소를 추가합니다.
```

커밋 메시지 가이드라인에 대한 자세한 내용은 다음을 참고해주세요:

- [codeinthehole.com](https://codeinthehole.com/tips/a-useful-template-for-commit-messages/)
- [chris.beams.io](https://chris.beams.io/posts/git-commit/)
- [help.github.com](https://docs.github.com/ko/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue)

### `main` 브랜치 업데이트하기

1. `main` 브랜치가 체크아웃 되어 있는지 확인해주세요.

```sh
$ git checkout main
```

2. `upstream/main` 브랜치에서 변경사항을 가져옵니다.

```sh
$ git pull --ff-only upstream main
# "upstream" 원격 저장소의 "main" 브랜치에서 변경사항을 가져옵니다.
```

> **Note** 오류가 발생했다면, 로컬 `main` 브랜치에 직접 커밋을 했다는 뜻입니다.
> [이 문제를 해결하는 방법을 보려면 여기를 클릭하세요](#git-pull---ff-only을-했더니-에러가-나요).

### 변경사항 만들기

0. 아직 `main` 브랜치를 업데이트하지 않았다면, 업데이트하세요.

1. 기능 추가나 버그 수정을 하려 할 때마다, 새 브랜치를 만들어주세요.

```sh
$ git branch new_feature
# "new_feature"라는 새 브랜치를 만듭니다.
$ git checkout new_feature
# "new_feature" 브랜치를 활성화합니다.
```

2. 로컬에서 커밋을 했다면, 깃허브에 있는 포크에 푸시해야 합니다.

```sh
$ git push origin new_feature
# origin은 복제할 때 자동으로 포크를 가리키도록 설정되어 있습니다.
```

3. 브랜치에서 작업을 마치고, 모든 변경사항을 커밋하고 푸시했다면, `new_feature` 브랜치에서 이
   저장소의 `main` 브랜치로 풀 리퀘스트를 보내주세요.

> **Note** 깃허브의 `new_feature` 브랜치에 새 커밋이 생기면, 풀 리퀘스트에 자동으로 포함됩니다.
> 따라서 같은 브랜치에 관련된 변경사항만 커밋해주세요.

## 풀 리퀘스트 초안

풀 리퀘스트를 만들었지만 아직 작업 중이라면,
[초안(draft)](https://docs.github.com/ko/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests#draft-pull-requests)으로
표시해주세요. 이렇게 하면 준비된 상태가 아니라는 것을 리뷰어에게 알려줘 리뷰 진행 속도를 높일 수
있습니다.

풀 리퀘스트를 만들 때 이슈를 참조할 필요는 없지만, 참조하지 않느나면 PR이 어떤 문제룰 해결하려는지
자세히 설명해야 합니다.

### 모든 풀 리퀘스트에는 `"Summary"`줄이 있어야 합니다.

개요(summary)는 [변경 내역](../game/changelog.md)에 추가할 한 줄 요약입니다.

개요 형식: `SUMMARY: 카테고리 "설명"`

고를 수 있는 카테고리는 Features, Content, Interface, Mods, Balance, Bugfixes, Performance,
Infrastructure, Build, I18N이 있습니다.

예시: `SUMMARY: Content "Adds new mutation category 'Mouse'"` (해석:
`SUMMARY: Content "새로운 변이 카테고리 'Mouse'를 추가합니다."`)

[변경 내역 가이드라인](./changelog_guidelines.md)에서 카테고리에 대한 설명을 볼 수 있습니다.

### 키워드로 이슈 닫기

한 가지 더: 이슈를 닫거나 수정하거나 해결하는 PR을 만들 때, 설명에 다음을 포함해주세요.

```
- {키워드} #{이슈 번호}
```

예를 들어: `- fixed #12345`

### 키워드

`{키워드}`는 다음 중 하나여야 합니다.

- `close`, `closes`, `closed`
- `fix`, `fixes`, `fixed`
- `resolve`, `resolves`, `resolved`

### 이슈

그리고 `{이슈 번호}`는 풀 리퀘스트가 원본 저장소에 합쳐지고 나서 자동으로 닫힐 이슈 번호입니다.

이슈와 풀 리퀘스트를 동시에 닫을 수 있어 관리가 편리합니다.

### 여러 이슈를 한 번에 닫기

```
- {키워드} #{이슈 번호}, {키워드} #{이슈 번호}
```

더 자세한 설명은
[깃허브 공식 문서](https://docs.github.com/ko/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue)를
참고해주세요.

## 개발 도구 지원

코딩 스타일을 지키도록 도와주는 여러 도구들이 있습니다. 자세한 내용은
[DEVELOPER_TOOLING](../dev/reference/tooling.md)을 참고해주세요.

## 고급

꼭 필요한 것은 아니지만, 이런 규칙을 따르면 더 쉽게 관리할 수 있습니다.

### 원격 추적 브랜치 사용하기

원본 저장소의 `main` 브랜치에 대한 원격 추적 브랜치를 설정하면 쉽게 최신 변경사항을 가져올 수
있습니다.

```sh
$ git branch -vv
* main        xxxx [origin/main] ....
  new_feature xxxx ....
```

`main` 브랜치는 `origin/main` 브랜치를 추적하고 있고, `new_feature` 브랜치는 아무 브랜치도 추적하고
있지 않습니다. 그 말은 git이 어디에서 `new_feature` 에 대한 변경사항을 가져올지 모른다는 뜻입니다.

```sh
$ git checkout new_feature
'new_feature' 브랜치로 전환합니다
$ git pull
현재 브랜치에 추적 정보가 없습니다.
어떤 브랜치를 대상으로 병합할지 지정하십시오.
```

`new_feature` 브랜치에서 `upstream/main` 브랜치의 변경사항을 쉽게 가져오려면, git에 어떤 브랜치를
추적할지 알려줘야 합니다. (로컬 `main` 브랜치에도 적용할 수 있습니다.)

```sh
$ git branch -u upstream/main new_feature
Branch new_feature set up to track remote branch main from upstream.
$ git pull
Updating xxxx..xxxx
....
```

브랜치를 생성할 때 추적 정보를 설정할 수도 있습니다.

```sh
$ git branch new_feature_2 --track upstream/main
Branch new_feature_2 set up to track remote branch main from upstream.
```

> **Note** : 이렇게 하면 `upstream/main` 브랜치에서 변경사항을 가져오는 것은 쉬워지지만,
> `git push`는 여전히 실패합니다. `git push`는 `upstream/main` 브랜치에 변경사항을 푸시할 권한이
> 없기 때문입니다.

```sh
$ git push
error: The requested URL returned error: 403 while accessing https://github.com/cataclysmbn/Cataclysm-BN.git
fatal: HTTP request failed
$ git push origin
....
To https://github.com/깃허브_사용자명/Cataclysm-BN.git
xxxx..xxxx  new_feature -> new_feature
```

## 단위 테스트

`tests/` 경로에 단위 테스트가 있습니다. 게임 소스를 변경한 후에는 반드시 테스트를 실행해야 합니다.
`make` 명령을 실행하면 `tests/cata_test` 실행 파일이 생성됩니다. 이 파일은 일반적인 실행 파일처럼
실행할 수 있습니다. `make check` 명령으로도 실행할 수 있습니다. 아무 인자 없이 실행하면 전체
테스트를 실행합니다. `--help` 인자를 사용하면 실행 옵션을 볼 수 있습니다.

```sh
$ make
... compilation details ...
$ tests/cata_test
Starting the actual test at Fri Nov  9 04:37:03 2018
===============================================================================
All tests passed (1324684 assertions in 94 test cases)
Ended test at Fri Nov  9 04:37:45 2018
The test took 41.772 seconds
```

습관적으로 `make YOUR BUILD OPTIONS && make check` 명령을 실행하는 것을 추천합니다.

## 게임 내에서 테스트하기, 테스트 환경, 디버그 메뉴

새 기능을 구현하거나 버그를 수정하는 경우, 게임 내에서 변경사항을 테스트하는 것이 좋습니다. 평소처럼
게임을 플레이해서 정확한 조건을 만들어내기는 힘들지만, 디버그 메뉴를 사용하면 쉽게 테스트할 수
있습니다. 기본적으로 메뉴를 띄우는 단축키가 없으므로 먼저 단축키를 지정해야 합니다.

단축키 설정 메뉴를 띄웁니다. (`Esc`키를 누른 다음 `1`키를 누릅니다.) 아래로 스크롤해서 _디버그 메뉴_
항목을 찾고, `+` 키를 눌러 새로운 단축키를 추가합니다. 테스트를 한 다면 새 캐릭터로 하는 것이
좋습니다. 방금 설정한 단축키를 누르면 다음과 같은 화면이 나타날 것입니다.

```
┌─────────────────────────────────────────────────────┐
│ 디버그 기능 - 현실을 뜯어고칠 시간입니다!           │
├─────────────────────────────────────────────────────┤
│ i Info                                              │
│ Q Quit to main menu                                 │
│ s Spawning...                                       │
│ p Player...                                         │
│ t Teleport...                                       │
│ m Map...                                            │
└─────────────────────────────────────────────────────┘
```

위 명령어들을 사용해서 변경사항을 테스트할 수 있습니다.
[BN 위키](https://cataclysmbn-guide.com/)에도 디버그 메뉴에 대한 정보가 있습니다.

## 자주 묻는 질문

### `git pull --ff-only`을 했더니 에러가 나요

`git pull --ff-only`를 실행했더니 에러가 났다면, `main` 브랜치에 직접 커밋을 했기 때문입니다. 그
이유는 `main` 브랜치의 내용이 원격과 로컬에서 각각 달라졌기 때문에, git이 원격과 로컬 중 무엇을
유지하고 무엇을 버릴 지 모르기 때문입니다. 이를 고치려면, 새 브랜치를 만들고, `upstream/main`
브랜치와 분기된 지점을 찾은 다음, `main` 브랜치를 그 지점으로 되돌려야 합니다.

```sh
$ git pull --ff-only upstream main
From https://github.com/cataclysmbn/Cataclysm-BN
 * branch            main     -> FETCH_HEAD
fatal: Not possible to fast-forward, aborting.
$ git branch new_branch main          # 현재 커밋 내역을 임시 브랜치에 백업합니다
$ git merge-base main upstream/main
cc31d0... # main에 커밋하기 직전 가장 마지막 커밋
$ git reset --hard cc31d0....
HEAD is now at cc31d0... ...
```

이제 `main`가 정리되었으니 `upstream/main`에서 변경 내역을 끌어오고, `new_branch`에서 계속 작업할 수
있습니다.

```sh
$ git pull --ff-only upstream main
# "upstream" 원격 저장소에서 "main" 브랜치의 변경사항을 가져옵니다
$ git checkout new_branch
```

더 자주 묻는 질문은 [개발자 FAQ](../dev/reference/faq.md)를 참고해주세요.
