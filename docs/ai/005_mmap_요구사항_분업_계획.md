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

## 파트 분리 원칙

각 페어는 자기 파트의 요구사항과 테스트 통과를 책임진다. 자기 파트가 아닌 영역은 AI를 이용해 검증용 임시 구현을 둘 수 있지만, 그 임시 구현은 본인 파트 검증을 위한 scaffolding으로만 취급한다. 최종 통합 전에는 임시 구현 여부를 PR 본문과 리뷰에서 명확히 드러내고, 상대 페어의 실제 구현과 맞춰 다시 확인한다.

| 구분 | 페어 A: syscall/검증 파트 | 페어 B: file-backed VM 파트 |
| --- | --- | --- |
| 책임지는 요구사항 | mmap syscall 진입 조건, 실패 조건, overlap 방어, 반환 정책 | file-backed page 등록, lazy load, write-back, munmap, cleanup |
| 본인 로직 검증에 필요한 비소유 파트 | `do_mmap`, `do_munmap`, file-backed page 동작 | `SYS_MMAP`, `SYS_MUNMAP`, fd/주소 검증 |
| 비소유 파트 처리 방식 | 테스트용 임시 VM 구현을 둘 수 있음 | 테스트용 임시 syscall 구현을 둘 수 있음 |
| 임시 구현의 지위 | 페어 A 테스트를 돌리기 위한 보조 코드이며 최종 소유 코드는 아님 | 페어 B 테스트를 돌리기 위한 보조 코드이며 최종 소유 코드는 아님 |
| 최종 통합 때 확인할 것 | 임시 VM 가정이 페어 B 실제 구현과 충돌하지 않는지 | 임시 syscall 가정이 페어 A 실제 구현과 충돌하지 않는지 |

두 페어가 반드시 이름과 의미만 맞춰야 하는 접점은 다음 네 가지다.

- `do_mmap(addr, length, writable, file, offset)`의 입력 전제와 실패 반환
- `do_munmap(addr)`가 해제해야 하는 mapping 범위 기준
- file-backed page metadata에 들어갈 필수 정보
- overlap 검사를 syscall 쪽에서만 할지, VM 쪽에서도 방어적으로 반복할지

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

이 섹션은 페어 A syscall 쪽과 페어 B VM 쪽이 서로 같은 전제를 갖고 있어야 하는 부분 목록이다. 각자 구현은 따로 해도 되지만, 아래 항목은 이름, 의미, 책임 경계가 맞지 않으면 나중에 붙일 때 깨질 수 있다.

근거는 `docs/reference/pintos-kaist-original/3_project3/4_memory_mapped_files.md`의 `mmap`, `munmap`, file-backed page 요구사항이다.

### 1. file-backed page가 보관해야 할 최소 상태

mmap으로 만든 page는 anonymous page가 아니라 파일을 backing store로 가진 page다. 그래서 page마다 최소한 이런 정보를 알아야 한다.

- `backing file reference`: 이 page가 어떤 파일을 보고 있는지
- `file offset`: 파일의 몇 번째 byte부터 읽어야 하는지
- `read_bytes`: 이 page에서 파일에서 실제로 읽을 byte 수
- `zero_bytes`: 파일 끝을 넘어선 나머지를 0으로 채울 byte 수
- `writable`: 이 mapping이 쓰기 가능한지
- mapping 시작 주소/범위 정보: 나중에 `munmap(addr)` 했을 때 어디부터 어디까지 해제해야 하는지

즉 page fault가 났을 때 이 page를 파일 어디에서 어떻게 채울지와, 해제할 때 파일 어디에 다시 써야 하는지를 기억해야 한다는 말이다.

### 2. mmap range 추적 방식

`mmap(addr, length, ...)`는 보통 여러 page를 한 번에 만든다. 예를 들어 12KB를 mmap하면 3개 page가 생긴다.

여기서 선택지가 있다.

- page마다 metadata를 넣고, `munmap` 때 page들을 따라가며 찾기
- 별도 mapping descriptor를 만들어서 이 mmap은 시작 주소 A, 길이 L이라는 식으로 관리하기

둘 다 가능하지만, 페어 A와 페어 B가 다르게 가정하면 문제가 생긴다. 예를 들어 페어 A는 `munmap(addr)`이 시작 주소만 받는다고 생각하는데, 페어 B는 page별 정보만 보고 범위를 못 찾으면 통합 때 막힌다.

### 3. `munmap(addr)`이 여러 페이지를 해제할 때 범위를 찾는 기준

reference에 따르면 `munmap(addr)`의 `addr`은 이전 `mmap`이 반환한 주소여야 한다. 그런데 실제로 해제해야 하는 건 그 주소 한 page가 아니라, 그 `mmap` 호출로 만들어진 전체 mapping 범위다.

그래서 결정해야 하는 질문은 이거다.

- `addr`이 mapping 시작 주소인지 어떻게 확인할 것인가?
- 그 mapping이 몇 page인지 어디서 알 것인가?
- 중간 page 주소로 `munmap`이 들어오면 어떻게 볼 것인가?

이건 syscall 쪽 검증과 VM 쪽 해제가 맞물리는 지점이라 합의가 필요하다.

### 4. dirty bit 확인 기준

reference는 프로세스가 write한 page만 파일에 write-back하고, write하지 않은 page는 write-back하면 안 된다고 한다.

그러려면 어떤 page가 수정됐는지 알아야 한다. 보통 후보는 두 가지다.

- MMU dirty bit: 하드웨어/page table이 이 page에 write가 있었다고 표시한 값
- page metadata: page 구조체 안에 별도 상태를 기록하는 방식

둘 중 무엇을 기준으로 삼을지 정해야 한다. 안 그러면 한쪽은 dirty bit만 보면 된다고 구현하고, 다른 쪽은 metadata가 true일 때만 write-back한다고 생각해서 write-back 조건이 어긋날 수 있다.

### 5. `close(fd)`와 mapping 생명주기 분리

reference가 명확히 말하는 부분이다. 파일을 `close(fd)`해도 이미 만든 mmap은 계속 살아 있어야 한다. 파일을 `remove`해도 마찬가지로 기존 mapping은 유지되어야 한다.

그래서 mmap은 원래 fd table의 `struct file *`에만 기대면 안 되고, mapping마다 독립적인 file reference를 가져야 한다. reference에서도 `file_reopen`을 쓰라고 한다.

쉽게 말하면 다음과 같다.

- fd는 닫혀도 된다.
- mmap은 여전히 파일 내용을 읽고 쓸 수 있어야 한다.
- 그러려면 mapping 쪽이 자기 file 참조를 따로 들고 있어야 한다.

### 6. `process_exit()`에서 explicit `munmap`과 같은 효과를 내는 경로

프로세스가 `munmap()`을 직접 호출하지 않고 종료해도, 모든 mapping은 자동으로 해제되어야 한다. 이때 dirty page는 파일에 write-back되어야 한다.

즉 `process_exit()` 흐름에서 SPT를 정리할 때 file-backed page destroy 경로가 사실상 `munmap`과 같은 일을 해야 한다는 뜻이다.

확인해야 할 질문은 다음과 같다.

- 프로세스 종료 시 모든 mmap page가 SPT 정리 과정에서 방문되는가?
- dirty page write-back이 이 경로에서도 실행되는가?
- file reference가 닫히는가?

### 7. `supplemental_page_table_copy()`에서 file-backed page를 어떻게 다룰지

reference의 SPT copy 설명은 fork 때 부모의 SPT를 자식에게 복사해야 한다고 말한다. 그런데 mmap 쪽 테스트 중 `mmap-inherit`는 child가 `exec`한 뒤 부모의 mmap 주소를 건드리면 죽어야 한다는 걸 확인한다.

여기서 헷갈리면 안 되는 점은 다음과 같다.

- fork 직후 SPT copy 정책은 reference의 VM 요구사항과 연결된다.
- exec 이후에는 새 주소 공간으로 바뀌므로 부모의 mapping이 남으면 안 된다.
- `mmap-inherit` 하나만 보고 mmap은 fork에서 절대 복사 안 해도 된다고 단정하면 위험하다.

그래서 이 부분은 페어 B가 SPT copy/kill 쪽을 볼 때, 페어 A와 테스트 기대를 같이 확인해야 하는 영역이다.

요약하면, 이 섹션은 구현 순서라기보다 두 페어가 같은 약속을 해야 하는 인터페이스다. 특히 중요한 약속은 세 개다.

- `do_mmap`에 들어오는 인자는 어디까지 검증된 상태인가?
- `munmap(addr)`에서 전체 mapping 범위를 어떻게 찾는가?
- dirty write-back과 file reference 정리는 어느 경로에서 보장되는가?

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
- fork 직후 file-backed page를 `supplemental_page_table_copy()`에서 어떻게 다룰지와 exec 이후 기존 mapping이 정리되는 경로를 어떻게 확인할지
- dirty 여부를 MMU dirty bit만으로 볼지, 추가 page state와 함께 볼지

## 페어별 독립 작업 범위와 검증 루트

이 섹션은 작업 순서를 강제하지 않는다. 각 페어는 자기 파트를 먼저 구현하고, 나머지 파트는 검증용 임시 구현으로 채워 테스트를 돌릴 수 있다.

### 공통 베이스라인

- 현재 브랜치의 미구현 지점을 먼저 공유한다.
- 이미 해결된 컴파일 에러를 남은 blocker로 취급하지 않는다.
- 기존 uncommitted 변경이 있다면 mmap 작업과 섞지 않는다.
- 테스트용 임시 구현은 커밋 메시지나 PR 설명에서 명확히 구분한다.

### 페어 A 파트: syscall 진입과 실패 조건

페어 A가 직접 책임지는 것:

- `SYS_MMAP`, `SYS_MUNMAP` dispatcher 연결
- fd table 기준 fd 검증
- stdin/stdout fd 거절
- `addr == NULL`, misaligned `addr`, `length == 0`, misaligned `offset` 거절
- kernel 주소 범위 거절
- 기존 SPT page, code/data page, stack page, 기존 mmap page와 겹치는 range 거절
- 실패 시 `MAP_FAILED` 또는 테스트가 허용하는 종료 정책 정리
- `do_mmap`, `do_munmap` 호출 전후의 반환값 계약 정리

페어 A가 자기 로직 검증을 위해 임시로 채워도 되는 것:

- `do_mmap`이 성공/실패를 단순히 반환하도록 하는 테스트용 구현
- `do_munmap`이 호출 여부만 확인할 수 있는 테스트용 구현
- file-backed page 내부 동작을 완성하지 않은 상태의 최소 stub

페어 A의 우선 목표 테스트:

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

페어 A가 완료했다고 말하려면:

- invalid mmap 요청이 성공 mapping으로 남지 않는다.
- valid 요청만 VM layer로 전달된다.
- 어떤 검증을 syscall layer에서 하고 어떤 검증을 VM layer에 맡기는지 설명할 수 있다.
- 임시 VM 구현 없이 페어 B 실제 구현과 붙였을 때도 같은 실패 조건이 유지된다.

### 페어 B 파트: file-backed page와 mapping 생명주기

페어 B가 직접 책임지는 것:

- `struct file_page` 또는 동등한 file-backed metadata 설계
- `do_mmap`에서 file-backed lazy page 등록
- fault 시 backing file에서 올바른 offset의 데이터 로드
- 마지막 page의 파일 밖 영역 zero fill
- writable mapping의 dirty page write-back
- clean page는 write-back하지 않는 정책
- read-only mapping의 쓰기 방지
- `do_munmap`에서 mapping 전체 제거
- `close(fd)`/`remove(file)` 이후에도 mapping이 유지되도록 file reference 관리
- process exit 시 implicit unmap 효과

페어 B가 자기 로직 검증을 위해 임시로 채워도 되는 것:

- `SYS_MMAP`, `SYS_MUNMAP`이 `do_mmap`, `do_munmap`을 직접 호출하도록 하는 테스트용 syscall 분기
- fd와 주소 검증을 최소화한 테스트용 syscall wrapper
- overlap 검사를 단순화한 검증용 코드

페어 B의 우선 목표 테스트:

- `mmap-read`
- `lazy-file`
- `swap-file`의 기본 mmap read 검증 부분
- `mmap-twice`
- `mmap-write`
- `mmap-clean`
- `mmap-ro`
- `mmap-unmap`
- `mmap-off`
- `mmap-close`
- `mmap-remove`
- `mmap-exit`
- `mmap-shuffle`

페어 B가 완료했다고 말하려면:

- mapping 직후 file-backed page는 lazy 상태다.
- 최초 접근 시 파일 내용과 zero tail이 맞다.
- dirty page만 `munmap` 또는 process exit 때 파일에 반영된다.
- `munmap` 이후 mapping 주소는 더 이상 유효하지 않다.
- file close/remove와 mapping lifetime이 분리되어 있다.
- 임시 syscall 구현 없이 페어 A 실제 구현과 붙였을 때도 같은 VM 동작이 유지된다.

### 통합 때만 공동으로 확인할 것

- `mmap-inherit`
- 전체 `mmap-*`
- 관련 회귀: `lazy-file`, `swap-file`, `pt-write-code2`, `page-merge-stk`
- 페어 A의 실패 조건 검증과 페어 B의 `do_mmap` 방어 로직이 서로 중복되더라도 모순되지 않는지
- 테스트용 임시 구현이 최종 코드에 섞여 남지 않았는지

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
- 통합 브랜치: 두 페어의 실제 구현만 모아 검증하는 브랜치

PR 단위:

- PR A: 페어 A의 syscall/검증 파트
- PR B: 페어 B의 file-backed VM 파트
- PR 통합: 테스트용 임시 구현 제거, 두 파트 결합, 전체 mmap 회귀 확인

각 페어 브랜치에는 자기 파트 검증을 위한 임시 의존성 구현이 들어갈 수 있다. 단, 통합 PR에는 임시 의존성 구현이 남지 않아야 한다.

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
- `mmap-inherit`는 child가 `exec`한 뒤 parent의 mapping 주소를 사용할 수 없음을 확인한다. 현재 코드에서 `__do_fork()`가 SPT copy를 호출하더라도 mmap/file-backed page 구현은 별도 대상이므로, 이 테스트 하나만 보고 fork copy 정책이 이미 끝났다고 판단하지 않는다.
- 둘 이상의 프로세스가 같은 파일을 mapping할 때 데이터 일관성은 요구되지 않는다. 공유 물리 페이지 구현을 목표로 잡지 않는다.
- swapping 관련 테스트까지 확장할 때는 `file_backed_swap_out`과 `file_backed_swap_in`의 역할이 `munmap` write-back과 겹치지 않도록 용어를 분리해서 리뷰한다.

## 최종 완료 기준

- `docs/reference/pintos-kaist-original/3_project3/4_memory_mapped_files.md`의 `mmap`, `munmap`, file-backed page lifecycle 요구사항을 모두 설명할 수 있다.
- `pintos/tests/vm/Rubric.functionality`의 mmap 항목이 모두 목표 테스트에 포함되어 있다.
- `pintos/tests/vm/Rubric.robustness`의 mmap robustness 항목이 모두 목표 테스트에 포함되어 있다.
- mmap 구현 후 close/remove/exit/munmap 경로의 file reference 생명주기를 설명할 수 있다.
- 각 페어가 자기 소유 파일과 상대 페어와의 interface를 문서화했다.
- 테스트 결과 문서에는 PASS뿐 아니라 실패 로그와 실패 단계도 함께 남긴다.
