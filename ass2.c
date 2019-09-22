#include <init.h>
#include <lib.h>
#include <context.h>
#include <memory.h>


void clean_pages(u64* VirtualAddress, u64 min_addr, u64 max_addr, int level)
{
  if(level == 1)
  {
    //printf("Level1\n");
    //printf("min_addr = %x\n",min_addr);
    //printf("max_addr = %x\n",max_addr);
    
    u64 min_offset = (min_addr>>12) & 0x1FF;
    u64 max_offset = (max_addr>>12) & 0x1FF;
    u64 diff = max_offset-min_offset+1;
    u64* current_addr = VirtualAddress + min_offset;
    for(int i=0;i<diff;i++)
    {
      u64 pfn = (*(current_addr))>>12;
      //printf("%x\n",pfn);
      if(((*current_addr)&1) )
        os_pfn_free(USER_REG,pfn);
      *current_addr = 0;
      current_addr = current_addr + 1;
      
    }
    //printf("laaf\n");
    return;
    //call function
  }

  int t = 39-(4-level)*9;//amount of shift needed
  u64 bound = 0xffffffffff;
  //printf("bound = %x\n",bound);
  
  u64 min_offset = min_addr>>t & 0x1FF;
  u64 max_offset = (max_addr>>t) & 0x1FF;
  u64 diff = max_offset-min_offset;
  
  if(diff==0)
  {
    //printf("pag0\n");
    u64* current_addr = VirtualAddress + min_offset;
    u64 pfn = (*(current_addr))>>12;
    u64* new_addr = osmap(pfn);
    u64 min_limit = min_addr;
    u64 max_limit = max_addr;
    //printf("min_limit = %x",min_limit);
    //printf("max_limit = %x",max_limit);
    
    clean_pages(new_addr,min_limit,max_limit,level-1);
    //if(min_limit == 0 && max_limit == bound)
    //  *current_addr = 0x0;
    //printf("pag0exit\n");
    return;
  }
  

  u64* current_addr = VirtualAddress + min_offset;
  u64 pfn = (*(current_addr))>>12;
  u64* new_addr = osmap(pfn);
  u64 min_limit = min_addr;
  u64 max_limit = bound;
  clean_pages(new_addr,min_limit,max_limit,level-1);
  
  //printf("pag1_exit\n");

  for(u64 i=1;i<diff;i++)
  {
    
    //printf("pag2_enter\n");
    current_addr = current_addr + 1;
    u64 pfn = (*(current_addr))>>12;
    u64* new_addr = osmap(pfn);
    u64 max_limit = bound;
    clean_pages(new_addr,0,max_limit,level-1);
    //printf("pag2_exit\n");
  }
  

  //printf("pag3_enter\n");
  current_addr = VirtualAddress+max_offset;
  pfn = (*(current_addr))>>12;
  new_addr = osmap(pfn);
  max_limit = max_addr;
  clean_pages(new_addr,0,max_limit,level-1);

  //printf("pag3_exit\n");
  //if(max_limit == bound)
  //  *current_addr = 0x0;

}


/*System Call handler*/
long do_syscall(int syscall, u64 param1, u64 param2, u64 param3, u64 param4)
{
    struct exec_context *current = get_current_ctx();
    printf("[GemOS] System call invoked. syscall no  = %d\n", syscall);
    switch(syscall)
    {
          case SYSCALL_EXIT:
                              printf("[GemOS] exit code = %d\n", (int) param1);
                              do_exit();
                              break;
          case SYSCALL_GETPID:
                              printf("[GemOS] getpid called for process %s, with pid = %d\n", current->name, current->id);
                              return current->id;      
          case SYSCALL_WRITE:
                             {  
                                //printf("param1 = %x\n",param1);
                                if(param2 > 1024 || param2<0)
                                  return -1;
                                u64 addr = param1;
                                u32 page_offset = addr&0xfff;
                                int no_of_pages = 1;
                                if(page_offset + param2 > (0x1 << 12))
                                  no_of_pages =2;

                                
                                u32 pfn= current->pgd;
                                u64* Virtual;
                                int count =0;
                                int i = 39;
                                while(count<4)
                                {
                                  //printf("%d\n",i);
                                  Virtual =(u64 *)osmap(pfn);
                                  u32 offset = (addr >> i) & 0x1FF;
                                  Virtual = Virtual + offset;
                                  //printf("PTE = %x\n",*Virtual);
                                  int temp2 = (*Virtual) & 1;
                                  //printf("temp2 = %d\n",temp2);
                                  if( temp2 == 0)
                                   return -1;
                                  
                                  pfn = ((*Virtual)>>12);
                                  count++;
                                  i=i-9;
                                }

                                u64* Virtual2;
                                u32 pfn2= current->pgd;
                                u64 new_addr;
                                if(no_of_pages == 2)
                                {
                                  new_addr = ((addr>>12)+1)<<12;  
                                  int count =0;
                                  int i = 39;
                                  while(count<4)
                                  {
                                    Virtual2 =(u64 *)osmap(pfn2);
                                    u32 offset = (new_addr >> i) & 0x1FF;
                                    Virtual2 = Virtual2 + offset;
                                    int temp2 = (*Virtual2) & 1;
                                    if(temp2 == 0)
                                    return -1;
                                    pfn2 = ((*Virtual2)>>12);
                                    count++;
                                    i=i-9;
                                  }
                                }

                                char* temp = (char*)param1;
                                for(u64 i=0;i<param2;i++)
                                {
                                  printf("%c",*(temp+i) );
                                }
                                //printf("\n");
                                return param2;

                             }
          case SYSCALL_EXPAND:
                             {  
                                if(param1>512 || param1<0)
                                  return (u64)NULL;
                                u64 flags;
                                if(param2 == MAP_RD)
                                  flags = MM_SEG_RODATA;
                                if(param2 == MAP_WR)
                                  flags = MM_SEG_DATA;
                                
                                u64 pages = param1<<12;
                                u64 n_free = current->mms[flags].next_free;
                                u64 end = current->mms[flags].end;
                                if(pages + n_free > end+1)
                                  return (u64)NULL;
                                current->mms[flags].next_free = n_free+pages;
                                //printf("%x\n",end);
                                //printf("%x\n",n_free);
                                //printf("%x\n",current->mms[flags].next_free);
                                return n_free;


                                     /*Your code goes here*/
                             }
          case SYSCALL_SHRINK:
                             {  
                                if(param1<0)
                                  return (u64)NULL;
                                u64 flags;
                                if(param2 == MAP_RD)
                                  flags = MM_SEG_RODATA;
                                if(param2 == MAP_WR)
                                  flags = MM_SEG_DATA;
                                
                                u64 n_free = current->mms[flags].next_free;
                                u64 s = current->mms[flags].start;
                                if (n_free < (param1<<12))
                                  return (u64)NULL;

                                u64 min_addr = n_free - (param1<<12);
                                if(min_addr < s)
                                  return (u64)NULL;
                                
                                u64 max_addr = n_free-1;
                                u64 cr3 = current->pgd;
                                u64* Virtual = osmap(cr3);
                                clean_pages(Virtual,min_addr,max_addr,4);
                                current->mms[flags].next_free = min_addr;
				asm volatile("mov %cr3,%rax");
				asm volatile("mov %rax,%cr3");
				return min_addr;
                             }
                             
          default:
                              return -1;
                                
    }
    return 0;   /*GCC shut up!*/
}

extern int handle_div_by_zero(void)
{
    /*Your code goes in here*/
    //register int temp asm("ebx");
    u64* i;
    asm volatile("mov %%rbp, %0" : "=r"(i));
    //printf("%x\n",*i);
    printf("Div-by-zero detected at %x\n",*(i+1));
    do_exit();
    //printf("After exit");
    return 0;
}

extern int handle_page_fault(void)
{
    u64* last_Addr;
    asm volatile("push %rax");
    asm volatile("push %rbx");
    asm volatile("push %rcx");
    asm volatile("push %rdx");
    asm volatile("push %rsi");
    asm volatile("push %rdi");
    asm volatile("push %r8");
    asm volatile("push %r9");
    asm volatile("push %r10");
    asm volatile("push %r11");
    asm volatile("push %r12");
    asm volatile("push %r13");
    asm volatile("push %r14");
    asm volatile("push %r15");

    asm volatile("mov %%rsp, %0" : "=r"(last_Addr));
    
    
    /*Your code goes in here*/
    u64 Virtual;
    
    asm volatile("mov %%cr2, %0" : "=r"(Virtual));
    

    struct exec_context *current = get_current_ctx();
    //printf("%x\n",current->pgd);
    //check for end inequality

    if(current->mms[MM_SEG_DATA].start <= Virtual &&  current->mms[MM_SEG_DATA].end >= Virtual)
    {
      u64 start = current->mms[MM_SEG_DATA].start;
      u64 n_free = current->mms[MM_SEG_DATA].next_free;
      u32 flags = current->mms[MM_SEG_DATA].access_flags & 2;
      //printf("flags = %x\n",flags);
      u64* sp;
      asm volatile("mov %%rbp, %0" : "=r"(sp));
      
      if(Virtual>= n_free)
      {
        printf("Error Code at Virtual address = %x,RIP =  %x, Error Code = %x\n",Virtual,*(sp+2),*(sp+1));  
        do_exit();
      }
      else
      {
        u64 pfn = current->pgd;
        u64 shift = 39;
        u64* VirtualAddress;
        while(shift>=21)
        {
          VirtualAddress = osmap(pfn);
          u64 offset = ((Virtual)>>shift) & 0x1ff;
          VirtualAddress = VirtualAddress + offset;
          int tmp = (*VirtualAddress) & 1;
          if(tmp ==0)
          {
            pfn = os_pfn_alloc(OS_PT_REG);
            u64* new_addr = osmap(pfn);
            for(int i=0;i<512;i++)
              *(new_addr+i) =0;
            *(VirtualAddress) = (pfn<<12 ) | 5 | flags;
          } 
          pfn = (*VirtualAddress)>>12;
          shift = shift - 9;
        }

        VirtualAddress = osmap(pfn);
        u64 offset = ((Virtual)>>shift) & 0x1ff;
        VirtualAddress = VirtualAddress + offset;
        u64 new_pfn = os_pfn_alloc(USER_REG);
        *(VirtualAddress) = (new_pfn<<12 ) | 5 | flags;
      }
    }
    else if(current->mms[MM_SEG_RODATA].start <= Virtual &&  current->mms[MM_SEG_RODATA].end >= Virtual)
    {
      u64 start = current->mms[MM_SEG_RODATA].start;
      u64 n_free = current->mms[MM_SEG_RODATA].next_free;
      u32 flags = current->mms[MM_SEG_RODATA].access_flags & 2;
      u64* sp;
      asm volatile("mov %%rbp, %0" : "=r"(sp));
      
      if(Virtual>= n_free || ((*(sp+1)) & 2)==2)
      {
        printf("Error Code at Virtual address = %x,RIP =  %x, Error Code = %x\n",Virtual,*(sp+2),*(sp+1));  
        do_exit();
      }
      else
      {
        u64 pfn = current->pgd;
        u64 shift = 39;
        u64* VirtualAddress;
        while(shift>=21)
        {
          VirtualAddress = osmap(pfn);
          u64 offset = ((Virtual)>>shift) & 0x1ff;
          VirtualAddress = VirtualAddress + offset;
          int tmp = (*VirtualAddress) & 1;
          if(tmp ==0)
          {
            u64 new_pfn = os_pfn_alloc(OS_PT_REG);
            u64* new_addr = osmap(new_pfn);
            for(int i=0;i<512;i++)
              *(new_addr+i) =0;
            *(VirtualAddress) = (new_pfn<<12 ) | 5 | flags;
          } 
          pfn = (*VirtualAddress)>>12;
          shift = shift - 9;
        }
        VirtualAddress = osmap(pfn);
        u64 offset = ((Virtual)>>shift) & 0x1ff;
        VirtualAddress = VirtualAddress + offset;
        u64 new_pfn = os_pfn_alloc(USER_REG);
        *(VirtualAddress) = (new_pfn<<12 ) | 5|flags;
        //printf("pfn = %x\n",new_pfn);
        //printf("VA = %x\n",*VirtualAddress);
      }
    }
    else if(current->mms[MM_SEG_STACK].start <= Virtual &&  current->mms[MM_SEG_STACK].end > Virtual)
    {
      u64 start = current->mms[MM_SEG_STACK].start;
      u32 flags = current->mms[MM_SEG_STACK].access_flags & 2;
      u64* sp;
      asm volatile("mov %%rbp, %0" : "=r"(sp));
      
      u64 pfn = current->pgd;
      u64 shift = 39;
      u64* VirtualAddress;
      while(shift>=21)
      {
        VirtualAddress = osmap(pfn);
        u64 offset = ((Virtual)>>shift) & 0x1ff;
        VirtualAddress = VirtualAddress + offset;
        int tmp = (*VirtualAddress) & 1;
        if(tmp ==0)
        {
          pfn = os_pfn_alloc(OS_PT_REG);
          u64* new_addr = osmap(pfn);
          for(int i=0;i<512;i++)
            *(new_addr+i) =0;
          *(VirtualAddress) = (pfn<<12 ) | 5 | flags;
        } 
        pfn = (*VirtualAddress)>>12;
        shift = shift - 9;
      }

      VirtualAddress = osmap(pfn);
      u64 offset = ((Virtual)>>shift) & 0x1ff;
      VirtualAddress = VirtualAddress + offset;
      u64 new_pfn = os_pfn_alloc(USER_REG);
      *(VirtualAddress) = (new_pfn<<12 ) | 5 | flags;
      
    }
    else
    {
      u64* sp;
      asm volatile("mov %%rbp, %0" : "=r"(sp));
      printf("Error Code at Virtual address = %x,RIP =  %x, Error Code = %x\n",Virtual,*(sp+2),*(sp+1));  
      do_exit();
    }


    asm volatile("mov %0, %%rsp" :: "r"(last_Addr));
    
    asm volatile("pop %r15");
    asm volatile("pop %r14");
    asm volatile("pop %r13");
    asm volatile("pop %r12");
    asm volatile("pop %r11");
    asm volatile("pop %r10");
    asm volatile("pop %r9");
    asm volatile("pop %r8");
    asm volatile("pop %rdi");
    asm volatile("pop %rsi");
    asm volatile("pop %rdx");
    asm volatile("pop %rcx");
    asm volatile("pop %rbx");
    asm volatile("pop %rax");
    

    
    asm volatile("mov %rbp,%rsp");
    asm volatile("pop %rbp");
    asm volatile("add $8, %rsp");

    asm volatile("iretq");
                    
    return 0;
}
