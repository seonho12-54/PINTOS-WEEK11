/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "threads/mmu.h"
#include "hash.h"
#include "threads/vaddr.h"
#include "lib/string.h"
#include "userprog/process.h"

static uint64_t page_hash (const struct hash_elem *e, void *aux);
static bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);
static void *hash_destructor (struct hash_elem *e, void *aux);
bool check_valid_stack_growth(uintptr_t rsp, void * addr);

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

	/* 초기화기 경로에서는 실제 타입이 VM_UNINIT이면 안 된다. */
	ASSERT (VM_TYPE(type) != VM_UNINIT);

	struct supplemental_page_table *spt = &thread_current ()->spt;

	/* 같은 가상 페이지가 없을 때만 새 엔트리를 만든다. */
    if (spt_find_page (spt, upage) == NULL) {
		/* VM 타입에 맞는 uninit 페이지를 만들고 SPT에 등록한다. */
		struct page *page_ = malloc(sizeof(struct page));

		if (page_ == NULL) {
			/* 할당 실패 시 공통 오류 경로로 보낸다. */
			goto err;
		}
		
		int ty = VM_TYPE(type);

		switch (ty) {
					case VM_ANON: {
						uninit_new(page_, upage, init, type, aux, anon_initializer);
						break;
					}
					case VM_FILE: {
						uninit_new(page_, upage, init, type, aux, file_backed_initializer);
						break;
					}
					default:
						free(page_);
						goto err;
				}		
		/* fault 처리 시 권한 검사를 위해 쓰기 가능 여부를 기록한다. */
		page_ ->writable = writable;

		if ( spt_insert_page(spt, page_) == false){
			/* SPT 등록이 실패하면 메타데이터도 함께 정리한다. */
			free (page_);
			return false;
		}
		return true;
	}
err:
	return false;
}

/* SPT에서 VA에 대응하는 페이지를 찾고, 없으면 NULL을 반환한다. */
struct page *
spt_find_page (struct supplemental_page_table *spt, void *va) {
	struct page *page = NULL;

	struct page p;
	struct hash_elem *e;

	p.va = pg_round_down(va);  // 가상 주소를 시작 주소로 내림
	e = hash_find(&spt->pages, &p.hash_elem);

	if (e != NULL) {
		page = hash_entry(e, struct page, hash_elem);
	}

	return page;
}

/* 검사를 거친 뒤 PAGE를 SPT에 삽입한다. */
bool
spt_insert_page (struct supplemental_page_table *spt,
		struct page *page) {
	/* 같은 va가 없을 때만 새 페이지를 해시에 등록한다. */

	bool succ = false;

	page->va = pg_round_down(page->va);  // 가상 주소를 시작 주소로 내림
	struct hash_elem *e = hash_insert(&spt->pages, &page->hash_elem);

	if (e == NULL) {
		succ = true;
	}

	return succ;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	/* SPT에서 제거한 뒤 공통 페이지 해제 경로를 따른다. */
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

/* 사용자 풀에서 프레임을 확보한다.
 * 지금은 빈 페이지를 바로 받고, 풀이 가득 찬 경우는 이후 eviction 경로에서 처리한다.
 * 이 함수는 추후 eviction 구현 후 대부분의 상황에서 가용할 수 있는 frame을 반환하도록 한다.
 * frame 반환이 불가능한 경우는 kernel heap 메모리 부족 등 kernel panic 으로 구현했다.
 * */
static struct frame *
vm_get_frame (void) {
  struct frame *frame_ = NULL;
 
  frame_ = calloc(sizeof(struct frame), 1);
  if (frame_ == NULL) {
    PANIC ("calloc() fail: kernel heap shortage");
  }

  frame_->kva = palloc_get_page(PAL_USER);

  if (frame_->kva == NULL) {
    free(frame_);
    PANIC ("todo: eviction");
	/* TODO: frame table과 eviction 정책은 이후 단계에서 연결한다. */
    /* 사용자 풀 페이지가 없으면 이후 eviction 경로로 이어진다. */
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

bool
check_valid_stack_growth(uintptr_t rsp, void * addr) {
	uintptr_t addr_ptr = addr;
	uintptr_t lower_bound = rsp - 4000;
	uintptr_t stack_limit = USER_STACK - (1 << 20);
	bool cond_near_rsp = addr_ptr >= lower_bound;
	bool cond_below_rsp = addr_ptr < rsp;
	bool cond_above_stack_limit = addr_ptr >= stack_limit;
	bool ok = cond_near_rsp && cond_below_rsp && cond_above_stack_limit;
	return ok;
}

/* fault 처리가 성공하면 true를 반환한다. */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr ,
		bool user , bool write , bool not_present ) {
	struct supplemental_page_table *spt = &thread_current ()->spt;
	struct page *page = NULL;

	/* 잘못된 fault와 권한 위반은 먼저 걸러낸다. */
	if(addr == NULL || is_kernel_vaddr(addr)) { /* 커널 주소 fault는 사용자 페이지로 처리하지 않는다. */
		return false;
	}
	if(!not_present) { /* not-present fault만 여기서 처리한다. */
		return false;
	}

	/* TODO: Your code goes here */
	page = spt_find_page(spt, pg_round_down(addr));
	 // 커널 모드 / 유저 모드 어디에서 페이지 폴트가 났는지
	if(page == NULL) {
		/*
		커널 모드이면 thread의 rsp를 rsp에 넣어주면 되고
		유저 모드이면 인터럽트 프레임의 rsp를 rsp에 넣어주면 되지
		*/
		uintptr_t rsp;
		if(user){ // 유저 모드에서 폴트가 났다면...
			rsp = f->rsp;
		} else {
			rsp = thread_current()->rsp;
		}

		if(check_valid_stack_growth(rsp, addr)) {
			vm_stack_growth(addr);
			page = spt_find_page(spt, pg_round_down(addr));
		} else {
			return false;
		}

	}

	if(write && !page->writable) { /* 읽기 전용 페이지 쓰기는 거절한다. */
		return false;
	}

	return vm_do_claim_page (page);
	
}



/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* VA에 등록된 페이지를 실제 프레임과 연결한다. */
bool
vm_claim_page (void *va UNUSED) {
  struct page *page_ = NULL;
  /* 스택 성장 전에는 SPT에 등록된 페이지만 즉시 claim한다. */
  page_ = spt_find_page (&(thread_current ()->spt), va);
  if (page_ != NULL) {
    return vm_do_claim_page (page_);
  }
  else {
    PANIC ("todo"); /* 스택 성장 경로는 이후 단계에서 추가한다. */
  }
}

/* PAGE를 claim하고 MMU 매핑까지 설정한다. */
static bool
vm_do_claim_page (struct page *page_) {
	struct frame *frame_ = vm_get_frame ();
	bool is_claimed = false;
	/* frame과 page를 서로 연결한다. */
	frame_->page = page_;
	page_->frame = frame_;
	/* 사용자 가상 주소를 새 프레임에 매핑한다. */
	is_claimed = pml4_set_page (thread_current ()->pml4, page_->va, frame_->kva, page_->writable);

	if (is_claimed) {
		/* page type에 따라 호출되는 swap_in function이 다르고, 그 function에 대한 정의는 swap 구현 시 필요 */
		if (swap_in (page_, frame_->kva)) {
			return true;
		}
		goto cleanup;
	}
	cleanup:
		/* 실패 시 양방향 링크를 되돌린다. */
		frame_->page = NULL;
		page_->frame = NULL;
		/* 매핑이 만들어졌다면 함께 정리한다. */
		if (is_claimed) {
			pml4_clear_page (thread_current ()->pml4, page_->va);
		}
		/* vm_get_frame() 내부에서 확보한 사용자 프레임을 반환한다. */
		palloc_free_page (frame_->kva);
		free (frame_);
		return false;
}

/* 새 supplemental page table을 초기화한다. */
void
supplemental_page_table_init (struct supplemental_page_table *spt) {
	/* 프로세스별 가상 페이지 집합을 해시로 관리한다. */
	struct hash *pages = &spt->pages;
	hash_init(pages, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst,
		struct supplemental_page_table *src) {
		struct hash_iterator i;

		hash_first(&i, &src->pages);
		while (hash_next(&i))
		{	
			/* 부모 프로세스 정보만 가져와서 uninit으로 다 새로 만들어줘야함.*/
			struct page *fp = hash_entry(hash_cur(&i), struct page, hash_elem);
			enum vm_type ty = fp->operations->type; // 이게 현재 페이지의 타입
			enum vm_type target_ty = page_get_type(fp); // UNINIT일 경우 이건 타겟 타입
			
			/* 부모의 SPT에 있는 UNINIT ANON FILE 페이지들을 전부 복사 */
			/* UNINIT은 vm_alloc_page_with_initializer랑 lazy_load_segment로 만들어줘야하지 않나? 그러면*/
			
			if (VM_TYPE(ty) == VM_UNINIT) {
				struct lazy_load_args *aux = malloc(sizeof *aux); //
				*aux = *((struct lazy_load_args *)fp->uninit.aux); //
				if (!aux || !vm_alloc_page_with_initializer(target_ty, fp->va, fp->writable, fp->uninit.init, aux)) {
					free(aux);
					return false;
				}
			}
			else {
				if (!vm_alloc_page(ty, fp->va, fp->writable)) {
					return false;
				}
				vm_claim_page(fp->va); 

				if(fp->frame != NULL) {
				/* UNINIT이 아닌 경우 즉시 claim */
				struct page *p = spt_find_page(dst, fp->va);
				memcpy(p->frame->kva, fp->frame->kva, PGSIZE); //
				}
			}

			/* 고민 1. vm_alloc_page_with_initializer의 4번째 인자에 도대체 뭐가 들어가야하냐? -> 필요 없어 */
			/* 고민 2. 고민 1이 해결이 되면, page가 할당되고 spt에도 들어가는데, 여기에 src의 페이지들을 어떻게 복사해주냐? */

		}
		return true;
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt) {
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	if (spt == NULL) {
		return;
	}

	hash_destroy(&spt->pages, hash_destructor); /* DESTRUCTOR may, if appropriate, deallocate the memory used by the hash element.*/

	spt->pages.hash = NULL;
	spt->pages.less = NULL;
}

/* 해시값의 가상 주소를 해시값으로 바꿔서 SPT 해시 테이블에서 찾기 쉽게 만듦 */
static uint64_t page_hash(const struct hash_elem *e, void *aux UNUSED) {
	const struct page *page = hash_entry(e, struct page, hash_elem);

	return hash_bytes(&page->va, sizeof page->va);
}

/* va 크기 순서로 비교해서 같은 해시 테이블 안에서 정렬 */
static bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
	const struct page *pa = hash_entry(a, struct page, hash_elem);
	const struct page *pb = hash_entry(b, struct page, hash_elem);

	return pa->va < pb->va;
}

/* destructor 구현 필요*/

/* Performs some operation on hash element E, given auxiliary
 * data AUX. */
static void *hash_destructor (struct hash_elem *e, void *aux) {
	// 1. hash_elem page_entry로 page SPT에서 지우기 -> 이거는 hash_clear 에서 list_pop_front로 bucket에서 제거되므로, 괜찮을듯?
	// 2. page free or dealloc
	if (e == NULL) {
		return;
	}

	struct page *p = hash_entry(e, struct page, hash_elem);
	vm_dealloc_page(p);
}
