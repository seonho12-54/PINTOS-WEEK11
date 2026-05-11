# Project3 Bootstrap Sprint 상세 계획

## 핵심 질문

> 1번이 SPT를 구현하지 않으면 뒤의 사람들이 아무것도 구현하지 못하는가?

완전히 그렇지는 않습니다. 다만 **SPT의 구현 완료**가 아니라 **SPT의 계약 확정**은 먼저 필요합니다.

`vm_alloc_page_with_initializer`, `vm_claim_page`, `vm_try_handle_fault`, `load_segment`, `setup_stack`은 모두 SPT를 직접 또는 간접적으로 사용합니다. 따라서 1번이 SPT 전체를 완성할 때까지 기다리는 방식으로 진행하면 나머지 3명이 막힙니다.

대신 Bootstrap Sprint는 다음 구조로 진행해야 합니다.

1. 먼저 4명이 SPT 계약을 같이 확정한다.
2. 1번은 SPT 내부 구현을 맡는다.
3. 나머지 3명은 SPT 함수가 동작한다는 가정 아래 자기 call site를 구현한다.
4. 중간 통합 시점에 SPT 구현과 call site를 붙여 compile/debug한다.

즉, **SPT가 완성되어 있어야만 다른 작업을 시작하는 구조가 아니라, SPT interface를 믿고 병렬 구현하는 구조**입니다.

## Bootstrap Sprint의 목표

Bootstrap Sprint는 테스트를 많이 통과시키는 단계가 아닙니다.

목표는 `lazy-anon`, `page-linear` 같은 테스트를 디버깅할 수 있는 최소 VM 실행 흐름을 만드는 것입니다.

필요한 최소 흐름은 다음과 같습니다.

```text
load_segment()
  -> vm_alloc_page_with_initializer()
  -> SPT에 uninit page 등록
  -> user program 실행
  -> page fault
  -> vm_try_handle_fault()
  -> spt_find_page()
  -> vm_do_claim_page()
  -> vm_get_frame()
  -> pml4_set_page()
  -> swap_in()
  -> uninit_initialize()
  -> lazy_load_segment()
```

이 흐름이 생겨야 이후부터 테스트 기반 스프린트가 의미 있어집니다.

## 사전 합의

코드를 나누기 전에 4명이 아래 항목을 먼저 합의합니다.

### SPT 계약

- SPT는 process별 자료구조다.
- key는 `pg_round_down(va)`로 정렬한 user virtual page address다.
- `spt_find_page(spt, va)`는 정렬되지 않은 주소를 받아도 page boundary 기준으로 찾는다.
- `spt_insert_page(spt, page)`는 같은 `page->va`가 이미 있으면 실패한다.
- `spt_remove_page(spt, page)`는 page를 SPT에서 제거하고 page resource 정리로 이어진다.

### Page 계약

- `page->va`는 항상 page-aligned user virtual address다.
- 처음 등록되는 executable/data page는 `VM_UNINIT`이다.
- 첫 fault에서 `VM_UNINIT`이 `VM_ANON` 또는 `VM_FILE`로 변한다.
- Bootstrap 단계에서는 executable lazy loading을 우선 다루고, mmap file-backed는 뒤 스프린트로 미룬다.

### Frame 계약

- user page frame은 `palloc_get_page(PAL_USER)` 기반으로 얻는다.
- Bootstrap 단계에서는 eviction을 구현하지 않는다.
- frame이 없으면 일단 명확하게 실패하거나 `PANIC("todo")` 계열로 남긴다.
- frame table은 swap 스프린트에서 다시 확장한다.

### Loader 계약

- VM 빌드의 `load_segment()`는 즉시 file을 읽어 frame에 넣지 않는다.
- 각 page마다 aux를 만들어 `vm_alloc_page_with_initializer()`에 넘긴다.
- `lazy_load_segment()`는 fault 시점에 file read와 zero fill을 한다.

## 4명 병렬 분업

### 1번: SPT 내부 구현 - 채강

담당 파일:

- `pintos/include/vm/vm.h`
- `pintos/vm/vm.c`

담당 함수/구조:

- `struct supplemental_page_table`
- `struct page`의 hash/list element
- `supplemental_page_table_init`
- `spt_find_page`
- `spt_insert_page`
- `spt_remove_page`의 최소 형태

직접 짜는 로직:

- page-aligned VA를 key로 삼는 lookup
- duplicate page 삽입 방지
- process별 SPT 초기화

다른 사람에게 제공해야 하는 계약:

- `spt_find_page()`는 정렬되지 않은 fault address를 받아도 동작한다.
- `spt_insert_page()`는 성공하면 true, 중복이면 false를 반환한다.

주의:

- 1번이 전체 팀의 blocker가 되지 않게, 가장 먼저 header 구조와 함수 contract를 공유한다.
- 내부 구현이 늦어져도 나머지는 prototype과 contract를 기준으로 call site를 작성할 수 있어야 한다.

### 2번: Page allocation / UNINIT / ANON 연결 - 선호

담당 파일:

- `pintos/vm/vm.c`
- `pintos/vm/uninit.c`
- `pintos/vm/anon.c`

담당 함수:

- `vm_alloc_page_with_initializer`
- `anon_initializer`
- `uninit_destroy`

직접 짜는 로직:

- requested type에 맞는 page initializer 선택
- `struct page` 할당
- `uninit_new()` 호출
- SPT 중복 검사 후 insert
- anonymous page initializer가 page operation을 제대로 세팅

SPT 의존성:

- `spt_find_page()`와 `spt_insert_page()`가 동작한다고 가정하고 구현한다.
- SPT 내부 자료구조에는 접근하지 않는다.

완료 기준:

- `load_segment()` 또는 `setup_stack()`에서 `vm_alloc_page_with_initializer()`를 호출하면 SPT에 page가 등록될 수 있다.

### 3번: Frame claim / pml4 mapping - 태선

담당 파일:

- `pintos/vm/vm.c`

담당 함수:

- `vm_get_frame`
- `vm_claim_page`
- `vm_do_claim_page`

직접 짜는 로직:

- `PAL_USER` frame 확보
- `struct frame` 할당 및 초기화
- `page->frame`, `frame->page` 연결
- `pml4_set_page()`로 user VA와 frame KVA mapping
- mapping 이후 `swap_in(page, frame->kva)` 호출

SPT 의존성:

- `vm_claim_page(va)`에서 `spt_find_page()`가 page를 찾아준다고 가정한다.
- SPT 내부 구현에는 접근하지 않는다.

완료 기준:

- SPT에 존재하는 page에 대해 `vm_claim_page()`가 frame을 확보하고 pml4 mapping을 만들 수 있다.

주의:

- Bootstrap 단계에서는 eviction을 구현하지 않는다.
- frame이 부족한 경우는 후속 swap 스프린트의 범위로 남긴다.

### 4번: Loader / Fault / Initial Stack - 진혁

담당 파일:

- `pintos/userprog/process.c`
- `pintos/vm/vm.c`

담당 함수:

- VM용 `load_segment`
- `lazy_load_segment`
- `setup_stack`
- `vm_try_handle_fault`의 최소 경로

직접 짜는 로직:

- executable segment를 page 단위로 나누어 `vm_alloc_page_with_initializer()` 호출
- aux에 file, offset, read bytes, zero bytes 등 lazy load 정보 보관
- fault 발생 시 SPT에서 page를 찾고 claim
- initial stack page를 VM 방식으로 등록하고 즉시 claim

SPT/frame 의존성:

- `vm_alloc_page_with_initializer()`가 page를 SPT에 등록한다고 가정한다.
- `vm_do_claim_page()`가 frame mapping을 해준다고 가정한다.

완료 기준:

- executable page가 load 시점에 즉시 frame을 먹지 않고 SPT에 등록된다.
- page fault가 발생하면 `vm_try_handle_fault()`가 SPT page를 찾아 claim한다.
- 첫 stack page가 VM 방식으로 만들어진다.

## 병렬 진행 단계

### 0단계: 계약 확정

산출물:

- SPT key와 자료구조 결정
- page/aux/frame 최소 metadata 결정
- 각자 수정 파일과 함수 확정

이 단계는 4명이 같이 합니다.

### 1단계: Header / contract 얇은 선반영

목표:

- 나머지 사람이 막히지 않도록 필요한 구조체 필드와 함수 contract를 먼저 맞춥니다.

작업:

- 1번이 `struct supplemental_page_table`, `struct page`의 필요한 element를 먼저 제안
- 4명이 보고 바로 합의
- 이후 각자 자기 함수 구현 시작

주의:

- 이 단계에서 완전한 SPT 구현이 필요하지는 않습니다.
- 중요한 것은 모두가 같은 구조와 API를 보고 코드를 쓰는 것입니다.

### 2단계: 병렬 구현

각자 자기 영역을 구현합니다.

- 1번: SPT 내부 구현
- 2번: page allocation / uninit / anon
- 3번: frame claim / pml4 mapping
- 4번: loader / fault / stack

규칙:

- 다른 사람 함수 내부를 고치지 않는다.
- 필요한 경우 함수 contract를 팀 채널에 먼저 공유한다.
- compile error가 나더라도 자기 영역의 의도를 먼저 완성한다.

### 3단계: 첫 통합

목표:

- build 가능한 상태 만들기
- 최소 user program 실행 흐름 확인
- `lazy-anon` 또는 `page-linear` 디버깅이 가능한 상태 만들기

점검 순서:

1. SPT init이 process 생성 시 호출되는지 확인
2. `load_segment()`가 page를 SPT에 넣는지 확인
3. page fault가 `vm_try_handle_fault()`로 들어오는지 확인
4. `spt_find_page()`가 fault address에 대응하는 page를 찾는지 확인
5. `vm_do_claim_page()`가 pml4 mapping을 만드는지 확인
6. `lazy_load_segment()`가 실제 내용을 채우는지 확인

### 4단계: 다음 스프린트 전환 조건

다음 조건을 만족하면 테스트 기반 스프린트로 넘어갑니다.

- VM build가 가능하다.
- initial stack setup이 VM 방식으로 된다.
- executable page가 SPT에 lazy page로 등록된다.
- page fault가 SPT lookup과 frame claim까지 이어진다.
- 최소 하나의 lazy/page 테스트를 사람이 직접 디버깅할 수 있다.

모든 테스트 통과가 전환 조건은 아닙니다.

## Bootstrap Sprint 완료 후 테스트 기대치

Bootstrap Sprint를 제대로 완료하면 통과 가능성이 있는 테스트는 있습니다. 다만 이 스프린트의 목표는 “테스트 대량 통과”가 아니라, **`lazy-anon`, `page-linear`를 의미 있게 디버깅할 수 있는 상태**를 만드는 것입니다.

Bootstrap 범위는 다음까지입니다.

- SPT 기본
- `VM_UNINIT -> VM_ANON` lazy loading
- frame claim
- pml4 mapping
- executable segment lazy loading
- initial stack setup
- 최소 `vm_try_handle_fault`

따라서 Bootstrap이 잘 끝나면 가장 먼저 노릴 수 있는 테스트는 다음입니다.

- `lazy-anon`
- `page-linear`
- `page-shuffle`

하지만 아직 아래 기능은 Bootstrap 범위 밖입니다.

- `supplemental_page_table_copy`
- `supplemental_page_table_kill`
- stack growth
- syscall user buffer lazy fault 대응
- mmap
- eviction / swap
- robust invalid fault 처리

따라서 Bootstrap만으로 통과를 기대하면 안 되는 테스트는 다음입니다.

- `pt-grow-*`: stack growth 필요
- `page-parallel`, `page-merge-*`: fork / SPT copy 필요
- `lazy-file`, `mmap-*`: mmap / file-backed page 필요
- `swap-*`: eviction / swap 필요
- `pt-bad-*`, `pt-write-code*`: fault validation과 writable bit 점검 필요

정리하면 Bootstrap은 **첫 VM 테스트 몇 개가 통과할 수도 있는 최소 골격**이고, 핵심 목표는 그 테스트들을 디버깅할 수 있는 실행 경로를 만드는 것입니다.

## Blocker 방지 원칙

1번의 SPT 구현이 늦어져도 나머지 3명이 완전히 멈추면 안 됩니다.

이를 위해 다음 원칙을 둡니다.

- SPT 내부 자료구조는 1번만 직접 만진다.
- 다른 사람은 `spt_find_page`, `spt_insert_page` API만 사용한다.
- header contract는 가장 먼저 합의한다.
- 각자 함수는 SPT가 동작한다는 가정으로 작성한다.
- 통합 단계에서 SPT 실제 동작과 call site를 같이 맞춘다.

## 결론

Bootstrap Sprint는 “1번이 끝나야 나머지가 시작하는” 구조가 아닙니다.

정확한 구조는 다음과 같습니다.

```text
4명이 SPT 계약을 먼저 합의
  -> 채강은 SPT 내부 구현
  -> 선호는 SPT API를 사용하는 page allocation 구현
  -> 태선은 SPT API를 사용하는 claim 구현
  -> 진혁은 SPT API를 사용하는 fault/loader 구현
  -> 통합 시점에 실제 구현을 붙여 디버깅
```

이렇게 하면 초반 함수 기반 분업의 효율을 유지하면서도, 4명 모두가 실제 VM 로직을 직접 작성할 수 있습니다.
