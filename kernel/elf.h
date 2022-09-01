// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
struct elfhdr {
	uint magic;  // must equal ELF_MAGIC
	uchar elf[12];
	ushort type;
	ushort machine;
	uint version;
	uint entry;
	uint phoff;
	uint shoff;
	uint flags;
	ushort ehsize;
	ushort phentsize;
	ushort phnum;
	ushort shentsize;
	ushort shnum;
	ushort shstrndx;
};

/* 
Program section header
Logicki opisuje jedan segment programa ; segment moze biti code, data, stack 
*/
struct proghdr {
	uint type;	// code, data, stack 
	uint off;	
	uint vaddr;	// virtuelna adresa segmenta
	uint paddr;	// fizicka adresa segmenta 
	uint filesz;	// velicina segmenta u file sistemu
	uint memsz;	// velicina segmenta u memoriji ; memsz >= filesz !
	uint flags;
	uint align;
};

// Values for Proghdr type
#define ELF_PROG_LOAD           1

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4
