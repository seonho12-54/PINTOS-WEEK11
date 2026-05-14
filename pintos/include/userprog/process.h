#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

// 최대 인자 수 정의
#define ARG_MAX 128

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/* 지연 로딩 시 페이지 초기화에 필요한 파일 정보 묶음. */
struct lazy_load_args {
	off_t ofs;
	struct file *file;
	uint32_t read_bytes;
	uint32_t zero_bytes; 
	bool writable;
};

#endif /* userprog/process.h */


