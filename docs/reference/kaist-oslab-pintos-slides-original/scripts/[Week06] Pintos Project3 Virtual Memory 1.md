# [Week06] Pintos Project3 Virtual Memory 1

Source: https://www.youtube.com/watch?v=8twIUEo1mIs

Note: This is a paraphrased study script based on the EE415 Pintos lecture and its auto-generated captions. It is useful for understanding the flow of the lecture, but for the current CS330 64-bit KAIST Pintos requirements, use the `pintos-kaist-*` reference documents first.

## Intro

This lecture begins the third Pintos project, virtual memory. The goal is to move from the simple address-space model used in the earlier projects to a more realistic virtual memory system.

In the current Pintos design, a process has its own virtual address space, while the machine has a separate physical memory. The address space contains regions such as stack, initialized data, uninitialized data, and text. At this point, Pintos does not have a normal heap region in the process address space.

The hardware-defined page table maps virtual pages to physical page frames. In the 32-bit Pintos context used by this lecture, the page table is described as a two-level structure, with a page directory and page table pages. Each virtual page is mapped to a physical page frame through this structure.

In the original implementation, when a process starts, the executable is loaded eagerly. The pages for the executable's text and data are read from disk into physical memory at process startup. This is inefficient because a program may never touch many of those pages.

Project 3 changes this model. Instead of loading every page immediately, Pintos should load pages only when they are needed.

## Project Goals

The lecture groups the virtual memory project into several major features.

The first goal is demand paging and swapping. Pages are loaded from disk only when a process actually accesses them. If physical memory is full, the operating system chooses a page frame to evict and may store its contents in swap space.

The second goal is stack growth. In the original implementation, a process receives a fixed-size stack. If the stack needs more space, the process dies. With stack growth, Pintos can allocate additional stack pages when a valid stack access occurs.

The third goal is memory-mapped files. The project introduces `mmap` and `munmap`, allowing file contents to be mapped into a process's virtual address space.

The fourth goal is safer access to user memory from the kernel. Once virtual memory and swapping exist, the kernel must be careful when it touches user buffers, because those pages may not be resident in memory.

## Page Types

From the physical page frame's point of view, pages can be divided into two broad categories.

Some pages have a matching location on disk. Text pages and initialized data pages from an executable are backed by the executable file. Pages mapped through `mmap` are backed by the mapped file. These are file-backed pages.

Other pages do not naturally correspond to a file location. Stack pages, heap-like anonymous memory, and uninitialized memory are examples. These are anonymous pages. If an anonymous page has to be evicted, its contents usually go to swap space.

This distinction matters because the operating system needs to know where to reload a page from after a page fault: an executable file, a mapped file, swap space, or a newly zero-filled page.

## Review: Page, Frame, Page Table, Swap

A virtual page is a page-sized portion of a process's virtual address space. In the 32-bit model explained in the lecture, the virtual address can be viewed as a virtual page number plus an offset.

A page frame is a page-sized unit of physical memory. A page frame also has a frame number and an offset.

The page table connects these two ideas by mapping virtual page numbers to physical frame numbers. Hardware uses this structure during address translation.

Swap space is a disk region used as an extension of memory. It is not the same as normal file storage. The operating system uses it to store anonymous pages that have been evicted from physical memory.

## Demand Paging

Demand paging means that a page is brought into physical memory when it is requested, not necessarily when the process starts.

When a process accesses a virtual address whose page is not present in memory, a page fault occurs. The page fault handler must decide whether this is a legal access that can be serviced or an illegal access that should terminate the process.

If the address is invalid, belongs to kernel space, violates permissions, or otherwise cannot be justified by the process's virtual memory metadata, the process should be killed.

If the address is valid but the page is missing from physical memory, Pintos must locate the contents that belong in that virtual page. Depending on the page, the data may come from an executable file, from a mapped file, from swap space, or from a zero-filled page.

After finding the source, Pintos allocates a physical page frame, loads the contents into that frame, updates the page table, and resumes the faulting instruction.

## Why a Supplemental Page Table Is Needed

The hardware page table only tells the processor how to translate a resident virtual page to a physical frame. It does not contain all the information Pintos needs for lazy loading and swapping.

For virtual memory, Pintos needs extra per-page metadata. The lecture calls this structure a virtual memory entry, or `vm_entry`.

This entry records information such as:

- The virtual page address or virtual page number.
- Whether the page is writable.
- The page type.
- The file and offset used to load a file-backed page.
- The number of bytes to read and the number of bytes to fill with zero.
- The swap slot, if an anonymous page has been swapped out.
- Whether the page is currently loaded.

The exact field names are an implementation choice, but the idea is that every meaningful virtual page needs metadata that survives even when the page is not resident in physical memory.

## Page Type Metadata

The lecture separates virtual pages into categories such as executable-backed pages, general file-backed pages, and anonymous pages.

Executable-backed pages need to be distinguished from ordinary file-backed pages because executable files are protected from modification while they are running. The implementation must respect permissions and avoid writing back inappropriate data to the executable image.

File-backed pages need a pointer to the file and an offset into that file. They also need enough information to handle the final partial page of a file, where fewer than one full page of valid bytes may exist and the rest must be zero-filled.

Anonymous pages need enough state to identify whether they are in memory or in swap, and if swapped out, where their contents are stored in swap space.

## Per-Process VM Entry Set

Each process needs a set of virtual memory entries. The lecture uses a hash table as the example structure, although a list, array, or other structure could also be used.

The process's `struct thread` is extended with a field that points to or contains the supplemental page table. In the lecture's example, this is a hash table containing the process's `vm_entry` objects.

When a process is created, the supplemental page table should be initialized. At that moment, there may be no entries yet, but the structure itself needs to be ready.

When a process exits, Pintos must destroy the process's virtual memory entries and release any resources associated with them.

## Lazy Loading an Executable

In the original Pintos loader, `load_segment` reads the executable data into physical memory and installs the corresponding page table mappings immediately.

With demand paging, this changes. Instead of allocating physical memory and reading all segment data at load time, Pintos creates virtual memory entries that describe how to load each page later.

For each page in the executable segment, the loader creates a `vm_entry`, initializes its file, offset, read size, zero size, writable flag, and type, then inserts it into the process's supplemental page table.

The actual physical page allocation and file read are deferred until a page fault occurs.

## Initial Stack Setup

The original stack setup allocates one physical page, maps it at the top of user virtual memory, and sets the stack pointer.

With virtual memory, the initial stack page also needs a corresponding virtual memory entry. Pintos still sets up the initial stack, but it records the page in the supplemental page table so later stack-related faults and cleanup can be handled consistently.

Later, stack growth will extend this idea by creating additional stack entries when valid stack accesses occur below the current stack.

## Page Fault Flow

The central control flow is:

- A process accesses memory.
- The hardware page table cannot translate the page as present.
- A page fault occurs.
- Pintos checks whether the faulting address is valid.
- Pintos searches the supplemental page table for the matching virtual memory entry.
- If the entry can be serviced, Pintos allocates a frame.
- Pintos loads the page contents from the proper source.
- Pintos installs the mapping in the page table.
- The faulting instruction restarts.

This is the core of demand paging.

## Modifying `page_fault`

In the original Pintos code, the page fault handler mainly kills the process. For virtual memory, this behavior is no longer sufficient.

The handler must first determine whether the fault is legal. If it is not legal, the process is terminated. If the fault is legal, the handler delegates the work to a memory-management fault routine. The lecture names this helper `handle_mm_fault`, but the exact function name is up to the implementation.

This helper is responsible for looking up the virtual memory entry, allocating a frame, loading data, and installing the page.

## Handling Executable-Backed Faults

The first version of `handle_mm_fault` can focus on executable-backed pages.

If the matching `vm_entry` says that the page comes from the executable file, Pintos allocates a physical page and reads the corresponding bytes from the file into that page. If the page is only partially backed by file data, the remaining bytes are cleared to zero.

After the frame is filled, Pintos calls the page installation function to connect the user virtual page to the kernel page frame with the proper writable permission.

If the page type is not one handled by the current version of the fault handler, the fault should fail. Later parts of the project extend this logic to anonymous pages, swapped pages, file-backed mappings, and stack growth.

## Loading a File Page

The file-loading helper needs three pieces of information: the destination physical frame, the file object, and the file offset and byte counts stored in the virtual memory entry.

The helper seeks or reads at the correct file offset, copies the requested bytes into the physical page, and zero-fills the rest of the page. This handles the common case where the final page of a segment contains fewer than `PGSIZE` bytes of actual file data.

The lecture emphasizes that forgetting the zero-fill portion is a common source of bugs.

## Suggested Helper Functions

The lecture suggests separating the supplemental page table work into helper functions.

Useful operations include initializing the supplemental page table, destroying it, finding a virtual memory entry by address, inserting an entry, deleting an entry, hashing an entry, comparing entries, and freeing an entry's memory.

Keeping these operations separate makes the page fault handler easier to read and makes process cleanup less error-prone.

## Existing Pintos Helpers

Several existing Pintos helper functions are important in this project.

`install_page` maps a user virtual page to a kernel page frame and sets the writable permission.

`palloc_get_page` allocates a page-sized physical frame, and `palloc_free_page` releases it back to the page allocator.

`malloc` and `free` are used for dynamic kernel objects such as virtual memory entries.

The lecture closes by emphasizing that virtual memory depends on combining these existing primitives with the new supplemental metadata and page fault logic.
