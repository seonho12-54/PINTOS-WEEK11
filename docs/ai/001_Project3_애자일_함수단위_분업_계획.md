# Project3 애자일 함수단위 분업 계획

맞아. 기준을 다시 잡으면 **기능 하나를 4명이 전부 만져보고 끝낸 뒤 다음 기능으로 이동**해야 합니다. 그래서 “A는 SPT 전담, B는 mmap 전담”이 아니라, 매 스프린트마다 함수/테스트 단위로 4조각을 나누고 역할을 회전하는 방식이 맞습니다.

근거는 `pintos-kaist-original/3_project3/1_vm_management.md`, `3_stack_growth.md`, `4_memory_mapped_files.md`, `5_swapping.md`, 그리고 `pintos/tests/vm/Rubric.*`입니다.

## 운영 방식

각 기능은 이렇게 진행하세요.

1. 30분: reference 해당 절 읽기
2. 20분: 수정 파일/함수/테스트 합의
3. 2~3시간: 4명이 각자 함수 단위 구현
4. 1시간: 통합 디버깅
5. 30분: 실패 원인/남은 리스크 정리

매 기능마다 4명이 모두 코드를 칩니다. 단, 같은 함수에 4명이 동시에 들어가지 않도록 **함수 단위 소유권**을 둡니다.

## 1. SPT 미니 스프린트

난이도: 중간. 구현량은 작지만 이후 모든 기능의 기반입니다.

목표 함수:

- `supplemental_page_table_init`
- `spt_find_page`
- `spt_insert_page`
- `spt_remove_page`
- `vm_alloc_page_with_initializer`의 SPT 삽입 부분

4명 분업:

- 1번: `include/vm/vm.h`의 SPT 자료구조 정의, page에 필요한 hash/list elem 추가
- 2번: `spt_find_page`, 주소 page-align 처리, 없는 page 반환 규칙
- 3번: `spt_insert_page`, 중복 VA 방지, insert 실패 처리
- 4번: `supplemental_page_table_init`, `spt_remove_page`, `vm_alloc_page_with_initializer` 연결

완료 기준:

- “SPT는 process별이고, key는 page-aligned VA다”를 코드로 보장
- 중복 page 삽입 실패
- 아직 page fault 전체가 안 돌아도 됨

## 2. Frame Claim 미니 스프린트

난이도: 중간. SPT 다음 첫 통합 지점입니다.

목표 함수:

- `vm_get_frame`
- `vm_claim_page`
- `vm_do_claim_page`
- `vm_try_handle_fault` 최소 경로
- `anon_initializer`, `uninit_initialize` 연결 확인

4명 분업:

- 1번: `vm_get_frame`, `PAL_USER` frame 확보, `struct frame` 초기화
- 2번: `vm_do_claim_page`, `pml4_set_page` 매핑, 실패 시 정리
- 3번: `vm_claim_page`, SPT 조회 후 claim 연결
- 4번: `vm_try_handle_fault`, not-present fault에서 SPT page claim하는 최소 경로

완료 기준:

- SPT에 등록된 page가 fault 시 frame을 받고 pml4에 매핑됨
- eviction은 아직 `PANIC` 또는 실패 처리로 둬도 됨
- 목표 테스트 후보: Project 2 회귀 일부, 이후 `lazy-anon` 준비

## 3. Lazy Loading / Executable 미니 스프린트

난이도: 높음. loader, uninit, anon이 처음으로 붙습니다.

목표 함수:

- `vm_alloc_page_with_initializer`
- `load_segment`
- `lazy_load_segment`
- `anon_initializer`
- `uninit_destroy`

4명 분업:

- 1번: `vm_alloc_page_with_initializer`에서 type별 initializer 선택
- 2번: `load_segment`에서 page별 aux 구성과 uninit page 등록
- 3번: `lazy_load_segment`에서 file read/zero fill 처리
- 4번: aux lifetime, `uninit_destroy`, 실패 경로 cleanup 점검

완료 기준:

- executable segment가 load 시 즉시 frame을 먹지 않음
- 첫 접근 때 실제 내용이 들어감
- 목표 테스트: `lazy-anon`, `page-linear` 일부

## 4. Stack Setup / Stack Growth 미니 스프린트

난이도: 중상. syscall user memory 검증과 충돌 가능성이 큽니다.

목표 함수:

- `setup_stack`
- `vm_stack_growth`
- `vm_try_handle_fault` stack growth 분기
- syscall 진입 시 user `rsp` 저장 또는 활용
- `validate_user_buffer/string`의 lazy page 대응

4명 분업:

- 1번: `setup_stack`, 첫 stack page를 VM 방식으로 등록/claim
- 2번: stack growth 조건, 1MB 제한, `rsp - 8` 허용 규칙
- 3번: `vm_stack_growth`, 필요한 anon page 생성/claim
- 4번: syscall 내부 fault 케이스, 특히 `pt-grow-stk-sc` 대응

완료 기준:

- `pt-grow-stack`, `pt-big-stk-obj`, `pt-grow-stk-sc`
- `pt-grow-bad`는 죽어야 함

## 5. SPT Copy / Kill / Cleanup 미니 스프린트

난이도: 높음. fork와 process exit 안정성입니다.

목표 함수:

- `supplemental_page_table_copy`
- `supplemental_page_table_kill`
- `anon_destroy`
- `uninit_destroy`
- `process_cleanup`과의 순서 확인

4명 분업:

- 1번: SPT 전체 순회 구조 작성
- 2번: process exit 시 page destroy/resource cleanup
- 3번: uninit page copy 규칙
- 4번: 이미 claim된 anon page copy 규칙, fork 실패 cleanup

완료 기준:

- exit 시 page/frame/file aux 누수 최소화
- fork 계열 기본 회귀 준비
- 목표 테스트: `page-parallel`, `page-merge-seq` 준비

## 6. mmap / File-backed 미니 스프린트

난이도: 높음, 구현량 큼. 2일 이상 잡는 게 현실적입니다.

4명 분업:

- 1번: syscall layer, `SYS_MMAP`, `SYS_MUNMAP`, fd validation
- 2번: `do_mmap`, page-align, length/offset, overlap check
- 3번: `file_backed_initializer`, `file_backed_swap_in`
- 4번: `do_munmap`, dirty write-back, `file_backed_destroy`

목표 테스트:

- 1차: `mmap-read`, `mmap-close`, `mmap-twice`
- 2차: `mmap-write`, `mmap-clean`, `mmap-ro`
- 3차: `mmap-null`, `mmap-misalign`, `mmap-overlap`, `mmap-kernel`

## 7. Eviction / Swap 미니 스프린트

난이도: 최고. 마지막 큰 산입니다.

4명 분업:

- 1번: frame table, global lock, frame list 관리
- 2번: victim selection, accessed bit 기반 clock 정책
- 3번: `anon_swap_out`, swap slot bitmap, disk write
- 4번: `anon_swap_in`, slot free, file-backed eviction/dirty 처리

목표 테스트:

- `swap-anon`
- `swap-file`
- `swap-iter`
- `swap-fork`
- `page-merge-par`, `page-merge-mm`, `page-merge-stk`

## 2주 현실 계획

1주차는 `SPT -> frame claim -> lazy loading -> stack growth -> cleanup`까지입니다.

2주차는 `mmap -> eviction -> swap -> robustness`입니다.

COW는 필수 구현이 안정화된 뒤 마지막 1~2일이 남을 때만 별도 branch로 봅니다.

## 오늘

오늘은 `1_vm_management.md`를 읽고 **SPT 미니 스프린트만 끝내는 것**을 목표로 잡으세요. 4명이 각각 위의 SPT 4조각을 실제로 구현하고, 마지막에 한 명씩 자기 함수가 다음 단계의 어떤 함수에서 호출되는지 설명하면 됩니다. 이 방식이면 속도는 조금 느려도, 네 명 모두가 핵심 VM 흐름을 직접 디버깅하게 됩니다.
