#include <context.h>
#include <memory.h>
#include <lib.h>


void create_entry(u64* Virtual_Addr, u32 access_flags,int i)
{
	u32 page_next_level;
	if(i==3) 
	{
		page_next_level = os_pfn_alloc(USER_REG);
	}
	else 
		page_next_level = os_pfn_alloc(OS_PT_REG);

	u64* virtual_pgd = (u64 *)osmap(page_next_level);
	u32 offset = 0;
	for(;offset<512;offset = offset +1)
	{
		*(virtual_pgd + offset) = 0x0; //initialization
	}
	
	access_flags = (access_flags & 2);	
	*(Virtual_Addr) = (page_next_level<<12 ) | access_flags | 5;
}

void create_table(u64 addr, u32 f, u32 PFN, int k,u32 data)
{
	u64* Virtual_start =(u64 *)osmap(PFN);
	u32 offset = (addr >> 39) & 0x1FF;		
	u64* Virtual_Addr = Virtual_start + (offset);
	int i=30;
	while(i>=0)
	{
		if ((*Virtual_Addr & 0x1) == 0)//no entry is present
		{
			printf("i = %d\n",i);
			if(k==1 && i==3)
			{
				*(Virtual_Addr) = ( data<<12) | (f&2) | 5;
				return;
			}
			create_entry(Virtual_Addr,f,i);
			printf("%x\n",(*Virtual_Addr));
			if(i==3)
			{
				break;
			}
		}
		else
		{
			u32 bit = (f&2);
			(* Virtual_Addr) = (* Virtual_Addr) | bit; 
		}
		PFN = (* Virtual_Addr)>>12;
		Virtual_start =(u64 *)osmap(PFN);
		offset = (addr >> i) & 0x1FF;
		i = i-9;
		Virtual_Addr = Virtual_start + (offset);// where entry has to be placed
	
	}

}

void prepare_context_mm(struct exec_context *ctx)
{	
	ctx->pgd = os_pfn_alloc(OS_PT_REG);
	u64* virtual_pgd = (u64 *)osmap(ctx->pgd);
	u32 offset = 0;
	for(;offset<512;offset = offset + 1)
	{
		*(virtual_pgd + offset) = 0x0; //initialization
	}
		

	u32 cr3 = ctx->pgd << 12;
	
	u32 data = ctx->arg_pfn;
	//entry for code-----------------------------------------
	u64 addr = ctx->mms[MM_SEG_CODE].start;
	u32 f = ctx->mms[MM_SEG_CODE].access_flags;
	u32 PFN = ctx->pgd;
	create_table(addr,f,PFN,0,data);

	//entry for stack---------------------------------------
	addr = ctx->mms[MM_SEG_STACK].end - 0x1000;
	f = ctx->mms[MM_SEG_STACK].access_flags;
	PFN = ctx->pgd;
	create_table(addr,f,PFN,0,data);
	
	addr = ctx->mms[MM_SEG_DATA].start;
	f = ctx->mms[MM_SEG_DATA].access_flags;
	PFN = ctx->pgd;
	create_table(addr,f,PFN,1,data);

	return;
}


void delete_PFN(u64 addr1,u64 addr2,u64 addr3,u64 PFN1,u64 PFN2,u64 PFN3,int i)
{
	if(i==3)
	{
		os_pfn_free(USER_REG,PFN1);
    	os_pfn_free(USER_REG,PFN2);
    	os_pfn_free(USER_REG,PFN3);
    	return;
	}
		
	u32 offset = (addr1 >> i) & 0x1FF;	
	u64* Virtual_start =(u64 *)osmap(PFN1);
	u64* Virtual_Addr1 = Virtual_start + (offset);
	u64 page_entry = (*Virtual_Addr1);
	u64 PFN11 = page_entry>>12;

	offset = (addr2 >> i) & 0x1FF;	
	Virtual_start =(u64 *)osmap(PFN2);
	Virtual_Addr1 = Virtual_start + (offset);
	page_entry = (*Virtual_Addr1);
	u64 PFN22 = page_entry>>12;

	offset = (addr3 >> i) & 0x1FF;	
	Virtual_start =(u64 *)osmap(PFN3);
	Virtual_Addr1 = Virtual_start + (offset);
	page_entry = (*Virtual_Addr1);
	u64 PFN33 = page_entry>>12;

	delete_PFN(addr1,addr2,addr3,PFN11,PFN22,PFN33,i-9);

	if(PFN1 == PFN2)
	{
		if(PFN1 == PFN3)
		{
			os_pfn_free(OS_PT_REG,PFN1);
    	}
    	else
    	{
    		os_pfn_free(OS_PT_REG,PFN1);
    		os_pfn_free(OS_PT_REG,PFN3);
    	}
	}
	else
	{
		if(PFN1 == PFN3)
		{
			os_pfn_free(OS_PT_REG,PFN1);
    		os_pfn_free(OS_PT_REG,PFN2);
    	}
    	else if(PFN2 == PFN3)
    	{
    		os_pfn_free(OS_PT_REG,PFN1);
    		os_pfn_free(OS_PT_REG,PFN2);
    	}
    	else
    	{
    		os_pfn_free(OS_PT_REG,PFN1);
    		os_pfn_free(OS_PT_REG,PFN2);
    		os_pfn_free(OS_PT_REG,PFN3);
    	}
	}
}

void cleanup_context_mm(struct exec_context *ctx)
{
	u32 PFN = ctx->pgd;
	u64 addr1 = ctx->mms[MM_SEG_CODE].start;
	u64 addr2 = ctx->mms[MM_SEG_STACK].end - 0x1000;
	u64 addr3 = ctx->mms[MM_SEG_DATA].start;
	delete_PFN(addr1,addr2,addr3,PFN,PFN,PFN,39);
	return;
}
