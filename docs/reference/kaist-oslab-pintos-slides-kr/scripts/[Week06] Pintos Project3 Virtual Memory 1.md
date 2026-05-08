# [Week06] Pintos Project3 Virtual Memory 1

Source: https://www.youtube.com/watch?v=8twIUEo1mIs

Note: 이 문서는 EE415 Pintos 강의와 YouTube 자동 생성 자막을 바탕으로 재구성한 학습용 스크립트입니다. 현재 CS330 64-bit KAIST Pintos 구현 요구사항 판단에는 `pintos-kaist-*` reference 문서를 우선해야 합니다.

## Intro

이 강의는 Pintos Project 3인 virtual memory를 시작합니다. 목표는 이전 project에서 사용하던 단순한 address-space model을 더 실제적인 virtual memory system으로 확장하는 것입니다.

현재 Pintos에서 process는 자기 virtual address space를 가지고, machine은 별도의 physical memory를 가집니다. address space에는 stack, initialized data, uninitialized data, text 같은 영역이 있습니다. 이 시점의 Pintos process address space에는 일반적인 heap 영역이 없습니다.

hardware가 정의한 page table은 virtual page를 physical page frame에 mapping합니다. 이 강의에서 다루는 32-bit Pintos 기준으로는 page directory와 page table page로 이루어진 two-level paging 구조를 설명합니다.

기존 구현에서는 process가 시작될 때 executable file 전체가 eager하게 load됩니다. text와 data page가 process startup 시점에 disk에서 physical memory로 읽힙니다. 하지만 program이 모든 page를 실제로 touch하지 않을 수 있으므로 이 방식은 비효율적입니다.

Project 3에서는 이 model을 바꿉니다. 모든 page를 즉시 load하는 대신, Pintos는 page가 실제로 필요할 때만 load해야 합니다.

## Project Goals

강의는 virtual memory project를 몇 가지 큰 feature로 나눕니다.

첫 번째 목표는 demand paging과 swapping입니다. page는 process가 실제로 접근할 때 disk에서 load됩니다. physical memory가 가득 차면 operating system은 eviction할 page frame을 고르고, 필요하면 그 내용을 swap space에 저장합니다.

두 번째 목표는 stack growth입니다. 기존 구현에서는 process stack 크기가 고정되어 있습니다. stack에 더 많은 공간이 필요하면 process가 죽습니다. stack growth를 구현하면 valid한 stack access가 발생했을 때 Pintos가 추가 stack page를 allocate할 수 있습니다.

세 번째 목표는 memory-mapped file입니다. project는 file contents를 process virtual address space에 mapping하는 `mmap`과 `munmap`을 도입합니다.

네 번째 목표는 kernel이 user memory에 안전하게 접근하는 것입니다. virtual memory와 swapping이 들어오면 user buffer가 항상 physical memory에 resident하다고 가정할 수 없기 때문에 kernel access가 더 조심스러워야 합니다.

## Page Types

physical page frame 관점에서 page는 크게 두 종류로 나눌 수 있습니다.

어떤 page는 disk의 특정 위치와 대응됩니다. executable의 text page와 initialized data page는 executable file에 의해 backing됩니다. `mmap`으로 mapping된 page는 mapped file에 의해 backing됩니다. 이런 page를 file-backed page라고 볼 수 있습니다.

다른 page는 자연스럽게 대응되는 file 위치가 없습니다. stack page, anonymous memory, uninitialized memory가 예입니다. 이런 page는 anonymous page입니다. anonymous page가 evict되면 보통 그 내용은 swap space로 갑니다.

이 구분은 page fault 이후 page를 어디서 다시 load해야 하는지 결정하는 데 중요합니다. source는 executable file, mapped file, swap space, 또는 zero-filled page일 수 있습니다.

## Review: Page, Frame, Page Table, Swap

virtual page는 process virtual address space의 page-sized 영역입니다. 강의에서 설명하는 32-bit model에서는 virtual address를 virtual page number와 offset으로 볼 수 있습니다.

page frame은 physical memory의 page-sized unit입니다. page frame도 frame number와 offset을 가집니다.

page table은 virtual page number를 physical frame number에 연결합니다. hardware는 address translation 중 이 구조를 사용합니다.

swap space는 memory 확장처럼 쓰이는 disk region입니다. 일반 file storage와는 다릅니다. operating system은 physical memory에서 evict한 anonymous page를 저장하기 위해 swap을 사용합니다.

## Demand Paging

demand paging은 page를 process startup 시점이 아니라 실제 요청 시점에 physical memory로 가져오는 방식입니다.

process가 present하지 않은 virtual page에 접근하면 page fault가 발생합니다. page fault handler는 이것이 service 가능한 legal access인지, process를 terminate해야 하는 illegal access인지 판단해야 합니다.

address가 invalid하거나 kernel space를 가리키거나 permission을 위반하거나 process의 virtual memory metadata로 정당화할 수 없다면 process를 kill해야 합니다.

address는 valid하지만 page가 physical memory에 없다면, Pintos는 해당 virtual page에 들어갈 내용을 찾아야 합니다. page에 따라 source는 executable file, mapped file, swap space, 또는 zero-filled page가 됩니다.

source를 찾은 뒤 Pintos는 physical page frame을 allocate하고, contents를 그 frame에 load하고, page table을 update한 다음 faulting instruction을 다시 실행하게 합니다.

## Why a Supplemental Page Table Is Needed

hardware page table은 resident한 virtual page를 physical frame으로 translate하는 정보만 담습니다. lazy loading과 swapping을 구현하는 데 필요한 모든 정보를 담지는 않습니다.

virtual memory 구현에는 page별 추가 metadata가 필요합니다. 강의에서는 이 구조를 virtual memory entry, 즉 `vm_entry`라고 부릅니다.

이 entry는 다음과 같은 정보를 기록합니다.

- virtual page address 또는 virtual page number.
- page가 writable인지 여부.
- page type.
- file-backed page를 load할 때 사용할 file과 offset.
- file에서 읽을 byte 수와 zero-fill할 byte 수.
- anonymous page가 swap out된 경우 swap slot.
- page가 현재 memory에 loaded되어 있는지 여부.

field 이름은 구현 선택이지만, 핵심은 resident하지 않은 page에 대해서도 필요한 metadata가 유지되어야 한다는 점입니다.

## Page Type Metadata

강의는 virtual page를 executable-backed page, 일반 file-backed page, anonymous page 같은 category로 나눕니다.

executable-backed page는 ordinary file-backed page와 구분해야 합니다. 실행 중인 executable file은 modification이 제한되기 때문입니다. implementation은 permission을 지켜야 하고, executable image에 부적절한 write-back이 일어나지 않게 해야 합니다.

file-backed page는 file pointer와 file offset이 필요합니다. 또한 file의 마지막 page처럼 한 page 전체가 valid file byte로 채워지지 않는 경우를 처리하기 위해 read byte 수와 zero byte 수가 필요합니다.

anonymous page는 현재 memory에 있는지, swap에 있는지, swap out된 경우 어느 swap slot에 있는지 알 수 있어야 합니다.

## Per-Process VM Entry Set

각 process는 virtual memory entry set을 가져야 합니다. 강의에서는 hash table을 예시로 사용하지만, list, array, 다른 자료구조도 선택 가능합니다.

process의 `struct thread`에는 supplemental page table을 가리키거나 포함하는 field가 추가됩니다. 강의 예시에서는 process의 `vm_entry` object들을 담는 hash table입니다.

process가 생성될 때 supplemental page table을 initialize해야 합니다. 아직 entry가 없더라도 구조 자체는 준비되어 있어야 합니다.

process가 exit할 때 Pintos는 process의 virtual memory entry들을 destroy하고, 관련 resource를 release해야 합니다.

## Lazy Loading an Executable

기존 Pintos loader에서 `load_segment`는 executable data를 physical memory로 읽고 즉시 page table mapping을 설치합니다.

demand paging에서는 이 방식이 바뀝니다. load time에 physical memory를 allocate하고 segment data를 모두 읽는 대신, Pintos는 나중에 page를 어떻게 load할지 설명하는 virtual memory entry를 만듭니다.

executable segment의 각 page에 대해 loader는 `vm_entry`를 만들고 file, offset, read size, zero size, writable flag, type을 initialize한 다음 process supplemental page table에 insert합니다.

실제 physical page allocation과 file read는 page fault가 발생할 때까지 미뤄집니다.

## Initial Stack Setup

기존 stack setup은 physical page 하나를 allocate하고 user virtual memory의 top에 mapping한 뒤 stack pointer를 설정합니다.

virtual memory에서는 initial stack page도 대응되는 virtual memory entry를 가져야 합니다. Pintos는 initial stack을 설정하되, 이후 stack fault와 cleanup을 일관되게 처리하기 위해 supplemental page table에도 그 page를 기록합니다.

나중에 stack growth는 current stack 아래에서 valid한 stack access가 발생할 때 추가 stack entry를 만드는 방식으로 이 아이디어를 확장합니다.

## Page Fault Flow

핵심 control flow는 다음과 같습니다.

- process가 memory에 접근합니다.
- hardware page table이 해당 page를 present page로 translate하지 못합니다.
- page fault가 발생합니다.
- Pintos가 faulting address가 valid한지 확인합니다.
- Pintos가 supplemental page table에서 matching virtual memory entry를 찾습니다.
- service 가능한 entry라면 frame을 allocate합니다.
- Pintos가 적절한 source에서 page contents를 load합니다.
- Pintos가 page table mapping을 install합니다.
- faulting instruction이 다시 실행됩니다.

이 흐름이 demand paging의 핵심입니다.

## Modifying `page_fault`

기존 Pintos code에서 page fault handler는 대부분 process를 kill합니다. virtual memory에서는 이 동작만으로 충분하지 않습니다.

handler는 먼저 fault가 legal한지 판단해야 합니다. legal하지 않으면 process를 terminate합니다. legal하다면 memory-management fault routine에 처리를 위임합니다. 강의에서는 이 helper를 `handle_mm_fault`라고 부르지만, 실제 function name은 구현자가 정할 수 있습니다.

이 helper는 virtual memory entry lookup, frame allocation, data loading, page installation을 담당합니다.

## Handling Executable-Backed Faults

`handle_mm_fault`의 첫 번째 version은 executable-backed page에 집중할 수 있습니다.

matching `vm_entry`가 executable file에서 온 page라고 말하면, Pintos는 physical page를 allocate하고 file의 해당 byte들을 그 page에 읽습니다. file data가 page 일부만 채우는 경우 나머지 byte는 zero로 clear합니다.

frame을 채운 뒤 Pintos는 page installation function을 호출해 user virtual page와 kernel page frame을 적절한 writable permission으로 연결합니다.

현재 version의 fault handler가 처리하지 않는 page type이면 fault는 fail해야 합니다. 이후 project 단계에서 anonymous page, swapped page, file-backed mapping, stack growth가 이 logic에 추가됩니다.

## Loading a File Page

file-loading helper에는 destination physical frame, file object, virtual memory entry에 저장된 file offset과 byte count가 필요합니다.

helper는 올바른 file offset에서 requested byte를 physical page로 copy하고, page의 나머지 부분을 zero-fill합니다. 이는 segment의 마지막 page가 `PGSIZE`보다 적은 실제 file data를 포함하는 흔한 경우를 처리합니다.

강의는 zero-fill 부분을 잊는 것이 흔한 bug source라고 강조합니다.

## Suggested Helper Functions

강의는 supplemental page table 관련 작업을 helper function으로 분리할 것을 제안합니다.

유용한 operation에는 supplemental page table 초기화, destruction, address로 virtual memory entry 찾기, entry insert, entry delete, entry hash, entry compare, entry memory free 등이 있습니다.

이 operation을 분리하면 page fault handler가 읽기 쉬워지고 process cleanup도 덜 error-prone해집니다.

## Existing Pintos Helpers

이 project에서 중요한 기존 Pintos helper들이 있습니다.

`install_page`는 user virtual page를 kernel page frame에 mapping하고 writable permission을 설정합니다.

`palloc_get_page`는 page-sized physical frame을 allocate하고, `palloc_free_page`는 이를 page allocator로 돌려줍니다.

`malloc`과 `free`는 virtual memory entry 같은 dynamic kernel object에 사용됩니다.

강의는 기존 primitive와 새 supplemental metadata, page fault logic을 조합하는 것이 virtual memory 구현의 핵심이라고 정리합니다.
