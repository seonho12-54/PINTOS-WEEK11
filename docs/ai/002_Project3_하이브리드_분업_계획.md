# Project3 하이브리드 분업 계획

하이브리드 방식이 지금 상황에 가장 맞습니다.

기준은 다음과 같습니다.

- **초반**: 테스트가 돌 수 있는 최소 VM 골격까지는 함수 기반
- **이후**: 테스트 케이스 오너제로 전환
- **항상**: 4명 모두 직접 로직을 작성하고 디버깅
- **금지**: “연결만 하는 사람”을 별도 역할로 두지 않기

## 전체 흐름

1. Bootstrap Sprint: 함수 기반으로 최소 VM 실행 흐름 만들기
2. Test Sprint 1: lazy / page fault 기본 테스트
3. Test Sprint 2: stack growth
4. Test Sprint 3: fork / SPT copy / cleanup
5. Test Sprint 4: mmap 기본
6. Test Sprint 5: mmap write-back / validation
7. Test Sprint 6: eviction / swap
8. Final Sprint: robustness / 회귀 / 정리

## Bootstrap Sprint: 함수 기반

목표는 테스트를 많이 통과시키는 것이 아니라, `lazy-anon`이나 `page-linear`를 디버깅할 수 있는 최소 흐름을 만드는 것입니다.

범위:

- SPT 기본
- page allocation
- frame claim
- minimal fault handling
- lazy executable loading
- initial stack setup

4명 분업:

- 1번: SPT 구조
  - `struct supplemental_page_table`
  - `struct page`에 hash/list elem 추가
  - `supplemental_page_table_init`
  - `spt_find_page`
  - `spt_insert_page`
  - 핵심 경험: page-aligned VA를 key로 process별 page table을 직접 구성

- 2번: page allocation / uninit / anon
  - `vm_alloc_page_with_initializer`
  - type별 initializer 선택
  - `uninit_new` 호출 흐름 연결
  - `anon_initializer`
  - `uninit_destroy` 최소 cleanup
  - 핵심 경험: page가 처음에는 `VM_UNINIT`이었다가 fault 시 `VM_ANON`으로 바뀌는 흐름 이해

- 3번: frame claim / pml4 mapping
  - `vm_get_frame`
  - `vm_claim_page`
  - `vm_do_claim_page`
  - `pml4_set_page` mapping
  - 핵심 경험: virtual page가 physical frame을 얻고 실제 page table에 매핑되는 흐름 이해

- 4번: loader / fault / initial stack
  - VM용 `load_segment`
  - `lazy_load_segment`
  - `setup_stack`
  - `vm_try_handle_fault` 최소 경로
  - 핵심 경험: executable segment가 load 시점이 아니라 page fault 시점에 올라오는 흐름 이해

완료 기준:

- user program이 VM 빌드에서 최소 실행 가능
- executable segment가 SPT에 등록됨
- page fault가 SPT page를 찾아 frame claim 가능
- stack 첫 page가 VM 방식으로 만들어짐

여기까지 끝나면 테스트 기반으로 전환합니다.

## Test Sprint 1: Lazy / Fault 기본

대상 테스트:

- `lazy-anon`
- `page-linear`
- `page-shuffle`
- `pt-bad-addr`, `pt-write-code`

4명 분업:

- 1번: `lazy-anon`
  - 접근 전 physical frame이 없어야 하고, 접근한 page만 올라와야 함.
- 2번: `page-linear`
  - 큰 static buffer read/write/read 흐름에서 내용이 유지되어야 함.
- 3번: `page-shuffle`
  - 여러 page를 반복 접근해도 data integrity가 깨지면 안 됨.
- 4번: `pt-bad-addr`, `pt-write-code`
  - invalid address와 read-only code write를 정확히 죽여야 함.

이 스프린트부터는 각자 자기 테스트를 읽고 필요한 로직을 직접 고칩니다. 겹치는 함수는 마지막에 통합합니다.

## Test Sprint 2: Stack Growth

대상 테스트:

- `pt-grow-stack`
- `pt-big-stk-obj`
- `pt-grow-stk-sc`
- `pt-grow-bad`

4명 분업:

- 1번: `pt-grow-stack`
  - 기본 stack growth.
- 2번: `pt-big-stk-obj`
  - 여러 page stack growth.
- 3번: `pt-grow-stk-sc`
  - syscall 안에서 stack growth가 필요한 경우.
- 4번: `pt-grow-bad`
  - 너무 아래 주소 접근은 stack growth로 인정하면 안 됨.

핵심 수정 영역:

- `vm_try_handle_fault`
- `vm_stack_growth`
- syscall user buffer validation
- thread에 user `rsp` 저장이 필요한지 검토

## Test Sprint 3: Fork / Cleanup

대상 테스트:

- `page-parallel`
- `page-merge-seq`
- `page-merge-par`
- `page-merge-stk`

4명 분업:

- 1번: `page-parallel`
  - 여러 child process의 address space 독립성.
- 2번: `page-merge-seq`
  - fork/exec/wait 반복과 큰 buffer 유지.
- 3번: `page-merge-par`
  - 여러 child가 동시에 VM을 쓰는 경우.
- 4번: `page-merge-stk`
  - child process와 stack-heavy workload.

핵심 수정 영역:

- `supplemental_page_table_copy`
- `supplemental_page_table_kill`
- `anon_destroy`
- fork 실패 cleanup

## Test Sprint 4: mmap 기본

대상 테스트:

- `lazy-file`
- `mmap-read`
- `mmap-close`
- `mmap-twice`

4명 분업:

- 1번: `lazy-file`
  - mmap page도 접근 전에는 frame이 없어야 함.
- 2번: `mmap-read`
  - file 내용 read + page 끝 zero-fill.
- 3번: `mmap-close`
  - fd close 이후에도 mapping은 살아 있어야 함.
- 4번: `mmap-twice`
  - 같은 file을 여러 주소에 mapping.

핵심 수정 영역:

- `SYS_MMAP`, `SYS_MUNMAP`
- `do_mmap`
- `file_backed_initializer`
- `file_backed_swap_in`
- `file_reopen`

## Test Sprint 5: mmap write-back / validation

대상 테스트:

- `mmap-write`
- `mmap-clean`
- `mmap-ro`, `mmap-off`
- `mmap-overlap`, `mmap-kernel`, `mmap-misalign`, `mmap-zero-len`, `mmap-bad-off`

4명 분업:

- 1번: `mmap-write`
  - mapping에 쓴 내용이 `munmap` 후 file에 반영.
- 2번: `mmap-clean`
  - dirty가 아니면 write-back하지 않음.
- 3번: `mmap-ro`, `mmap-off`
  - read-only protection, offset mapping.
- 4번: validation 묶음
  - 주소 정렬, kernel range, overlap, zero length, bad offset.

핵심 수정 영역:

- `do_munmap`
- `file_backed_swap_out`
- `file_backed_destroy`
- dirty bit 확인
- mmap range validation

## Test Sprint 6: Eviction / Swap

대상 테스트:

- `swap-anon`
- `swap-file`
- `swap-iter`
- `swap-fork`

4명 분업:

- 1번: `swap-anon`
  - anonymous page swap out/in.
- 2번: `swap-file`
  - file-backed page eviction.
- 3번: `swap-iter`
  - anon/file-backed가 섞인 반복 swap.
- 4번: `swap-fork`
  - swap과 fork/child stack interaction.

핵심 수정 영역:

- frame table
- victim selection
- `anon_swap_in`
- `anon_swap_out`
- swap bitmap
- file-backed eviction

## 권장 진행 순서

- Bootstrap Sprint
- Test Sprint 1
- Test Sprint 2
- Test Sprint 3
- 회귀, cleanup, mmap 준비
- Test Sprint 4
- Test Sprint 5
- Test Sprint 6
- robustness 전체
- 제출 정리, PR 리뷰, debug print 제거

## 결론

SPT, frame, lazy loading, initial stack의 최소 골격까지는 함수 기반으로 진행합니다. 그 뒤부터는 테스트 오너제로 전환합니다.

초반에 테스트 기반으로 바로 가면 네 명이 같은 빈 함수를 서로 다르게 고치게 되고, 처음부터 끝까지 함수 기반으로 가면 직접 디버깅 경험이 불균형해집니다. 하이브리드 방식이 둘 사이의 균형점입니다.
