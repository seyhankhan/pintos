#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "threads/malloc.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static void pass_args_and_setup_stack(char **argv, int argc, struct intr_frame *if_);

#define NULL_BYTE_SIZE 1;

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy, *mod_fn;
  char *file_name_only, *save_ptr;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a modifiable file name so that we can run strtok_r while
     Passing the full command to start_process */
  mod_fn = palloc_get_page (0);
  if (mod_fn == NULL)
    return TID_ERROR;
  strlcpy (mod_fn, file_name, PGSIZE);

  // Extract Filename and pass it as the thread name
  file_name_only = strtok_r (mod_fn, " ", &save_ptr);
  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name_only, PRI_DEFAULT, start_process, fn_copy);
  palloc_free_page(mod_fn);

  // If thread wasn't created successfully free fn_copy
  // TODO: Find out where it's supposed to be freed
  if (tid == TID_ERROR) {
    palloc_free_page (fn_copy); 
  }
  // Sema down on the thread that was just created
  sema_down(&get_thread_by_tid(tid)->sema_execute);

  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  struct thread *curr = thread_current();
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  char *token, *save_ptr, **argv;
  int argc = 0;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  /*Splits commandline arguments into an array of strings*/
  // Do a check if mem allocation was successful
  argv = palloc_get_page(0);
  for (token = strtok_r (file_name, " ", &save_ptr); token != NULL; token = strtok_r (NULL, " ", &save_ptr)) {
    argv[argc] = token;
    ++argc;
  }
  argv[argc] = (char *) 0;

  success = load (argv[0], &if_.eip, &if_.esp);


  if(success){
    pass_args_and_setup_stack(argv, argc, &if_);
    // If file currently running, deny write to executable
    // Lock file system and release lock
    struct file *file = filesys_open(file_name);
    curr->exec_file = file;
    file_deny_write(file);
    sema_up (&curr->sema_execute);  
  } else {
    curr->exit_status->exit_code = -1;
    sema_up (&curr->sema_execute); 
    thread_exit ();
  }
  

  /* If load failed, quit. */
  palloc_free_page (file_name);  

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}


static void
pass_args_and_setup_stack(char **argv, int argc, struct intr_frame *if_) {
  /* Push Arguments to the stack frame - order doesn't matter for now 
     as they will be referenced by pointers*/
  void *ptr_arr[argc];
  for (int i = argc - 1; i >= 0; i--) {
    // Need to keep in mind the null byte at the end of the string
    size_t token_length = strlen (argv[i]) + NULL_BYTE_SIZE;
    // Decrement stackpointer to fit length of string + null byte
    if_->esp = (void *) (((char*) if_->esp) - token_length);
    // Note: strlcpy automatically ends token with null byte so no need to append
    strlcpy ((char*)if_->esp, argv[i], token_length);
    // Store the pointer of the argument that was just pushed on to the stack
    ptr_arr[i] = if_->esp;
    // TODO: Remove Debug Print statement so tests pass
    // printf("Stack pointer: %p %s\n", if_->esp, (char*)if_->esp);
  }

  /* Round stack pointer down to a multiple of 4 before pushing pointers
      for better performance (word-aligned access)*/
  if_->esp -= (unsigned) if_->esp % 4;  
  // printf("Stack pointer: %p Word Align\n", if_->esp);
  // decrement stack pointer by size of sentinel
  if_->esp = (((void**) if_->esp) - 1);
  // push null pointer sentinel onto stack
  *((void**)(if_->esp)) = NULL;
  // printf("Stack pointer: %p Null Pointer sentinel, %p\n", if_->esp, *((void**)(if_->esp)));

  // Push pointers to arguments in reverse order
  for (int i = argc - 1; i >= 0; i--) {
    if_->esp = (((void**) if_->esp) - 1);
    memcpy(if_->esp, &ptr_arr[i], sizeof(ptr_arr[i]));
    // printf("Stack pointer: %p val: %p\n", if_->esp, *((void**) if_->esp));
  }

  // Push address of argv[0]
  // Stack Pointer currently pointing towards argv[0]
  void *addr_argv = if_->esp;
  //1. decrement stack pointer by size of pointer
  if_->esp = (((void**) if_->esp) - 1);
  //2. push pointer to base of argv on stack
  memcpy(if_->esp, &addr_argv , sizeof(addr_argv));
  // printf("Stack pointer: %p val: %p\n", ((void**) if_->esp), *((void**) if_->esp));

  /*Push argc
    1. decrement stack pointer by size of argc
    2. push argc to stack */
  if_->esp -= sizeof(argc);
  memcpy(if_->esp, &argc, sizeof(argc));
  // printf("Stack pointer: %p argc: %i\n", if_->esp, *(int*)if_->esp);

  /*Make space for fake return address, and push it (NULL), 
    so that stack frame has same structure as any other
    1. decrement stack pointer to the next address
    2. push fake "return address" (null)*/
  if_->esp = (((void**) if_->esp) - 1);
  // memcpy doesn't work here
  *((void**)(if_->esp)) = NULL;
  // printf("Stack pointer: %p Fake Return Address %i\n", if_->esp, *(int*)if_->esp);

  // hex_dump((uintptr_t) if_->esp, if_->esp, PHYS_BASE - if_->esp, true);
  // Free up memory
  palloc_free_page(argv);
}

/* Waits for thread TID to die and returns its exit status. 
 * If it was terminated by the kernel (i.e. killed due to an exception), 
 * returns -1.  
 * If TID is invalid or if it was not a child of the calling process, or if 
 * process_wait() has already been successfully called for the given TID, 
 * returns -1 immediately, without waiting.
 * 
 * This function will be implemented in task 2.
 * For now, it does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  //if pid_t not in current_thread()->children return -1
  //if child pid_t was terminated by the kernel, return -1. maybe against exited
  //if child pid_t has already been waited on return -1
  struct thread *cur = thread_current();
  int exit_code;
  struct list_elem *e;
  struct process_exit_status *status = NULL;
  for (e = list_begin(&cur->children_status); e != list_end(&cur->children_status); e = list_next(e)) {
    status = list_entry(e, struct process_exit_status, elem);
    if (status->child_pid == child_tid) {
      break;
    }
  }
  if (status == NULL) {
    return -1;
  }

  sema_down(&status->sema);
  
  //while (!status->exited) {}
  exit_code = status->exit_code;
  
  /*A process should only be able to wait on another once*/
  list_remove(&status->elem);
  free(status);
  return exit_code;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  printf ("%s: exit(%d)\n", cur->name, cur->exit_status->exit_code);
  dec_ref_count(cur->exit_status);

  struct process_exit_status *status;
  for (struct list_elem *e = list_begin(&cur->children_status); e != list_end(&cur->children_status); e = list_next(e)) {
    status = list_entry(e, struct process_exit_status, elem);
    dec_ref_count(status);
    list_remove(e);
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  // Tests multi-oom - tp stop kernel panick do NULL check
  if (cur->exec_file != NULL) {
    file_allow_write(cur->exec_file);
  }
  
  sema_up(&cur->exit_status->sema);
}

void dec_ref_count(struct process_exit_status *exit_status) {
  lock_acquire(&exit_status->lock);
  exit_status->ref_count--;
  if (exit_status->ref_count == 0) {
    free(exit_status);
  } else {
    lock_release(&exit_status->lock);
  }
}


/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      
      /* Check if virtual page already allocated */
      struct thread *t = thread_current ();
      uint8_t *kpage = pagedir_get_page (t->pagedir, upage);
      
      if (kpage == NULL){
        
        /* Get a new page of memory. */
        kpage = palloc_get_page (PAL_USER);
        if (kpage == NULL){
          return false;
        }
        
        /* Add the page to the process's address space. */
        if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }     
        
      } else {
        
        /* Check if writable flag for the page should be updated */
        if(writable && !pagedir_is_writable(t->pagedir, upage)){
          pagedir_set_writable(t->pagedir, upage, writable); 
        }
        
      }

      /* Load data into the page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes){
        return false; 
      }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE - 12;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
