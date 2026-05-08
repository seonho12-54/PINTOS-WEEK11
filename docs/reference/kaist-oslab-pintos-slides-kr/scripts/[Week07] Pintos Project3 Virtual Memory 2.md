# [Week07] Pintos Project3 Virtual Memory 2

Source: https://www.youtube.com/watch?v=McOBQG0tEWo

Note: 이 문서는 EE415 Pintos 강의와 YouTube 자동 생성 자막을 바탕으로 재구성한 학습용 스크립트입니다. 현재 CS330 64-bit KAIST Pintos 구현 요구사항 판단에는 `pintos-kaist-*` reference 문서를 우선해야 합니다.

## Intro

이 강의는 Project 3를 이어서 다루며, swapping, stack growth, memory-mapped file, user memory access 네 가지 주제를 설명합니다.

첫 번째 주제는 swapping입니다. virtual memory를 지원하려면 Pintos는 physical page frame을 표현하고, memory가 부족할 때 victim frame을 선택하고, evict된 page를 올바른 backing store에 기록하고, page를 다시 demand에 따라 load할 수 있어야 합니다.

## Hardware Bits: Dirty and Accessed

memory management unit은 page table entry에 두 가지 유용한 bit를 제공합니다.

dirty bit는 page에 write가 발생하면 hardware가 set합니다. dirty page가 victim으로 선택되면, operating system은 frame을 재사용하기 전에 modified data를 어딘가에 보존해야 합니다.

accessed bit는 page가 reference될 때 hardware가 set합니다. clock이나 second chance 같은 replacement algorithm은 이 bit를 사용해 최근 사용 여부를 추정합니다.

hardware는 이 bit들을 set하지만, operating system은 replacement policy를 구현하기 위해 이 bit들을 inspect하거나 reset해야 합니다.

이 강의가 다루는 32-bit Pintos code에서는 dirty/accessed state를 확인하거나 설정하는 page directory helper function을 사용합니다.

## Frame Table and `struct page`

physical page를 evict하려면 Pintos는 physical page frame에 대한 metadata가 필요합니다. 강의에서는 이 구조를 `struct page`라고 부릅니다.

frame table entry는 현재 user page를 담고 있는 physical page frame을 설명해야 합니다. physical frame이 비어 있다면 대응되는 page object가 없어도 됩니다.

일반적인 frame-table entry는 다음 정보를 포함합니다.

- frame의 physical 또는 kernel address.
- associated virtual memory entry.
- owning thread 또는 process.
- replacement policy에 쓰이는 list element.
- 나중에 page pinning을 위해 추가되는 pinning flag.

강의에서는 global LRU-style list를 frame table 예시로 사용하지만, 실제 data structure는 구현 선택입니다.

## Replacement List

global replacement list에는 현재 allocate된 user frame들이 들어 있습니다. list 자체가 자동으로 LRU가 되는 것은 아닙니다. replacement behavior는 operating system이 entry를 insert, remove, scan, update하는 방식에서 나옵니다.

page fault가 frame을 필요로 하는데 free frame이 없으면 Pintos는 이 list에서 victim을 선택합니다. victim selection은 LRU, clock, second chance, 또는 다른 적절한 policy를 사용할 수 있습니다.

victim을 고른 뒤 Pintos는 victim data를 어디로 보낼지 결정하고, metadata와 page table state를 update하고, physical frame을 free한 다음 demanded page에 사용합니다.

## Swap-Out Cases

swap-out 동작은 page type에 따라 달라집니다.

anonymous page의 경우 Pintos는 page contents를 swap space에 쓰고, page metadata에 swap slot을 기록합니다.

memory-mapped file page의 경우 Pintos는 page가 dirty인지 확인합니다. dirty라면 modified contents를 mapped file에 write back해야 합니다. clean이라면 file에 같은 data가 이미 있으므로 frame을 단순히 discard할 수 있습니다.

executable-backed page의 경우 강의는 dirty executable page를 swap에 보존하고 이후 reload source를 anonymous swap state로 바꾸는 방식으로 설명합니다. clean executable-backed page는 executable file에서 다시 load할 수 있습니다.

victim이 evict된 뒤에는 old owner의 page table entry를 clear하거나 not present 상태로 표시해야 합니다.

## Swap Space

Pintos는 별도의 swap block device를 사용합니다. swap partition은 page-sized swap slot 단위로 관리됩니다.

operating system은 어떤 swap slot이 free인지 used인지 추적하기 위해 in-memory bitmap을 유지합니다. 이 bitmap은 volatile합니다. system이 crash하면 사라지지만, swap은 running process를 위한 temporary backing storage이므로 괜찮습니다.

Pintos가 anonymous page를 evict해야 하면 swap bitmap에서 free slot을 찾고, page를 그 slot에 쓰고, virtual memory entry에 slot을 기록합니다.

강의는 기존 block device function을 언급합니다. role로 block device를 얻고, block I/O function을 통해 sector 단위 read/write를 수행합니다. page-sized swap slot은 여러 disk sector로 구성됩니다.

## Functions to Implement

강의는 swapping 작업을 여러 operation으로 나눕니다.

Pintos에는 frame table 또는 LRU list를 initialize하고, frame entry를 insert/remove하고, frame을 allocate/free하는 function이 필요합니다.

또한 victim selection function과 swap-out function이 필요합니다. victim selection function은 replacement policy를 구현하고, swap-out은 victim page의 type에 따라 preserve 또는 discard 작업을 수행합니다.

마지막으로 swap-in path가 필요합니다. page fault가 swap space에 contents가 있는 page를 참조하면, handler는 frame을 allocate하고, swap slot을 memory로 읽고, bitmap에서 slot을 clear하고, page를 install합니다.

## Integrating Swapping with Page Faults

page fault handler는 physical page를 memory pressure 고려 없이 직접 allocate하지 않도록 수정되어야 합니다.

allocation helper는 먼저 page allocator에서 free page를 얻으려 시도합니다. 성공하면 frame을 replacement list에 기록하고 반환합니다.

free page가 없으면 Pintos는 victim을 선택하고, type에 따라 swap하거나 discard하고, victim frame을 free한 뒤 allocation을 다시 시도합니다.

memory-management fault handler는 demanded page를 load하기 전에 이 allocation helper를 사용합니다.

virtual memory entry가 anonymous swapped-out page를 나타내면 fault handler는 swap에서 page를 읽습니다. file-backed page라면 file에서 load합니다. 이후 stack growth는 또 다른 case를 추가합니다.

## Stack Growth

기존 Pintos stack은 fixed size입니다. program이 initial page보다 더 많은 stack space를 필요로 하면 실패합니다.

Project 3는 stack growth를 추가합니다. user program이 current stack pointer 바로 아래 address에 접근하면, Pintos는 이를 legitimate stack growth request로 보고 stack page를 추가로 allocate할 수 있습니다.

강의는 흔히 쓰이는 rule을 사용합니다. faulting address가 current stack pointer보다 32 byte 이내 아래에 있으면 stack access로 볼 수 있습니다. 이는 instruction이 stack pointer보다 약간 아래 memory를 touch할 수 있는 동작을 고려한 것입니다.

maximum stack size도 필요합니다. 강의에서는 8 MB limit을 예시로 사용합니다. faulting address가 stack pointer에서 너무 멀거나 stack limit을 초과하면 invalid access입니다.

stack growth가 허용되면 Pintos는 필요한 virtual memory entry를 만들고, physical frame을 allocate하거나 load하고, mapping을 설치하고, supplemental page table을 update합니다.

## Memory-Mapped Files

다음 주제는 `mmap`과 `munmap`으로 구현되는 memory-mapped file입니다.

`mmap`은 file descriptor와 user virtual address를 받습니다. file을 process address space의 해당 address부터 mapping합니다. mapping이 만들어진 뒤 mapped memory에 접근하면 file data를 demand paging으로 load할 수 있습니다.

program이 mapped memory에 write하면, modified data는 mapping이 unmapped되거나 process가 exit할 때 file로 다시 write back됩니다.

mapping은 page 단위입니다. file size가 page size의 정확한 배수가 아니면 마지막 page는 일부만 file data로 backing되고 나머지는 zero-fill됩니다.

## `mmap` Requirements

강의는 몇 가지 failure case를 나열합니다.

`mmap`은 file size가 zero이면 fail합니다. requested address가 page-aligned가 아니거나, address가 이미 사용 중이거나, address가 zero인 경우에도 fail합니다.

standard input과 standard output은 mappable하지 않습니다.

성공하면 Pintos는 mapped file의 각 page에 대해 virtual memory entry를 만들고 process supplemental page table에 insert합니다. file data 자체는 여전히 demand paging으로 lazy하게 load됩니다.

call은 나중에 mapping을 식별할 수 있도록 mapping ID를 반환합니다.

## `munmap` Requirements

`munmap`은 mapping을 제거합니다. mapped page가 modified되었다면 Pintos는 제거 전에 modified page를 file에 write back해야 합니다.

mapping이 제거될 때 corresponding virtual memory entry들은 process supplemental page table에서 제거됩니다. resident physical frame이 mapping과 연결되어 있으면 그것도 처리해야 합니다.

process가 exit할 때 그 process가 가진 모든 mapping은 implicit하게 unmapped됩니다.

mapping이 만들어지면 explicit하게 unmapped되거나 process가 exit할 때까지 valid합니다. file을 close하거나 delete한다고 해서 mapping이 자동으로 사라지지는 않습니다.

강의는 같은 file을 여러 process가 mapping하더라도 이 project에서는 완전히 consistent한 shared view를 유지할 필요는 없다고 설명합니다.

## Mapping Data Structures

강의는 `struct mmap_file` 같은 mapping object를 소개합니다.

이 구조는 mapping ID, file object pointer, process mapping list를 위한 list element, 그리고 해당 mapping에 속한 virtual memory entry list를 포함합니다.

thread structure에는 mapping list가 추가됩니다. 이 list는 process가 소유한 모든 memory-mapped file을 추적합니다.

각 mapping object는 mapped page를 나타내는 virtual memory entry들로 연결됩니다.

## Final Page Fault Shape

virtual memory project가 끝날 때 page fault handler는 여러 page type을 처리합니다.

executable-backed page는 executable file에서 load합니다.

ordinary file-backed page, 특히 memory-mapped page는 mapped file에서 load합니다.

anonymous swapped page는 swap space에서 읽습니다.

valid stack-growth fault는 새 stack page를 allocate합니다.

이 중 어떤 case에도 해당하지 않으면 invalid fault이며 process를 terminate해야 합니다.

## Page Faults While the Kernel Touches User Memory

마지막 주제는 system call 중 user memory에 접근하는 문제입니다.

`read` system call을 생각해 봅시다. kernel은 disk에서 data를 읽고 user buffer에 copy합니다. 이 과정에서 kernel은 file-system 또는 device lock을 잡고 있을 수 있습니다.

kernel이 copy하는 동안 user buffer page가 swap out되어 있다면, copy 과정에서 kernel code 안에서 page fault가 발생할 수 있습니다. 이 page fault를 처리하려면 disk access가 필요할 수 있습니다. 그런데 kernel이 이미 disk나 file lock을 잡고 있다면, fault handler가 같은 lock을 얻으려 하면서 deadlock이 발생할 수 있습니다.

강의는 이 scenario를 통해 page pinning이 필요한 이유를 설명합니다.

## Page Pinning

page pinning은 특정 page가 eviction victim으로 선택되지 않도록 막는 기법입니다.

system call이 user buffer에 접근하기 전에, Pintos는 그 buffer를 포함하는 page들을 찾고 corresponding frame을 pin해야 합니다. pinned 상태인 frame은 replacement algorithm의 victim으로 선택되면 안 됩니다.

system call이 buffer 사용을 마친 뒤에는 Pintos가 해당 page들을 unpin합니다.

replacement algorithm은 victim을 고를 때 pinned page를 skip해야 합니다. 이렇게 해야 kernel code가 active하게 사용하는 page를 보호하고, page fault path가 필요로 하는 lock을 이미 잡은 상태에서 buffer fault가 발생해 deadlock으로 이어지는 문제를 막을 수 있습니다.
