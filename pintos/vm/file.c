/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"
#include "threads/mmu.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
void delete_resources_by_munmap(struct supplemental_page_table *spt, struct page * page_, void *first);
/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type UNUSED, void *kva UNUSED) {
	struct file_page *aux = page->uninit.aux;

	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	*file_page = *aux;
	free (aux);

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page  = &page->file;
	file_backed_swap_out (page);
	pml4_clear_page(thread_current ()->pml4, page->va);
	file_close (file_page->file);
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

}
void
delete_resources_by_munmap(struct supplemental_page_table *spt, struct page * page_, void *first) {
	spt_remove_page(spt,page_);
	pml4_clear_page(thread_current()->pml4, first);
}
/* Do the munmap */
void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page * page_ = spt_find_page(spt, addr);
	if(page_ == NULL) {
		return;
	}
	void * first = page_->file.addr;
	void * first_dup = first;
	page_ = spt_find_page(spt, first);
	if(page_ == NULL) {
		return;
	}
	delete_resources_by_munmap(spt, page_, first);
	first += PGSIZE;
	struct page * page2 = spt_find_page(spt, first);
	if(page2 == NULL) {
		return;
	}
	while(page2->file.addr != first_dup) {
		delete_resources_by_munmap(spt, page2, first);
		first += PGSIZE;
		page2 = spt_find_page(spt, first);
		if(page2 == NULL) {
			return;
		}
	}
}
