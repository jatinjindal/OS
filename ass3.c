#include <context.h>
#include <memory.h>
#include <schedule.h>
#include <lib.h>
#include <types.h>
#include <apic.h>
#include <idt.h>
#include <init.h>


static u64 numticks;

static void save_current_context()
{
  //copied all the registers
  /*struct exec_context *current = get_current_ctx(); 
  asm volatile("mov %%r15, %0" : "=r"(current->regs.r15));
  asm volatile("mov %%r14, %0" : "=r"(current->regs.r14));
  asm volatile("mov %%r13, %0" : "=r"(current->regs.r13));
  asm volatile("mov %%r12, %0" : "=r"(current->regs.r12));
  asm volatile("mov %%r11, %0" : "=r"(current->regs.r11));
  asm volatile("mov %%r10, %0" : "=r"(current->regs.r10));
  asm volatile("mov %%r9, %0" : "=r"(current->regs.r9));
  asm volatile("mov %%r8, %0" : "=r"(current->regs.r8));
  asm volatile("mov %%rsi, %0" : "=r"(current->regs.rsi));
  asm volatile("mov %%rdi, %0" : "=r"(current->regs.rdi));
  asm volatile("mov %%rdx, %0" : "=r"(current->regs.rdx));
  asm volatile("mov %%rcx, %0" : "=r"(current->regs.rcx));
  asm volatile("mov %%rbx, %0" : "=r"(current->regs.rbx));
  asm volatile("mov %%rax, %0" : "=r"(current->regs.rax));
  
  u64* sp;
  asm volatile("mov %%rbp, %0" : "=r"(sp));
  current->regs.rbp = *sp;
  current->regs.entry_rip = *(sp+1);
  current->regs.entry_cs = *(sp+2);
  current->regs.entry_rflags = *(sp+3);
  current->regs.entry_rsp = *(sp+4);
  current->regs.entry_ss = *(sp+5);*/

  /*Your code goes in here*/ 
} 

static void schedule_context(struct exec_context *next)
{
  /*Your code goes in here. get_current_ctx() still returns the old context*/
  struct exec_context *current = get_current_ctx();
  printf("schedluing: old pid = %d  new pid  = %d\n", current->pid, next->pid); /*XXX: Don't remove*/
  /*These two lines must be executed*/
  u64* sp;
  //asm volatile("mov %%rbp, %0" : "=r"(sp) :: "memory");
  
  // current->regs.rbp = *sp;
  // current->regs.entry_rip = *(sp+1);
  // current->regs.entry_rsp=(u64)(sp+2);
  
  next->state = RUNNING;
  
  asm volatile("sub $0x28,%rsp;");
  asm volatile("mov %%rsp,%0":"=r"(sp)::"memory");
  
  *(sp)   = next->regs.rbp; 
  *(sp+1) = next->regs.entry_rip;
  *(sp+2) = next->regs.entry_cs;
  *(sp+3) = next->regs.entry_rflags;
  *(sp+4) = next->regs.entry_rsp;
  *(sp+5) = next->regs.entry_ss;
  set_tss_stack_ptr(next);
  set_current_ctx(next);
  asm volatile("mov  %0,%%r15" :: "r"(next->regs.r15):"memory");
  asm volatile("mov  %0,%%r14" :: "r"(next->regs.r14):"memory");
  asm volatile("mov  %0,%%r13" :: "r"(next->regs.r13):"memory");
  asm volatile("mov  %0,%%r12" :: "r"(next->regs.r12):"memory");
  asm volatile("mov  %0,%%r11" :: "r"(next->regs.r11):"memory");
  asm volatile("mov  %0,%%r10" :: "r"(next->regs.r10):"memory");
  asm volatile("mov %0,%%r9" :: "r"(next->regs.r9):"memory");
  asm volatile("mov %0,%%r8" :: "r"(next->regs.r8):"memory");
  asm volatile("mov  %0,%%rsi" :: "r"(next->regs.rsi):"memory");
  asm volatile("mov  %0,%%rdi" :: "r"(next->regs.rdi):"memory");
  asm volatile("mov  %0,%%rdx" :: "r"(next->regs.rdx):"memory");
  asm volatile("mov  %0,%%rcx" :: "r"(next->regs.rcx):"memory");
  asm volatile("mov  %0,%%rbx" :: "r"(next->regs.rbx):"memory");
  asm volatile("mov  %0,%%rax" :: "r"(next->regs.rax):"memory");

  asm volatile("mov %0,%%rsp"::"r"(sp):"memory");
  printf("check\n");
  asm volatile("pop %rbp;iretq;");

  return;
}

static struct exec_context *pick_next_context(struct exec_context *list)
{
  /*Your code goes in here*/
  struct exec_context *current = get_current_ctx(); 
  struct exec_context *next = NULL;

  int cur = current->pid + 1;
  int flag=0;
  int count = 0;
  for(int count=1; count<=16; count++)
  {
    cur = (current->pid + count)%16;
    if(cur!=0)
    {
      if(list[cur].state == READY)
      {
        next = &(list[cur]);
        flag = 1;
        break;
      }
    }
  }  
  if(flag==0)
  {
    next = &(list[0]);
  }
  return next;
}
static void schedule()
{
 struct exec_context *next;
 struct exec_context *list = get_ctx_list();
 next = pick_next_context(list);
 schedule_context(next);
     
}

static void do_sleep_and_alarm_account()
{
 /*All processes in sleep() must decrement their sleep count*/ 
}

/*The five functions above are just a template. You may change the signatures as you wish*/
void handle_timer_tick()
{
 /*
   This is the timer interrupt handler. 
   You should account timer ticks for alarm and sleep
   and invoke schedule
 */

  u64* last_Addr;
  asm volatile("push %rax"); asm volatile("push %rbx"); asm volatile("push %rcx"); asm volatile("push %rdx");
  asm volatile("push %rsi"); asm volatile("push %rdi"); asm volatile("push %r8"); asm volatile("push %r9");
  asm volatile("push %r10"); asm volatile("push %r11"); asm volatile("push %r12"); asm volatile("push %r13");
  asm volatile("push %r14"); asm volatile("push %r15"); asm volatile("mov %%rsp, %0" : "=r"(last_Addr));
  


  struct exec_context *list = get_ctx_list();
  struct exec_context *background_process = &(list[1]);
  struct exec_context *current = get_current_ctx();
  
  printf("Got a tick. #ticks = %u\n", ++numticks); 
  
  if(current->pid == 0)
  {

   if(background_process->ticks_to_sleep > 1)
    {
      background_process->ticks_to_sleep = background_process->ticks_to_sleep-1;
    } 
    else if(background_process->ticks_to_sleep == 1)
    {
      printf("shifting from swapper to init\n");
      
      u64* sp;
      asm volatile("mov %%rbp, %0" : "=r"(sp));


      current->regs.r15=*(last_Addr);
      current->regs.r14=*(last_Addr+1);
      current->regs.r13=*(last_Addr+2);
      current->regs.r12=*(last_Addr+3);
      current->regs.r11=*(last_Addr+4);
      current->regs.r10=*(last_Addr+5);
      current->regs.r9=*(last_Addr+6);
      current->regs.r8=*(last_Addr+7);
      current->regs.rdi=*(last_Addr+8);
      current->regs.rsi=*(last_Addr+9);
      current->regs.rdx=*(last_Addr+10);
      current->regs.rcx=*(last_Addr+11);
      current->regs.rbx=*(last_Addr+12);
      current->regs.rax=*(last_Addr+13);

      current->state = READY;
      background_process->state = RUNNING;
      background_process->ticks_to_sleep = background_process->ticks_to_sleep-1;
     
     *sp = background_process->regs.rbp;
     *(sp+1) = background_process->regs.entry_rip;
     *(sp+2) = background_process->regs.entry_cs;
     *(sp+3) = background_process->regs.entry_rflags;
     *(sp+4) = background_process->regs.entry_rsp;
     *(sp+5) = background_process->regs.entry_ss;

      set_tss_stack_ptr(background_process);
      set_current_ctx(background_process);

    
      asm volatile("mov  %0,%%r15" :: "r"(background_process->regs.r15):"memory");
      asm volatile("mov  %0,%%r14" :: "r"(background_process->regs.r14):"memory");
      asm volatile("mov  %0,%%r13" :: "r"(background_process->regs.r13):"memory");
      asm volatile("mov  %0,%%r12" :: "r"(background_process->regs.r12):"memory");
      asm volatile("mov  %0,%%r11" :: "r"(background_process->regs.r11):"memory");
      asm volatile("mov  %0,%%r10" :: "r"(background_process->regs.r10):"memory");
      asm volatile("mov %0,%%r9" :: "r"(background_process->regs.r9):"memory");
      asm volatile("mov %0,%%r8" :: "r"(background_process->regs.r8):"memory");
      asm volatile("mov  %0,%%rsi" :: "r"(background_process->regs.rsi):"memory");
      asm volatile("mov  %0,%%rdi" :: "r"(background_process->regs.rdi):"memory");
      asm volatile("mov  %0,%%rdx" :: "r"(background_process->regs.rdx):"memory");
      asm volatile("mov  %0,%%rcx" :: "r"(background_process->regs.rcx):"memory");
      asm volatile("mov  %0,%%rbx" :: "r"(background_process->regs.rbx):"memory");
      asm volatile("mov  %0,%%rax" :: "r"(background_process->regs.rax):"memory");

    
      ack_irq();
      asm volatile("mov %rbp,%rsp");
      asm volatile("pop %rbp");
      asm volatile("iretq");
  
    } 
  }


  u64* sp;
  asm volatile("mov %%rbp, %0" : "=r"(sp));
      
  if(current->ticks_to_alarm > 1)
  current->ticks_to_alarm = current->ticks_to_alarm  - 1;
  else if(current->ticks_to_alarm == 1)
  {
    current->ticks_to_alarm = current->ticks_to_alarm  - 1;

    u64* ustackp = sp+4;
    u64* urip = sp+1;
    u64 user_ip = *urip;
    invoke_sync_signal(SIGALRM,ustackp,urip);
  }

  ack_irq(); 
  
  asm volatile("mov %0, %%rsp" :: "r"(last_Addr));
  asm volatile("pop %r15"); asm volatile("pop %r14");
  asm volatile("pop %r13"); asm volatile("pop %r12"); asm volatile("pop %r11"); asm volatile("pop %r10");
  asm volatile("pop %r9"); asm volatile("pop %r8"); asm volatile("pop %rdi"); asm volatile("pop %rsi");
  asm volatile("pop %rdx"); asm volatile("pop %rcx"); asm volatile("pop %rbx");asm volatile("pop %rax");
    

  asm volatile("mov %%rbp, %%rsp;"
               "pop %%rbp;"
               "iretq;"
               :::"memory");
}

void do_exit()
{
  /*You may need to invoke the scheduler from here if there are
    other processes except swapper in the system. Make sure you make 
    the status of the current process to UNUSED before scheduling 
    the next process. If the only process alive in system is swapper, 
    invoke do_cleanup() to shutdown gem5 (by crashing it, huh!)
    */
  struct exec_context *current = get_current_ctx(); 
  
  current->state =UNUSED;
  //os_pfn_free(OS_PT_REG,current->os_stack_pfn);
  int flag=0;

  struct exec_context *list = get_ctx_list();

  for(int pid= 1;pid<16;pid++)
  {
    if(list[pid].state!= UNUSED)
    {
      flag=1;
    }
  }

  if(flag == 0)
  {
    for(int i=0;i<3;i++)
    {
      os_pfn_free(OS_PT_REG,list[i].os_stack_pfn);
    }
    do_cleanup();  /*Call this conditionally, see comments above*/
  }
  else
  {
    schedule();
  }
}

/*system call handler for sleep*/
long do_sleep(u32 ticks)
{
  struct exec_context *current = get_current_ctx(); 

  /*asm volatile("mov %%r15, %0" : "=r"(current->regs.r15));
  asm volatile("mov %%r14, %0" : "=r"(current->regs.r14));
  asm volatile("mov %%r13, %0" : "=r"(current->regs.r13));
  asm volatile("mov %%r12, %0" : "=r"(current->regs.r12));
  asm volatile("mov %%r11, %0" : "=r"(current->regs.r11));
  asm volatile("mov %%r10, %0" : "=r"(current->regs.r10));
  asm volatile("mov %%r9, %0" : "=r"(current->regs.r9));
  asm volatile("mov %%r8, %0" : "=r"(current->regs.r8));
  asm volatile("mov %%rsi, %0" : "=r"(current->regs.rsi));
  asm volatile("mov %%rdi, %0" : "=r"(current->regs.rdi));
  asm volatile("mov %%rdx, %0" : "=r"(current->regs.rdx));
  asm volatile("mov %%rcx, %0" : "=r"(current->regs.rcx));
  asm volatile("mov %%rbx, %0" : "=r"(current->regs.rbx));
  asm volatile("mov %%rax, %0" : "=r"(current->regs.rax));*/
  
  u64* sp;
  asm volatile("mov %%rbp, %0" : "=r"(sp) :: "memory");

  current->regs.rbp = *sp;

  current->regs.entry_rip = *(sp+1);
  current->regs.entry_rsp=(u64)(sp+2);
  //current->regs.entry_rip = *(sp+1);
  /*current->regs.entry_cs = *(sp+2);
  current->regs.entry_rflags = *(sp+3);
  current->regs.entry_rsp = *(sp+4);
  current->regs.entry_ss = *(sp+5);*/

  //-------------------------------------------------------------
  current->ticks_to_sleep = ticks;  
  current->state = WAITING;

  struct exec_context *list = get_ctx_list();
  struct exec_context swapper_process = list[0];
  
  list[0].state = RUNNING;


  
  asm volatile("sub $0x28,%rsp;");
  asm volatile("mov %%rsp,%0":"=r"(sp)::"memory");
  
  *(sp)   = list[0].regs.rbp; 
  *(sp+1) = list[0].regs.entry_rip;
  *(sp+2) = list[0].regs.entry_cs;
  *(sp+3) = list[0].regs.entry_rflags;
  *(sp+4) = list[0].regs.entry_rsp;
  *(sp+5) = list[0].regs.entry_ss;

  set_tss_stack_ptr(&list[0]);
  set_current_ctx(&list[0]);

  asm volatile("mov  %0,%%r15" :: "r"(list[0].regs.r15):"memory");
  asm volatile("mov  %0,%%r14" :: "r"(list[0].regs.r14):"memory");
  asm volatile("mov  %0,%%r13" :: "r"(list[0].regs.r13):"memory");
  asm volatile("mov  %0,%%r12" :: "r"(list[0].regs.r12):"memory");
  asm volatile("mov  %0,%%r11" :: "r"(list[0].regs.r11):"memory");
  asm volatile("mov  %0,%%r10" :: "r"(list[0].regs.r10):"memory");
  asm volatile("mov %0,%%r9" :: "r"(list[0].regs.r9):"memory");
  asm volatile("mov %0,%%r8" :: "r"(list[0].regs.r8):"memory");
  asm volatile("mov  %0,%%rsi" :: "r"(list[0].regs.rsi):"memory");
  asm volatile("mov  %0,%%rdi" :: "r"(list[0].regs.rdi):"memory");
  asm volatile("mov  %0,%%rdx" :: "r"(list[0].regs.rdx):"memory");
  asm volatile("mov  %0,%%rcx" :: "r"(list[0].regs.rcx):"memory");
  asm volatile("mov  %0,%%rbx" :: "r"(list[0].regs.rbx):"memory");
  asm volatile("mov  %0,%%rax" :: "r"(list[0].regs.rax):"memory");

  asm volatile("mov %0,%%rsp"::"r"(sp):"memory");
  asm volatile("pop %rbp;iretq;");

}

/*
  system call handler for clone, create thread like 
  execution contexts
*/

long do_clone(void *th_func, void *user_stack)
{
    struct exec_context * new_process  = get_new_ctx();
    struct exec_context *current = get_current_ctx();
    
    new_process->type =  current->type;
    new_process->state = READY;
    new_process->used_mem = current->used_mem;
    new_process->pgd = current->pgd;
    new_process->ticks_to_sleep =  0x0;
    new_process->ticks_to_alarm = 0x0;
    
    for(int i=0; i< MAX_SIGNALS;i++)
    {
      new_process->sighandlers[i] = current->sighandlers[i];
    }
    new_process->pending_signal_bitmap = 0x0;
    for(int i=0;i<MAX_MM_SEGS;i++)
    {
      new_process->mms[i] = current->mms[i];
    }
    //new_process->mms = current->mms;
    new_process->regs = current->regs;
    new_process->regs.rbp = (u64)user_stack;
    new_process->regs.entry_ss = 0x2b;
    new_process->regs.entry_cs = 0x23;
    new_process->regs.entry_rip = (u64)th_func;
    new_process->regs.entry_rflags = current->regs.entry_rflags;
    new_process->regs.entry_rsp = (u64)user_stack;
    

    char c[100];
    memcpy(c,current->name,strlen(current->name));
    
    int d=new_process->pid;
    char tmp;
    if(d<10)
    {
      c[strlen(c)-1]=d+48;
      c[strlen(c)-1]='\0';
      memcpy(new_process->name,c,strlen(c));   
    }
    //memcpy ( new_process->name, "init-", 6);
    // char temp[10];
    // itoa(current->pid,temp,10);
    // memcpy(new_process->name + 5,temp,10);

    u32 pfn = os_pfn_alloc(OS_PT_REG);
    new_process->os_stack_pfn = (u64)pfn;
    new_process->os_rsp = pfn<<12;


}   

long invoke_sync_signal(int signo, u64 *ustackp, u64 *urip)
{
   /*If signal handler is registered, manipulate user stack and RIP to execute signal handler*/
   /*ustackp and urip are pointers to user RSP and user RIP in the exception/interrupt stack*/
   printf("Called signal with ustackp=%x urip=%x\n", *ustackp, *urip);
   struct exec_context *current = get_current_ctx();
   
   if( (current->sighandlers)[signo] == NULL)
   {
      /*Default behavior is exit( ) if sighandler is not registered for SIGFPE or SIGSEGV.
      Ignore for SIGALRM*/
      //printf("Default Signal behavior\n");
      if(signo != SIGALRM)
      do_exit();
      else
      {
        printf("Default SignalAlarm behavior");
      }

   }
   else
   {
      u64* temp = (u64 *)*ustackp;
      *(temp - 1) = *urip;
      *(ustackp) = (u64)(temp - 1);
      *(urip) = (u64)((current->sighandlers)[signo]);
      //asm volatile("mov %0, %%rip" :: "r"((current->sighandlers)[signo]));
   }

}
/*system call handler for signal, to register a handler*/
long do_signal(int signo, unsigned long handler)
{
  struct exec_context *current = get_current_ctx();
  (current->sighandlers)[signo] = (void *)handler; 
}

/*system call handler for alarm*/
long do_alarm(u32 ticks)
{
  struct exec_context *current = get_current_ctx();
  current->ticks_to_alarm = ticks;  
  //printf("check\n");
}
