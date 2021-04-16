/*
 * System call handlers
 * Copyright (c) 2003, Jeffrey K. Hollingsworth <hollings@cs.umd.edu>
 * Copyright (c) 2003,2004 David Hovemeyer <daveho@cs.umd.edu>
 * $Revision: 1.59 $
 * 
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "COPYING".
 */

#include <geekos/syscall.h>
#include <geekos/errno.h>
#include <geekos/kthread.h>
#include <geekos/int.h>
#include <geekos/elf.h>
#include <geekos/malloc.h>
#include <geekos/screen.h>
#include <geekos/keyboard.h>
#include <geekos/string.h>
#include <geekos/user.h>
#include <geekos/timer.h>
#include <geekos/vfs.h>
#include <libc/sema.h>
#include <geekos/list.h>

/*system call implementation functions*/
#define MAX_LEN 25

static struct Semaphore_List s_SemaphoreList;
static int ID=0;


struct Semaphore;
/*master list of semphores*/
DEFINE_LIST(Semaphore_List,Semaphore);

struct Semaphore{
     int semaphoreID;
     char *semaphoreName;
     int value;
     int registeredThreadCount;
     struct Kernel_Thread *registeredThreads[MAX_REGISTERED_THREADS];
     struct Thread_Queue waitingThreads;
     DEFINE_LINK(Semaphore_List,Semaphore);
     };

/*implement (expand macros) Semaphore List Functionality*/
IMPLEMENT_LIST(Semaphore_List,Semaphore);

int Semaphore_Create(char *semName,int initialValue)
{ struct Kernel_Thread *current=g_currentThread;
  struct Semaphore *s,*sem;
  s=Get_Front_Of_Semaphore_List(&s_SemaphoreList);
  while(s!=0)
  { if(!strcmp(s->semaphoreName,semName)){ s->registeredThreads[s->registeredThreadCount]=current;
                                  ++s->registeredThreadCount;
                                  return s->semaphoreID;
                                 }
    s=Get_Next_In_Semaphore_List(s);
  }
  sem=Malloc(sizeof(struct Semaphore));
  sem->registeredThreadCount=0;
  sem->registeredThreads[sem->registeredThreadCount]=current;
  ++sem->registeredThreadCount;
  sem->semaphoreName=semName;
  sem->value=initialValue;
  sem->semaphoreID=ID;
    ++ID;
  Add_To_Back_Of_Semaphore_List(&s_SemaphoreList,sem);
  Clear_Thread_Queue(&sem->waitingThreads);
  return sem->semaphoreID;
}

int Semaphore_Acquire(int semID)
{ struct Kernel_Thread *current=g_currentThread;
  struct Semaphore *s;
  int i;
  s=Get_Front_Of_Semaphore_List(&s_SemaphoreList);
  while(s!=0)
  { if(s->semaphoreID==semID)
               { for(i=0;i<s->registeredThreadCount;++i)
                       {if(s->registeredThreads[i]==current)
                          {         s->value--;
                            if(s->value<0)Wait(&s->waitingThreads);
                           
                            return 0;
                           }
                        }
                 if(i==s->registeredThreadCount)return -1;
               }
    s=Get_Next_In_Semaphore_List(s);
   }
 return -1;
} 

int Semaphore_Release(int semID)
{ struct Kernel_Thread *current=g_currentThread;
  struct Semaphore *s;
  int i;
  s=Get_Front_Of_Semaphore_List(&s_SemaphoreList);
  while(s!=0)
  { if(s->semaphoreID==semID)
               { for(i=0;i<s->registeredThreadCount;++i)
                       {if(s->registeredThreads[i]==current)
                          {        s->value++;
                            if(s->value<=0)Wake_Up_One(&s->waitingThreads);

                            return 0;
                           }
                        }
                 if(i==s->registeredThreadCount)return -1;
               }
    s=Get_Next_In_Semaphore_List(s);
   }
  return -1;
} 

int Semaphore_Destroy(int semID)
{ struct Kernel_Thread *current=g_currentThread;
  struct Semaphore *s;
  int i,j;
  s=Get_Front_Of_Semaphore_List(&s_SemaphoreList);
  while(s!=0)
  { if(s->semaphoreID==semID)
                           {for(i=0;i<s->registeredThreadCount;++i)
                                { if(s->registeredThreads[i]==current)
                                       {for(j=i;j<s->registeredThreadCount;++j)
                                             { s->registeredThreads[j]=s->registeredThreads[j+1];
                                              }
                                               --s->registeredThreadCount;
                                              if(s->registeredThreadCount==0)
                                                    { Remove_From_Semaphore_List(&s_SemaphoreList,s);
                                                      Free(s);
                                                      return 0;
                                                     }
                                        }
                                  }
                             if(i==s->registeredThreadCount)return -1;
                             }
      s=Get_Next_In_Semaphore_List(s);
  }
    return -1;
}


/*
 * Allocate a buffer for a user string, and
 * copy it into kernel space.
 * Interrupts must be disabled.
 */
static int Copy_User_String(ulong_t uaddr, ulong_t len, ulong_t maxLen, char **pStr)
{
    int rc = 0;
    char *str;

    /* Ensure that string isn't too long. */
    if (len > maxLen)
	return EINVALID;

    /* Allocate space for the string. */
    str = (char*) Malloc(len+1);
    if (str == 0) {
	rc = ENOMEM;
	goto done;
    }

    /* Copy data from user space. */
    if (!Copy_From_User(str, uaddr, len)) {
	rc = EINVALID;
	Free(str);
	goto done;
    }
    str[len] = '\0';

    /* Success! */
    *pStr = str;

done:
    return rc;
}
/*
 * Null system call.
 * Does nothing except immediately return control back
 * to the interrupted user program.
 * Params:
 *  state - processor registers from user mode
 *
 * Returns:
 *   always returns the value 0 (zero)
 */
static int Sys_Null(struct Interrupt_State* state)
{
    return 0;
}

/*
 * Exit system call.
 * The interrupted user process is terminated.
 * Params:
 *   state->ebx - process exit code
 * Returns:
 *   Never returns to user mode!
 */
static int Sys_Exit(struct Interrupt_State* state)
{
    Exit(state->ebx);
    /* We will never get here. */
}

/*
 * Print a string to the console.
 * Params:
 *   state->ebx - user pointer of string to be printed
 *   state->ecx - number of characters to print
 * Returns: 0 if successful, -1 if not
 */
static int Sys_PrintString(struct Interrupt_State* state)
{
    int rc = 0;
    uint_t length = state->ecx;
    char *buf = 0;

    if (length > 0) {
	/* Copy string into kernel. */
	if ((rc = Copy_User_String(state->ebx, length, 1023, (char**) &buf)) != 0)
	    goto done;

	/* Write to console. */
	Put_Buf(buf, length);
    }

done:
    if (buf != 0)
	Free(buf);
    return rc;
}

/*
 * Get a single key press from the console.
 * Suspends the user process until a key press is available.
 * Params:
 *   state - processor registers from user mode
 * Returns: the key code
 */
static int Sys_GetKey(struct Interrupt_State* state)
{
    return Wait_For_Key();
}

/*
 * Set the current text attributes.
 * Params:
 *   state->ebx - character attributes to use
 * Returns: always returns 0
 */
static int Sys_SetAttr(struct Interrupt_State* state)
{
    Set_Current_Attr((uchar_t) state->ebx);
    return 0;
}

/*
 * Get the current cursor position.
 * Params:
 *   state->ebx - pointer to user int where row value should be stored
 *   state->ecx - pointer to user int where column value should be stored
 * Returns: 0 if successful, -1 otherwise
 */
static int Sys_GetCursor(struct Interrupt_State* state)
{
    int row, col;
    Get_Cursor(&row, &col);
    if (!Copy_To_User(state->ebx, &row, sizeof(int)) ||
	!Copy_To_User(state->ecx, &col, sizeof(int)))
	return -1;
    return 0;
}

/*
 * Set the current cursor position.
 * Params:
 *   state->ebx - new row value
 *   state->ecx - new column value
 * Returns: 0 if successful, -1 otherwise
 */
static int Sys_PutCursor(struct Interrupt_State* state)
{
    return Put_Cursor(state->ebx, state->ecx) ? 0 : -1;
}

/*
 * Create a new user process.
 * Params:
 *   state->ebx - user address of name of executable
 *   state->ecx - length of executable name
 *   state->edx - user address of command string
 *   state->esi - length of command string
 * Returns: pid of process if successful, error code (< 0) otherwise
 */
static int Sys_Spawn(struct Interrupt_State* state)
{
    
    int rc;
    char *program = 0;
    char *command = 0;
    struct Kernel_Thread *process;

    /* Copy program name and command from user space. */
    if ((rc = Copy_User_String(state->ebx, state->ecx, VFS_MAX_PATH_LEN, &program)) != 0 ||
	(rc = Copy_User_String(state->edx, state->esi, 1023, &command)) != 0)
	goto done;

    Enable_Interrupts();


    /*
     * Now that we have collected the program name and command string
     * from user space, we can try to actually spawn the process.
     */
    
//mydebug
 Print("Spawn process %s \n", program);



    rc = Spawn(program, command, &process);
    if (rc == 0) {
	KASSERT(process != 0);
	rc = process->pid;
    }
    Print("process %d moving to ready queue %d\n", process->pid, process->currentReadyQueue);
    Print_Queues();
     
    Disable_Interrupts();

done:
    if (program != 0)
	Free(program);
    if (command != 0)
	Free(command);

    return rc;
}

/*
 * Wait for a process to exit.
 * Params:
 *   state->ebx - pid of process to wait for
 * Returns: the exit code of the process,
 *   or error code (< 0) on error
 */
static int Sys_Wait(struct Interrupt_State* state)
{
    int exitCode;
    struct Kernel_Thread *kthread = Lookup_Thread(state->ebx);
    if (kthread == 0)
	return -1;

    Enable_Interrupts();
    exitCode = Join(kthread);
    Disable_Interrupts();

    return exitCode;
}

/*
 * Get pid (process id) of current thread.
 * Params:
 *   state - processor registers from user mode
 * Returns: the pid of the current thread
 */
static int Sys_GetPID(struct Interrupt_State* state)
{
    return g_currentThread->pid;
}

/*
 * Set the scheduling policy.
 * Params:
 *   state->ebx - policy,
 *   state->ecx - number of ticks in quantum
 * Returns: 0 if successful, -1 otherwise
 */
static int Sys_SetSchedulingPolicy(struct Interrupt_State* state)
{
    int n,quantum;
    n=state->ebx;
    quantum=state->ecx;
    if(n==0)Change_Scheduling_Policy(SCHED_RR,quantum);
    else { if(n==1)Change_Scheduling_Policy(SCHED_MLF,quantum);
              else return -1;}
    return 0;
}

/*
 * Get the time of day.
 * Params:
 *   state - processor registers from user mode
 *
 * Returns: value of the g_numTicks global variable
 */
static int Sys_GetTimeOfDay(struct Interrupt_State* state)
{
    return g_numTicks;
}

/*
 * Create a semaphore.
 * Params:
 *   state->ebx - user address of name of semaphore
 *   state->ecx - length of semaphore name
 *   state->edx - initial semaphore count
 * Returns: the global semaphore id
 */
static int Sys_CreateSemaphore(struct Interrupt_State* state)
{
    char *semName=NULL;
    int id;
    Copy_User_String(state->ebx,state->ecx,MAX_LEN,&semName);
    id=Semaphore_Create(semName,state->edx);
    return id;
}

/*
 * Acquire a semaphore.
 * Assume that the process has permission to access the semaphore,
 * the call will block until the semaphore count is >= 0.
 * Params:
 *   state->ebx - the semaphore id
 *
 * Returns: 0 if successful, error code (< 0) if unsuccessful
 */
static int Sys_P(struct Interrupt_State* state)
{
    int r,id=state->ebx;
    r=Semaphore_Acquire(id);
    return r;
}

/*
 * Release a semaphore.
 * Params:
 *   state->ebx - the semaphore id
 *
 * Returns: 0 if successful, error code (< 0) if unsuccessful
 */
static int Sys_V(struct Interrupt_State* state)
{
   int r,id=state->ebx;
    r=Semaphore_Release(id);
    return r;
}

/*
 * Destroy a semaphore.
 * Params:
 *   state->ebx - the semaphore id
 *
 * Returns: 0 if successful, error code (< 0) if unsuccessful
 */
static int Sys_DestroySemaphore(struct Interrupt_State* state)
{
    int r, id=state->ebx;
    r=Semaphore_Destroy(id);
    return r;
}


/*
 * Global table of system call handler functions.
 */
const Syscall g_syscallTable[] = {
    Sys_Null,
    Sys_Exit,
    Sys_PrintString,
    Sys_GetKey,
    Sys_SetAttr,
    Sys_GetCursor,
    Sys_PutCursor,
    Sys_Spawn,
    Sys_Wait,
    Sys_GetPID,
    /* Scheduling and semaphore system calls. */
    Sys_SetSchedulingPolicy,
    Sys_GetTimeOfDay,
    Sys_CreateSemaphore,
    Sys_P,
    Sys_V,
    Sys_DestroySemaphore,
};

/*
 * Number of system calls implemented.
 */
const int g_numSyscalls = sizeof(g_syscallTable) / sizeof(Syscall);
