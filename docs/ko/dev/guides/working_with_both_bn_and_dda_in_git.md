# BN과 DDA 함께 작업하기

DDA에서 변경사항을 가져와야 할 때가 있습니다. 리모트를 추가하고 체리픽하면 됩니다.

# 요약

```
git remote add dda https://github.com/CleverRaven/Cataclysm-DDA
git fetch dda
git checkout -b ddamaster dda/master
git checkout ddamaster && git pull
```

# 자세한 설명

## 설정

BN 디렉토리 `Cataclysm-BN`이 있다고 가정하고, 터미널을 엽니다.

DDA 원격 추적 브랜치를 추가합니다. 브랜치 이름은 `dda`로 지정합니다 (다른 이름도 가능):

```
git remote add dda https://github.com/CleverRaven/Cataclysm-DDA
```

새 브랜치의 내용을 다운로드하려면 fetch해야 합니다:

```
git fetch dda
```

이렇게 하면 원격 추적 브랜치의 모든 내용이 다운로드됩니다. 두 저장소에 공통된 내용은 다운로드하지 않으므로 시간이 오래 걸리지 않습니다. fetch한 후에는 원격 브랜치를 `merge`, `pull`, `checkout` 등 할 수 있지만, 브랜치 이름 앞에 `dda/`를 붙여야 합니다 (예: `dda/master`). 원격 추적 브랜치를 가리키는 로컬 브랜치를 만들면 편리합니다:

```
git checkout -b ddamaster dda/master
```

이렇게 하면 DDA의 `master` 브랜치를 추적하는 `ddamaster` 브랜치가 생성됩니다. `ddamaster` 대신 다른 이름을 사용해도 됩니다.

## 업데이트

가장 간단한 방법:

```
git checkout ddamaster
git pull
```

충돌이 발생하지 않아야 합니다. 충돌이 발생했다면 메인 브랜치에 변경사항을 커밋했을 가능성이 있습니다. 이 경우 백업해두는 것이 좋습니다:

```
git checkout -b temp-branch-name
```

그리고 메인 브랜치를 원격으로 리셋합니다:

```
git branch -f ddamaster dda/master
```

## 기여하기

```
# BN 브랜치로 전환
git checkout main
# 로컬 내용 업데이트
git pull
# 변경사항을 위한 새 브랜치 생성
git checkout -b chainsaw-toothbrush-rebalance
# [파일 수정]
...
# 변경사항 커밋
git commit -a
# 포크에 변경사항 업로드 (origin이 포크 브랜치 이름이라고 가정)
git push -u origin chainsaw-toothbrush-rebalance
# BN의 github으로 가서 풀 리퀘스트 생성
```

# 포팅

포팅은 `git cherry-pick`으로 수행합니다. 먼저 머지 커밋의 해시나 포팅하려는 커밋 범위의 첫 번째와 마지막 해시를 찾아야 합니다. github에서는 PR의 커밋 설명 오른쪽에 표시됩니다. 전체 해시나 짧은 버전을 사용할 수 있습니다. 예제에서 `fafafaf`는 머지 커밋이고, `a0a0a0a`와 `b1b1b1b`는 범위의 첫 번째와 마지막 커밋입니다 (순서가 중요). 포팅할 브랜치에서 머지 커밋이나 커밋 범위를 체리픽합니다:

```
# 머지 커밋
git cherry-pick fafafaf
# 커밋 범위
git cherry-pick a0a0a0a..b1b1b1b
```

충돌을 해결합니다 (간단하지 않은 PR의 경우 충돌이 많을 것이고 대부분 수동으로 해결해야 합니다):

```
git mergetool
```

위 명령은 머지 충돌 해결 도구 설정이 필요할 수 있습니다 (TODO: 설정 방법 설명). 그런 다음 평소처럼 커밋하고 푸시합니다.
