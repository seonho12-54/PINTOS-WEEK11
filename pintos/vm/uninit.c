/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init, enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);

	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* Initalize the page on first fault */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;

	/* Fetch first, page_initialize may overwrite the values */
	vm_initializer *init = uninit->init;
	void *aux = uninit->aux;

	/* TODO: You may need to fix this function. */
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {


	struct uninit_page *uninit UNUSED = &page->uninit; //uninit 페이지가 가지고 있는 자원을 해제하는 함수입니다. 
	//spt에 등록된거를 이제 폴트 나서 다른거를 호출해서 타입을 바꿔야해서 기존에 등록된 아무것도 아닌 uninit을 해제하는 함수입니다.

	//만약에 init이 존재한다면 init이 가지고 있는 자원을 해제하는 작업을 해야할 수도 있습니다.
	if(uninit->init != NULL){
		uninit->init(page, uninit->aux); //만약 init이 존재한다면 init이 가지고 있는 자원을 해제하는 작업을 해야할 수도 있습니다.
		free(uninit->init); //그리고 언이닛 돼 있는걸 init으로 바꿔야합니다.
	}


	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */





}
