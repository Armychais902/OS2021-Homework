#ifndef THREADS_H_
#define THREADS_H_

// TODO: necessary includes, if any
#include "user/setjmp.h"
// TODO: necessary defines, if any


struct thread {
    void (*fp)(void *arg);  // function pointer
    void *arg;              // function parameters
    void *stack;            // allocated stack for thread
    void *stack_p;          // stack pointer
    jmp_buf env;
    int ID;
    int buf_set;            //1: indicate jmp_buf has been set, 0: indicate jmp_buf not set
    struct thread *previous;
    struct thread *next;
};

struct thread *thread_create(void (*f)(void *), void *arg);
void thread_add_runqueue(struct thread *t);
void thread_yield(void);
void dispatch(void);
void schedule(void);
void thread_exit(void);
void thread_start_threading(void);

#endif // THREADS_H_
