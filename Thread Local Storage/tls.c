#include "tls.h"
#include <pthread.h>
#include <sys/mman.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define MAX_THREADS 128

/*
 * This is a good place to define any data structures you will use in this file.
 * For example:
 *  - struct TLS: may indicate information about a thread's local storage
 *    (which thread, how much storage, where is the storage in memory)
 *  - struct page: May indicate a shareable unit of memory (we specified in
 *    homework prompt that you don't need to offer fine-grain cloning and CoW,
 *    and that page granularity is sufficient). Relevant information for sharing
 *    could be: where is the shared page's data, and how many threads are sharing it
 *  - Some kind of data structure to help find a TLS, searching by thread ID.
 *    E.g., a list of thread IDs and their related TLS structs, or a hash table.
 */

typedef struct thread_local_storage{
	pthread_t tid;		//thread id
	unsigned int size;		//bite size
	unsigned int page_num;		//num of pages
	struct page **pages;	//array of pointers to pages
} TLS;

struct page {
	size_t address;		//start address of page
	int ref_count;		//counter for shared pages
};

struct tid_tls_pair
{
	pthread_t tid;
        TLS *tls;	
};

/*
 * Now that data structures are defined, here's a good place to declare any
 * global variables.
 */

unsigned int page_size;		//page size
bool init = false;		//has init been called yet?
static struct tid_tls_pair pairs[MAX_THREADS];		//TLS array

/*
 * With global data declared, this is a good point to start defining your
 * static helper functions.
 */

//protect and unprotect functions
void tls_protect(struct page *p)
{
	if (mprotect((void *) p->address, page_size, 0)) {
		fprintf(stderr, "tls_protect: could not protect page\n");
		exit(1);	
	}
}

void tls_unprotect(struct page *p)
{
	if (mprotect((void *) p->address, page_size, PROT_READ | PROT_WRITE)) {
		fprintf(stderr, "tls_unprotect: could not unprotect page\n");
		exit(1);	
	}
}

/*
 * Lastly, here is a good place to add your externally-callable functions.
 */ 

void tls_handle_page_fault(int sig, siginfo_t *si, void *context);

//init function
void tls_init()
{
	//initialize all tls to NULL
	for(int i = 0; i < MAX_THREADS; i++)
		pairs[i].tls = NULL;

	struct sigaction sigact;
	page_size = getpagesize();
	/* Handle page faults (SIGSEGV, SIGBUS) */
	sigemptyset(&sigact.sa_mask);
	/* Give context to handler */
	sigact.sa_flags = SA_SIGINFO;
	sigact.sa_sigaction = tls_handle_page_fault;
	sigaction(SIGBUS, &sigact, NULL);
	sigaction(SIGSEGV, &sigact, NULL);
}

//page fault handler
void tls_handle_page_fault(int sig, siginfo_t *si, void *context)
{
	unsigned int p_fault = ((size_t) si->si_addr) & ~(page_size - 1);

	//check if real segfault
	bool violation = false;
	//brute force check
	TLS* curr;
	for(int j = 0; j < MAX_THREADS; j++)
	{
		curr = pairs[j].tls;
		if(curr == NULL) continue;
		for(int i = 0; i < curr->page_num;i++)
		{
			if((unsigned int)curr->pages[i]->address == p_fault)
			{
				violation = true;
				break;
			}
		}
	}

	//exit thread if violation
	if(violation) pthread_exit(NULL);
	//default handling otherwise
	else
	{
		signal(SIGSEGV, SIG_DFL);
		signal(SIGBUS, SIG_DFL);
		raise(sig);
	}
}

int tls_create(unsigned int size)
{
	//get current thread id
	pthread_t id = pthread_self();

	//call tls_init if hasnt been initialized yet
	if(!init)
	{
		tls_init();
		init = true;
	}

	//error check size
	if(size<=0) return -1;

	TLS* curr;	//current tls tracker
	int space;	//variable to see where there is space in array
	bool space_found = false;	//make sure that first open space is used
	bool full = true;
	for(int i = 0; i < MAX_THREADS; i++)
	{
		curr = pairs[i].tls;
		if(curr == NULL) 
		{
			full = false;	//the array isnt full if something is null
			if(!space_found)
			{
				space_found = true;
				space = i;	//there is space at this location
			}
			continue;	
		}

		//check if current thread has LSA
		if(curr->tid == id)
			return -1;
	}

	//cant add TLS if array full
	if(full) return -1;

	//ALLOCATE MEMORY	
	curr = calloc(1,sizeof(TLS));	//allocate mem
	curr->tid = id;		//init id
	pairs[space].tid = id;
	curr->size = size;	//init size

	//init page
	curr->page_num = (size+page_size-1)/page_size;
	curr->pages = (struct page**)calloc(1, curr->page_num*sizeof(struct page*));
	for(int i = 0; i<curr->page_num; i++)
	{
		struct page *temp = calloc(1, sizeof(struct page));	//calloc page
		//use mmap as seen in slides
		temp->address = (size_t) mmap(0, page_size, PROT_NONE, MAP_ANON | MAP_PRIVATE, 0, 0);	
		temp->ref_count=1;	//ref count initial 1
		tls_protect(temp);	//protect
		curr->pages[i] = temp;	//assign
	}	

	pairs[space].tls = curr;	//put the new tls in the next open space

	//return 0 upon success
	return 0;
}

int tls_destroy()
{
	//get current thread id
	pthread_t id = pthread_self();
	TLS* curr;
	bool exist = false;
	//find the current thread's lsa
	for(int i = 0; i < MAX_THREADS; i++)
	{
		curr = pairs[i].tls;
		if(curr == NULL) continue;
		if(curr->tid == id)
		{
			exist = true;
			break;
		}
	}

	//if doesnt exist then cant destroy
	if(!exist) return -1;

	for(int i = 0; i < curr->page_num; i++)
	{
		curr->pages[i]->ref_count--;	//refcount down
		if(curr->pages[i]->ref_count == 0)	//if refcount is 0 now then it was not shared
			free(curr->pages[i]);	//free paeg
			//munmap((void *) curr->pages[i]->address, page_size)0;	//free page - this caused mem leaks
	}

	//free everything else now
	free(curr->pages);
	free(curr);
	curr = NULL;	//make ptr null again in array
	
	//return 0 upon success
	return 0;
}

int tls_read(unsigned int offset, unsigned int length, char *buffer)
{
	//get current thread id
	pthread_t id = pthread_self();
        TLS* curr;
        bool exist = false;
	//find the current thread's lsa
        for(int i = 0; i < MAX_THREADS; i++)
        {
                curr = pairs[i].tls;
                if(curr == NULL) continue;
                if(curr->tid == id)
                {
                        exist = true;
                        break;
                }
        }

        //if doesnt exist then cant read
        if(!exist) return -1;
	
	//error check 
	if(offset+length>curr->size) return -1;

	//unprotect all pages - INEFFICIENT
	//for(int i = 0; i < curr->page_num; i++)
	//	tls_unprotect(curr->pages[i]);

	//read operation
	for(int cnt = 0, idx = offset; idx < (offset+length); ++cnt, ++idx)
	{
		//calculate page number and offset
		struct page *temp;
		unsigned int number, offset;
		number = idx/page_size;
		offset = idx%page_size;

		//locate relevant byte to take in using offset and number calculated
		temp = curr->pages[number];
		tls_unprotect(curr->pages[number]); //protect
		buffer[cnt] = *((unsigned char*)temp->address + offset);
		tls_protect(curr->pages[number]); //unprotect
	}

	//protect all pages again - INEFFICIENT
	//for(int i = 0; i<curr->page_num; i++)
	//	tls_protect(curr->pages[i]);

	//return 0 upon success
	return 0;
}

int tls_write(unsigned int offset, unsigned int length, const char *buffer)
{
	//get current thread id
	pthread_t id = pthread_self();
        TLS* curr;
        bool exist = false;
	//find the current thread's lsa
        for(int i = 0; i < MAX_THREADS; i++)
        {
                curr = pairs[i].tls;
                if(curr == NULL) continue;
                if(curr->tid == id)
                {
                        exist = true;
                        break;
                }
        }

        //if doesnt exist then cant write
        if(!exist) return -1;

	//error check
	if(offset+length>curr->size) return -1;

	//unprotect all pages - INEFFICIENT
	//for(int i = 0; i<curr->page_num; i++)
	//	tls_unprotect(curr->pages[i]);

	//writing operation
	for(int cnt = 0, idx = offset; idx < (offset+length); ++cnt, ++idx)
	{
		//calculate page number and offset
		struct page* temp, *copy;
		unsigned int number, offset;
		number = idx/page_size;
		offset = idx%page_size;

		//locate relevant byte to write using offset and number calculated
		temp = curr->pages[number];
		tls_unprotect(curr->pages[number]); //unprotect
		//create private copy (CoW) if shared page
		if(temp->ref_count > 1)	//refcount > 1 means shared
		{
			//cow implementation
			copy = (struct page *) calloc(1, sizeof(struct page));
			copy->address = (size_t) mmap(0, page_size, PROT_WRITE, MAP_ANON | MAP_PRIVATE, 0, 0);
		
			//unprotect copy since it will be used instead of temp now
			tls_unprotect(copy);
			
			//reference counts
			copy->ref_count = 1;
			temp->ref_count--;	//now that copy assigned, original page referenced one less time
			
			//copy memory over from temp to copy before protecting again
			memcpy((void *) copy->address, (void *) temp->address, page_size);

			//protect temp since it is no longer associated with current tls
			tls_protect(temp);	

			//make sure copy is the only thing being worked with now
			curr->pages[number] = copy;
			temp = copy;
		}
		*(((unsigned char*) temp->address) + offset) = buffer[cnt];

		tls_protect(curr->pages[number]); //reprotect
	}

	//protect all pages - INEFFICIENT
	//for(int i = 0; i<curr->page_num; i++)
	//	tls_protect(curr->pages[i]);

	//return 0 upon success
	return 0;
}

int tls_clone(pthread_t tid)
{
	//get current thread id
	pthread_t id = pthread_self();
        TLS* curr;
	bool full = true;	//check if array is full
	bool space_found = false;	//make sure first space found is used
	int space;		//save index in array thats null
        bool exist = false;	
        //find the current thread's lsa
        for(int i = 0; i < MAX_THREADS; i++)
        {
                curr = pairs[i].tls;
                if(curr == NULL) 
		{
			if(!space_found)
			{
				space_found = true;
				space = i;	//if its null, there is space here
			}
			full = false;	//if theres a null, array not full
			continue;
		}
                if(curr->tid == id)
                {
                        exist = true;
                        break;
                }
        }

        //if current thread lsa exists then cant clone
        if(exist) return -1;

	//if tls array is full tehn cant add new cloned tls
	if(full) return -1;

	//now do same for target id, but this time make sure lsa does exist
	exist = false;
	//find target thread's lsa
	for(int i = 0; i < MAX_THREADS; i++)
	{
		curr = pairs[i].tls;
		if(curr == NULL) continue;
		if(tid == curr->tid)
		{
			exist = true;
			break;
		}
	}
	//curr is now at target thread after this loop executes

	//if target thread lsa doesnt exist then cant clone
	if(!exist) return -1;

	//make new TLS to clone to
	TLS *new = calloc(1, sizeof(TLS));
	new->tid = id;	//currentid
	
	//copy tid's attributes:
	new->size = curr->size;
	new->page_num = curr->page_num;
	//copy pointers:
	new->pages = calloc(1,new->page_num * sizeof(struct page*));
	for(int i = 0; i < new->page_num; i++)
	{
		new->pages[i] = curr->pages[i];
		new->pages[i]->ref_count++;	//update ref count since its a shared page
	}

	//place new tls in array
	pairs[space].tls = new;	

	//return 0 upon success
	return 0;
}
