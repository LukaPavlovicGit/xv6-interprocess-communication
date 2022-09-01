#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

int
main(void)
{
	provera();
	exit();
}
/*void flagscheck(){

	int x;
	struct proc *curproc = myproc();
	x=curproc->idx;

	pde_t *pde;
	pte_t *pte;
	
	
	cprintf("child:\n");
	for(int i=0 ; i<x ; i++){
		pde = &curproc->pgdir[PDX(curproc->shared[i].memstart)];
		pte = walkpgdir(curproc->pgdir, curproc->shared[i].memstart, 0);
		cprintf("pde: ", curproc->shared[i].memstart);
		if(*pde & PTE_P)
			cprintf("PTE_P ");
		if(*pde & PTE_W)
			cprintf("PTE_W ");
		if(*pde & PTE_U)
			cprintf("PTE_U ");
		if(*pde & PTE_PS)
			cprintf("PTE_PS ");

		cprintf("\npte: ");
		
		if(*pte & PTE_P)
			cprintf("PTE_P ");
		if(*pte & PTE_W)
			cprintf("PTE_W ");
		if(*pte & PTE_U)
			cprintf("PTE_U ");
		if(*pte & PTE_PS)
			cprintf("PTE_PS ");

		cprintf("\n");
	
	}
	
	cprintf("parent:\n");
	for(int i=0 ; i<x ; i++){
		pde = &curproc->parentpgdir[PDX(curproc->shared[i].memstart)];
		pte = walkpgdir(curproc->parentpgdir, curproc->shared[i].memstart, 0);
		
		cprintf("pde: ", curproc->shared[i].memstart);
		if(*pde & PTE_P)
			cprintf("PTE_P ");
		if(*pde & PTE_W)
			cprintf("PTE_W ");
		if(*pde & PTE_U)
			cprintf("PTE_U ");
		if(*pde & PTE_PS)
			cprintf("PTE_PS ");

		cprintf("\npte: ");
	
		if(*pte & PTE_P)
			cprintf("PTE_P ");
		if(*pte & PTE_W)
			cprintf("PTE_W ");
		if(*pte & PTE_U)
			cprintf("PTE_U ");
		if(*pte & PTE_PS)
			cprintf("PTE_PS ");

		cprintf("\n");
	}	
}
*/
