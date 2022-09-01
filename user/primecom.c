#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"

//int flag=1;
//share_mem("primecalc",&flag,4);

int n;

int
main(int argc, char *argv[])
{	
	uint *kom, *niz, *idx;
	char buf[20];
	int komanda = -1;
	char x;

	for(;;){
		
		memset(buf, 0, 20);
		gets(buf, 20);
		komanda = solve(buf);

		switch(komanda){

			case 1 :
				get_shared("index", &idx);			
				get_shared("niz", &niz);					
				printf("Latest prime is no: %d value: %d\n", *((uint*)idx), niz[*((uint*)idx) - 1]);
				break;

			case 2 :
				get_shared("index", &idx);
				get_shared("niz", &niz);
				if(n > *((uint*)idx))
					printf("Still haven't calculated prime no. %d. We have primes up to %d\n", n, *((uint*)idx));
				else
					printf("Prime number no. %d is: %d\n", n, niz[(uint)n - 1]);
	
				break;
	
			case 3 :
				get_shared("komanda", &kom);
				*((uint*)kom) = 3;	
				printf("Resuming...\n");
				break;

			case 4 :
				get_shared("komanda", &kom);
				*((uint*)kom) = 4;
				printf("Pausing...\n");			
				break;

			case 5 :
				get_shared("komanda", &kom);
				*((uint*)kom) = 5;
				exit();

			default :
				break; 
		}
	}

	exit();
}

int
solve(char *com)
{	
	//printf("%s ", com);
	if(strcmp("latest\n", com) == 0)
		return 1;
	if(strcmp("resume\n", com) == 0)
		return 3;
	if(strcmp("pause\n", com) == 0)	
		return 4;
	if(strcmp("end\n", com) == 0)
		return 5;
	
	char str[6] = "prime ";
	int k=0;
	n=0;
	for(k=0 ; k<6 ; k++)
		if(str[k] != com[k])
			return 0;
	
	while(com[k] != '\n'){
		int c = com[k++];
		if(!(c>=48 && c<=57))
				return 0;
		n = n*10 + c - 48;
	}

	return 2;
}


