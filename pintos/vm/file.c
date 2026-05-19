/* file.c: mmap 파일 기반 page를 처리한다. */

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
static void * get_mapping_addr (struct page *page_);

/* 이 구조체는 수정하지 않는다. */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* 파일 기반 page 하위 시스템을 초기화한다. */
void
vm_file_init (void) {
}

/* uninit page를 파일 기반 page로 초기화한다. */
bool
file_backed_initializer (struct page *page, enum vm_type type UNUSED, void *kva UNUSED) {
	struct file_page *aux = page->uninit.aux;

	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	*file_page = *aux;

	return true;
}

/* 파일 기반 page의 내용을 파일에서 프레임으로 읽어온다. */
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

/* dirty 파일 기반 page를 매핑된 파일에 되쓴다. */
static bool
file_backed_swap_out (struct page *page) {
	uint64_t *pml4 = thread_current()->pml4;
	struct file_page *file_page = &page->file;

	ASSERT(page != NULL);
    ASSERT(page->frame != NULL);
    ASSERT(file_page->file != NULL);

	if (pml4_is_dirty(pml4, page->va)) {
		/* 파일에 대응되는 read_bytes만 되쓴다. */
		lock_acquire(&filesys_lock);
		off_t bytes = file_write_at(file_page->file, page->frame->kva, file_page->read_bytes, file_page->ofs);
		lock_release(&filesys_lock);

		if (file_page->read_bytes != (uint32_t)bytes) {
			return false;
		}

		pml4_set_dirty(pml4, page->va, false);
	}

	pml4_clear_page(pml4, page->va);
	page->frame->page = NULL;
	page->frame = NULL;
	return true;
}

/* 파일 기반 page를 정리하고 재오픈한 파일을 닫는다. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page = &page->file;
	uint64_t *pml4 = thread_current()->pml4;

	if (pml4_is_dirty(pml4, page->va) && page->frame != NULL) {
		if (!file_backed_swap_out(page)) {
			pml4_clear_page(pml4, page->va);
 			page->frame->page = NULL;
 			page->frame = NULL;

			file_close(file_page->file);
			return;
		}
		file_close(file_page->file);
		return;
	}

	pml4_clear_page(pml4, page->va);
	if (page->frame != NULL) {
		page->frame->page = NULL;
		page->frame = NULL;
	}

	file_close(file_page->file);
}

/* mmap 범위의 파일 기반 page들을 지연 로딩 page로 등록한다. */
void *
do_mmap (void *addr, size_t length, int writable, struct file *file, off_t offset) {
	void *start = addr;
	size_t len = length;
	off_t file_left = file_length(file) - offset;

	while (len > 0) {
		size_t page_total_bytes = len < PGSIZE ? len : PGSIZE;
		size_t page_read_bytes = page_total_bytes;

		if (file_left <= 0) {
			page_read_bytes = 0;
		}
		else if (file_left < (off_t)page_read_bytes) {
			page_read_bytes = file_left;
		}
		
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* 각 page는 독립 file 참조와 offset 정보를 가진다. */
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
			file_close(aux->file);
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

/* mmap page 하나를 SPT와 pml4에서 제거한다. */
void
delete_resources_by_munmap(struct supplemental_page_table *spt, struct page * page_, void *first) {
	spt_remove_page(spt,page_);
	pml4_clear_page(thread_current()->pml4, first);
}

/* 같은 mmap 범위에 속한 page들을 모두 해제한다. */

void
do_munmap (void *addr) {
	struct supplemental_page_table *spt = &thread_current()->spt;
	struct page * page_ = spt_find_page(spt, addr);

	if(page_ == NULL) {
		return;
	}

	void * first = get_mapping_addr(page_);
	if (!first) {
		return;
	}
	void * va = first;

	while (true) {
		struct page *mapped = spt_find_page(spt, va);
		if (mapped == NULL || get_mapping_addr(mapped) != first) {
			return;
		}
		delete_resources_by_munmap(spt, mapped, va);
		va += PGSIZE;
	}
}

/* 지연 로딩 파일 기반 page의 첫 page fault를 처리한다. */
bool file_lazy_load (struct page *page, void *aux) {
	if(page == NULL || page->frame == NULL) {
		return false;
	}

	if (!file_backed_swap_in(page, page->frame->kva)) {
		return false;
	}
	return true;
}

/* page 하나를 받아 해당 페이지의 mapping address를 반환한다. */
static void * get_mapping_addr (struct page *page_) {
	if(page_ == NULL) {
		return NULL;
	}

	enum vm_type ty = page_->operations->type;

	if (VM_TYPE(ty) == VM_UNINIT && page_get_type(page_) == VM_FILE) {
		return ((struct file_page *) page_->uninit.aux)->addr;
	}
	else if (VM_TYPE(ty) == VM_FILE) {
		return page_->file.addr;
	}

	return NULL;
}
