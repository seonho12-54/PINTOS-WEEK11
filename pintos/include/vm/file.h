#ifndef VM_FILE_H
#define VM_FILE_H
#include "filesys/file.h"
#include "vm/vm.h"

struct page;
enum vm_type;

struct file_page {
	off_t ofs;
	struct file *file;  /* 이 매핑을 뒷받침하는 재오픈 파일. */
	uint32_t read_bytes;
	uint32_t zero_bytes;
	bool writable;
	void * addr;
	bool dirty;
};

void vm_file_init (void);
bool file_backed_initializer (struct page *page, enum vm_type type, void *kva);
void *do_mmap(void *addr, size_t length, int writable,
		struct file *file, off_t offset);
void do_munmap (void *va);
bool file_lazy_load (struct page *page, void *aux);
#endif
