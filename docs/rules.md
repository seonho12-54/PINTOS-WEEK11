# Team Rules

이 문서는 우리 팀의 기본 개발 규칙 초안이다. 세부 내용은 스프린트 진행 중 합의에 따라 계속 업데이트한다.

## 1. Commit 컨벤션

### 기본 원칙

- 커밋은 하나의 목적만 담는다.
- 커밋 메시지만 보고도 변경 의도를 이해할 수 있어야 한다.

### 형식

```text
type: summary
```

예시:

```text
feat: priority donation 기능 추가
fix: timer sleep 경쟁 상태 수정
refactor: thread scheduling 헬퍼 분리
docs: PR 규칙 문서 업데이트
test: list 유틸 테스트 추가
```

### type 목록과 의미

| type       | 의미                                                                         | 사용 기준                                                           |
| ---------- | ---------------------------------------------------------------------------- | ------------------------------------------------------------------- |
| `feat`     | 새로운 기능을 추가할 때 사용한다.                                            | 사용자 입장에서 새로운 동작이 생기거나 기존 기능 범위가 확장된 경우 |
| `fix`      | 버그를 수정할 때 사용한다.                                                   | 의도한 동작과 다르게 작동하던 문제를 바로잡는 경우                  |
| `refactor` | 동작 변화 없이 구조를 개선할 때 사용한다.                                    | 코드 구조는 바꾸지만 외부 동작과 결과는 유지하는 경우               |
| `style`    | 코드 동작 변화 없이 포맷, 정렬, 공백, 네이밍 등 스타일만 수정할 때 사용한다. | 로직 수정 없이 코드 스타일이나 표현만 정리하는 경우                 |
| `docs`     | README, 규칙 문서, 주석 등 문서성 내용을 수정할 때 사용한다.                 | 실행 결과에는 영향이 없고 문서나 설명만 바꾸는 경우                 |
| `test`     | 테스트 코드를 추가하거나 수정할 때 사용한다.                                 | 운영 코드보다 테스트 코드 변경이 핵심인 경우                        |
| `chore`    | 빌드 설정, 스크립트, 의존성, 기타 잡성 유지보수 작업에 사용한다.             | 기능이나 버그 수정보다는 개발 환경 유지가 핵심인 경우               |

### type 예시

```text
feat: alarm clock 스케줄러 구현
fix: donation에서 null lock holder 처리
refactor: ready list 헬퍼 함수 분리
style: thread struct 주석 정렬
docs: dev PR 체크리스트 문구 정리
test: priority preemption 테스트 추가
chore: unit test 빌드 스크립트 정리
```

### 작성 기준

- summary는 한국어로 짧고 명확하게 작성한다.
- 한 커밋에 서로 다른 주제의 수정은 넣지 않는다.

## 2. dev, main PR 기준과 템플릿

### 브랜치 역할

- `dev`: 팀 개발 통합 브랜치
- `main`: 배포 또는 제출 기준 브랜치

### PR 기본 규칙

- 모든 작업은 브랜치를 따서 진행한다.
- `dev`로 올리는 PR은 담당 기능이 완성되었을 때, 퇴근 전 진행 상황 공유를 위해 올린다.
- `main`으로 올리는 PR은 팀 기준 검증이 끝난 통합 결과만 올린다.
- GitHub PR 리뷰에서 `Approve`가 2개 이상 달리면 작성자가 직접 머지한다.
- 충돌이 있으면 작성자가 직접 해결하고 다시 검증한다.

### `dev` PR 기준

- `dev` PR은 기능이 완성되었을 때 올린다.
- 여기서 기능 완성이란, 본인이 맡은 테스트 케이스가 통과되는 것을 직접 확인한 상태를 의미한다.
- 테스트 케이스 의존성은 테스트 실행을 위해 AI의 도움을 받아 해결하고, PR은 AI 생성 코드를 제거한 후 올린다. (본인 code commit -> AI로 의존성 해결 -> 테스트 -> 통과 시 통과한 본인 code 기준으로 rollback)
- `dev` PR은 팀원에게 진행 상황 공유를 위해 퇴근 전에 올린다.
- 미완성 작업은 `dev` 머지용 PR로 올리지 않고 Draft PR로 공유한다.
- 리뷰어가 확인할 수 있도록 변경 이유와 통과한 테스트 케이스를 적는다.

### `main` PR 기준

- `main` PR은 팀원 모두 회귀 테스트가 모두 통과한 뒤에 올린다.
- 새로 단 주석이 정리되어 있어야 한다. 주석 작성은 본인이 직접 해도 되고 AI 도움을 받아도 된다.
- 4번의 코드 컨벤션에 맞게 코드 수정이 반영되어 있어야 한다.
- AI가 해준 PR 리뷰 기준에 맞게 필요한 리팩터링이 반영되어 있어야 한다.

### PR 제목

```text
[page|frame|swap|mmap|fault|stack] 핵심 변경 한 줄
```

예시:

```text
[page] supplemental page table 구현
[frame] frame allocation 구현
[swap] swap in/out 구현
[mmap] memory mapped file 구현
[fault] page fault 처리 구현
[stack] stack growth 구현
```

### PR 템플릿

```md
## 작업 요약

-

## 변경 내용

-

## 목표 테스트

- [ ] `make tests/vm/<test-name>.result`

## 테스트 결과

| 테스트        | 결과 |
| ------------- | ---- |
| `<test-name>` | PASS |

## 리뷰 포인트

-

## 체크리스트

- [ ] build 성공
- [ ] 목표 테스트 통과
- [ ] 관련 회귀 테스트 확인
- [ ] debug print 제거
- [ ] 테스트 이름 기반 하드코딩 없음
- [ ] 복잡한 invariant는 주석 또는 문서에 설명
```

## 3. 팀 skills 사용법

### 설치 방법

레포지토리 루트에서 아래를 실행한다.

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

### SKILL 1: `gh-create-issue-branch`

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

### SKILL 2: `gh-create-pr`

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

PR 본문은 `docs/rules.md`의 2번 PR 템플릿을 따른다.

`Draft PR 여부`에 `예`라고 답하면 Codex가 `--draft` 옵션을 사용해 Draft PR로 생성한다.
비워두거나 `아니오`라고 답하면 일반 PR로 생성한다.

PR 본문 마지막에는 `Closes #<issue-number>`가 들어간다.
이 문구가 있어야 PR이 머지될 때 연결된 이슈가 GitHub에서 자동으로 닫힌다.

### SKILL 3: `gh-review-pr`

Codex로 GitHub PR을 리뷰하고, 수정이 필요한 부분에 라인별 리뷰 코멘트를 남기고 싶을 때 사용한다.

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

## 4. 코드 컨벤션

### 요약

- 기존 PintOS와 주변 `develop` 코드 스타일을 우선 따른다.
- naming, indentation, comment style은 주변 코드와 통일한다.
- helper가 파일 내부 전용이면 `static`, 여러 파일에서 쓰면 header에 선언한다.
- 함수명은 동사, 변수명은 명사 중심으로 짓고 기존 모듈 prefix를 유지한다.
- 새 함수에는 짧은 헤더 주석을 달고, 주석은 무엇보다 왜를 설명한다.
- `if`, `for`, `while`은 한 줄이어도 중괄호를 사용한다.

### 기본 원칙

- 기존 PintOS 코드 스타일을 최우선으로 따른다.
- 새 코드도 현재 `develop`의 주변 코드 스타일에 맞춘다.
- indentation, naming, comment style은 주변 코드와 통일한다.

### 함수와 변수 이름

- 함수명은 동사 중심으로 짓는다.
- 변수명은 명사 중심으로 짓는다.
- 기존 모듈 prefix가 있으면 유지한다.
- 예: `thread_...`, `process_...`, `file_...`
- 함수 이름은 역할이 드러나게 짓되, 너무 길게 만들지 않는다.
- 반복문 index 외에는 의미 없는 짧은 이름을 피한다.

### 함수 구조

- helper 함수가 파일 내부 전용이면 `.c` 파일 안에서 `static`으로 선언한다.
- 여러 파일에서 쓰는 함수만 header에 prototype을 추가한다.
- 불필요한 public 함수 선언을 늘리지 않는다.

### 주석

- 새 함수를 만들면 짧은 함수 헤더 주석을 단다.
- 주석은 코드가 무엇을 하는지보다 왜 필요한지를 설명한다.
- 인라인 주석은 최소화한다.

### Formatting

- 줄바꿈, 공백, 들여쓰기는 주변 PintOS 코드와 맞춘다.
- 예:

```c
if (cond) {
  do_work ();
}
```

- `if`, `for`, `while`의 body가 한 줄이어도 중괄호를 사용한다.

## 5. 스크럼 타임

- 매일 `12:55`에 스크럼을 진행한다. (일요일 제외)
- 늦으면 팀원들에게 커피를 산다.

## 6. AI 사용 기준

### 사용 원칙

- 아래 규칙 및 AGENTS.md에 따라 AI를 사용한다.
- AI가 만든 코드와 문서는 반드시 사람이 검토한다.
- 프로젝트 맥락 없이 나온 답변은 그대로 반영하지 않는다.

### 허용 범위

- GitBook을 읽을 때 핵심 내용 정리나 요약을 받는 용도
- 이론 학습 중 개념 설명이나 비교를 받는 용도
- 테스트 케이스를 분석하고 의도나 확인 포인트를 정리하는 용도
- PR 리뷰를 통해 리팩터링이 필요한 부분을 확인하는 용도

### 금지 또는 주의 사항

- 동작 검증 없이 AI 코드 그대로 머지하지 않는다.
- 근거 없는 성능 개선 주장이나 설계 변경을 바로 수용하지 않는다.
- 민감 정보, 비밀키, 계정 정보는 AI 입력에 넣지 않는다.
- 팀 합의가 필요한 아키텍처 변경은 AI 제안만으로 결정하지 않는다.

### PR에 남기면 좋은 내용

- 어디까지 AI 도움을 받았는지
- 사람이 직접 검증한 범위가 어디인지
- 남아 있는 리스크가 무엇인지
