#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <setjmp.h>
#include <assert.h>
#include "ec440threads.h"

#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>

#define MAX_THREADS 128			/* number of threads you support */
#define THREAD_STACK_SIZE (1<<15)	/* size of stack in bytes */
#define QUANTUM (50 * 1000)		/* quantum in usec */

/* 
   Thread_status identifies the current state of a thread. What states could a thread be in?
   Values below are just examples you can use or not use. 
 */
enum thread_status
{
 TS_EXITED,
 TS_RUNNING,
 TS_READY,
 TS_BLOCKED,	//for mutex
 TS_WAIT	//for barrier
};

/* The thread control block stores information about a thread. You will
 * need one of this per thread. What information do you need in it? 
 * Hint, remember what information Linux maintains for each task?
 */
struct thread_control_block {
	pthread_t id;			//thread id
	jmp_buf context;		//need context in thread control block
	enum thread_status state;	//keep track of thread state	
	void *stack;			//need stack pointer
	void *retval;			//join return value	
	int mutex;			//keep track of which mutex thread belongs to
	int barrier;			//keep track of which barrier thread belongs to
};

//MUTEX STUFF
typedef union {
	pthread_mutex_t mutex;
	struct {
		int locked;
		int init;
		pthread_t owner;
		int mut;	//keeps track of which mutex # this is
	} var;
} Mutex;

typedef union {
	pthread_barrier_t barrier;
	struct {
		unsigned int currcount;
		unsigned int count;
		int bar;	//keeps track of which barrier # this is
	} var;
} Barrier;

sigset_t block,noblock; 	//for lock/unlock
int barriercount;
int mutexcount;

static int idcount = 0;					//count # of threads, id 0 is main
struct thread_control_block *threads[MAX_THREADS];	//array for threads
struct thread_control_block *running = NULL;		//pointer to active thread
struct sigaction sa;					//sigaction for scheduling

static void schedule(int signal)
{
  /* 
     TODO: implement your round-robin scheduler, e.g., 
     - if whatever called us is not exiting 
       - mark preempted thread as runnable
       - save state of preempted thread
     - determin which thread should be running next
     - mark thread you are context switching to as running
     - restore registers of that thread
   */
	if(running->state == TS_RUNNING)
		//save context of running state
		running->state =TS_READY;
	
	int index;
	if(setjmp(running->context)==0)
	{
		//get running index, find next running state after that
		for(int i = 0; i<MAX_THREADS; i++)
		{
			if(threads[i] == running)
			{
				index=i;
				break;
			}
		}
		bool found = false;
		//search from index to end for a ready thread, longjmp and get it running
		for(int i = index+1; i < MAX_THREADS; i++)
		{
			if(threads[i]!=NULL &&threads[i]->state == TS_READY)
			{
				running = threads[i];
				running->state = TS_RUNNING;
				longjmp(running->context, 1);
				found = true;	//found ready thread
				break;
			}
		}
		//if wasnt found above then will need to keep searching for ready, even if end up at index of OG running thread again; search from 0 to index
		if(!found)
		{
			for(int i = 0; i < index+1; i++)
			{
				if(threads[i]!=NULL&&threads[i]->state == TS_READY)
				{
					running = threads[i];
					running->state = TS_RUNNING;
					longjmp(running->context, 1);
					found = true;	//doesnt really matter now but consistency
					break;
				}
			}
		}

		//exit main if not found ready since that means all threads exited
		if(!found) exit(0);
	}
	else
		//continue running thread
		return;
}

// to supress compiler error saying these static functions may not be used...
static void schedule(int signal) __attribute__((unused));


static void scheduler_init()
{
  /* 
     TODO: do everything that is needed to initialize your scheduler.
     For example:
     - allocate/initialize global threading data structures
     - create a TCB for the main thread. so your scheduler will be able to schedule it
     - set up your timers to call scheduler...
  */
	struct thread_control_block *main = (struct thread_control_block *)malloc(sizeof(struct thread_control_block));
	threads[0] = main;			//main is first thread
	main->state = TS_RUNNING;		//running state initially
	running = main;				//running ptr to main for now
	main->id = idcount;			//id is 0
	main->mutex = -1;
	main->barrier = -1;
	idcount++;				//increment id so next thread will be 1 etc

	//make all other threads null for now
	for(int i = 1; i <MAX_THREADS; i++)
		threads[i] = NULL;

	//scheduler timer&sig handler
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = schedule;	//schedule
	sa.sa_flags = SA_NODEFER;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGALRM, &sa, NULL);	//sig handler
	ualarm(QUANTUM, QUANTUM);	//alarm every quantum
}

void pthread_exit(void *value_ptr);

static bool is_first_call = true;

int pthread_create(
	pthread_t *thread, const pthread_attr_t *attr,
	void *(*start_routine) (void *), void *arg)
{
  // Create the timer and handler for the scheduler. Create thread 0.
  if (is_first_call) {
    is_first_call = false;
    scheduler_init();
  }
  
  /* TODO: Return 0 on successful thread creation, non-zero for an error.
   *       Be sure to set *thread on success.
   *
   * You need to create and initialize a TCB (thread control block) including:
   * - Allocate a stack for the thread
   * - Set up the registers for the functions, including:
   *   - Assign the stack pointer in the thread's registers to point to its stack. 
   *   - Assign the program counter in the thread's registers.
   *   - figure out how to have pthread_exit invoked if thread returns
   * - After you are done, mark your new thread as READY
   * Hint: Easiest to use setjmp to save a set of registers that you then modify, 
   *       and look at notes on reading/writing registers saved by setjmp using 
   * Hint: Be careful where the stackpointer is set to it may help to draw
   *       an empty stack diagram to answer that question.
   * Hint: Read over the comment in header file on start_thunk before 
   *       setting the PC.
   * Hint: Don't forget that the thread is expected to "return" to pthread_exit after it is done
   * 
   * Don't forget to assign RSP too! Functions know where to
   * return after they finish based on the calling convention (AMD64 in
   * our case). The address to return to after finishing start_routine
   * should be the first thing you push on your stack.
   */

  //initialize new thread
  struct thread_control_block *Thread = malloc(sizeof(struct thread_control_block));
  Thread->stack = malloc(THREAD_STACK_SIZE);

  //id is going to be previous id + 1 so just amount of threads so far, also casting int to pthread_t
  Thread->id = (pthread_t)idcount;
  *thread = (pthread_t)idcount;
  idcount++;
  Thread->state = TS_READY;	//ready state
  Thread->mutex = -1;
  Thread->barrier = -1;

  //pointer for end of stack
  void *stackend = Thread->stack + THREAD_STACK_SIZE;

  //need to put exit status of the thread at end of the stack
  //remove space needed for void function from end of stack and place it there
  void *exitspace = stackend-sizeof(void *);
  *(unsigned long*)exitspace = (unsigned long)pthread_exit;	//unsigned longs to avoid conversion errors (below as well)

  //context handling
  set_reg(&(Thread->context), JBL_R12, (unsigned long)start_routine);
  set_reg(&(Thread->context), JBL_R13, (unsigned long)arg);
  set_reg(&(Thread->context), JBL_PC, (unsigned long)start_thunk);
  set_reg(&(Thread->context), JBL_RSP, (unsigned long)exitspace);
  
  //add thread to array
  int temp = 0;
  while(threads[temp]!=NULL)
	  temp++;
  threads[temp] = Thread;	//newly created thread placed in array

  return 0;
}

void pthread_exit(void *value_ptr)
{
  /* TODO: Exit the current thread instead of exiting the entire process.
   * Hints:
   * - Release all resources for the current thread. CAREFUL though.
   *   If you free() the currently-in-use stack then do something like
   *   call a function or add/remove variables from the stack, bad things
   *   can happen.
   * - Update the thread's status to indicate that it has exited
   * What would you do after this?
   */
	running->state = TS_EXITED;	//exit state
	if(value_ptr != NULL)
		running->retval = value_ptr;	//save retval

	//check if all strings terminated
	bool alldone = true;
	for(int i = 0; i<MAX_THREADS; i++)
	{
		if(threads[i]!=NULL)
			alldone = false;
	}
	//exit with 0 if all strings terminated
	if(alldone)
		exit(0);
	else
		while(1) {}
}

pthread_t pthread_self(void)
{
  /* 
   * TODO: Return the current thread instead of -1, note it is up to you what ptread_t refers to
   */
  if(running!=NULL)
	  return running->id;
  else
	  return -1;
}

int pthread_join(pthread_t thread, void **retval)
{
	struct thread_control_block *target = NULL;
	//find the target thread
	for(int i = 0; i <MAX_THREADS; i++)
	{
		if(threads[i]->id  == thread)
		{
			target = threads[i];
			break;
		}
	}

	//make sure not null target
	if(target==NULL)
		return -1;

	while(target->state!=TS_EXITED)
	{
		//wait until exited
	}	

	//return retval like this, but only if it's not null
	if(retval!=NULL)
		*retval = target->retval;

	//now free stack 
	free(target->stack);
	target=NULL;	//make that point in array null again so a new thread cna be placed here
	
	return 0;
}

//SYNCHRONIZATION FUNCTIONS BELOW

static void lock(){
	sigemptyset(&block);
	sigaddset(&block, SIGALRM);
	sigprocmask(SIG_BLOCK,&block,&noblock);
}

static void unlock(){
	sigemptyset(&noblock);
	sigaddset(&noblock, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &noblock, NULL);
}

int pthread_mutex_init (pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr) {	
	Mutex mymutex = {0};
	mymutex.mutex = *mutex;


  if (is_first_call) {
    is_first_call = false;
    scheduler_init();
  }
	mymutex.var.locked = 0;
	mymutex.var.init = 1;
	mymutex.var.owner = -1;	
	mymutex.var.mut = mutexcount;
	mutexcount++;

	*mutex = mymutex.mutex;

	//return 0 on success
	return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex){
	Mutex mymutex = {0};
	mymutex.mutex = *mutex;

	if(mymutex.var.locked) return EBUSY;
	
	mymutex.var.owner = -1;
	mymutex.var.init = 0;	//no longer init

	*mutex = mymutex.mutex;

	//return 0 on success
	return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex){
	//JUMP:
	
	lock();

	Mutex mymutex = {0};
	mymutex.mutex = *mutex;
	if(mymutex.var.init == 0) 
	{
		unlock();
		return EINVAL;
	}

	//if locked, mark thread as blocked and add to queue
	if(mymutex.var.locked) {
		running->state = TS_BLOCKED;
		unlock();
		while(running->state == TS_BLOCKED) {
			schedule(SIGALRM);
			running->mutex = mymutex.var.mut;
			*mutex = mymutex.mutex;
		}
		lock();
	}

	//lock
	//else {
		mymutex.var.locked = 1;
		mymutex.var.owner = running->id;
		*mutex = mymutex.mutex;
		unlock();
		return 0;
	//}

	//scheduler continue
	//unlock();
	//schedule(SIGALRM);
	//goto JUMP;

	//return 0 on success
	//return 0;
}

int pthread_mutex_unlock (pthread_mutex_t *mutex){
	lock();
	
	Mutex mymutex = {0};
        mymutex.mutex = *mutex;	
	if(mymutex.var.init == 0) 
	{
		unlock();
		return EINVAL;
	}

	int i;
	for(i = 0; i < MAX_THREADS; i++) {
		//find next everything in  mutex queue
		if(threads[i]!= NULL && threads[i]->state == TS_BLOCKED)
		{
			//mark state as ready
			threads[i]->state = TS_READY;
			threads[i]->mutex = -1;
			mymutex.var.owner = running->id;
			break;
		}
	}

	//if no threads are in queue then just unlock the mutex completely
	if(i == MAX_THREADS) 
	{
			mymutex.var.owner = -1;
			mymutex.var.locked = 0;
	}
	
	*mutex = mymutex.mutex;
	
	//scheduler continue
	unlock();
	//schedule(SIGALRM);

	//return 0 on success
	return 0;
}

//BARRIER FUNCTIONS

int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned count){
	lock();
	if(count <= 0) 
	{
		unlock();
		return EINVAL;
	}

	Barrier mybarrier = {0};
	mybarrier.barrier = *barrier;

	mybarrier.var.count = count; //store count
	mybarrier.var.currcount = count;
	mybarrier.var.bar = barriercount;	//which barrier indicator
	barriercount++;

	*barrier = mybarrier.barrier;

	unlock();
	return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier){
	lock();

	Barrier mybarrier = {0};
	mybarrier.barrier = *barrier;
	
	//cant destroy when a thread is waiting in barrier
	if(mybarrier.var.count < mybarrier.var.currcount)
	{
		unlock();
		return EBUSY;
	}

	mybarrier.var.count = -1;	//invalid count to represent destroyed
	mybarrier.var.currcount = -1;

	*barrier = mybarrier.barrier;

	unlock();
	return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier){
	lock();
	
	Barrier mybarrier = {0};
	mybarrier.barrier = *barrier;

	//if destroyed cant wait
	if(mybarrier.var.currcount<0) {
		unlock();
		return EINVAL;
	}

	mybarrier.var.currcount--;	//decrement count
	running->barrier = mybarrier.var.bar;

	//last thread in barrier
	if(mybarrier.var.currcount ==0) 
	{
		//all waiting threads for this barrier now ready
		for(int i = 0; i < MAX_THREADS; i++)
			if(threads[i]!=NULL && threads[i]->state == TS_WAIT && mybarrier.var.bar == threads[i]->barrier) 
			{
				threads[i]->state = TS_READY;
				threads[i]->barrier = -1;	//no longer in barrier
			}

		mybarrier.var.currcount = mybarrier.var.count; 	//reset counter
		*barrier = mybarrier.barrier;

		unlock();
		schedule(SIGALRM);
		return PTHREAD_BARRIER_SERIAL_THREAD;
	}

	//mark thread as waiting until barrier opens
	running->state = TS_WAIT;

	*barrier = mybarrier.barrier;

	unlock();
	schedule(SIGALRM);
	
	return 0;
}

/* 
 * Don't implement main in this file!
 * This is a library of functions, not an executable program. If you
 * want to run the functions in this file, create separate test programs
 * that have their own main functions.
 */
