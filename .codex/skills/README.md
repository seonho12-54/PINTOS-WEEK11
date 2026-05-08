# Codex 스킬

이 디렉터리는 GitHub 작업 흐름을 돕는 Codex 스킬 3개를 담고 있다.

- `gh-create-issue-branch`: 한국어 GitHub 이슈를 만들고, 해당 이슈 번호에 맞는 `<type>-#<issue>` 브랜치를 생성한다.
- `gh-create-pr`: 이슈 번호가 포함된 브랜치에서 한국어 GitHub PR을 생성한다.
- `gh-review-pr`: GitHub PR을 리뷰하고, 필요한 경우 PR diff에 라인별 리뷰 코멘트를 남긴다.

## 설치 방법

macOS 또는 Linux:

```sh
./scripts/install-skills.sh
```

Windows PowerShell:

```powershell
.\scripts\install-skills.ps1
```

두 스크립트 모두 `CODEX_HOME` 환경 변수가 설정되어 있으면 `$CODEX_HOME/skills`에 설치한다.
설정되어 있지 않으면 기본 Codex 홈인 `~/.codex/skills`에 설치한다.

## 사용법

### `gh-create-issue-branch`

GitHub 이슈를 먼저 만들고, 그 이슈 번호에 맞는 브랜치까지 한 번에 만들고 싶을 때 사용한다.

프롬프트를 작성할 때 `$ GitHub Issue Branch` 스킬을 적용한 뒤, 할 작업을 간략하게 적는다.

예시:

```text
$ GitHub Issue Branch
priority donation 구조를 donations 리스트 기반으로 개선하고 싶어
```

그러면 Codex가 이슈 제목 초안, 본문 초안, 타입 prefix, repo override, assignee 등을 물어본다.
작업 내용을 이미 충분히 적었다면 `알아서 해`라고 답해도 된다.

Codex는 답변 내용을 바탕으로 다음 작업을 수행한다.

- 이슈 제목과 본문을 한국어로 정리한다.
- `gh`로 GitHub issue를 생성한다.
- 생성된 이슈 번호에 맞는 브랜치를 만든다.
- 새 브랜치로 전환한다.

브랜치 이름은 기본적으로 다음 형식을 따른다.

```text
<type>-#<issue-number>
```

예시:

```text
feat-#12
fix-#18
```

### `gh-create-pr`

현재 작업이 `feat-#12`처럼 이슈 번호가 포함된 브랜치에 있고, 그 브랜치로 GitHub PR을 만들고 싶을 때 사용한다.

프롬프트를 작성할 때 `$ GitHub PR Creator` 스킬을 적용한 뒤, PR 생성을 요청한다.

예시:

```text
$ GitHub PR Creator
현재 브랜치 작업으로 PR 만들어줘
```

그러면 Codex가 PR 제목 초안, 본문 초안, 타입 prefix, Draft PR 여부, base branch, assignee, issue 번호 override 등을 물어본다.

Codex는 PR 제목을 다음 형식으로 정리한다.

```text
<type>: <한국어 PR 제목>
```

PR 본문은 `docs/rules.md`의 팀 PR 템플릿을 따른다.

```md
## 작업 요약

## 변경 내용

## 목표 테스트

## 테스트 결과

## 리뷰 포인트

## 체크리스트
```

`Draft PR 여부`에 `예`라고 답하면 Codex가 `--draft` 옵션을 사용해 Draft PR로 생성한다.
비워두거나 `아니오`라고 답하면 일반 PR로 생성한다.

PR 본문 마지막에는 `Closes #<issue-number>`가 들어간다.
이 문구가 있어야 PR이 머지될 때 연결된 이슈가 GitHub에서 자동으로 닫힌다.

### `gh-review-pr`

GitHub PR을 리뷰하고, 수정이 필요한 부분에 라인별 리뷰 코멘트를 남기고 싶을 때 사용한다.

프롬프트를 작성할 때 `$ GitHub PR Review` 스킬을 적용한 뒤, PR URL 또는 PR 번호를 전달한다.

예시:

```text
$ GitHub PR Review
https://github.com/owner/repo/pull/123 리뷰해줘
```

Codex는 `gh`를 사용해 PR 정보와 diff를 확인하고, 로컬 코드 맥락과 비교하면서 리뷰한다.
주로 다음 항목을 우선 확인한다.

- 동작이 깨질 수 있는 변경
- edge case 누락
- 필수 요구사항 누락
- 데이터 손실 가능성
- race condition
- 테스트 누락 또는 검증 부족

수정이 필요한 부분을 찾으면 PR diff의 해당 라인에 리뷰 코멘트를 남긴다.
이 스킬이 남기는 GitHub 리뷰 코멘트는 구분하기 쉽도록 `AI) `로 시작한다.

라인별로 남길 만한 문제가 없으면 억지로 코멘트를 만들지 않고, 발견된 문제가 없다는 점과 전체 PR 위험도, 실행하지 못한 테스트를 정리해서 알려준다.
