// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
#include "kernel/fcntl.h"

char *argv[] = { "sh", 0 };

int
main(void)
{
	int pid, wpid;

	if(getpid() != 1){
		fprintf(2, "init: already running\n");
		exit();
	}

	if(open("/dev/console", O_RDWR) < 0){
		mknod("/dev/console", 1, 1);
		open("/dev/console", O_RDWR);
	}
	dup(0);  // stdout
	dup(0);  // stderr

	for(;;){
		printf("init: starting sh\n");
		pid = fork();			// pid parenta je id child-a, pid child je 0
		if(pid < 0){
			printf("init: fork failed\n");
			exit();
		}
		if(pid == 0){
			exec("/bin/sh", argv);
			printf("init: exec sh failed\n");
			exit();
		}
		while((wpid=wait()) >= 0 && wpid != pid) // wpid=wait() - ceka da se zavrsi shell ( exec("/bin/sh", argv) ) kojeg je pozvao child proces
			printf("zombie!\n");		 // kada child pozove exit, to ce da odblokira parenta, i perent unutar poziva wait() cisti child (iz zombija prebacuje u unused)
	}					// ocekujemo da je wpid == pid , jer wait() vraca id procesa koji se zavrsio u shellu (u ovom slucaju to ce biti id child-a) a pid od parenta je id childa
						// svaki put kada se shell ugasi, proces se opet zavrti u petlji i opet ga poziva preko child-a
}
