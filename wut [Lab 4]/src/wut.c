#include "wut.h"

#include <assert.h> // assert
#include <errno.h> // errno
#include <stddef.h> // NULL
#include <stdio.h> // perror
#include <stdlib.h> // reallocarray
#include <sys/mman.h> // mmap, munmap
#include <sys/signal.h> // SIGSTKSZ
#include <sys/queue.h> // TAILQ_*
#include <sys/ucontext.h>
#include <ucontext.h> // getcontext, makecontext, setcontext, swapcontext
#include <valgrind/valgrind.h> // VALGRIND_STACK_REGISTER

#define SIGSTKSZ 8192

typedef struct list_entry {
    ucontext_t context;
    char* stack;
    int id;
    int status;
    TAILQ_ENTRY(list_entry) pointers;
} list_entry;

TAILQ_HEAD(queue, list_entry);
static struct queue queue;

static int threadcount = 0;
static int threadid = 0;

static list_entry** library;

static void die(const char* message) {
    int err = errno;
    perror(message);
    exit(err);
}

static char* new_stack(void) {
    char* stack = mmap(
        NULL,
        SIGSTKSZ,
        PROT_READ | PROT_WRITE | PROT_EXEC,
        MAP_ANONYMOUS | MAP_PRIVATE,
        -1,
        0
    );
    if (stack == MAP_FAILED) {
        die("mmap stack failed");
    }
    VALGRIND_STACK_REGISTER(stack, stack + SIGSTKSZ);
    return stack;
}

static void delete_stack(char* stack) {
    if (munmap(stack, SIGSTKSZ) == -1) {
        die("munmap stack failed");
    }
}

void wut_init() {
    TAILQ_INIT(&queue);
    assert(TAILQ_EMPTY(&queue));

    library = (list_entry**)malloc(2 * sizeof(list_entry*));

    list_entry* mainthread = (list_entry*)malloc(sizeof(list_entry));
    mainthread->id = threadcount;
    mainthread->status = -1;
    TAILQ_INSERT_TAIL(&queue, mainthread, pointers); 

    getcontext(&mainthread->context);
    library[threadcount] = mainthread;
    threadcount++;
}

int wut_id() {
    return TAILQ_FIRST(&queue)->id;
}

int wut_create(void (*run)(void)) {
    list_entry* new_thread = (list_entry*)malloc(sizeof(list_entry));

    int first = -1;
    for (int i = 0; i < threadcount; i++) {
        if (library[i] == NULL) {
            first = i;
            break;
        }
    }

    if (first == -1) {
        first = threadcount;
        threadcount++;
        library = (list_entry**)realloc(library, (threadcount+1)*sizeof(list_entry*));
    }

    getcontext(&new_thread->context);
    new_thread->id = first;
    new_thread->stack = new_stack();
    new_thread->status = -1;
    new_thread->context.uc_stack.ss_sp = new_thread->stack;
    new_thread->context.uc_stack.ss_size = SIGSTKSZ;

    list_entry* run_and_exit = (list_entry*)malloc(sizeof(list_entry));

    run_and_exit->stack = new_stack();

    getcontext(&run_and_exit->context);

    run_and_exit->context.uc_stack.ss_sp = run_and_exit->stack;
    run_and_exit->context.uc_stack.ss_size = SIGSTKSZ;

    makecontext(&run_and_exit->context, (void (*)(void))wut_exit, 0);

    new_thread->context.uc_link = &run_and_exit->context;

    makecontext(&new_thread->context, run, 0);

    library[first] = new_thread;
    TAILQ_INSERT_TAIL(&queue, new_thread, pointers); 

    return new_thread->id;
}

int wut_cancel(int id) {
    int currrent_id = TAILQ_FIRST(&queue)->id;
    if (id == currrent_id) {
        return -1;
    }

    struct list_entry* e;

    TAILQ_FOREACH(e, &queue, pointers) {
        if (e->id == id) {    
            TAILQ_REMOVE(&queue, e, pointers);

            library[id]->status = 128;
            delete_stack(library[id]->context.uc_stack.ss_sp);

            return 0;
        }
    }

    return -1;
}

int wut_join(int id) {

    int curr_id = TAILQ_FIRST(&queue)->id;
    if (curr_id == id) {
        return -1;
    }

    list_entry* target = library[id];

    if (target == NULL) return -1;
    if (target->status == 128) return -1;

    while (target->status == -1) {
        if (wut_yield() == -1) return -1;
    }

    wut_exit(0);
    TAILQ_INSERT_TAIL(&queue, library[threadid], pointers);
    library[id] = NULL;

    return 0;
}

int wut_yield() {
    if(TAILQ_NEXT(TAILQ_FIRST(&queue), pointers) == NULL) {
        return -1;
    }
    list_entry* curr = library[threadid];
    list_entry* next = TAILQ_NEXT(curr, pointers);

    TAILQ_REMOVE(&queue, curr, pointers);
    TAILQ_INSERT_TAIL(&queue, curr, pointers);

    threadid = next->id;
    getcontext(&curr->context);
    swapcontext(&curr->context, &next->context);

    return 0;
}

void wut_exit(int status) {
    if (status < 0 || status > 255) status &= 0xFF;
    list_entry* curr = library[threadid];

    curr->status = status;

    list_entry* next = TAILQ_NEXT(curr, pointers);

    if (next && next != curr) {
        threadid = next->id;
        setcontext(&library[threadid]->context);

        return;
    } 

    if (!next) curr->status = 0;

    TAILQ_REMOVE(&queue, curr, pointers);
}
