# Stack Growth 작업 상세 계획

## 핵심 질문

> stack growth는 `vm_stack_growth()`만 채우면 끝나는가?

그렇게 보기는 어렵습니다. 실제로는 **stack growth 판별 경로**와 **실제 stack 확장 경로**가 함께 맞물려야 합니다.

`vm_try_handle_fault()`, `vm_stack_growth()`, `setup_stack()`, `syscall_handler()`, `validate_user_buffer()`가 모두 연결되기 때문에, 한 사람이 전부 끝낼 때까지 기다리는 방식으로 진행하면 2인 분업의 이점이 줄어듭니다.

따라서 이번 단계는 다음 구조로 가는 편이 좋습니다.

1. 먼저 2명이 stack growth 계약을 같이 정한다.
2. 한 사람은 fault 판별과 user rsp 보존 경로를 맡는다.
3. 다른 한 사람은 실제 stack page 확장과 syscall 검증 조정을 맡는다.
4. 중간 통합 시점에 heuristic, metadata, fault 경로를 붙여 확인한다.

즉, **한 함수 구현이 끝나야 다음 사람이 시작하는 구조가 아니라, 공통 계약을 먼저 맞추고 병렬 구현하는 구조**입니다.

## Stack Growth 단계의 목표

이번 단계는 세부 기능을 많이 붙이는 단계가 아닙니다.

목표는 `pt-grow-stack`, `pt-big-stk-obj`, `pt-grow-stk-sc`, `pt-grow-bad`를 기준으로, stack growth가 실제로 동작하는 최소 실행 흐름을 만드는 것입니다.

필요한 최소 흐름은 다음과 같습니다.

```text
user access / syscall buffer access
  -> page fault
  -> vm_try_handle_fault()
  -> stack growth 후보 판별
  -> vm_stack_growth()
  -> 새 stack page 등록
  -> vm_claim_page()
  -> pml4 mapping
  -> user code 또는 syscall 경로 복귀
```

이 흐름이 생겨야 이후 `mmap` 단계에서도 stack overlap과 fault 처리를 자연스럽게 이어갈 수 있습니다.

## 사전 합의

코드를 나누기 전에 2명이 아래 항목을 먼저 합의합니다.

### Stack Growth 계약

- stack growth는 아무 fault에나 적용하지 않는다.
- fault address가 현재 rsp 근처의 자연스러운 stack 접근처럼 보일 때만 허용한다.
- x86-64 기준으로 `rsp`보다 최대 8 bytes 아래 fault 가능성을 반영한다.
- stack 최대 크기는 1 MiB로 제한한다.

### RSP 계약

- user mode fault면 `intr_frame->rsp`를 기준으로 본다.
- kernel mode fault면 저장해 둔 user rsp를 기준으로 본다.
- syscall 진입 시점에 user rsp를 저장해 두는 경로가 필요하다.

### Stack Metadata 계약

- 첫 stack page는 기존처럼 `VM_ANON | VM_MARKER_0` 기반으로 시작한다.
- 현재 stack 하한을 별도 field로 둘지, marker 기반으로 추적할지 먼저 정한다.
- `mmap` 단계까지 생각하면 현재 stack 하한을 바로 확인할 수 있는 방식이 더 단순하다.

### Syscall 검증 계약

- unmapped user page를 무조건 선제 종료하면 `pt-grow-stk-sc`를 막을 수 있다.
- 따라서 명백한 invalid address만 즉시 거절하고, stack growth 후보는 fault 경로로 넘기는 방향을 기준으로 잡는다.

## 2명 병렬 분업

### A: Fault 판별 / user rsp 보존 - 판단 경로 담당

담당 파일:

- `pintos/include/threads/thread.h`
- `pintos/userprog/syscall.c`
- `pintos/vm/vm.c`

담당 함수:

- `struct thread`
- `syscall_handler(struct intr_frame *f)`
- `vm_try_handle_fault(struct intr_frame *f, void *addr, bool user, bool write, bool not_present)`

직접 정리할 로직:

- 저장할 user rsp field 추가
- syscall 진입 시 user rsp 보존
- fault가 invalid access인지, 기존 SPT page인지, stack growth 후보인지 판별
- user fault와 kernel fault에서 각각 어떤 rsp를 기준으로 볼지 정리

다른 사람에게 제공해야 하는 계약:

- stack growth 후보 주소를 어떤 기준으로 인정하는지
- `vm_try_handle_fault()`가 SPT miss에서 바로 끝나지 않고 stack growth까지 검사한다는 점

### B: Stack 확장 / syscall 검증 조정 - 실행 경로 담당

담당 파일:

- `pintos/include/threads/thread.h`
- `pintos/userprog/process.c`
- `pintos/vm/vm.c`
- `pintos/userprog/syscall.c`

담당 함수:

- `setup_stack(struct intr_frame *if_)`
- `vm_stack_growth(void *addr)`
- `validate_user_ptr(const void *uaddr)`
- `validate_user_buffer(const void *buffer, size_t size)`

직접 정리할 로직:

- 첫 stack page 생성 시 stack metadata 초기화
- 필요한 범위까지 stack page를 연속으로 늘리는 경로 구성
- 1 MiB limit 방어
- syscall pointer validation이 stack growth를 막지 않도록 조정

다른 사람에게 제공해야 하는 계약:

- 현재 stack 하한 metadata의 의미
- `vm_stack_growth()`가 어떤 주소를 받아 어디까지 page를 늘리는지

## 병렬 진행 단계

### 0단계: 계약 확정

산출물:

- saved user rsp 저장 방식 결정
- stack 하한 추적 방식 결정
- stack growth heuristic 결정
- syscall 검증 정책 결정

이 단계는 2명이 같이 합니다.

### 1단계: Header / contract 얇은 선반영

목표:

- 서로 막히지 않도록 `struct thread` field와 함수 역할을 먼저 맞춥니다.

작업:

- A와 B가 `thread.h`에 들어갈 metadata 의미를 먼저 합의
- `vm_try_handle_fault()`와 `vm_stack_growth()`의 입출력 기대를 먼저 정리
- 이후 각자 자기 함수 구현 시작

주의:

- 이 단계에서 완성된 stack growth 구현이 필요한 것은 아닙니다.
- 중요한 것은 같은 heuristic과 metadata를 보고 둘 다 구현을 진행하는 것입니다.

### 2단계: 병렬 구현

각자 자기 영역을 구현합니다.

- A: user rsp 저장, fault 판별
- B: stack 확장, syscall 검증 조정

규칙:

- 상대가 맡은 함수 내부를 임의로 바꾸지 않는다.
- heuristic이나 metadata 의미가 바뀌면 먼저 공유한다.
- compile error보다 계약 일치를 우선한다.

### 3단계: 첫 통합

목표:

- stack growth가 실제 fault 경로에서 동작하는지 확인
- syscall 내부 접근까지 같은 흐름으로 이어지는지 확인

점검 순서:

1. 첫 stack page와 stack metadata가 함께 초기화되는지 확인
2. syscall 진입 시 user rsp가 저장되는지 확인
3. `vm_try_handle_fault()`가 stack growth 후보를 판별하는지 확인
4. `vm_stack_growth()`가 필요한 page를 실제로 등록하는지 확인
5. 새 page가 claim되고 user execution으로 복귀하는지 확인
6. syscall buffer 접근도 같은 경로로 살아나는지 확인

### 4단계: 다음 스프린트 전환 조건

다음 조건을 만족하면 `mmap` 단계로 넘어갑니다.

- stack growth가 user fault와 syscall 중 kernel fault 둘 다에서 동작한다.
- 필요한 만큼 stack page를 추가로 만들 수 있다.
- `pt-grow-stk-sc`를 막는 선제 종료 경로가 제거되어 있다.
- 현재 stack 범위를 코드에서 판단할 수 있다.
- invalid access와 valid stack growth가 구분된다.

모든 VM 테스트 통과가 전환 조건은 아닙니다.

## Stack Growth 완료 후 테스트 기대치

이번 단계가 잘 끝나면 가장 먼저 기준으로 삼을 테스트는 다음입니다.

- `pt-grow-stack`
- `pt-big-stk-obj`
- `pt-grow-stk-sc`
- `pt-grow-bad`

같이 봐야 하는 회귀 테스트는 다음입니다.

- `pt-bad-addr`
- `pt-bad-read`
- `pt-write-code`
- `pt-write-code2`

다만 아직 아래 기능은 이번 단계의 직접 범위 밖입니다.

- fork / `supplemental_page_table_copy`
- `mmap`
- eviction / swap

따라서 `page-merge-stk`, `mmap-*`, `swap-*` 계열은 다음 단계 의존성이 남아 있을 수 있습니다.

## Blocker 방지 원칙

한 사람이 `vm_try_handle_fault()`를 오래 붙잡거나, 다른 사람이 `vm_stack_growth()`를 오래 붙잡아도 상대가 완전히 멈추면 안 됩니다.

이를 위해 다음 원칙을 둡니다.

- fault 판별 기준은 가장 먼저 합의한다.
- stack metadata 의미는 가장 먼저 합의한다.
- A는 판단 경로만, B는 실행 경로만 우선 책임진다.
- 통합 단계에서 heuristic과 실제 page 생성 경로를 같이 맞춘다.

## 결론

이번 stack growth 단계는 “`vm_stack_growth()` 하나 구현”으로 끝나는 작업이 아닙니다.

정확한 구조는 다음과 같습니다.

```text
2명이 stack growth 계약을 먼저 합의
  -> A는 user rsp 저장과 fault 판별 구현
  -> B는 stack 확장과 syscall 검증 조정 구현
  -> 통합 시점에 heuristic, metadata, fault 경로를 붙여 점검
```

이렇게 하면 문서가 지나치게 세부 구현으로 흩어지지 않으면서도, 실제 작업 순서와 2인 분업 기준은 분명하게 유지할 수 있습니다.
