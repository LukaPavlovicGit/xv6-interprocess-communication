#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler() ; typedef uint pde_t

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void
seginit(void)
{
	struct cpu *c;

	// Map "logical" addresses to virtual addresses using identity map.
	// Cannot share a CODE descriptor for both kernel and user
	// because it would have to have DPL_USR, but the CPU forbids
	// an interrupt from CPL=0 to DPL=3.
	c = &cpus[cpuid()];
	c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
	c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
	c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
	c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
	lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
	pde_t *pde; // page directory entry
	pte_t *pgtab;
	pde = &pgdir[PDX(va)];			// PDX(va) izvlaci gornjih 10 bita iz va i to ce biti index u direktorijumu (index >= 0 && index < 1024) ; kada mapiramo kernel u pgdir pde ce biti 512
							 // kada se mapira deljena memorija od roditelja u pgdir kod dece pde bi trebalo da bude 256, jer krecemo od 1GB da mapiramo
	if(*pde & PTE_P){      				 // dereferenciramo pde i proveravamo P bit (present bit) ; *pde ima 32 bita gornjih 20 bita predstavlja lokaciju tabele, donjih 12 su flagovi
		pgtab = (pte_t*)P2V(PTE_ADDR(*pde)); 	 // page table citamo iz gornjih 20 bitova iz stavke u direktorijumu (pde-a) ; PTE_ADDR(*pde) izvlaci gornjih 20 bita
	} else {	// kreira page tabelu
		if(!alloc || (pgtab = (pte_t*)kalloc()) == 0) {
			//cprintf("%s ", myproc()->name);			
			return 0;
		}
			
		// Make sure all those PTE_P bits are zero.
		memset(pgtab, 0, PGSIZE);
		// The permissions here are overly generous, but they can
		// be further restricted by the permissions in the page table
		// entries, if necessary.
		
		*pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U; // u page directory upisujemo gde se nalazi fizicka adresa tabele + flagove
	}
	return &pgtab[PTX(va)]; // PTX(va) - izvlaci srednjih 10 bita iz virtuelne adrese koji cine pokazivac na stavku u tabeli
				// vracamo pokazivac na stavku u tabeli ; povratna vrednost ce biti od 0 do 1023 ????
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might  not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm) 
{
	char *a, *last;
	pte_t *pte; 
	
	a = (char*)PGROUNDDOWN((uint)va); 
	last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
	for(;;){
		if((pte = walkpgdir(pgdir, a, 1)) == 0)
			return -1;
		if(*pte & PTE_P)
			panic("remap");	
					  
		*pte = pa | perm | PTE_P;
					  
		if(a == last)
			break;
		a += PGSIZE;
		pa += PGSIZE;
	}
	return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap { 		// opisuje jedan region u memoriji 		; memorija koju roditelj deli treba mapirati u virtuelnu memoriju dece pocevsi od 1GB, 
	void *virt;		// gde pocinje u virtuelnoj memoriji?		  dok kod roditelja memorija koja se deli se nalazi regularno u virtuelnoj memoriji u delu podataka
	uint phys_start;	// gde pocinje u fizickoj memoriji?		; kod roditelja i dece phys_start i phys_end treba da budu iste vrednosti, tj iste adrese 
	uint phys_end;		// gde se zavrsava u fizickoj memoriji?		; phys_end - phys_start = velicina regiona
	int perm;		// da li je ukljucen writable ? 		; jedno dete treba da moze da pise, a drugo dete treba samo da cita sa deljene memorije
} kmap[] = {
	{ (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space	  ; KERNBASE=2GB 
	{ (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
	{ (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
	{ (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table
pde_t*
setupkvm(void) // set up kernel virtual memory
{
	pde_t *pgdir;
	struct kmap *k; // 

	if((pgdir = (pde_t*)kalloc()) == 0) // kalloc - vraca slobodnu stranicu iz fizcke memorije velicine 4KB ; postoji uvezana lista slobodnih stranica fizicke memorije 
		return 0;
	memset(pgdir, 0, PGSIZE); // PGSIZE = 4KB ; P bit(present) od svih stavku u direktorijumu se setuje na 0 cime se naznacava da sve stavke u direktorijumu "nisu dobre"
	if (P2V(PHYSTOP) > (void*)DEVSPACE)
		panic("PHYSTOP too high");
	for(k = kmap; k < &kmap[NELEM(kmap)]; k++) // 4 prolaza kroz pretlju jer kmap ima 4 stavke (funkcija iznad)
		/* popunjava se pgdir pocevsi od k->virt adrese, mapirano na (uint)k->phys_start fizicku adresu, k->phys_end - k->phys_start bajtova i sa permisijama k->perm
		   k->phys_end-k->phys_start treba po modulu sa 4KB da daje 0, jer se u mappages mapira stranica po stranica 
		   Dakle, popunjavamo pgdir pocevsi od 2GB pa na vise, jer mapiramo kernel na pgdir		
		*/
		if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
		            (uint)k->phys_start, k->perm) < 0) {
			freevm(pgdir);
			return 0;
		}
	return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
	kpgdir = setupkvm(); // kpgdir - globalna promenljva, tipa je pde_t (types.h) ; kreira globalni direktorjum za kernel
	switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{	
	// ucitava cr3 registar ; cr3 registar sadrzi FIZICKU ADRESU direktorjma trenutno aktivnog procesa
	lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p) 	// switch user virtual memory
{				// postavlja u cr3 registar page directory od procesa p ; cr3 moze da ima samo fizicku adresu
	if(p == 0)
		panic("switchuvm: no process");
	if(p->kstack == 0)
		panic("switchuvm: no kstack");
	if(p->pgdir == 0)
		panic("switchuvm: no pgdir");

	pushcli();
	mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
		sizeof(mycpu()->ts)-1, 0);
	SEG_CLS(mycpu()->gdt[SEG_TSS]);
	mycpu()->ts.ss0 = SEG_KDATA << 3;
	mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
	// setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
	// forbids I/O instructions (e.g., inb and outb) from user space
	mycpu()->ts.iomb = (ushort) 0xFFFF;
	ltr(SEG_TSS << 3);
	lcr3(V2P(p->pgdir));  // switch to process's address space
	popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz) // initialize user virtuel memory
{
	char *mem;

	if(sz >= PGSIZE)
		panic("inituvm: more than a page");
	mem = kalloc(); // rezervise jednu stranicu koja ce biti stranica u korisnickom prostoru za init proces
	memset(mem, 0, PGSIZE);
	// u pgdir na virtuelnu adresu (0) mapira stranicu velicine PGSIZE (4KB) koja se nalazi na fizicku adresu (V2P(mem)) 
	mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
	memmove(mem, init, sz); // na adresi mem se sada nalazi kod init procesa
}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz) // load user virtual memory
{									  // loaduvm(pgdir, (char*)ph.vaddr, ip, ph.off, ph.filesz)
	uint i, pa, n;
	pte_t *pte;

	if((uint) addr % PGSIZE != 0)
		panic("loaduvm: addr must be page aligned");
	for(i = 0; i < sz; i += PGSIZE){
		if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
			panic("loaduvm: address should exist");
		pa = PTE_ADDR(*pte);
		if(sz - i < PGSIZE)
			n = sz - i;
		else
			n = PGSIZE;
		if(readi(ip, P2V(pa), offset+i, n) != n) // readi(struct inode *ip, char *destination, uint off, uint n)
			return -1;
	}
	return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz) // allocate user virtual memory ; dinamicna alokacija memorije
{
	char *mem;
	uint a;

	if(newsz >= SHAREBASE) // Ogranicavamo proces na 1GB
		return 0;
	if(newsz < oldsz)
		return oldsz;

	a = PGROUNDUP(oldsz); // zaokruzi na gore i u odnosu na novo a se prosurije memorija
	for(; a < newsz; a += PGSIZE){
		mem = kalloc(); // nova stranica u fizickoj memoriji
		if(mem == 0){
			cprintf("allocuvm out of memory\n");
			deallocuvm(pgdir, newsz, oldsz);
			return 0;
		}
		memset(mem, 0, PGSIZE);
		// u pgdir na virtuelnu adresu (a ) mapira stranicu velicine PGSIZE (4KB) koja se nalazi na fizicku adresu (V2P(mem))
		if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
			cprintf("allocuvm out of memory (2)\n");
			deallocuvm(pgdir, newsz, oldsz);
			kfree(mem);
			return 0;
		}
	}
	return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz) // deallocate user virtual memory
{
	pte_t *pte;
	uint a, pa;

	if(newsz >= oldsz)
		return oldsz;

	a = PGROUNDUP(newsz);
	for(; a  < oldsz; a += PGSIZE){
		pte = walkpgdir(pgdir, (char*)a, 0); // vraca page table entry
		if(!pte)
			a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
		else if((*pte & PTE_P) != 0){
			pa = PTE_ADDR(*pte); // pa je fizicka adrese gde se nalazi stranica
			if(pa == 0)
				panic("kfree");
			char *v = P2V(pa);
			kfree(v); // kalloc.c ; 
			*pte=0; // u page table entry upisujemo 0 ; dovoljno bi bilo i da se P bit obori na 0
		}
	}
	return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
	uint i;

	if(pgdir == 0)
		panic("freevm: no pgdir");
	deallocuvm(pgdir, SHAREBASE, 0);
	for(i = 0; i < NPDENTRIES; i++){
		if(pgdir[i] & PTE_P){
			char * v = P2V(PTE_ADDR(pgdir[i]));
			kfree(v); // kalloc.c
		}
	}
	kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = walkpgdir(pgdir, uva, 0);
	if(pte == 0)
		panic("clearpteu");
	*pte &= ~PTE_U;
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
	pde_t *d; // iz page directory pgdir kopira se sve u page directory d ; 
	pte_t *pte;
	uint pa, i, flags;
	char *mem;

	if((d = setupkvm()) == 0) // kreira novi direktorijum
		return 0;
	// kopira stari proces u novi ; kopira iste virtuelne stranice pocesa samo na drugu memoriju
	for(i = 0; i < sz; i += PGSIZE){
		if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0) // Return the address of the PTE in page table pgdir
			panic("copyuvm: pte should exist");	 // that corresponds to virtual address va.  If alloc!=0,
								 // create any required page table pages.				
		if(!(*pte & PTE_P))
			panic("copyuvm: page not present");
		pa = PTE_ADDR(*pte); 		// u pa smestamo gornjih 20 bita iz *pte
		flags = PTE_FLAGS(*pte); 	// u flags smestamo donjih 12 bita iz *pte koji predstavljaju flagove
		if((mem = kalloc()) == 0)	// kalloc alocira stranicu u fizickoj memoriji
			goto bad;
		
		memmove(mem, (char*)P2V(pa), PGSIZE);	// memmove ( void * destination, const void * source, size_t num );
/*
u mem kopiramo sve sta se nalazi na virtuelnoj adresi (char*)P2V(pa) velicine 4KB
Dakle u mem cemo klonirati celu stranicu virtuelne memorije na koju pokazuje (char*)P2V(pa)
*/
		// u direktorijum d mapira na virtuelnu adresu (i) stranicu velicine PGSIZE (4KB) koja se nalazi na fizickoj adresi (V2P(mem))
		// saljemo pocetak fizicke adrese na koju pokazuje mem, tj saljemo ???
		if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
			kfree(mem);
			goto bad;
		}
	}
	return d;

bad:
	freevm(d);
	return 0;
}

// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
	pte_t *pte;

	pte = walkpgdir(pgdir, uva, 0);
	if((*pte & PTE_P) == 0)
		return 0;
	if((*pte & PTE_U) == 0)
		return 0;
	return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
	char *buf, *pa0;
	uint n, va0;

	buf = (char*)p;
	while(len > 0){
		va0 = (uint)PGROUNDDOWN(va);
		pa0 = uva2ka(pgdir, (char*)va0);
		if(pa0 == 0)
			return -1;
		n = PGSIZE - (va - va0);
		if(n > len)
			n = len;
		memmove(pa0 + (va - va0), buf, n);
		len -= n;
		buf += n;
		va = va0 + PGSIZE;
	}
	return 0;
}

/*
	Ako je deljena memorija veca od PGSIZE potrebno je svakoj stranici da pronadjemo pocetak njene fizicku adrese u memoriji
	Na svakih PGSIZE podeljebe memorije, u parentpgdir cemo morati da trazimo fizicku adresu sledece stranice koju zelimo da podelimo
	U svakoj iteraciji maprimo po jednu fizicku stranicu PGSIZE sa fizicke lokacije 'pa' na virtuelnu adresu 'a' + i*1024
	
	Mapiramo X stranica
*/

int
mappa2va(pde_t *pgdir, pde_t *parentpgdir, uint sz, char *memstart, uint *va, int perm)
{	
	struct proc *curproc = myproc();
	pte_t *pte;
	uint pa, a, X, Y;
	
	if(va + sz >= KERNBASE)
		return -1;

	a = PGROUNDUP((uint)va);
	sz = PGROUNDUP(sz);
	
	for(X=0 ; X<sz ; X+=PGSIZE){
		
		if((pte = walkpgdir(parentpgdir, memstart, 0)) == 0)
			return -1;
		if(!(*pte & PTE_P)){
			cprintf("linkphysaddr : !(*parentpte & PTE_P)\n");
			return -1;	
		}
		
		pa = PTE_ADDR(*pte);

		if(mappages(pgdir, (uint*)a, PGSIZE, pa, perm) < 0){
			cprintf("mappa2va\n");
			deallocuvm(pgdir, va + sz, va);
			return -1;
		}	

		a += PGSIZE;
		memstart += PGSIZE;
	}
	
	return sz;
}

//define PTE_W           0x002   // Writeable
//define PTE_U           0x004   // User
int
linkphysaddr(pde_t *pgdir, int Wbit) // allocate user virtual memory ; dinamicna alokacija memorije
{	
	pde_t *pde;
	pte_t *pte;
	uint *x, L12bits, sz, perm;
	char *va = SHAREBASE, *pa, *memstart;
	struct proc *curproc = myproc(); // kopija procesa primestart-a, u kome se nalazi parentpgdir. To je zapravo direktorijum od originalnog primestart-a

	if(Wbit)
		perm = 6; // PTE_U + PTE_W ;
	else 
		perm = 4; // PTE_U;
	
	for(int i=0; ;++i){
	
		if(curproc->shared[i].sz == 0)
			break;
		
		L12bits = PTE_FLAGS(curproc->shared[i].memstart);
		
		if((sz = mappa2va(pgdir, curproc->parentpgdir, curproc->shared[i].sz, curproc->shared[i].memstart, (void *)va, perm)) < 0){
			freevm(pgdir);
			return -1;
		}
		curproc->shared[i].memstart = va + L12bits; // u parent procesu ne dolazi do promene
		va = va + sz;				    // za 'va' uvek vazi: va % 4KB = 0
	}
	
	return 1;
}
