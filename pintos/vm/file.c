/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "threads/malloc.h"
#include "threads/mmu.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/syscall.h"

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);
void delete_resources_by_munmap(struct supplemental_page_table *spt, struct page * page_, void *first);
static bool file_lazy_load (struct page *page, void *aux);

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

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page = &page->file;

	lock_acquire(&filesys_lock);
	if(file_page->read_bytes != file_read_at(file_page->file, kva, file_page->read_bytes, file_page->ofs)) {
		lock_release(&filesys_lock);
		return false;
	} 
	lock_release(&filesys_lock);

	memset((uint8_t *) (kva) + file_page->read_bytes, 0, file_page->zero_bytes);
	return true;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	/* 수정된 적 있으면
	 * read_bytes만큼만 다시 쓰기
	 * pml4 에서 매핑 제거
	*/

	uint64_t pml4 = thread_current()->pml4;
	struct file_page *file_page = &page->file;

	if (pml4_is_dirty(pml4, page->va)) {
		if (file_page->read_bytes != file_write_at(file_page->file, page->frame->kva, file_page->read_bytes, file_page->ofs)) {
			return false;
		}
		
		pml4_set_dirty(pml4, page->va, false);
	}

	pml4_clear_page(pml4, page->va);
	if (page->frame != NULL) {
		page->frame = NULL;
	}
	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page  = &page->file;
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	//0. file은 reopen해서 복사
	//1. offset 위치에서 length만큼 바이트를 addr에 매핑
	//2. spt에 파일정보를 등록
	//3. 성공시 addr 반환, 실패 시 NULL 반환
	// read_bytes, zero_bytes가 필요하다.
	
	void *start = addr;
	size_t len = length;
	off_t file_left = file_length(file) - offset;
	
	while (len > 0) {
		size_t page_total_bytes = len < PGSIZE ? len : PGSIZE;
		size_t page_read_bytes = page_total_bytes;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		if (file_left <= 0) {
			page_read_bytes = 0;
		} 
		else if (file_left < (off_t)page_read_bytes) {
			page_read_bytes = file_left;
		}

		struct file_page *aux = malloc(sizeof(struct file_page));
		if (aux == NULL) {
			return NULL;
		}

		aux->ofs = offset;
		aux->file = file_reopen(file);
		if (aux->file == NULL) {
			free(aux);
			return NULL;
		}

		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;
		aux->writable = writable;
		aux->addr = start;
		aux->dirty = false;

		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, file_lazy_load, aux)) {
			free(aux);
			return NULL;
		}

		len -= page_total_bytes;
		offset += page_read_bytes;
		file_left -= page_read_bytes;
		addr += PGSIZE;
	}

	return start;

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
	void * va = first;

	while (true) {
		struct page *mapped = spt_find_page(spt, va);
		if (mapped == NULL || mapped->file.addr != first) {
			break;
		}
		spt_remove_page(spt, mapped);
		va += PGSIZE;
	}
}

static bool
file_lazy_load (struct page *page, void *aux) {
	/* 첫 page fault 시 파일 내용을 프레임에 채운다. */
	if(page == NULL || page->frame == NULL) {
		return false;
	}

	struct file_page *lazy_load_args_ = (struct file_page*) aux;
	if(lazy_load_args_ == NULL) {
		return false;
	}

	struct file *file_ = lazy_load_args_->file;
	lock_acquire(&filesys_lock);
	file_seek(file_, lazy_load_args_->ofs);
	lock_release(&filesys_lock);
	void * kva_ = page->frame->kva;
	off_t read_bytes_ =  lazy_load_args_->read_bytes;
	off_t zero_bytes_ = lazy_load_args_->zero_bytes;
	lock_acquire(&filesys_lock);
	if(read_bytes_ != file_read(file_, kva_, read_bytes_)) {
		lock_release(&filesys_lock);
		free(lazy_load_args_);
		return false;
	} 
	lock_release(&filesys_lock);
	/* 파일에서 읽지 않은 나머지 바이트는 0으로 채운다. */
	memset((uint8_t *) (page->frame->kva) + read_bytes_, 0, zero_bytes_);
	free(lazy_load_args_);
	return true;
}
