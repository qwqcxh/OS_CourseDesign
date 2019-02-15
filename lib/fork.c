// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).
	// LAB 4: Your code here.
	if(!((err&FEC_WR)&&(uvpt[PGNUM(addr)] & PTE_COW)))
		panic("pgfault: VA of %x isn't COW page fault\n",addr);

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	r = sys_page_alloc(0,PFTEMP,PTE_U|PTE_W|PTE_P);
	if(r < 0) panic("pgfault:%e\n",r);
	//copy the page which addr belongs to to a new allocated page
	memcpy(PFTEMP,(void*)ROUNDDOWN(addr,PGSIZE),PGSIZE);
	r = sys_page_map(0,PFTEMP,0,ROUNDDOWN(addr,PGSIZE),PTE_U|PTE_W|PTE_P);
	if(r < 0) panic("pgfault:%e\n",r);
	r = sys_page_unmap(0,PFTEMP);
	if(r < 0) panic("pgfault:%e\n",r);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;

	// LAB 4: Your code here.
	void *addr = (void*)(pn*PGSIZE);
	if((uvpt[pn] & PTE_W) || (uvpt[pn] & PTE_COW)){
		if((r=sys_page_map(0,addr,envid,addr,PTE_COW|PTE_P|PTE_U))<0)
			panic("duppage: %e\n",r);
		if((r=sys_page_map(0,addr,0,addr,PTE_COW|PTE_P|PTE_U))<0)
			panic("duppage: %e\n",r);			
	}
	else{
		if((r=sys_page_map(0,addr,0,addr,PTE_P|PTE_U))<0){
			cprintf("page num is %d\n",pn);
			panic("duppage: %e\n",r);
		}
	}
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	extern void _pgfault_upcall(void);
	set_pgfault_handler(pgfault);
	envid_t pchild = sys_exofork();
	if(pchild<0) return pchild;
	else if(pchild == 0){ //子进程
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}
	else{ //父进程
		uint32_t addr;
		for(addr=0x400000;addr <USTACKTOP;addr+=PGSIZE){
			if((uvpd[PDX(addr)] & PTE_P)&&
			((uvpt[PGNUM(addr)] & (PTE_P|PTE_U))==(PTE_P|PTE_U))){
				duppage(pchild,PGNUM(addr));
			}
		}
		int r;
		if((r = sys_page_alloc(pchild,(void*)(UXSTACKTOP-PGSIZE),PTE_U|PTE_W|PTE_P))<0)
			panic("fork:%e\n",r);
		if((r = sys_env_set_pgfault_upcall(pchild,_pgfault_upcall))<0)
			panic("fork:%e\n",r);
		if((r = sys_env_set_status(pchild,ENV_RUNNABLE))<0)
			panic("fork:%e\n",r);
		return pchild;
	}
}

// Challenge!
int
sfork(void)
{
	panic("sfork not implemented");
	return -E_INVAL;
}
