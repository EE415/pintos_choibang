#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of 64 priorities for mlfqs and load avg */
static struct list mlfqs_list[64];
static int load_avg = 0;

/* List of all threads. Added when thread is first created
 * and removed when the thread is removed */
static struct list all_threads;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

// for fixed point operation. p=17, q=14 then f = 1<<q for expressing float
int f_value = 1<<14;

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&all_threads);
  int i;
  /* if running mlfqs, initiate mlfqs_list 
   else initiate ready_list*/
  
  if (!thread_mlfqs) 
    list_init (&ready_list);
  else
  {
    for (i=0;i<64;i++)
      list_init (&mlfqs_list[i]);
  }

  /*nice and recent_cpu are zero for the initial_thread*/
  initial_thread->nice = 0;
  initial_thread->recent_cpu = 0;

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  struct thread *curr = thread_current ();

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Get nice and recent_cpu values from parents' thread */
  t->nice = curr->nice;
  t->recent_cpu = curr->recent_cpu;
  
  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;

  /* Add to run queue. */
  thread_unblock (t);

  /* When the thread is first created, yields to the new one if 
 * it has higher priority than the current thread */
  if (priority > thread_current ()->priority)
    thread_yield ();

  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  
  /*if mlfqs , we push back thread elem to the mlfqs list 
   else , we push thread elem to the ready list */
  if (!thread_mlfqs)
    list_insert_ordered(&ready_list, &t->elem, list_higher_priority, NULL);
  else
    list_push_back(&mlfqs_list[t->priority], &t->elem);  

  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Just set our status to dying and schedule another process.
     We will be destroyed during the call to schedule_tail(). */
  intr_disable ();

  /* we have to remove the elem which thread status is THREAD_DYING*/
  list_remove(&thread_current() -> all_elem);

  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  /*consider the interrupt. Do intrrupt disabled.*/
  struct thread *curr = thread_current ();
  enum intr_level old_level;
  ASSERT (!intr_context ());
  old_level = intr_disable ();
  
  /* insert the thread to ready_list(RR) or mlfqs_list(MLFQs)  */
  if (curr != idle_thread) 
  {
    if (!thread_mlfqs)
      list_insert_ordered(&ready_list, &curr->elem, list_higher_priority, NULL); 
    else
      list_push_back (&mlfqs_list[curr->priority], &curr->elem);
  }

  curr->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  enum intr_level old_level;
  old_level = intr_disable();
  /*do not activate the function when MLFQ*/
  if (!thread_mlfqs)
  {
    if (new_priority < thread_current ()->priority)
    {
      if (!list_empty(&thread_current()->lock_list))
	thread_current()->base_priority = new_priority;
      else
	thread_current ()->priority = new_priority;
      thread_yield ();
    }
    else
      thread_current ()->priority = new_priority;
  }

  intr_set_level(old_level);

}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/*round the float*/
int round (int x)
{
  if (x >= 0)
    return (x+f_value/2)/f_value;
  else
    return (x-f_value/2)/f_value;
}

<<<<<<< HEAD:src/threads/thread.c
/* set the priority when MLFQs */
void priority_update(struct thread *t)
{
  t->priority = PRI_MAX - round(t->recent_cpu/4) - (t->nice*2);

  if (t->priority > PRI_MAX)
    t->priority = PRI_MAX;
  if (t->priority < PRI_MIN)
    t->priority = PRI_MIN;
}

/* Sets the current thread's nice value to NICE. 
   update the priority and change the running thread according to thread priori   ty  */
void
thread_set_nice (int nice UNUSED) 
{
  struct thread *curr = thread_current();
  if(nice >= -20 && nice <= 20) 
    curr -> nice = nice;
  
  priority_update(curr);

  int i;
  for (i=63; i>=0; i--)
  {
    if (!list_empty(&mlfqs_list[i]))
    {
      if (curr->priority < i)
      {
        thread_yield();
        break;
      }
    }
  }   
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current ()->nice; 
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  return round(load_avg * 100);
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  return round(thread_current ()->recent_cpu * 100);
}

/* calculating thread-> recent_cpu : using in timer.c */
void calc_recent_cpu (struct thread *t)
{
  int i,j;
  i = 2*load_avg;
  j = i + (1*f_value);
  i = ((int64_t)i) * f_value / j;
  j = ((int64_t)i) * t->recent_cpu / f_value;
  t->recent_cpu = j + f_value * t->nice;
}

/* calculating thread->priority : using in timer.c*/
void calc_priority (struct thread *t)
{
  enum intr_level old_level;
  old_level = intr_disable();

  int base_priority = t->priority;
  priority_update(t);

  if (t->priority != base_priority  && t->status == THREAD_READY)
  {
    list_remove(&t->elem);
    list_push_back(&mlfqs_list[t->priority], &t->elem);
  }

  intr_set_level(old_level);

}

/* num_ready is the number of thread that are either running or 
 * ready to run at time of update.  */
int get_num_ready_threads(void)
{
  int i;
  int num_ready=0;

  for (i=0;i<64;i++)
    num_ready  += list_size(&mlfqs_list[i]);
  
  if (running_thread () != idle_thread)
    num_ready += 1;

  return num_ready;
}

/*return the idle_thread : using timer.c*/
struct thread *get_idle(void)
{
  return idle_thread;
}

/*return load_avg*/
int get_load_avg(void)
{
  return load_avg;
}

/*set the load_avg to temp_load*/
void set_load_avg(int temp_load)
{
  load_avg = temp_load;
}

/*update the something(depends on f) of all threads which is not dying. */
void all_thread_update(calc_func *f) 
{
  ASSERT(intr_get_level() == INTR_OFF);
  
  struct list_elem *a;

  for(a= list_begin (&all_threads); a!= list_end(&all_threads); a = list_next(a))
  {
    struct thread* t = list_entry(a, struct thread, all_elem);
    f(t);
  }
  

}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);
                    
  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Since `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->base_priority = -1;
  t->magic = THREAD_MAGIC;

  /*adding priority update for mlfqs*/
  list_push_back(&all_threads, &t->all_elem);//make all thread list. 
  /* if mlfqs, we have to update the priority*/
  if(thread_mlfqs)
    priority_update(t);

  list_init(&t->lock_list);
  t->wait_lock = NULL;
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. we implent it when rr case and mlfqs case */
static struct thread *
next_thread_to_run (void) 
{
  if(!thread_mlfqs)
  {
    if (list_empty (&ready_list))
      return idle_thread;
    else
    {
      list_sort (&ready_list, list_higher_priority, NULL);
      return list_entry (list_pop_front (&ready_list), struct thread, elem);
    }
  }
  else
  { 
    int i ;
    for (i = 63 ; i>=0 ; i--)
    {
      if(!list_empty(&mlfqs_list[i]))
	return list_entry(list_pop_front(&mlfqs_list[i]), struct thread, elem);
    }
    return idle_thread;
  }
}


/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
schedule_tail (struct thread *prev) 
{
  struct thread *curr = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  curr->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != curr);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.
   
   It's not safe to call printf() until schedule_tail() has
   completed. */
static void
schedule (void) 
{
  struct thread *curr = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (curr->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  //Sort the list every time a new thread is being scheduled, 
  //because the priority might change.
  //list_sort (&ready_list, list_higher_priority, NULL);

  if (curr != next)
    prev = switch_threads (curr, next);
  schedule_tail (prev); 
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

