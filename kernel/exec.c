#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

/*
	exec treba da procita program sa diska i da pripremi memoriju za taj program, tako da on moze da se izvrsava
*/

int
exec(char *path, char **argv)
{			
	char *s, *last;
	int i, off;   
	uint argc, sz, sp, ustack[3+MAXARG+1];
	struct elfhdr elf;
	struct inode *ip;
	struct proghdr ph;
	pde_t *pgdir, *oldpgdir;
	struct proc *curproc = myproc();

	begin_op();

	if((ip = namei(path)) == 0){ // vraca inode za path (na kraju patha se nalazi izvrsni kod koji zelimo da stavimo na izvrsavanje)
		end_op();
		cprintf("exec: fail\n");
		return -1;
	}
	ilock(ip);
	pgdir = 0;

	// Check ELF header ; ELF je izvrsni fajl
	if(readi(ip, (char*)&elf, 0, sizeof(elf)) != sizeof(elf)) // readi(struct inode *ip, char *destination, uint off, uint n)
		goto bad;											  // citamo izvrsni program sa diska koji treba da se stavi na izvrsavanje
	if(elf.magic != ELF_MAGIC)
		goto bad;

	if((pgdir = setupkvm()) == 0) // vm.c ; priprema direktorijum za novi proces, direktorijum je popunjem sa kernelom od 2GB pa navise
		goto bad;				  
	
	// Load program into memory.
	sz = 0; 	// sz ce biti velicina novog procesa
	for(i=0, off=elf.phoff; i<elf.phnum; i++, off+=sizeof(ph)){ // prolazimo kroz svako zaglavlje
		if(readi(ip, (char*)&ph, off, sizeof(ph)) != sizeof(ph)) // ph je proghdr - Program section header
			goto bad;
		if(ph.type != ELF_PROG_LOAD)
			continue;
		if(ph.memsz < ph.filesz)
			goto bad;
		if(ph.vaddr + ph.memsz < ph.vaddr)
			goto bad;											 
		if((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0) // allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
			goto bad;				         // vm.c ; allcoira dodatnu memoriju za proces u korisnickom delu
		if(ph.vaddr % PGSIZE != 0)
			goto bad;
		//if(strncmp("primecalc", argv[0], SHAREDNAME) == 0 ){
		//	cprintf("sz:%d ph.vaddr + ph.memsz%d\n", sz, ph.vaddr + ph.memsz);		
		//}
		if(loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz) < 0) // vm.c ; loaduvm - Load a program segment from disk into pgdir.  addr must be page-aligned
			goto bad;						      // and the pages from addr to addr+sz must already be mapped.
	}									      // loaduvm(pde_t *pgdir, char *va, struct inode *ip, uint offset, uint sz)
	iunlockput(ip);
	end_op();
	ip = 0;
	
	if(strncmp("primecalc", argv[0], SHAREDNAME) == 0 )
		linkphysaddr(pgdir, 1);		// procesu primecalm dozvoljavamo da pise u deljenu memoriju
									
	else if(strncmp("primecom", argv[0], SHAREDNAME) == 0)
		linkphysaddr(pgdir, 1);		// procesu primecom dozvoljavamo da pise u deljenu memoriju				
	
	
	// Allocate two pages at the next page boundary.
	// Make the first inaccessible.  Use the second as the user stack.
	// alocira se dve stranice za stek, s tim sto se koristi samo gordnja od ove dve, i stek raste ka dole i kada dodje do prve stranice izbacuje gresku (page fault)
	sz = PGROUNDUP(sz);
	if((sz = allocuvm(pgdir, sz, sz + 2*PGSIZE)) == 0)
		goto bad;
	clearpteu(pgdir, (char*)(sz - 2*PGSIZE)); // obaramo user bit na donjoj stranici steka od 2 alocirane
	sp = sz; 								  // stek pointer pokazuje na vrh druge stranice od 2 alocirane (to je zapravo kraj steka)
	
	// Push argument strings, prepare rest of stack in ustack. ; stavlja argumente na stek
	for(argc = 0; argv[argc]; argc++) {
		if(argc >= MAXARG)
			goto bad;
		sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
																
		if(copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)  // copyout(pde_t *pgdir, uint va, void *p, uint len)
			goto bad;						// Copy len bytes from p to user address va in page table pgdir
		ustack[3+argc] = sp;						// Most useful when pgdir is not the current page table.
	}	
	
	ustack[3+argc] = 0;

	ustack[0] = 0xffffffff;  // fake return PC
	ustack[1] = argc;
	ustack[2] = sp - (argc+1)*4;  // argv pointer
	
	sp -= (3+argc+1) * 4; // pomeramo vrh steka
	
	if(copyout(pgdir, sp, ustack, (3+argc+1)*4) < 0)// copyout(pde_t *pgdir, uint va, void *p, uint len) 
		goto bad;			        // Copy len bytes from p to user address va in page table pgdir.
							// Most useful when pgdir is not the current page table.

	// Save program name for debugging.
	for(last=s=path; *s; s++)
		if(*s == '/')
			last = s+1;

	safestrcpy(curproc->name, last, sizeof(curproc->name));	

	// Commit to the user image.
	
	oldpgdir = curproc->pgdir;
	curproc->pgdir = pgdir;
	curproc->sz = sz;
	curproc->tf->eip = elf.entry;  // main ; elf.entry je pokazivac na main funkciju programa
	curproc->tf->esp = sp;
	switchuvm(curproc);     	   // postavlja u cr3 registar page directory od procesa curproc
	freevm(oldpgdir);	
	
	
	return 0;		       // kada se odradi return, program sa trapframe-a skida argumente cs i ip i skace na tu lokaciju (elf.entry)
				       // prethodno je program ucitan u memoriju

	bad:
	if(pgdir)
		freevm(pgdir);
	if(ip){
		iunlockput(ip);
		end_op();
	}
	return -1;
}
