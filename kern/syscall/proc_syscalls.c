#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include "opt-A2.h"
#include "clock.h"
#include <mips/trapframe.h>


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  #if OPT_A2
    KASSERT(curproc != NULL);
    *retval = curproc->pid;
  #else 
    *retval = 1;
  #endif
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}



int sys_fork(pid_t* retval, struct trapframe *tf)
{
  // create a process structure for child process
  struct proc *child = proc_create_runprogram(curproc->p_name);
  KASSERT(child != NULL);
  KASSERT(child->pid > 0);
  
  
  child->exit_code = -1;
  child->exited = false;

  array_add(curproc->proc_children, child, NULL);

  // create and copy the address space
 
  // struct addrspace *cur_address = curproc_getas();
	// struct addrspace *new_address;
	// int err = as_copy(cur_address, &new_address);
  int err = as_copy(curproc_getas(), &(child->p_addrspace));

  if (err != 0) {
    DEBUG(DB_SYSCALL,"sys_fork | ERROR: Failed to copy address space");
    proc_destroy(child);
    return EADDRNOTAVAIL;
  }
  // child->p_addrspace = new_address;
  child->parent = curproc; 

// new trapframe
 struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
 *child_tf = *tf;
 
 if(child_tf == NULL)
	{
		proc_destroy(child);
		return ENOMEM;
	}
 
 int new_fork = thread_fork(child->p_name, child, (void*)&enter_forked_process, (void*)child_tf, (unsigned long)0);
 if (new_fork != 0) {
  kfree(child_tf);
  proc_destroy(child);
  return new_fork;
 }

  *retval = child->pid;
  clocksleep(1);
  return(0);

}




