/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "hash.h"
#include "threads/vaddr.h"

static uint64_t page_hash (const struct hash_elem *e, void *aux);
static bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* 초기화기를 사용하여 대기 중인 페이지 객체를 생성합니다. 페이지를 생성하려면
 * 직접 생성하지 말고 이 함수나
 * `vm_alloc_page`를 통해 생성하십시오. */
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	ASSERT (VM_TYPE(type) != VM_UNINIT) //VM은 초기화 되지 않은 유닛이므로 초기화기를 이용하
										//여 페이지 객체를 생성할때는 VM_UNINIT이 아니어야 한다.
								
	struct supplemental_page_table *spt = &thread_current ()->spt; //현재 스레드의 보조 페이지 테이블을 가져옵니다.

/* 해당 페이지가 이미 할당되었는지 확인합니다. */
    if (spt_find_page (spt, upage) == NULL) {


		/* TODO: 페이지를 생성하고, VM 유형에 따라 초기화기를 가져온 다음,
	
         * TODO: uninit_new를 호출하여 “uninit” 페이지 구조체를 생성합니다.
		 
         * TODO: uninit_new 호출 후 해당 필드를 수정해야 합니다. */

        /* TODO: 페이지를 spt에 삽입합니다. */
		struct page *page = malloc(sizeof(struct page)); // 일단 페이지 메타데이터를 만듭니다.

		if (page == NULL) //만약 에러뜨면 goto 하는거여
			free(page);//에러가 뜨면 프리를 해버리는겨
			goto err;
		

		switch (type) {
					case VM_ANON: {
						uninit_new(page, upage, init, type, aux, anon_initializer);
						break;
					}
					case VM_FILE: {
						uninit_new(page, upage, init, type, aux, file_backed_initializer);
						break;
					}
					default:
						free(page);
						goto err;
				}		

		//페이지 구조체 밑에 bool writable을 바꾸어 줘야합니다. 
		page ->writable = writable; //페이지의 쓰기 가능 여부를 설정합니다.
			//writable이 참이면 VM_MARKER_0을 설정하고, 그렇지 않으면 0으로 설정합니다.
			
		//writable = writable ? VM_MARKER_0 : 0;

		
		/*
		supplemental_page_table_init(spt); //보조 페이지 테이블을 초기화합니다.
		*/
		
		if ( spt_insert_page(spt, page) ==false){ //페이지를 spt에 삽입합니다.
			free (page);
			goto err; 
		}


		return true;

	}
err:
	return false;
}





/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *page = NULL;

	struct page p;
	struct hash_elem *e;

	p.va = pg_round_down(va);
	e = hash_find(&spt->pages, &p.hash_elem);

	if (e != NULL) {
		page = hash_entry(e, struct page, hash_elem);
	}

	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	/* va가 존재하는지 확인
	 * pg_round_down()으로 규칙 공유
	 * 없으면 hash에 삽입하고 null pointer 반환(hash_insert)
	 * 있으면 hash 수정하지 않고 element 반환(hash_insert)
	 * 결과에 따라서 bool 값 반환(없어서 삽입했으면 false)
	*/

	bool succ = false;

	page->va = pg_round_down(page->va);
	struct hash_elem *e = hash_insert(&spt->pages, &page->hash_elem);

	if (e == NULL) {
		succ = true;
	}

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {

	/* 해시테이블에서 page 제거
	 * page 해제 경로로 이어지게 만들기
	*/

	hash_delete(&spt->pages, &page->hash_elem);
	vm_dealloc_page (page);
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	struct frame *victim = NULL;
	 /* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
  struct frame *frame_ = NULL;
  /* TODO: Fill this function. */
  frame_ = calloc(sizeof(struct frame), 1);
  if (frame_ == NULL) {
    PANIC ("calloc() fail: kernel heap shortage");
  }

  frame_->kva = palloc_get_page(PAL_USER);

  if (frame_->kva == NULL) {
    free(frame_);
    PANIC ("todo: eviction");
    /* Eviction은 나중에 추가 구현 필요 */
    /* if frame_->kva == NULL {frame table 순회하면서 victim 정하고 eviction} */
  }

  ASSERT (frame_ != NULL);
  ASSERT (frame_->page == NULL);
  return frame_;
}

/* Growing the stack. */
static void
vm_stack_growth (void *addr UNUSED) {
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */

	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
bool
vm_claim_page (void *va UNUSED) {
  struct page *page_ = NULL;
  /* TODO: Fill this function */
  page_ = spt_find_page (&(thread_current ()->spt), va);
  if (page_ != NULL) {
    return vm_do_claim_page (page_);
  }
  else {
    PANIC ("todo"); // stack growth 조건 만족하는지 이후 추가 구현 필요
  }
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page_) {
	struct frame *frame_ = vm_get_frame ();
	bool is_claimed = false;
	/* Set links */
	frame_->page = page_;
	page_->frame = frame_;
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	is_claimed = pml4_set_page (thread_current ()->pml4, page_->va, frame_->kva, page_->writable);

	if (is_claimed) {
		if (swap_in (page_, frame_->kva)) {
			return true;
		}
		goto cleanup;
	}
	cleanup:
		/* Link 초기화 */
		frame_->page = NULL;
		page_->frame = NULL;
		/* is_swapped_in 에서 실패했을 경우 mapping 제거 */
		if (is_claimed) {
			pml4_clear_page (thread_current ()->pml4, page_->va);
		}
		/* frame KVA 반환 */
		palloc_free_page (frame_->kva);
		free (frame_);
		return false;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	/* 빈 해시테이블 생성(페이지 집합을 담을 용도) 
	 * 한 프로세스 안에서 virtual page에는 한 struct page만 존재
	*/
	struct hash *pages = &spt->pages;
	hash_init(pages, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
}

static uint64_t page_hash(const struct hash_elem *e, void *aux UNUSED) {
	const struct page *page = hash_entry(e, struct page, hash_elem);

	return hash_bytes(&page->va, sizeof page->va);
}

static bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	const struct page *pa = hash_entry(a, struct page, hash_elem);
	const struct page *pb = hash_entry(b, struct page, hash_elem);

	return pa->va < pb->va;
}