// Create a zombie process that
// must be reparented at exit.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user.h"
//printf("primecalc niz:%d\n", *(niz+10));
//printf("primecalc com:%d\n", *((uint*)kom));
//printf("primecalc index:%d\n", *((uint*)idx));

int
main(void)
{
	int komanda, flag;
	uint *kom, *niz, *idx, *addr, n = 0;	

	for(int i = 2 ; ; ++i){
		flag = 1;
		for(int j = 2 ; j < i / 2 ; j++){
			if(i == 2)		
				break;
			if(i % j == 0){
				flag = 0;
				break;
			}
		}
		
		if(flag){
			get_shared("niz", &niz);
			get_shared("index", &idx);					

			niz[n] = i;	
			*((uint*)idx) = n;
			n++;
		}
		get_shared("komanda", &kom);
		komanda = *((uint*)kom);

		if(komanda == 4){
			while(1){
				sleep(1);
				get_shared("komanda", &kom);
				komanda = *((uint*)kom);
				
				if(komanda == 3)
					break;				
				if(komanda == 5)
					exit();
			}
		}
		else if(komanda == 5)
			exit();
	}

	exit();
}

