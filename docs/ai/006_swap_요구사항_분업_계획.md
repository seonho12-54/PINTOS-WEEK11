# swap 요구사항 구현 2페어 분업 계획

## 문서 목적

이 문서는 Project 3 `Swap In/Out` 구현을 4명이 2명씩 페어 프로그래밍으로 진행하기 위한 분업 계획이다.
구현 코드를 대신 작성하지 않고, `docs/reference`와 현재 로컬 코드, 로컬 테스트 파일에서 확인되는 요구사항을 기준으로 무엇을 해야 하는지만 정리한다.

목표 테스트는 우선 다음 세 개로 한정한다.

- `pintos/tests/vm/swap-anon.c`
- `pintos/tests/vm/swap-file.c`
- `pintos/tests/vm/swap-iter.c`

`swap-fork`는 같은 영역의 확장 테스트지만, 이번 요청의 직접 목표에는 포함하지 않는다. 다만 SPT copy나 process cleanup을 건드리면 나중에 영향을 줄 수 있으므로 통합 리뷰 때 위험 항목으로만 본다.

## 기준 자료와 우선순위

- `AGENTS.md`
  - Pintos 구현 판단은 `docs/reference`의 KAIST Pintos 문서를 기준으로 한다.
  - 구현 코드를 통째로 생성하지 않고, 요구사항 정리와 검토를 보조한다.
  - AI 산출물은 `docs/ai` 아래 다음 번호 문서로 저장한다.
- `docs/reference/pintos-kaist-original/3_project3/5_swapping.md`
  - swap in/out의 최우선 요구사항 문서다.
- `docs/reference/pintos-kaist-kr/3_project3/5_swapping.md`
  - 위 문서의 한국어 보조 자료다.
- `docs/reference/pintos-kaist-original/3_project3/0_introduction.md`
  - frame table, swap table, eviction, accessed/dirty bit, page fault 흐름의 기준이다.
- `docs/reference/pintos-kaist-kr/3_project3/0_introduction.md`
  - 위 문서의 한국어 보조 자료다.
- `docs/reference/pintos-kaist-original/3_project3/1_vm_management.md`
  - page operation 함수 포인터와 page type별 lifecycle의 기준이다.
- `docs/reference/pintos-kaist-original/3_project3/4_memory_mapped_files.md`
  - file-backed page가 swapped out될 때 변경 내용을 파일에 반영해야 한다는 mmap 쪽 기준이다.
- `pintos/tests/vm/swap-anon.c`, `pintos/tests/vm/swap-file.c`, `pintos/tests/vm/swap-iter.c`
  - 이번 분업이 직접 목표로 삼는 테스트의 실제 입력 조건과 기대 동작이다.
- `pintos/tests/vm/Make.tests`
  - 각 swap 테스트의 제한 메모리와 swap disk 크기 설정의 기준이다.

## reference 기준 요구사항

### swapping 기본 의미

- 물리 frame이 부족해졌을 때 현재 사용 중인 page 중 하나를 evict해서 다른 page가 사용할 frame을 확보해야 한다.
- evicted page의 내용은 page type에 따라 swap disk 또는 backing file에 보존되어야 한다.
- 나중에 해당 page에 다시 접근하면 page fault 경로에서 정확한 내용을 frame으로 복구해야 한다.
- swap operation은 직접 호출하는 일반 함수가 아니라 `struct page_operations`의 `swap_in`, `swap_out` function pointer를 통해 page type별로 호출된다.

### frame table 요구사항

- frame table은 각 user frame이 어떤 page를 담고 있는지 추적해야 한다.
- user page용 frame은 user pool에서 얻어야 한다.
- free frame이 없으면 page replacement policy로 victim frame을 고르고 eviction해야 한다.
- eviction 과정에서는 victim page와 frame의 연결, page table mapping, page type별 backing store 반영이 서로 일관되어야 한다.
- victim 선택에는 accessed bit와 dirty bit를 활용할 수 있다.

### swap table 요구사항

- swap table은 swap disk 안의 free slot과 used slot을 추적해야 한다.
- swap slot은 PGSIZE 단위로 관리된다.
- swap slot은 eviction으로 실제 필요해졌을 때만 배정한다.
- swapped-out page가 다시 frame으로 읽혀 오면 해당 swap slot은 해제되어야 한다.
- swap slot이 더 이상 없으면 reference 기준으로 kernel panic을 허용한다.

### anonymous page 요구사항

`docs/reference/pintos-kaist-original/3_project3/5_swapping.md`와 한국어 문서가 직접 언급한 anonymous page 함수는 다음과 같다.

| 함수 | reference 기준 역할 |
| --- | --- |
| `vm_anon_init` | swap disk를 설정하고 swap disk의 free/used 영역을 관리할 구조를 준비한다. |
| `anon_initializer` | anonymous page가 swapping에 필요한 정보를 보관할 수 있게 초기 상태를 잡는다. |
| `anon_swap_out` | frame의 anonymous page 내용을 swap disk의 빈 slot으로 내보내고, page가 나중에 찾을 수 있는 위치 정보를 남긴다. |
| `anon_swap_in` | page에 저장된 위치 정보를 기준으로 swap disk에서 내용을 다시 읽고, swap table 상태를 갱신한다. |

### file-backed page 요구사항

`docs/reference/pintos-kaist-original/3_project3/5_swapping.md`와 `4_memory_mapped_files.md`가 직접 연결되는 file-backed page 요구사항은 다음과 같다.

| 함수 | reference 기준 역할 |
| --- | --- |
| `file_backed_swap_in` | backing file에서 내용을 읽어 `kva` 위치의 frame에 채운다. 파일 시스템 접근은 동기화되어야 한다. |
| `file_backed_swap_out` | dirty page라면 mapping된 파일에 변경 내용을 되쓴다. clean page라면 파일을 수정하지 않아도 된다. swap out 뒤 dirty bit 정리가 필요하다. |

file-backed page는 anonymous page처럼 swap disk를 backing store로 쓰지 않는다. mmap된 파일 자체가 backing store다.

## 현재 코드에서 확인한 상태

현재 코드베이스 기준으로 이미 갖춰진 부분은 이번 swap 신규 구현 범위에서 제외하고, 통합 검증 대상으로만 둔다.

### 이미 갖춰진 것으로 보고 제외할 부분

- `pintos/userprog/syscall.c`
  - `SYS_MMAP`, `SYS_MUNMAP` dispatcher 분기가 있다.
  - `sys_mmap`, `sys_munmap` 경로가 `do_mmap`, `do_munmap`과 연결되어 있다.
- `pintos/vm/file.c`
  - `file_backed_initializer`가 존재한다.
  - `file_backed_swap_in` 구현이 있다.
  - `file_backed_swap_out` 구현이 있다.
  - `file_backed_destroy`, `do_mmap`, `do_munmap`, `file_lazy_load` 구현이 있다.
- `pintos/include/vm/file.h`
  - file-backed page metadata가 이미 정의되어 있다.
- `pintos/vm/vm.c`
  - SPT hash 기반 조회/삽입/삭제 흐름이 있다.
  - page fault에서 `swap_in` function pointer로 이어지는 claim 흐름이 있다.
- `pintos/userprog/process.c`
  - process cleanup에서 SPT kill을 호출하는 경로가 있다.

위 항목들은 "아무 작업도 안 해도 된다"는 뜻이 아니다. 새로 만드는 주 작업에서는 제외하되, eviction이 실제로 붙었을 때 `swap-file`, `swap-iter`에서 기존 file-backed 경로가 정상 호출되는지는 반드시 통합 리뷰해야 한다.

### 아직 남아 있는 핵심 작업

- `pintos/vm/anon.c`
  - `vm_anon_init`은 TODO 상태이고 `swap_disk`가 `NULL`로 남아 있다.
  - `struct anon_page`가 비어 있어 swapped-out 위치 같은 상태를 보관할 자리가 없다.
  - `anon_swap_in`, `anon_swap_out`이 실질 동작과 반환값 없이 비어 있다.
  - `anon_destroy`는 swapped-out page cleanup 관점에서 아직 볼 부분이 있다.
- `pintos/vm/vm.c`
  - `vm_get_victim`은 victim 선택을 하지 않는다.
  - `vm_evict_frame`은 eviction 흐름을 만들지 않는다.
  - `vm_get_frame`은 user pool frame이 부족하면 eviction으로 넘어가지 않고 panic한다.
  - 전역 frame table 또는 동등한 frame 추적 구조가 보이지 않는다.
  - frame table 동시 접근을 보호할 기준이 아직 없다.
- 공통 설계
  - swap table의 범위, 동기화, slot lifecycle이 아직 정해져 있지 않다.
  - eviction 후 page, frame, page table mapping 상태를 어떤 불변식으로 유지할지 합의가 필요하다.

## 한눈에 보는 작업 경계

| 구분 | 페어 A: frame eviction 담당 | 페어 B: page backing store 담당 |
| --- | --- | --- |
| 핵심 질문 | frame이 부족할 때 누구를 내보내고 빈 frame을 어떻게 확보할 것인가? | evicted page의 내용을 어디에 보관하고 어떻게 되살릴 것인가? |
| 주 작업 파일 | `pintos/vm/vm.c`, `pintos/include/vm/vm.h` | `pintos/vm/anon.c`, `pintos/include/vm/anon.h`, `pintos/vm/file.c`, `pintos/include/vm/file.h` |
| 직접 담당 | frame table, victim 선정, eviction 진입/완료 흐름, frame lifecycle | swap disk/table, anonymous swap in/out, file-backed swap 경로 통합 검증 |
| 직접 담당하지 않음 | swap disk slot 내부 관리, anonymous page별 저장 위치 | victim 선정 정책, frame table 순회 정책 |
| 상대 페어에 넘겨야 하는 것 | page type별 `swap_out`이 호출될 수 있는 eviction 시점과 page/frame 상태 | `swap_in`/`swap_out` 성공 실패 의미와 page가 복구 가능한 상태인지 여부 |
| 먼저 볼 테스트 | `swap-anon`, `swap-iter`의 메모리 압박 구조 | `swap-anon`, `swap-file`, `swap-iter`의 데이터 복구 구조 |

## 페어 구성

### 페어 A: frame table과 eviction 경로 담당

담당 범위:

- user frame들을 추적할 frame table 또는 동등한 구조 마련
- frame 부족 상황에서 panic으로 끝나지 않고 eviction으로 이어지는 공통 경로 마련
- victim frame 선정 기준 정리
- victim page의 page table mapping과 frame 연결 상태 정리
- page type별 `swap_out` 호출 계약 정리
- eviction된 frame을 새 page claim에 재사용하는 흐름 정리
- accessed/dirty bit를 replacement와 write-back 판단에서 어떻게 볼지 페어 B와 합의
- frame table 접근 동기화 기준 정리

주요 소유 파일:

- `pintos/vm/vm.c`
- `pintos/include/vm/vm.h`
- 필요 시 `pintos/include/threads/thread.h`와 thread/process cleanup 경계

페어 A가 완료했다고 말하려면:

- free frame이 있을 때와 없을 때의 frame 확보 경로를 설명할 수 있다.
- frame table에서 어떤 frame이 어떤 page에 연결되어 있는지 추적할 수 있다.
- eviction 대상이 정해졌을 때 page type별 `swap_out`으로 넘기는 계약이 명확하다.
- eviction 후 old page와 reused frame의 상태가 서로 섞이지 않는다.
- `swap-anon`처럼 user memory 제한이 작은 테스트에서 frame 부족이 곧바로 panic으로 끝나지 않는다.
- 페어 B의 anonymous/file-backed swap 구현을 갈아 끼워도 frame 쪽 전제가 깨지지 않는다.

페어 A가 직접 구현하지 않는 영역:

- swap disk에서 free slot을 찾고 해제하는 세부 관리
- anonymous page가 swap disk의 어느 위치에 저장되었는지 보관하는 방식
- file-backed page가 파일에서 몇 byte를 읽고 쓸지 판단하는 방식
- mmap syscall 검증 조건

### 페어 B: anonymous swap과 file-backed swap 통합 담당

담당 범위:

- swap disk 초기화와 swap table 준비
- anonymous page가 swap 상태를 보관할 수 있는 metadata 정리
- anonymous page swap out/in lifecycle 정리
- swapped-in anonymous page의 slot 해제 기준 정리
- swapped-out anonymous page가 process 종료로 사라질 때 swap slot이 누수되지 않도록 정리
- 기존 file-backed swap in/out 구현이 eviction 경로에서 호출될 때 요구사항과 맞는지 검토
- file-backed page의 dirty write-back, clean page 처리, dirty bit 정리 기준을 페어 A와 합의
- `swap-file`과 `swap-iter`에서 mmap 기반 page가 frame 압박 상황에서도 복구되는지 통합 관점으로 확인

주요 소유 파일:

- `pintos/vm/anon.c`
- `pintos/include/vm/anon.h`
- `pintos/vm/file.c`
- `pintos/include/vm/file.h`
- 필요 시 `pintos/vm/uninit.c`의 destroy 경계

페어 B가 완료했다고 말하려면:

- anonymous page가 swap out된 뒤 다시 fault가 나면 원래 내용을 복구할 수 있다.
- anonymous page가 swap in되면 더 이상 필요 없는 swap slot이 해제된다.
- process 종료 시 swapped-out anonymous page가 남긴 swap slot이 누수되지 않는다.
- file-backed page는 swap disk가 아니라 backing file을 기준으로 복구/반영된다는 점이 유지된다.
- clean file-backed page와 dirty file-backed page의 처리 차이를 설명할 수 있다.
- 페어 A의 eviction 경로에서 호출되어도 page type별 operation 계약이 맞는다.

페어 B가 직접 구현하지 않는 영역:

- frame table의 자료구조 선택과 순회 정책
- victim 선정 알고리즘
- frame 부족 시 frame 확보 경로의 전체 제어 흐름
- mmap syscall의 fd, 주소, offset 검증

## 페어 간 합의가 필요한 공유 설계

이 섹션은 구현 순서를 강제하지 않는다. 두 페어가 서로 다른 전제를 갖고 작업하면 통합 때 깨질 수 있는 interface만 정리한다.

### 1. eviction 시점의 page/frame 상태

페어 A는 victim을 고른 뒤 page type별 `swap_out`을 호출한다. 페어 B는 그 시점에 page와 frame이 어떤 상태인지 알아야 한다.

합의할 질문은 다음과 같다.

- `swap_out` 호출 시 page는 아직 frame을 가리키고 있는가?
- `swap_out` 성공 후 frame과 page의 연결은 어느 쪽에서 끊는가?
- page table mapping 정리는 frame 공통 경로에서 할 것인가, page type별 operation에서 할 것인가?
- 실패 시 frame을 재사용하지 않고 복구 가능한 상태로 남길 수 있는가?

### 2. swap slot lifecycle

anonymous page의 swap slot은 global 자원이다. 그래서 slot을 언제 잡고 언제 풀지 합의가 필요하다.

합의할 질문은 다음과 같다.

- slot은 anonymous page가 실제로 evict될 때만 잡는가?
- swap in 성공 후 slot은 즉시 free로 돌아가는가?
- swap out된 채 process가 종료되면 slot은 어느 cleanup 경로에서 해제되는가?
- swap disk가 가득 찼을 때 panic을 어느 계층에서 발생시키는가?

### 3. dirty/accessed bit 기준

reference는 accessed bit와 dirty bit가 eviction policy와 file-backed write-back에 도움이 된다고 설명한다.

합의할 질문은 다음과 같다.

- victim 선정에서 accessed bit를 어떻게 해석할 것인가?
- file-backed page가 dirty인지 판단할 때 어떤 page table entry 기준을 볼 것인가?
- dirty bit 정리는 file-backed operation이 책임지는가, frame 공통 eviction 경로가 책임지는가?
- kernel alias 때문에 dirty/accessed 관찰이 누락될 가능성을 어떻게 리뷰할 것인가?

### 4. uninit page와 initialized page의 차이

swap 테스트는 lazy loading과 swapping이 같이 섞인다. 처음 접근하지 않은 page는 아직 `VM_UNINIT`일 수 있고, 한 번 fault를 거친 page는 `VM_ANON` 또는 `VM_FILE` 상태일 수 있다.

합의할 질문은 다음과 같다.

- frame table에는 실제 frame을 가진 page만 들어가는가?
- 아직 frame이 없는 uninit page는 eviction 후보가 아닌 것으로 볼 수 있는가?
- file-backed lazy page가 fault-in된 뒤 eviction되면 기존 metadata가 유지되는가?
- executable lazy page가 초기화된 뒤 anonymous page로 취급되는 현재 구조와 swap 요구사항이 충돌하지 않는가?

### 5. process cleanup과 resource leak

테스트가 직접 process 종료 후 swap slot 누수를 검사하지 않더라도, swap table은 global 자원이므로 누수되면 이후 테스트나 fork 계열에서 문제가 된다.

합의할 질문은 다음과 같다.

- SPT kill에서 initialized anonymous page와 swapped-out anonymous page가 모두 정리되는가?
- frame을 가진 page와 frame 없이 backing store만 가진 page의 destroy 경로가 구분되는가?
- file-backed page의 file reference 정리와 anonymous swap slot 정리가 서로 섞이지 않는가?

## 페어별 독립 작업 범위와 검증 루트

### 공통 베이스라인

- 현재 브랜치에서 mmap syscall과 file-backed mmap 기본 구현은 이미 들어간 것으로 간주한다.
- `pintos/vm/file.c`의 기존 구현은 이번 작업에서 새로 갈아엎지 않고, eviction과 붙는 부분만 검토한다.
- `pintos/vm/anon.c`와 `pintos/vm/vm.c`의 TODO는 이번 swap 목표 테스트의 핵심 blocker로 본다.
- 테스트 실행은 팀원이 `TESTING.md` 기준으로 직접 수행하고, AI 문서는 요구사항과 작업 경계를 정리하는 용도로 사용한다.
- 테스트용 임시 구현을 만들 경우 최종 통합 전에 반드시 제거하거나 실제 소유 페어 구현으로 교체한다.

### 페어 A 파트: frame pressure와 eviction

페어 A가 직접 책임지는 것:

- frame table의 존재와 lifecycle
- frame 확보 실패 시 eviction으로 이어지는 경로
- victim frame 선정
- eviction 대상 page의 공통 상태 정리
- page type별 `swap_out` 호출 계약
- eviction된 frame 재사용 경로
- frame table lock 또는 동등한 동시성 기준

페어 A가 자기 로직 검증을 위해 임시로 채워도 되는 것:

- page type별 `swap_out` 성공/실패를 단순 확인하기 위한 stub
- swap disk 내용을 실제로 보존하지 않는, frame 경로 검증용 임시 operation
- file-backed 세부 write-back을 생략한 frame pressure 검증 scaffolding

단, 위 임시 구현은 frame table과 eviction 흐름 검증용일 뿐 최종 구현으로 취급하지 않는다.

페어 A의 우선 목표 테스트 관찰 포인트:

- `swap-anon`
  - 10MB memory 제한에서 20MB anonymous 영역 접근이 frame 부족으로 panic나지 않아야 한다.
- `swap-iter`
  - anonymous page 접근 뒤 file-backed mmap 접근이 섞여도 frame table 상태가 깨지지 않아야 한다.
- `swap-file`
  - file-backed page fault가 frame 부족 상황에서도 eviction 경로와 연결될 수 있어야 한다.

### 페어 B 파트: anonymous backing store와 file-backed operation 검증

페어 B가 직접 책임지는 것:

- swap disk setup
- swap table의 used/free 상태 관리
- anonymous page별 swap 상태 metadata
- anonymous page swap out
- anonymous page swap in
- anonymous page destroy에서 swap resource 정리
- 기존 file-backed swap in/out 구현의 reference 요구사항 대조
- file-backed dirty write-back과 clean page 처리 리뷰
- `swap-file`, `swap-iter`에서 file-backed page가 backing file 기준으로 복구되는지 확인

페어 B가 자기 로직 검증을 위해 임시로 채워도 되는 것:

- 단순 victim을 하나만 고르는 임시 eviction 경로
- frame table 없이 제한된 상황에서 anonymous swap operation을 호출해 보는 검증용 scaffolding
- mmap syscall 검증을 최소화한 file-backed swap 검증용 임시 경로

단, 위 임시 구현은 backing store operation 검증용일 뿐 최종 구현으로 취급하지 않는다.

페어 B의 우선 목표 테스트 관찰 포인트:

- `swap-anon`
  - sparse write한 anonymous page 값이 eviction 이후에도 유지되어야 한다.
- `swap-file`
  - mmap된 file-backed page 내용과 file tail zero 영역이 frame 압박 상황에서도 유지되어야 한다.
- `swap-iter`
  - anonymous page와 file-backed page가 번갈아 pressure를 만들 때 둘 다 원래 내용을 유지해야 한다.

## 테스트 매트릭스

| 테스트 | `Make.tests` 조건 | 확인하는 요구사항 | 주 담당 |
| --- | --- | --- | --- |
| `swap-anon` | `MEMORY = 10`, `SWAP_DISK = 30`, `TIMEOUT = 180` | 20MB anonymous 영역의 sparse write/read가 frame 부족과 swap in/out 이후에도 유지된다. | 페어 A + 페어 B |
| `swap-file` | `MEMORY = 8`, `SWAP_DISK = 10`, `TIMEOUT = 180` | mmap file-backed page가 제한 메모리에서 fault-in되고, 파일 내용과 zero tail이 유지된다. | 페어 B 중심, 페어 A 지원 |
| `swap-iter` | `MEMORY = 10`, `SWAP_DISK = 50`, `TIMEOUT = 180` | anonymous page와 file-backed page가 함께 존재할 때 eviction, swap in/out, mmap read가 서로 깨지지 않는다. | 페어 A + 페어 B |

## 권장 브랜치와 PR 단위

한 브랜치에서 네 명이 동시에 `vm.c`, `anon.c`, `file.c`를 수정하면 충돌이 커질 수 있다. 다음 단위를 권장한다.

- 페어 A 브랜치: `feat-#<issue>-swap-frame-eviction`
- 페어 B 브랜치: `feat-#<issue>-swap-page-backing`
- 통합 브랜치: 두 페어의 실제 구현만 모아 `swap-anon`, `swap-file`, `swap-iter`를 확인하는 브랜치

PR 단위:

- PR A: frame table과 eviction 공통 경로
- PR B: anonymous swap과 file-backed swap operation 검증
- PR 통합: 임시 구현 제거, 두 파트 결합, swap 테스트 3개 결과 정리

## 페어별 리뷰 체크리스트

### 페어 A가 페어 B 구현을 리뷰할 때

- anonymous page가 swap out 이후 자신의 backing 위치를 잃지 않는지
- swap in 성공 뒤 slot이 free 상태로 돌아가는지
- process 종료 시 swapped-out anonymous page의 slot이 남지 않는지
- file-backed page가 swap disk를 사용하지 않고 backing file 기준으로 처리되는지
- file-backed dirty/clean 처리 기준이 frame eviction 흐름과 충돌하지 않는지

### 페어 B가 페어 A 구현을 리뷰할 때

- frame 부족 시 panic으로 끝나지 않고 eviction 경로로 들어가는지
- frame table에서 frame과 page의 연결이 중복되거나 누락되지 않는지
- victim 선정이 frame을 갖지 않은 page를 대상으로 삼지 않는지
- `swap_out` 실패 시 frame 재사용이 잘못 진행되지 않는지
- eviction 뒤 old page가 나중에 fault를 통해 복구 가능한 상태로 남는지

## 주의할 해석 지점

- `5_swapping.md`는 page type별 `swap_in`, `swap_out` operation을 설명하지만, 실제 테스트 통과에는 `0_introduction.md`의 frame table과 eviction 요구사항도 함께 필요하다.
- `swap-anon`은 anonymous page만 보는 것처럼 보이지만, frame 부족을 만들기 때문에 frame table과 swap table이 둘 다 필요하다.
- `swap-file`은 file-backed page가 핵심이지만, 현재 코드에 file-backed swap 함수가 이미 있으므로 신규 구현보다 eviction 통합 검증이 중요하다.
- `swap-iter`는 anonymous page와 file-backed page가 섞이는 테스트라서 두 페어의 interface가 맞는지 확인하는 통합 테스트에 가깝다.
- file-backed page는 swap disk slot을 쓰지 않는다. mmap된 file이 backing store라는 reference 설명을 우선한다.
- swap slot은 process 시작 시 미리 예약하지 않는다. reference는 실제 eviction이 필요할 때 lazy하게 배정하라고 한다.
- dirty/accessed bit는 user page와 kernel alias 문제가 있을 수 있으므로, write-back 누락 가능성을 리뷰 항목으로 남긴다.

## 최종 완료 기준

- `docs/reference/pintos-kaist-original/3_project3/5_swapping.md`의 함수 목록을 모두 설명할 수 있다.
- `docs/reference/pintos-kaist-original/3_project3/0_introduction.md`의 frame table, swap table, eviction lifecycle 요구사항을 현재 구현과 연결해 설명할 수 있다.
- `pintos/vm/anon.c`의 anonymous swap 관련 미구현 지점이 해결되어 있다.
- `pintos/vm/vm.c`의 frame 부족 경로가 eviction으로 이어진다.
- 기존 `pintos/vm/file.c` file-backed swap 구현이 eviction 경로와 통합되어 있다.
- `swap-anon`, `swap-file`, `swap-iter` 테스트 결과를 팀원이 실행 로그 기준으로 남긴다.
- 각 페어가 자기 소유 파일과 상대 페어와의 interface를 PR 설명에 명확히 적는다.
