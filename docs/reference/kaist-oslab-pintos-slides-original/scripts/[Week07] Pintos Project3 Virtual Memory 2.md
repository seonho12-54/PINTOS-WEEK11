# [Week07] Pintos Project3 Virtual Memory 2

Source: https://www.youtube.com/watch?v=McOBQG0tEWo

Note: This is a paraphrased study script based on the EE415 Pintos lecture and its auto-generated captions. It is useful for understanding the flow of the lecture, but for the current CS330 64-bit KAIST Pintos requirements, use the `pintos-kaist-*` reference documents first.

## Intro

This lecture continues Project 3 and covers four major topics: swapping, stack growth, memory-mapped files, and safe access to user memory.

The first topic is swapping. To support virtual memory, Pintos must represent physical page frames, choose victim frames when memory is full, write evicted pages to the correct backing store, and load pages back on demand.

## Hardware Bits: Dirty and Accessed

The memory management unit provides two useful bits in the page table entry.

The dirty bit is set by hardware when a page is written. If a dirty page is selected as a victim, the operating system must preserve the modified data somewhere before reusing the frame.

The accessed bit is set by hardware when a page is referenced. Replacement algorithms such as clock or second chance use this bit to estimate which pages have been used recently.

Hardware sets these bits, but the operating system must reset or inspect them when implementing a replacement policy.

In the 32-bit Pintos code discussed by this lecture, page directory helper functions are used to read and modify these bits, such as functions for checking or setting dirty and accessed state.

## Frame Table and `struct page`

To evict physical pages, Pintos needs metadata for physical page frames. The lecture calls this structure `struct page`.

A frame table entry should describe a physical page frame that currently contains a user page. If a physical frame is empty, it does not need to have an associated page object.

A typical frame-table entry contains:

- The physical or kernel address of the frame.
- The associated virtual memory entry.
- The owning thread or process.
- A list element used by the replacement policy.
- A pinning flag, added later to prevent some pages from being evicted.

The lecture uses a global LRU-style list as the example frame table, but the exact data structure is an implementation choice.

## Replacement List

The global replacement list contains the currently allocated user frames. A list by itself is not automatically LRU; the replacement behavior comes from how the operating system inserts, removes, scans, and updates entries.

When a page fault needs a frame and no free frame is available, Pintos selects a victim from this list. The victim selection can use LRU, clock, second chance, or another acceptable policy.

After selecting a victim, Pintos decides where the victim's data should go, updates metadata and page table state, frees the physical frame, and then uses that frame for the demanded page.

## Swap-Out Cases

The swap-out action depends on the type of page.

For an anonymous page, Pintos writes the page contents to swap space and records the swap slot in the page's metadata.

For a memory-mapped file page, Pintos checks whether the page is dirty. If it is dirty, the modified contents must be written back to the mapped file. If it is clean, the frame can usually be discarded because the file already contains the same data.

For an executable-backed page, the lecture treats a dirty executable page as needing preservation through swap and then changing its later reload source to anonymous swap state. A clean executable-backed page can be reloaded from the executable file.

After a victim is evicted, the page table entry for the old owner must be cleared or marked not present.

## Swap Space

Pintos uses a separate swap block device. The swap partition is managed in page-sized swap slots.

The operating system keeps an in-memory bitmap to track which swap slots are free and which are in use. The bitmap is volatile; if the system crashes, it disappears. That is acceptable because swap is only temporary backing storage for running processes.

When Pintos needs to evict an anonymous page, it searches the swap bitmap for a free slot, writes the page there, and records the slot in the virtual memory entry.

The lecture points to existing block device functions. A block device can be obtained by role, and individual sectors can be read or written through block I/O functions. A page-sized swap slot spans multiple disk sectors.

## Functions to Implement

The lecture groups the swapping work into several operations.

Pintos needs functions to initialize the frame table or LRU list, insert frame entries, remove frame entries, allocate frames, and free frames.

It also needs a victim selection function and a swap-out function. The victim selection function implements the replacement policy, while swap-out performs the type-specific work of preserving or discarding the victim page.

Finally, Pintos needs a swap-in path. When a page fault refers to a page whose contents are in swap space, the handler allocates a frame, reads the swap slot back into memory, clears the slot in the bitmap, and installs the page.

## Integrating Swapping with Page Faults

The page fault handler must be modified so that physical pages are not allocated directly without considering memory pressure.

The allocation helper first tries to obtain a free page from the page allocator. If that succeeds, it records the frame in the replacement list and returns it.

If no free page is available, Pintos selects a victim, swaps or discards that victim as appropriate, frees the victim frame, and then retries allocation.

The memory-management fault handler then uses this allocation helper before loading the demanded page.

If the virtual memory entry indicates an anonymous swapped-out page, the fault handler reads the page from swap. If the entry indicates a file-backed page, it loads from the file. Later, stack growth adds another possible case.

## Stack Growth

The original Pintos stack has a fixed size. If a program needs more stack space than the initial page, it fails.

Project 3 adds stack growth. If a user program accesses an address just below the current stack pointer, Pintos may treat the access as a legitimate stack growth request and allocate another stack page.

The lecture uses a common rule: if the faulting address is within 32 bytes below the current stack pointer, it can be treated as a stack access. This accounts for instruction behavior that may touch memory slightly below the stack pointer.

There is also a maximum stack size. The lecture uses an 8 MB limit as the example. If the faulting address is too far below the stack pointer or would exceed the stack limit, the access is invalid.

When stack growth is accepted, Pintos creates the needed virtual memory entry, allocates or loads a physical frame, maps it, and updates the supplemental page table.

## Memory-Mapped Files

The next topic is memory-mapped files, implemented through `mmap` and `munmap`.

`mmap` takes a file descriptor and a user virtual address. It maps the file into the process's address space starting at that address. After the mapping is created, accessing the mapped memory can load file data on demand.

If the program writes to the mapped memory, the modified data is eventually written back to the file when the mapping is unmapped or the process exits.

The mapping is page-based. If a file is not an exact multiple of the page size, the final page is partly backed by file data and partly zero-filled.

## `mmap` Requirements

The lecture lists several failure cases.

`mmap` fails if the file size is zero. It also fails if the requested address is not page-aligned, if the address is already in use, or if the address is zero.

Standard input and standard output are not mappable.

On success, Pintos creates a virtual memory entry for each page in the mapped file and inserts those entries into the process's supplemental page table. The file data itself is still loaded lazily through demand paging.

The call returns a mapping ID so the process can later identify the mapping.

## `munmap` Requirements

`munmap` removes a mapping. If mapped pages have been modified, Pintos writes the modified pages back to the file before removing them.

When a mapping is removed, the corresponding virtual memory entries are removed from the process's supplemental page table. Any resident physical frames associated with the mapping must also be handled.

All mappings owned by a process are implicitly unmapped when the process exits.

Once a mapping is created, it remains valid until it is explicitly unmapped or the process exits. Closing or deleting the file does not automatically remove the mapping.

The lecture also notes that different processes mapping the same file do not have to maintain a perfectly consistent shared view for this project.

## Mapping Data Structures

The lecture introduces a mapping object, such as `struct mmap_file`.

This structure contains the mapping ID, a pointer to the file object, a list element for the process's mapping list, and a list of virtual memory entries that belong to the mapping.

The thread structure gains a mapping list. This list tracks all memory-mapped files owned by the process.

Each mapping object then points to the virtual memory entries that represent the mapped pages.

## Final Page Fault Shape

By the end of the virtual memory project, the page fault handler handles several page types.

For executable-backed pages, it loads from the executable file.

For ordinary file-backed pages, including memory-mapped pages, it loads from the mapped file.

For anonymous swapped pages, it reads from swap space.

For valid stack-growth faults, it allocates a new stack page.

If none of these cases apply, the fault is invalid and the process should be terminated.

## Page Faults While the Kernel Touches User Memory

The final topic is accessing user memory during system calls.

Consider a `read` system call. The kernel reads data from disk and copies it into a user buffer. While doing this, the kernel may hold a file-system or device lock.

If the user buffer page is swapped out while the kernel is copying into it, the copy may cause a page fault inside kernel code. Handling that page fault may require disk access. If the kernel already holds the disk or file lock, the fault handler can deadlock trying to acquire a lock the current path already holds.

The lecture uses this scenario to motivate page pinning.

## Page Pinning

Page pinning prevents selected pages from being chosen as eviction victims.

Before a system call accesses a user buffer, Pintos should identify the pages that contain that buffer and pin the corresponding frames. While pinned, those frames must not be selected by the replacement algorithm.

After the system call has finished using the buffer, Pintos unpins those pages.

The replacement algorithm must skip pinned pages when choosing a victim. This protects pages that are actively being used by kernel code and prevents deadlocks caused by faulting on a buffer while holding locks needed by the page fault path.
