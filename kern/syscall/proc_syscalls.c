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
#include <synch.h>


  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  //lock_acquire(p->child_lock);

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
 
  as = curproc_setas(NULL);
  as_destroy(as);

  proc_remthread(curthread);

// go through the childrens array and if they have exited, delete them
// if they are still running, set their parent to null
// this handles the case that if ur exiting and ur children are dead, everything
// gets deleted (case 2)
#if OPT_A2
    lock_acquire(p->child_lock);
		int children = array_num(p->proc_children);
		for(int i = children - 1; i >= 0; i--) {
			struct proc *temp = (struct proc *)array_get(p->proc_children, i);
      lock_acquire(temp->child_lock);

      if (temp->exited == 1) {
        array_remove(p->proc_children, i);
        lock_release(temp->child_lock);
        proc_destroy(temp);
      } else {
        temp->parent = NULL;
        lock_release(temp->child_lock);
      }
		}
  
  if (p->parent == NULL) {
    lock_release(p->child_lock);
    proc_destroy(p);

  } else {
    p->exited = 1;
    p->exit_code = _MKWAIT_EXIT(exitcode);
    cv_signal(p->proc_cv, p->child_lock);
    lock_release(p->child_lock);
  }

#endif
 
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
  struct proc *p = curproc;

    if (options != 0) {
    return(EINVAL);
  }
  //exitstatus = 0;

 #if OPT_A2
 lock_acquire(p->child_lock);
bool found = false; 
 struct proc *temp_child = NULL;
 int children = array_num(p->proc_children);
		for(int i = children - 1; i >= 0; i--) {

      temp_child = (struct proc *)array_get(p->proc_children, i);
      if (pid == temp_child->pid) {
      
        found = true;
        break;
    }
   // lock_release(temp_child->child_lock);
  }

  if (!found) {
    lock_release(p->child_lock);
    return ECHILD ; 
  }
  // if we're here then it means we've found the child. 
  lock_release(p->child_lock);
  // if waitpid is called after the child process has exited, then j get the exit status
  if (temp_child->exited == 1) {
    exitstatus = temp_child->exit_code;
  } else {  // else wait for the child to exit with cv and then get exit code
    lock_acquire(temp_child->child_lock);
    while (temp_child->exited == 0) {
    cv_wait(temp_child->proc_cv, temp_child->child_lock);
  }
    exitstatus = temp_child->exit_code;
    lock_release(temp_child->child_lock);

  }
  
 #else
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
 #endif


  /* for now, just pretend the exitstatus is 0 */
  
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

  child->parent = curproc; 
  array_add(curproc->proc_children, child, NULL);
  
  int copy_err = as_copy(curproc_getas(), &(child->p_addrspace));
  if (copy_err != 0) {
    proc_destroy(child);
    return EADDRNOTAVAIL;
  }
  // child->p_addrspace = new_address;
  
// new trapframe
 struct trapframe *child_tf = kmalloc(sizeof(struct trapframe));
 
 if(child_tf == NULL)
	{
		proc_destroy(child);
		return ESRCH;
	}
  *child_tf = *tf;

  
 // threadfork
 int forked = thread_fork(child->p_name, child, (void*)&enter_forked_process, (void*)child_tf, (unsigned long)0);
 if (forked != 0) {
  kfree(child_tf);
  proc_destroy(child);
  return ESRCH;
 }

  *retval = child->pid;
  return(0);

}




