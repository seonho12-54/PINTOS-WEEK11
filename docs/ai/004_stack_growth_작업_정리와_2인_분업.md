# Stack Growth 작업 정리와 2인 분업

## 문서 목적

- `stack growth` 구현 범위를 `docs/reference` 기준으로 정리한다.
- 2명이 비슷한 난이도와 비슷한 시간으로 나눠 맡을 수 있도록 함수 단위 분업안을 만든다.
- 완료 판정 테스트와, 다음 단계인 `mmap`으로 넘어가기 위한 조건을 정리한다.

## 기준 자료

- 팀 규칙: `docs/rules.md`
- 우선 reference:
  - `docs/reference/pintos-kaist-original/3_project3/3_stack_growth.md`
  - `docs/reference/pintos-kaist-original/3_project3/2_anon.md`
  - `docs/reference/pintos-kaist-original/3_project3/4_memory_mapped_files.md`
- 보조 확인:
  - `docs/reference/pintos-kaist-original/3_project3/0_introduction.md`
  - `pintos/tests/vm/Make.tests`
  - `pintos/tests/vm/pt-grow-stack.c`
  - `pintos/tests/vm/pt-big-stk-obj.c`
  - `pintos/tests/vm/pt-grow-stk-sc.c`
  - `pintos/tests/vm/pt-grow-bad.c`
  - `pintos/tests/vm/page-merge-stk.c`
  - `pintos/tests/vm/mmap-over-stk.c`

## reference 기준 핵심 요구사항

`docs/reference/pintos-kaist-original/3_project3/3_stack_growth.md` 기준으로 이번 단계의 요구사항은 아래와 같다.

1. Project 2의 단일 stack page를 넘어서면 필요한 만큼 stack page를 추가로 할당해야 한다.
2. 추가 할당은 아무 fault에나 하면 안 되고, "stack access처럼 보이는 경우"에만 해야 한다.
3. x86-64 `PUSH` 특성 때문에 fault address가 `rsp`보다 최대 8 bytes 아래일 수 있다는 점을 반영해야 한다.
4. user mode fault뿐 아니라 kernel mode에서 발생한 page fault도 고려해야 한다.
   - system call 처리 중 kernel이 user buffer를 접근하다가 fault가 나는 경우가 대표적이다.
   - 이 경우 `page_fault()`에 들어온 `intr_frame->rsp`는 user rsp가 아닐 수 있으므로, user rsp를 다른 경로로 저장해 둬야 한다.
5. `vm/vm.c`의 `vm_try_handle_fault()`가 stack growth 여부를 판별해야 한다.
6. 실제 page 추가는 `vm/vm.c`의 `vm_stack_growth()`가 담당해야 한다.
7. stack 최대 크기는 1 MiB로 제한해야 한다.

## 현재 코드 기준 상태

현재 저장소에서 stack growth와 직접 연결되는 상태는 아래와 같다.

1. `pintos/vm/vm.c:192`의 `vm_stack_growth()`는 비어 있다.
2. `pintos/vm/vm.c:202-224`의 `vm_try_handle_fault()`는 현재 "이미 SPT에 있는 page를 claim하는 경우"만 처리하고, stack growth는 아직 식별하지 않는다.
3. `pintos/userprog/process.c:1277-1285`의 VM 버전 `setup_stack()`은 첫 stack page를 `VM_ANON | VM_MARKER_0`로 잡고 즉시 claim하고 있다.
4. `pintos/include/threads/thread.h:133-149`의 `struct thread`에는 SPT는 있지만, 저장된 user rsp나 현재 stack 하한을 추적하는 field는 없다.
5. `pintos/userprog/syscall.c:132-175`의 `validate_user_ptr()` / `validate_user_buffer()`는 unmapped user page를 선제적으로 reject하는 방식이다.
   - 이 구조를 그대로 두면 `pt-grow-stk-sc`처럼 "system call 내부에서 처음 닿는 stack page"를 살리기 어렵다.

정리하면, 첫 stack page 준비는 이미 들어가 있지만, 그 아래로 grow하는 경로와 syscall 중 kernel fault 대응 경로가 아직 비어 있다.

## 작업 단위 정리

이번 단계를 기능으로 쪼개면 아래 네 덩어리다.

1. user rsp를 믿을 수 있게 확보하는 일
2. fault가 stack growth인지 판단하는 일
3. stack page를 실제로 여러 장 늘리는 일
4. syscall의 user buffer 검증이 stack growth를 막지 않게 조정하는 일

이 네 가지를 2명에게 비슷한 무게로 나누려면, 한 사람은 "판단 경로", 다른 한 사람은 "실제 확장 경로"를 맡는 구성이 가장 균형이 좋다.

## 2인 분업안

### A 담당: fault 판별과 user rsp 보존 경로

담당 파일과 함수:

- `pintos/include/threads/thread.h`
  - `struct thread`
- `pintos/userprog/syscall.c`
  - `syscall_handler(struct intr_frame *f)`
- `pintos/vm/vm.c`
  - `vm_try_handle_fault(struct intr_frame *f, void *addr, bool user, bool write, bool not_present)`

필요하면 A가 같은 파일 안에 private helper를 추가해도 된다.

예상 로직:

1. `struct thread`에 "마지막으로 신뢰할 수 있는 user rsp"를 저장할 field를 추가한다.
2. `syscall_handler()` 진입 직후 `f->rsp`를 현재 thread에 저장한다.
   - reference가 말하는 "initial transition from user to kernel mode에서 rsp를 저장" 역할이다.
3. `vm_try_handle_fault()`에서 fault 원인을 3단계로 나눈다.
   - 진짜 invalid access인지
   - 기존 SPT page에 대한 lazy load / claim인지
   - stack growth 후보인지
4. stack growth 후보 판별 시 아래 조건을 함께 본다.
   - `addr`가 user virtual address인지
   - not-present fault인지
   - write fault인지 여부를 어떻게 볼지 팀 내에서 합의할 것
   - 기준 rsp 근처인지
     - user fault면 `f->rsp`
     - kernel fault면 thread에 저장한 user rsp
   - 현재 rsp 바로 아래의 자연스러운 stack 확장으로 볼 수 있는지
   - 사용자 stack의 합법적인 상단 영역 안에 있는지
5. 기존 SPT에 page가 없고 stack growth 후보면 `vm_stack_growth(addr)`를 호출한 뒤, 새로 생긴 page를 claim하도록 경로를 정리한다.

이 사람이 끝내야 하는 결과:

- user fault와 syscall 중 kernel fault가 모두 같은 heuristic으로 stack growth 판단에 들어간다.
- `vm_try_handle_fault()`가 "SPT miss면 바로 false"로 끝나지 않는다.

### B 담당: 실제 stack page 확장과 syscall 검증 경로

담당 파일과 함수:

- `pintos/include/threads/thread.h`
  - `struct thread`
- `pintos/userprog/process.c`
  - `setup_stack(struct intr_frame *if_)`
- `pintos/vm/vm.c`
  - `vm_stack_growth(void *addr)`
- `pintos/userprog/syscall.c`
  - `validate_user_ptr(const void *uaddr)`
  - `validate_user_buffer(const void *buffer, size_t size)`

필요하면 B가 same-file helper를 추가해도 된다.

예상 로직:

1. `struct thread`에 현재 stack의 하한을 추적할 field를 둘지, 아니면 `VM_MARKER_0`가 붙은 stack page를 SPT에서 찾는 방식으로 갈지 결정한다.
   - 이번 단계에서는 "현재 stack 하한 field"를 두는 쪽이 `vm_stack_growth()`와 `mmap` overlap 검사까지 생각하면 더 단순하다.
2. `setup_stack()`에서 첫 stack page를 만든 직후, 현재 stack 하한 metadata도 같이 초기화한다.
   - 현재 코드는 첫 page를 `VM_ANON | VM_MARKER_0`로 잡는 준비가 이미 되어 있다.
3. `vm_stack_growth()`에서 `pg_round_down(addr)`부터 현재 stack 하한 바로 위까지 필요한 익명 page를 연속으로 할당한다.
   - page마다 stack marker를 유지한다.
   - 1 MiB limit을 넘기지 않도록 방어한다.
   - 성공 시 thread의 stack 하한 metadata를 갱신한다.
4. `validate_user_ptr()` / `validate_user_buffer()`가 stack growth를 가로막지 않도록 조정한다.
   - 현재처럼 "unmapped이면 즉시 exit(-1)"로 두면 `pt-grow-stk-sc`가 막힌다.
   - 최소한 "합법적인 user range이고, 아직 mapping은 없지만 stack growth 후보가 될 수 있는 주소"는 선제 종료하지 않게 바꿔야 한다.
   - 이 단계에서는 검증 helper가 "명백한 kernel/null/범위 밖 주소만 즉시 reject"하고, 실제 page 확보는 fault 경로에 맡기는 쪽이 자연스럽다.

이 사람이 끝내야 하는 결과:

- 한 번의 fault로 필요한 stack page가 실제로 생긴다.
- syscall에서 user buffer를 검사할 때, 아직 grow되지 않은 stack page를 성급하게 죽이지 않는다.

## 왜 이 분업이 균형이 맞는가

A는 "언제 grow해야 하는가"를 맡고, B는 "어떻게 grow하고 syscall 검증과 연결할 것인가"를 맡는다.

- A는 예외 처리, heuristic, user/kernel 경계 처리라는 논리 난도가 높다.
- B는 실제 page 생성, metadata 유지, syscall 검증 조정이라는 구현량이 많다.

둘 다 `thread.h`, `vm.c`, `syscall.c`를 일부 건드리지만 책임이 겹치지 않도록 아래 기준을 고정하면 충돌을 줄일 수 있다.

- A 소유:
  - user rsp 저장 field 이름과 의미
  - `syscall_handler()`
  - `vm_try_handle_fault()`
- B 소유:
  - stack 하한 metadata 이름과 의미
  - `setup_stack()`
  - `vm_stack_growth()`
  - `validate_user_*()`

## 의존성과 작업 순서

### 먼저 합의할 것

둘이 먼저 10분 안에 아래 3개만 합의하면 이후 충돌이 크게 줄어든다.

1. `struct thread`에 넣을 field 2개 이름
   - saved user rsp
   - current stack lower bound 또는 같은 역할의 metadata
2. stack growth heuristic
   - fault address가 현재 rsp 바로 아래의 자연스러운 stack 접근처럼 보이는지
   - 사용자 stack의 유효한 범위를 벗어나지 않는지
3. syscall 검증 정책
   - unmapped user page를 즉시 죽일지
   - 아니면 fault 경로로 넘길지

### 추천 작업 순서

1. B가 `setup_stack()`과 stack metadata contract를 먼저 정한다.
2. A가 `syscall_handler()`와 `vm_try_handle_fault()`에서 saved user rsp 경로를 만든다.
3. B가 `vm_stack_growth()`를 붙인다.
4. B가 `validate_user_buffer()`를 조정해 `pt-grow-stk-sc` 경로를 연다.
5. A와 B가 같이 invalid fault 회귀를 확인한다.

### 상호 의존성

- A의 `vm_try_handle_fault()`는 B가 정한 stack metadata 의미를 알아야 한다.
- B의 `validate_user_*()`는 A가 정한 "stack growth 후보 주소" 기준을 알아야 한다.
- 즉, 구현은 병렬로 가능하지만 heuristic과 metadata contract는 먼저 맞춰야 한다.

## 완료 판정 테스트

### 직접 완료 테스트

아래 4개는 stack growth 완료를 직접 보여 주는 핵심 테스트다.

1. `pt-grow-stack`
   - 4 KiB stack object 접근이 성공해야 한다.
2. `pt-big-stk-obj`
   - 64 KiB stack object 접근이 성공해야 한다.
3. `pt-grow-stk-sc`
   - system call 내부에서 처음 닿는 stack buffer도 성공해야 한다.
4. `pt-grow-bad`
   - `rsp`보다 4096 bytes 아래 접근은 죽어야 한다.

### 꼭 같이 봐야 하는 회귀 테스트

`vm_try_handle_fault()`와 syscall pointer validation을 건드리기 때문에 아래 테스트는 같이 보는 편이 안전하다.

1. `pt-bad-addr`
   - 완전히 잘못된 주소 접근을 여전히 죽이는지 확인
2. `pt-bad-read`
   - 잘못된 buffer를 system call에 넘겼을 때 여전히 죽이는지 확인
3. `pt-write-code`
   - code page 쓰기 금지를 깨지 않았는지 확인
4. `pt-write-code2`
   - file path가 섞인 write-protection 경로가 여전히 맞는지 확인

### 조건부 회귀 테스트

아래 테스트는 stack growth와 관련은 깊지만, 다른 기능 의존성이 이미 준비돼 있을 때 의미가 있다.

1. `page-merge-stk`
   - stack growth + fork/SPT copy가 함께 안정적인지 보는 테스트
   - fork 관련 구현이 아직 덜 되어 있으면 이번 단계의 완료 판정에서는 제외해도 된다.

## mmap으로 넘어가기 위한 조건

`docs/reference/pintos-kaist-original/3_project3/4_memory_mapped_files.md` 기준으로 mmap은 "기존 mapped page와 겹치면 실패"해야 하고, 그 기존 mapped page에는 stack도 포함된다. 따라서 stack growth가 끝났다고 말하려면 아래 조건이 갖춰져 있어야 한다.

1. stack page를 다른 page와 구분할 수 있어야 한다.
   - `VM_MARKER_0` 유지
   - 또는 thread stack metadata로 범위를 바로 알 수 있어야 한다.
2. 현재 process의 stack range를 신뢰할 수 있어야 한다.
   - 그래야 `mmap-over-stk`에서 stack 위로 mapping을 막을 수 있다.
3. syscall 중 kernel fault에서도 stack growth가 동작해야 한다.
   - mmap 이후에도 user buffer를 다루는 syscall은 계속 나오기 때문이다.
4. invalid access와 valid stack growth를 확실히 구분해야 한다.
   - mmap 구현 때도 overlap/invalid fault 판정이 더 복잡해진다.
5. 1 MiB stack limit이 고정돼 있어야 한다.
   - 그래야 mmap address range 검사 시 stack의 가능한 범위를 과하게 넓게 잡지 않는다.

## mmap 착수 전 체크리스트

stack growth가 끝난 뒤, 아래 질문에 모두 "예"라고 답할 수 있으면 mmap으로 넘어가도 된다.

1. `vm_try_handle_fault()`가 SPT miss를 stack growth 후보로 다시 판단하는가?
2. `vm_stack_growth()`가 한 장이 아니라 필요한 범위까지 연속 page를 늘리는가?
3. `pt-grow-stk-sc`를 막는 syscall-side 선제 종료가 제거되었는가?
4. 현재 stack 범위를 코드에서 확인할 방법이 있는가?
5. `pt-grow-bad` 같은 invalid access는 여전히 죽는가?

## mmap 단계에서 바로 이어서 쓸 수 있는 산출물

이번 stack growth 단계에서 아래 두 가지를 잘 만들어 두면 mmap 구현 때 재사용 가치가 높다.

1. "주소가 이미 어떤 mapped range에 속하는지" 판별하는 기준
   - stack range 판별
   - SPT overlap 판별
2. page range를 page 단위로 순회하는 방식
   - stack growth는 downward range allocation
   - mmap은 forward range registration

즉, 이번 단계에서 stack 범위 추적과 page range 순회 규칙을 정리해 두는 것이 다음 단계의 실제 준비물이다.

## 최종 정리

이번 `stack growth`는 단순히 `vm_stack_growth()` 하나를 채우는 일이 아니다. 실제로는 아래 두 축이 같이 끝나야 한다.

1. fault를 stack growth로 인정할지 판별하는 축
2. 인정된 fault에 대해 page를 만들고 syscall 경로와 연결하는 축

2인 분업은 아래처럼 나누는 것이 가장 균형이 좋다.

- A: `thread.h`, `syscall_handler()`, `vm_try_handle_fault()` 중심으로 "판단 경로" 담당
- B: `setup_stack()`, `vm_stack_growth()`, `validate_user_*()` 중심으로 "실제 확장 경로" 담당

완료 판정은 `pt-grow-stack`, `pt-big-stk-obj`, `pt-grow-stk-sc`, `pt-grow-bad` 네 개를 중심으로 잡고, 회귀로 `pt-bad-*`, `pt-write-code*`를 함께 보는 구성이 적절하다.

이 조건이 맞으면 다음 단계인 mmap에서 가장 먼저 필요한 `stack overlap 금지`까지 자연스럽게 이어진다.
