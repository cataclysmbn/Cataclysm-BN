# 개발 문서 수정하기

준비사항:

- [y분 만에 markdown 배우기](https://learnxinyminutes.com/ko/markdown/) 읽기
- [github 계정 생성](https://github.com/join) (개발 문서의 소스 코드가 github에 호스팅되어 있기 때문입니다)

## 브라우저

![페이지 편집](img/edit.webp)

1. 페이지 하단의 `Edit page` 버튼을 클릭합니다.

![alt text](img/github-edit.webp)

2. 그러면 개발 문서의 github 페이지로 리다이렉트될 것입니다. 여기에서 변경 사항을 편집하고 미리 볼 수 있습니다.

> [!NOTE]
>
> - `CONTRIBUTING.md`의 `.md`는 마크다운 파일을 의미합니다.
> - `docs.mdx`의 `.mdx`는 [MarkDown eXtended](https://mdxjs.com)를 의미합니다.
>   - 자바스크립트와 [jsx 컴포넌트][jsx] 지원이 포함된 마크다운의 상위 집합입니다.
>   - 조금 더 복잡하지만 반응형 컴포넌트를 사용할 수 있습니다.

[jsx]: https://www.typescriptlang.org/docs/handbook/jsx.html

![변경 사항 제안 창](https://github.com/scarf005/Cataclysm-BN/assets/54838975/d4a06795-1680-4706-a84c-072346bff109)

1. 오른쪽 상단의 `Commit changes...` 버튼을 클릭하여 [변경 사항을 커밋](https://github.com/git-guides/git-commit)합니다.

- 짧고 이해하기 쉬운 `Commit message`를 작성해주세요.
- `Create a new branch for this commit and start a pull request` 체크박스 선택

![변경 사항 비교 페이지](https://github.com/scarf005/Cataclysm-BN/assets/54838975/3551797e-847b-45fe-8869-8b0b15bfb948)

4. 그러면 `Comparing changes` 페이지로 리다이렉트되게 됩니다. 이제 `Create pull request` 버튼을 클릭하여 [풀 리퀘스트를 생성](./contributing.md#pull-request-notes)합니다.

![풀 리퀘스트 열기 페이지](https://github.com/scarf005/Cataclysm-BN/assets/54838975/2a987c19-b165-43c2-a5a2-639f22202926)

1. `Create pull request` 버튼을 클릭하여 [PR을 생성합니다](./contributing.md#pull-request-notes). 작은 변경의 경우 PR 내용을 비워도 괜찮습니다.

## 개인 PC에서 개발

> [!NOTE]
>
> 이 항목은 [git](https://git-scm.com)과 [javascript](https://developer.mozilla.org/en-US/docs/Web/JavaScript)에 대한 지식이 있다고 가정합니다. 물론 하면서 배워도 무방합니다.

개발 문서를 로컬(개인 개발 환경)에서 실행하려면 다음이 필요합니다:

- [deno](https://deno.com) 설치 (문서 자동 생성 및 포매팅 프로그램)

### 개발 서버 설정

```sh
(Cataclysm-BN) $ deno task docs serve

# 또는 이미 docs 디렉토리 안에 있는 경우
(Cataclysm-BN/docs) $ deno task serve
```

`http://localhost:3000`에서 개발 문서에 접속할 수 있습니다. 문서를 수정하면 웹사이트가 자동으로 업데이트됩니다.

### 자동화된 페이지 생성

Lua 및 CLI 문서는 소스 코드를 바탕으로 자동으로 생성됩니다. 생성하려면 프로젝트 루트로 이동하여 다음을 실행하세요:

```sh
(Cataclysm-BN) $ deno task docs:gen
```

## 라이선스

- 마크다운 파일(`.md` 및 `.mdx` 파일을 포함하되 이에 국한되지 않음)에 기여함으로써 귀하는 귀하의 기여를 게임과 동일한 라이선스인 [CC-BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/)에 따라 라이선스하는 데 동의하는 것입니다.

- 문서 페이지의 소스 코드(`.ts` 파일을 포함하되 이에 국한되지 않음)에 기여함으로써 귀하는 귀하의 기여를 [AGPL 3.0](https://www.gnu.org/licenses/agpl-3.0.en.html)에 따라 라이선스하는 데 동의하는 것입니다.
