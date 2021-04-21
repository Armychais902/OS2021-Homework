#include "kernel/types.h"
#include "user/setjmp.h"
#include "user/threads.h"
#include "user/user.h"
#define NULL 0


static struct thread* current_thread = NULL;
static int id = 1;
static jmp_buf env_st;      // store sp for returning to main
//static jmp_buf env_tmp;

struct thread *thread_create(void (*f)(void *), void *arg){
    struct thread *t = (struct thread*) malloc(sizeof(struct thread));
    //unsigned long stack_p = 0;
    unsigned long new_stack_p;
    unsigned long new_stack;
    new_stack = (unsigned long) malloc(sizeof(unsigned long)*0x100);    // position cast to integer
    new_stack_p = new_stack +0x100*8-0x2*8;     // where does these calc from?
    t->fp = f;
    t->arg = arg;
    t->ID  = id;
    t->buf_set = 0;
    t->stack = (void*) new_stack;
    t->stack_p = (void*) new_stack_p;
    id++;
    return t;
}
void thread_add_runqueue(struct thread *t){
    if(current_thread == NULL){
        // TODO
        current_thread=t;
        t->previous=t;
        t->next=t;
    }
    else{
        // TODO
        current_thread->previous->next=t;
        t->previous=current_thread->previous;
        t->next=current_thread;
        current_thread->previous=t;
    }
}
void thread_yield(void){
    // TODO
    if(setjmp(current_thread->env)==0){
        schedule();
        dispatch();
    }
}
void dispatch(void){
    // TODO
    if(current_thread->buf_set==1){
        longjmp(current_thread->env,1);
    }
    else{
        if(setjmp(current_thread->env)==0){
            current_thread->env->sp=(unsigned long)current_thread->stack_p;
            current_thread->buf_set=1;
            longjmp(current_thread->env,1);
        }
        current_thread->fp(current_thread->arg);    //execute the targeted function
    }
    thread_exit();
}
void schedule(void){
    current_thread = current_thread->next;
}
void thread_exit(void){
    if(current_thread->next != current_thread){
        // TODO
        current_thread->next->previous=current_thread->previous;
        current_thread->previous->next=current_thread->next;
        struct thread* exit_thread=current_thread;
        current_thread=current_thread->next;
        free(exit_thread->stack);
        free(exit_thread);
        dispatch();
    }
    else{
        // TODO
        // Hint: No more thread to execute
        free(current_thread->stack);
        free(current_thread);
        longjmp(env_st,1);
    }
}
void thread_start_threading(void){
    // TODO
    // after the first thread is added to the runqueue
    if(setjmp(env_st)==0){
        schedule();     // don't call schedule() is okay
        dispatch();
    }
}
