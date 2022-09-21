# xv6 Operating System
## Adding interprocess communication

Xv6 modified to support comunication between processes. Processes communicate through the shared memory and it's resticted only to a parent-child relation.<br/>
Parent process needs to notify operating system which memory addresses are going to be shared, and all parent's children should be given by operating system the same physical addresses in their virtual memory space.<br/><br/>

In order to make sharing memory possible, i've made new structure called **shared** which contains metadata of the memory parts which are intended to be shared.<br/>Also i've added two additional attributes in the **proc** structure, the first is the pointer to the **parent page directory** and the second is the **array of the shared structures**.<br/><br/>

The system changes i've made to support process communication functionality are:<br/>

1.  **fork() modification**:<br/>
    When system call fork is called, new procces should inherit all shared structures from it's parent. At this point child has just copy of the shared memory. That means that child still can't communicate with it's parent.<br/>

2.  **exec() modification**:<br/>
    I've restricted regular process size to be maximum 1GB. Starting at 1GB up to the 2GB are mapped physical addresses of the shared memory inherited from the parent. At this point communication between child and parent is possible, because the real physical address of the parent's shared memory are mapped in the child virtual memory starting at 1GB up to the 2GB.<br/><br/>


Additional changes to the xv6 OS:<br/>

1. Two system calls :<br/>
    
    -   **int share_mem(char \*name, void \*addr, int size)** :<br/>
        This system call is called from parent process to report memory which are going to be shared.<br/>
        @*param \*name* - unique name for the shared memory.<br/>
        @*param \*addr* - address of the shared memory.<br/>
        @*param size* - size of the shared memory.<br/>
        Returnig value is: 0\~9 if sharing went successfull, -1 - if any of parametars is wrong, -2 - shared structure with a 'name' already exist, -3 - if exist more than 10 shared structures.<br/><br/>

    -   **int get_shared(char \*name, void \*\*addr)** :<br/>
        This system call is called from child procces to access shared structure.<br/>
        @*param \*name* - name of the shared structure which child process want to access.<br/>
        @*param \*\*addr* - pointer (passed by reference) which needs to point on shared memory after system call is over.<br/>
        Returning value is: 0 - if calls went successful, -1 - if any of parametars is wrong, -2 - if there is no shared structure named with 'name'.<br/><br/>

2. Three user programs:<br/>

    -   **primestart** :<br/>
        This program prepares structrures which are going to be shared with it's children.<br/>
        That ctructure are:<br/>
            - empty array sized of 400000 bytes (type of int).<br/>
            - counter of the position in the array (type of int)<br/>
            - command indicator (type of int)<br/>

    -   **primecom** :<br/>
        This program receives commands from the user.<br/>
        Possible commands are:<br/>
            - **prime \<n\>** - prints n-th prime number. If n-th number is not calculated error is thrown.<br/>
            - **latest** - prints latest calculated prime number.<br/>
            - **pause** - pause calculation of prime numbers.<br/>
            - **resume** - resume calculation of prime numbers.<br/>
            - **end** - notifies **primecalc** to exit process, and shut down himself.<br/>

    -   **primecalc** :<br/>
        This program calculates prime numbers. When prime number is found, he writes number in the shared array and also increments position counter of the array. Also this program checks whether the some command is sent by the **primecom** program.<br/><br/>

