#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <hash.h>
#include "fixed-point.h"
#include "synch.h"
#include "filesys/file.h"
#include "vm/page.h"
#include "vm/mmap.h"


/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

#define PRI_NESTING_MAX_DEPTH 8

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */

struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Base Priority. */
    int effective_priority;             /* Effective Priority*/
    struct list_elem allelem;           /* List element for all threads list. */
    struct list donations;              /* List of donating threads*/
    struct lock *lock_waiting;          /* Lock that thread is waiting on*/
    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */
    /* BSD Values*/ 
    int nice;                           /* Niceness of a thread*/
    int32_t recent_cpu;                 /* Estimate of CPU time the thread has
                                           used recently. (In FP)*/

#ifdef USERPROG
    struct list children_status;
    struct process_exit_status *exit_status;

    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    struct semaphore sema_execute;      /* Synchronization in process_execute */
    struct file* exec_file;             /* Process is using this executable file */
    struct list opened_files;           /* A list of files opened by the thread*/
    struct list memory_mapped_files;    /* List of Memory Mapped Files*/
#endif
    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
    struct hash spt;                    /* Supplemental Page Table*/
  };

/* Used to store a thread in a list*/
struct thread_elem {
   struct list_elem elem; /* list_elem for storing the thread in a list*/
   struct thread *thread; /* pointer to the thread that is being stored in the list*/
};

struct file_wrapper {
    struct file *file;
    struct list_elem file_elem;
    int fd;
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);
size_t threads_ready(void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

// BSD functions
void calculate_thread_priority(struct thread *t, void *aux UNUSED);
void calculate_load_avg(void);
void calculate_recent_cpu(struct thread *t, void *aux UNUSED);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

// Helper Functions
void thread_update_effective_priority(struct thread *t);
void thread_update_effective_priority_no_yield(struct thread *t);
struct list_elem *list_remove_max(struct list *list, list_less_func *less_func);
bool thread_prio_list_less(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
bool thread_elem_prio_list_less(const struct list_elem *a, const struct list_elem *b, void *aux UNUSED);
struct thread *get_thread_by_tid (tid_t tid);

#endif /* threads/thread.h */
