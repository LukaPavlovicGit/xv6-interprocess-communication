#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

char *argvcalc[] = { "primecalc", 0 };
char *argvcom[] = { "primecom", 0 };

int
main(void)
{	
	uint idx=0, cmd=0, pid, wpid1, wpid2, w=1, c=1;
	uint *primes = malloc(100000*sizeof(int));
	
	memset(primes, 0, 100000);

	share_mem("index", &idx, 4);
	share_mem("komanda", &cmd, 4);
	share_mem("niz", primes, 400000);
	
	int child_a, child_b;

	child_a = fork();

	if (child_a == 0) {

		exec("/bin/primecalc", argvcalc);
		exit();

	}else {
	
		child_b = fork();

		if (child_b == 0) {
			exec("/bin/primecom", argvcom);
			exit();
		} else {
		  	wpid2 = wait(); // Wait for primecom
		}
	}
	
	wpid1 = wait(); // Wait for primecom for primecalc
						
	free(primes); 
	
	exit();
}

