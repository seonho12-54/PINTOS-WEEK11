# mmap 요구사항 구현 2페어 분업 계획

## 문서 목적

이 문서는 Project 3 `mmap`/`munmap` 구현을 4명이 2명씩 페어 프로그래밍으로 진행하기 위한 분업 계획이다.
구현 코드를 대신 작성하지 않고, `docs/reference`와 로컬 테스트 파일에서 확인되는 요구사항을 기준으로 무엇을 해야 하는지만 정리한다.

## 기준 자료와 우선순위

- `AGENTS.md`
  - Pintos 구현 판단은 `docs/reference`의 KAIST Pintos 문서를 기준으로 한다.
  - 구현 코드를 통째로 생성하지 않고, 요구사항 정리와 검토를 보조한다.
  - AI 산출물은 `docs/ai` 아래 다음 번호 문서로 저장한다.
- `docs/reference/pintos-kaist-original/3_project3/4_memory_mapped_files.md`
  - mmap 관련 최우선 요구사항 문서다.
- `docs/reference/pintos-kaist-kr/3_project3/4_memory_mapped_files.md`
  - 위 문서의 한국어 보조 자료다.
- `docs/reference/pintos-kaist-original/3_project3/1_vm_management.md`
  - SPT, frame claim, page operations, `destroy()` 흐름의 기준이다.
- `docs/reference/pintos-kaist-original/3_project3/2_anon.md`
  - lazy loading, uninit page, SPT copy/kill 흐름의 기준이다.
- `docs/reference/pintos-kaist-original/2_project2/7_FAQ.md`
  - open file remove semantics의 기준이다.
- `pintos/tests/vm/mmap-*.c`, `pintos/tests/vm/*.ck`, `pintos/tests/vm/Make.tests`
  - mmap 테스트가 실제로 확인하는 입력 조건과 기대 결과의 기준이다.

## reference 기준 요구사항

### mmap 기본 의미

- `mmap(addr, length, writable, fd, offset)`은 `fd`로 열린 파일의 `offset`부터 `length` 바이트를 `addr`부터 시작하는 연속 가상 페이지에 매핑한다.
- 성공하면 매핑 시작 주소를 반환하고, 실패하면 `NULL`에 해당하는 실패 값을 반환해야 한다.
- memory-mapped page는 anonymous page와 달리 file-backed page이며, 페이지 내용은 backing file의 데이터를 반영한다.
- mmap 영역은 lazy loading 방식이어야 한다. 매핑 시점에 실제 프레임과 파일 내용을 모두 올리는 것이 아니라, fault 시점에 필요한 페이지를 가져오는 구조여야 한다.
- 파일 길이가 페이지 크기의 배수가 아니면 마지막 페이지의 파일 밖 영역은 fault-in 때 0으로 채우고, write-back 때는 파일 밖 영역을 버려야 한다.
- mmap page가 unmapped되거나 swapped out될 때 변경된 내용은 파일에 반영되어야 한다.

### mmap 실패 조건

reference에서 명시된 실패 조건은 다음과 같다.

- 파일 길이가 0바이트인 경우 실패할 수 있다.
- `addr`이 page-aligned가 아니면 반드시 실패해야 한다.
- 매핑하려는 페이지 범위가 기존 mapped page set과 겹치면 반드시 실패해야 한다.
  - stack과 겹치는 경우 포함
  - 실행 파일 load 시점에 매핑된 code/data page와 겹치는 경우 포함
- `addr`이 0이면 실패해야 한다.
- `length`가 0이면 실패해야 한다.
- console input/output을 나타내는 file descriptor는 mmap할 수 없다.

테스트에서 추가로 확인되는 조건은 다음과 같다.

- `offset`이 page-aligned가 아닌 경우 실패해야 한다. 이 항목은 reference의 mmap 문단에는 직접 문장으로 보이지 않지만 `mmap-bad-off` 테스트가 요구한다.
- kernel virtual address 범위에 걸치는 매핑은 실패해야 한다.
- invalid fd, stdin fd, stdout fd는 실패하거나 프로세스 종료로 처리될 수 있지만, 로컬 `.ck`는 정상 종료와 `exit(-1)` 둘 다 허용하는 테스트가 있다.

### munmap 기본 의미

- `munmap(addr)`은 같은 프로세스에서 이전 `mmap`이 반환했고 아직 해제되지 않은 매핑을 해제한다.
- explicit `munmap`뿐 아니라 프로세스 종료 시에도 모든 mapping이 implicit하게 해제되어야 한다.
- 해제 시 프로세스가 write한 page는 파일에 write-back되어야 하고, write하지 않은 page는 write-back하면 안 된다.
- 해제 후 해당 page들은 프로세스의 virtual page 목록에서 제거되어야 한다.

### file lifetime 요구사항

- 파일을 `close`하거나 `remove`해도 이미 만들어진 mapping은 사라지지 않는다.
- mapping은 `munmap` 또는 process exit까지 유효하다.
- 각 mapping은 파일에 대한 독립 참조를 가져야 하므로 `file_reopen`을 사용해야 한다.
- 둘 이상의 프로세스가 같은 파일을 매핑하더라도 서로 일관된 데이터를 볼 필요는 없다.

## 현재 코드에서 mmap 작업이 닿는 지점

현재 저장소 기준으로 mmap 작업이 닿는 주요 파일은 다음과 같다.

- `pintos/userprog/syscall.c`
  - syscall dispatcher에 `SYS_MMAP`, `SYS_MUNMAP` 분기 추가가 필요하다.
  - fd table, 사용자 포인터 검증, filesys lock과 연결된다.
- `pintos/include/userprog/syscall.h`
  - 필요하면 syscall helper 노출 범위를 정리한다.
- `pintos/vm/file.c`
  - `file_backed_initializer`, `file_backed_swap_in`, `file_backed_swap_out`, `file_backed_destroy`, `do_mmap`, `do_munmap`가 현재 핵심 구현 대상이다.
- `pintos/include/vm/file.h`
  - file-backed page가 보관해야 할 상태를 정리할 위치다.
- `pintos/include/vm/vm.h`
  - `struct page`, `struct supplemental_page_table` 확장이 필요할 수 있다.
- `pintos/vm/vm.c`
  - SPT 중복 검사, page type별 copy/kill, destroy 경로와 연결된다.
- `pintos/userprog/process.c`
  - process exit 시 SPT kill이 mmap write-back과 연결된다.
  - exec로 주소 공간이 교체될 때 기존 mapping 정리가 정상 동작해야 한다.

현재 코드베이스에서 확인한 mmap 관련 상태는 다음과 같다.

- `filesys_lock`은 `pintos/include/userprog/syscall.h`에서 `extern`으로 선언되고, `pintos/userprog/syscall.c`에서 전역 정의로 연결되어 있다.
- `pintos/lib/user/syscall.c`와 `pintos/include/lib/user/syscall.h`에는 사용자 프로그램용 `mmap`, `munmap` wrapper와 `MAP_FAILED` 정의가 이미 있다.
- `pintos/include/lib/syscall-nr.h`에는 `SYS_MMAP`, `SYS_MUNMAP` syscall 번호가 이미 있다.
- `pintos/userprog/syscall.c`의 dispatcher에는 아직 `SYS_MMAP`, `SYS_MUNMAP` 분기가 없다.
- `pintos/vm/file.c`의 file-backed page 함수들과 `do_mmap`, `do_munmap`은 구현 대상 상태다.
- `pintos/include/vm/file.h`의 `struct file_page`는 아직 비어 있어 file-backed page metadata 설계가 필요하다.
- `pintos/vm/vm.c`의 `vm_alloc_page_with_initializer()`는 `VM_FILE` 타입을 `file_backed_initializer`에 연결할 수 있는 구조를 이미 가지고 있다.

## 한눈에 보는 작업 경계

| 구분 | 페어 A: syscall/검증 | 페어 B: file-backed VM |
| --- | --- | --- |
| 핵심 질문 | 이 mmap 요청을 받아도 되는가? | 받아들인 mapping을 page fault, unmap, exit에서 어떻게 유지/정리할 것인가? |
| 주 작업 파일 | `userprog/syscall.c`, `include/userprog/syscall.h` | `vm/file.c`, `include/vm/file.h`, `include/vm/vm.h` |
| 보조 작업 파일 | `vm/vm.c`의 SPT 조회/범위 판단 경계 | `vm/vm.c`의 SPT kill/copy/destroy 경계, `userprog/process.c`의 exit cleanup 경계 |
| 직접 담당 | syscall 분기, fd 검증, addr/length/offset 검증, overlap 검증, 실패 반환 정책 | file-backed page metadata, lazy fault-in, dirty write-back, munmap, file reference 정리 |
| 직접 담당하지 않음 | 파일 내용을 page에 읽어 넣는 세부 lifecycle | syscall 인자 파싱과 invalid fd 정책 |
| 페어 B에 넘겨야 하는 것 | 검증된 `addr`, `length`, `writable`, `file`, `offset`과 실패 시 `MAP_FAILED` 정책 | 해당 없음 |
| 페어 A에 보장해야 하는 것 | 해당 없음 | `do_mmap` 성공/실패 반환, `do_munmap` 후 mapping 제거, cleanup 경로의 일관성 |
| 먼저 볼 테스트 | invalid mmap 계열 | read/write/cleanup 계열 |

## 의존성 지도

아래 순서의 의존성을 기준으로 작업하면 충돌과 재작업이 줄어든다.

| 순서 | 의존성 | 선행 담당 | 후행 담당 | 후행 작업이 기대하는 상태 |
| --- | --- | --- | --- | --- |
| 1 | syscall 번호와 dispatcher 연결 | 페어 A | 페어 B | 사용자 `mmap`/`munmap` 호출이 kernel 내부 함수까지 도달한다. |
| 2 | fd와 인자 검증 | 페어 A | 페어 B | `do_mmap`은 mmap 가능한 file과 정렬된 주소/offset만 받는다고 가정할 수 있다. |
| 3 | overlap 검사 정책 | 페어 A 주도, 페어 B 합의 | 페어 B | `do_mmap`이 중간 실패 정리를 하더라도 애초에 겹치는 range는 들어오지 않는다. |
| 4 | file-backed metadata 형태 | 페어 B 주도, 페어 A 합의 | 페어 A | syscall layer가 `do_mmap`에 넘길 정보와 반환 의미를 안다. |
| 5 | lazy fault-in | 페어 B | 페어 A | syscall의 user buffer 검증이 mmap page fault와 충돌하지 않는다. |
| 6 | dirty write-back과 munmap | 페어 B | 페어 A | `munmap(addr)` syscall은 VM layer에 위임해 mapping 전체를 정리할 수 있다. |
| 7 | process exit cleanup | 페어 B | 두 페어 | explicit `munmap` 없이 종료해도 file-backed page destroy 경로가 요구사항을 만족한다. |
| 8 | 통합 회귀 | 두 페어 | 두 페어 | mmap 구현이 기존 syscall user memory, stack growth, lazy loading 테스트를 깨지 않는다. |

페어별로 독립 작업을 시작하더라도 2, 3, 4번은 반드시 짧게 합의하고 진행한다. 특히 overlap 검사와 mapping 범위 추적 방식은 두 페어가 동시에 가정하면 충돌 가능성이 높다.

## 페어 구성

### 페어 A: syscall 경계와 mapping 등록 검증 담당

담당 범위:

- `SYS_MMAP`, `SYS_MUNMAP` syscall 경로 연결
- 사용자 인자 검증과 fd 검증
- mmap 실패 조건 정리
- 매핑 범위가 기존 SPT entry, stack, executable page, kernel 영역과 겹치지 않는지 확인하는 정책 정리
- `do_mmap` 호출 전후의 syscall-level 반환값 규칙 정리
- `munmap` syscall에서 잘못된 주소가 들어왔을 때의 정책을 reference와 테스트 기준으로 정리

주요 소유 파일:

- `pintos/userprog/syscall.c`
- `pintos/include/userprog/syscall.h`
- `pintos/vm/vm.c` 중 SPT 조회/중복 판단과 맞닿는 작은 경계

페어 A가 우선 확인할 테스트:

- `mmap-null`
- `mmap-misalign`
- `mmap-zero-len`
- `mmap-bad-fd`
- `mmap-bad-fd2`
- `mmap-bad-fd3`
- `mmap-bad-off`
- `mmap-kernel`
- `mmap-overlap`
- `mmap-over-code`
- `mmap-over-data`
- `mmap-over-stk`

완료 기준:

- invalid mmap 요청이 성공 mapping으로 남지 않는다.
- 성공 가능한 mmap 요청만 `do_mmap`으로 넘어간다.
- 실패 시 테스트가 기대하는 실패 반환 또는 종료 정책과 맞는다.
- range overlap 검사가 SPT 기준으로 설명 가능하다.
- 페어 B가 요구하는 file-backed page metadata를 syscall layer에서 누락 없이 넘길 수 있다.

### 페어 B: file-backed page 생명주기와 write-back 담당

담당 범위:

- file-backed page 초기화
- mmap 영역의 lazy loading
- fault-in 시 파일 내용 읽기와 마지막 페이지 zero fill
- dirty page write-back
- `munmap`에서 SPT 제거와 file reference 정리
- process exit에서 implicit unmap 효과가 나도록 destroy 경로 정리
- close/remove 이후에도 mapping이 유지되도록 file reference 생명주기 정리

주요 소유 파일:

- `pintos/vm/file.c`
- `pintos/include/vm/file.h`
- `pintos/include/vm/vm.h` 중 file-backed page 상태 확장
- `pintos/vm/vm.c` 중 `supplemental_page_table_copy`, `supplemental_page_table_kill`, `destroy()` 연결부
- `pintos/userprog/process.c` 중 process cleanup과 맞닿는 지점

페어 B가 우선 확인할 테스트:

- `mmap-read`
- `lazy-file`
- `swap-file` 중 file-backed page fault-in 관찰 부분
- `mmap-write`
- `mmap-clean`
- `mmap-ro`
- `mmap-unmap`
- `mmap-close`
- `mmap-remove`
- `mmap-exit`
- `mmap-off`
- `mmap-shuffle`

완료 기준:

- mapping 직후에는 file-backed page가 lazy 상태로 존재한다.
- 최초 접근 시 파일에서 해당 page 내용이 들어오고, 파일 밖 page tail은 0으로 보인다.
- writable mapping에서 변경된 page만 `munmap` 또는 process exit 때 파일에 반영된다.
- clean page는 `munmap` 때 파일 내용을 덮어쓰지 않는다.
- read-only mapping은 쓰기 가능한 매핑으로 설치되지 않는다.
- close/remove 이후에도 mapping의 데이터 접근이 유지된다.
- `munmap` 이후 해당 가상 주소 접근은 더 이상 유효하지 않다.

## 페어 간 합의가 필요한 공유 설계

아래 항목은 어느 한 페어가 독단적으로 정하면 충돌이 나기 쉽다. 첫 작업 시작 전에 30분 정도 함께 합의하고 문서화한 뒤 구현에 들어간다.

- file-backed page가 보관해야 할 최소 상태
  - backing file reference
  - file offset
  - page 안에서 읽을 byte 수
  - page 안에서 0으로 채울 byte 수
  - writable 여부
  - mapping의 시작 주소 또는 munmap 범위 추적 방법
- mmap range를 어디까지 하나의 mapping으로 추적할지
  - page별 SPT entry만으로 충분한지
  - 별도 mapping descriptor가 필요한지
- `munmap(addr)`이 여러 페이지를 해제할 때 범위를 찾는 기준
- dirty bit 확인 기준
  - MMU dirty bit와 page metadata 중 어떤 정보를 신뢰할지
- `close(fd)`와 mapping 생명주기 분리 방법
- `process_exit()`에서 explicit `munmap`과 같은 효과를 내는 경로
- `supplemental_page_table_copy()`에서 file-backed page를 어떻게 다룰지
  - reference의 SPT copy 요구사항과 `mmap-inherit` 테스트 관찰을 함께 놓고 결정한다.
  - `mmap-inherit` 테스트는 child가 `exec`한 뒤 parent의 mapping 주소에 접근할 수 없음을 확인한다.

## 페어별 작업 제한선

### 페어 A가 집중할 영역

- syscall dispatcher에 `SYS_MMAP`, `SYS_MUNMAP`을 연결한다.
- fd table에서 mmap 가능한 파일을 판별한다.
- `addr`, `length`, `offset`, fd, kernel/user address 범위, 기존 SPT와의 overlap을 검증한다.
- 실패 요청은 VM file-backed layer까지 내려가지 않게 막는다.
- `do_mmap`, `do_munmap`의 호출 계약을 페어 B와 맞춘다.

페어 A가 직접 맡지 않는 영역:

- 파일 내용을 frame에 읽어 넣는 lazy-load 처리
- dirty bit 기준 write-back
- file-backed page destroy
- process exit에서 mmap page를 write-back하는 세부 처리

### 페어 B가 집중할 영역

- file-backed page가 보관할 metadata를 정한다.
- `do_mmap`에서 lazy file-backed page들을 SPT에 등록한다.
- page fault 시 backing file에서 해당 page 내용을 읽고, 파일 밖 영역을 0으로 채운다.
- `do_munmap`과 destroy 경로에서 dirty page만 file에 write-back한다.
- mapping별 file reference를 `close`/`remove`와 독립적으로 유지한다.
- process exit 시 implicit unmap 요구사항이 만족되는지 확인한다.

페어 B가 직접 맡지 않는 영역:

- syscall argument register 해석
- invalid fd, stdin/stdout fd 정책
- mmap 요청이 kernel/code/data/stack/existing page와 겹치는지 판단하는 syscall 진입 검증
- user-facing syscall 반환값의 최종 정책

### 같이 합의해야만 하는 영역

- `do_mmap`이 검증을 어디까지 신뢰하고 어디부터 자체 방어할지
- `munmap(addr)`이 mapping 전체 범위를 찾는 방식
- mapping descriptor를 별도로 둘지, page별 metadata로 충분한지
- `supplemental_page_table_copy()`에서 file-backed page를 복사할지, exec 이후 정리만 신뢰할지
- dirty 여부를 MMU dirty bit만으로 볼지, 추가 page state와 함께 볼지

## 작업 순서 제안

### 0단계: 베이스라인 확인

목표:

- mmap 구현 전 현재 브랜치가 mmap 작업을 시작할 수 있는 상태인지 확인한다.
- 이 단계는 mmap 요구사항 구현이 아니라 이후 테스트 결과를 해석하기 위한 기준선 정리다.

확인 기준:

- 최소한 `syscall.c`, `vm/file.c`, `vm/vm.c`, `process.c`의 현재 상태와 미구현 stub 위치를 두 페어가 공유해야 한다.
- 이미 해결된 컴파일 에러를 문서상 남은 blocker로 취급하지 않는다.
- 기존 uncommitted 변경이 있다면 mmap 작업과 섞지 않는다.

### 1단계: 실패 조건과 syscall 연결

담당:

- 페어 A 주도, 페어 B 리뷰

목표:

- syscall dispatcher에서 `mmap`/`munmap`이 호출 가능한 상태를 만든다.
- invalid argument 테스트들이 성공 mapping을 만들지 않게 한다.
- 이 단계에서는 file-backed lazy loading 전체가 완성되지 않아도 된다.

목표 테스트:

- `mmap-null`
- `mmap-misalign`
- `mmap-zero-len`
- `mmap-bad-fd`
- `mmap-bad-fd2`
- `mmap-bad-fd3`
- `mmap-bad-off`
- `mmap-kernel`
- `mmap-overlap`
- `mmap-over-code`
- `mmap-over-data`
- `mmap-over-stk`

### 2단계: lazy file-backed read path

담당:

- 페어 B 주도, 페어 A syscall boundary 리뷰

목표:

- `do_mmap`이 file-backed lazy page들을 SPT에 등록한다.
- 접근 전에는 실제 frame을 할당하지 않는 lazy 상태를 유지한다.
- 최초 접근 시 파일에서 올바른 offset의 데이터를 읽는다.
- 마지막 페이지의 file 밖 영역은 0으로 보인다.

목표 테스트:

- `mmap-read`
- `lazy-file`
- `swap-file`의 기본 mmap read 검증 부분
- `mmap-twice`

### 3단계: munmap, dirty write-back, read-only

담당:

- 페어 B 주도, 페어 A syscall/테스트 케이스 리뷰

목표:

- `munmap`이 mapping 전체를 해제한다.
- writable mapping에서 수정된 page만 파일에 반영된다.
- clean page는 파일을 덮어쓰지 않는다.
- read-only mapping은 사용자 쓰기를 허용하지 않는다.
- `offset > 0` mapping의 write-back 위치가 파일 offset과 일치한다.

목표 테스트:

- `mmap-write`
- `mmap-clean`
- `mmap-ro`
- `mmap-unmap`
- `mmap-off`

### 4단계: 파일 생명주기와 process cleanup

담당:

- 페어 B 주도, 페어 A fd/syscall 영향 리뷰

목표:

- `close(fd)` 이후에도 mapping은 유효하다.
- `remove(file)` 이후에도 기존 mapping은 유효하다.
- process exit 시 explicit `munmap` 없이도 dirty mapping이 write-back된다.
- exec 이후 이전 주소 공간의 mapping이 새 프로그램에 남지 않는다.

목표 테스트:

- `mmap-close`
- `mmap-remove`
- `mmap-exit`
- `mmap-inherit`

### 5단계: 통합 부하와 회귀

담당:

- 두 페어 공동

목표:

- 여러 페이지, 반복 접근, 다량 write가 포함된 mmap 동작을 확인한다.
- mmap 구현이 기존 lazy loading, stack growth, syscall user memory 검증을 깨지 않았는지 확인한다.

목표 테스트:

- `mmap-shuffle`
- 전체 `mmap-*`
- 관련 회귀: `lazy-file`, `swap-file`, `pt-write-code2`, `page-merge-stk`

## 테스트 매트릭스

| 분류 | 테스트 | 확인하는 요구사항 |
| --- | --- | --- |
| 기본 read | `mmap-read` | 파일 내용을 mmap 주소에서 읽을 수 있고 tail zero가 보인다. |
| lazy loading | `lazy-file` | mmap 직후 page가 실제로 load되지 않고 접근한 page만 load된다. |
| write-back | `mmap-write` | mmap 주소에 쓴 내용이 `munmap` 이후 파일에 반영된다. |
| clean page | `mmap-clean` | clean mmap page가 외부 file write 결과를 덮어쓰지 않는다. |
| read-only | `mmap-ro` | writable=0 mapping에 사용자 쓰기를 허용하지 않는다. |
| explicit unmap | `mmap-unmap` | `munmap` 이후 주소 접근이 유효하지 않다. |
| close lifetime | `mmap-close` | fd close 이후에도 mapping은 유지된다. |
| remove lifetime | `mmap-remove` | file remove 이후에도 기존 mapping은 유지된다. |
| exit cleanup | `mmap-exit` | process exit 시 dirty mapping이 파일에 반영된다. |
| offset | `mmap-off` | page-aligned offset mapping과 write-back 위치가 맞다. |
| repeated mapping | `mmap-twice` | 같은 파일을 서로 다른 주소에 두 번 mapping할 수 있다. |
| shuffle | `mmap-shuffle` | 여러 page에 반복 write/read가 안정적으로 동작한다. |
| invalid fd | `mmap-bad-fd`, `mmap-bad-fd2`, `mmap-bad-fd3` | invalid fd와 stdin/stdout mapping을 거절한다. |
| invalid addr | `mmap-null`, `mmap-misalign`, `mmap-kernel` | NULL, misaligned, kernel 범위 mapping을 거절한다. |
| overlap | `mmap-overlap`, `mmap-over-code`, `mmap-over-data`, `mmap-over-stk` | 기존 mapping, code, data, stack과 겹치는 mapping을 거절한다. |
| zero length | `mmap-zero-len`, `mmap-zero` | length 0 mapping은 유효 mapping을 만들지 않는다. |
| invalid offset | `mmap-bad-off` | page-aligned가 아닌 offset mapping을 거절한다. |

## 권장 브랜치와 PR 단위

한 브랜치에서 네 명이 동시에 작업하면 `vm/file.c`, `vm/vm.c`, `syscall.c` 충돌이 커질 수 있다. 다음 단위를 권장한다.

- 페어 A 브랜치: `feat-#<issue>-mmap-syscall-validation`
- 페어 B 브랜치: `feat-#<issue>-mmap-file-backed`
- 통합 브랜치: 두 브랜치를 `dev` 또는 mmap 통합 브랜치에 순서대로 병합

PR 단위:

- PR 1: syscall 연결과 invalid argument 방어
- PR 2: file-backed lazy loading과 기본 read
- PR 3: munmap, dirty write-back, cleanup
- PR 4: 통합 회귀와 주석 정리

작업 중 PR을 쪼개기 어렵다면 최소한 커밋 단위는 위 경계에 맞춘다.

## 페어별 리뷰 체크리스트

### 페어 A가 페어 B 구현을 리뷰할 때

- file-backed page가 lazy 상태로 등록되는지
- mmap range 전체가 SPT에 빠짐없이 들어가는지
- 실패 중간 지점에서 이미 등록한 page와 file reference가 정리되는지
- `munmap` 이후 SPT와 pml4 mapping이 함께 정리되는지
- dirty가 아닌 page를 write-back하지 않는지

### 페어 B가 페어 A 구현을 리뷰할 때

- invalid fd, stdin, stdout, zero length, NULL, misaligned addr가 막히는지
- offset alignment는 테스트 기준으로 처리되는지
- kernel address와 address overflow를 고려했는지
- code/data/stack/existing mmap overlap을 같은 기준으로 검사하는지
- syscall user pointer 검증이 mmap 주소의 권한과 충돌하지 않는지

## 주의할 해석 지점

- `mmap-bad-off`의 offset alignment 요구는 테스트 기반 요구사항이다. reference의 mmap 문단에는 offset alignment 실패 조건이 직접 문장으로 보이지 않는다.
- `mmap-zero`는 빈 파일과 length 0 호출을 다루며, reference는 0바이트 파일 mmap이 실패할 수 있다고 한다. `mmap-zero-len`은 length 0 실패를 명확히 기대한다.
- `mmap-inherit`는 child가 `exec`한 뒤 parent의 mapping 주소를 사용할 수 없음을 확인한다. 이 테스트 하나만으로 fork 직후 file-backed mapping copy 정책 전체를 단정하지 말고, SPT copy reference와 함께 판단해야 한다.
- 둘 이상의 프로세스가 같은 파일을 mapping할 때 데이터 일관성은 요구되지 않는다. 공유 물리 페이지 구현을 목표로 잡지 않는다.
- swapping 관련 테스트까지 확장할 때는 `file_backed_swap_out`과 `file_backed_swap_in`의 역할이 `munmap` write-back과 겹치지 않도록 용어를 분리해서 리뷰한다.

## 최종 완료 기준

- `docs/reference/pintos-kaist-original/3_project3/4_memory_mapped_files.md`의 `mmap`, `munmap`, file-backed page lifecycle 요구사항을 모두 설명할 수 있다.
- `pintos/tests/vm/Rubric.functionality`의 mmap 항목이 모두 목표 테스트에 포함되어 있다.
- `pintos/tests/vm/Rubric.robustness`의 mmap robustness 항목이 모두 목표 테스트에 포함되어 있다.
- mmap 구현 후 close/remove/exit/munmap 경로의 file reference 생명주기를 설명할 수 있다.
- 각 페어가 자기 소유 파일과 상대 페어와의 interface를 문서화했다.
- 테스트 결과 문서에는 PASS뿐 아니라 실패 로그와 실패 단계도 함께 남긴다.
