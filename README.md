# xv6 Operating System

## [project specification](OS-Domaći3.pdf)

## Adding process communication support

Xv6 has been modified to support communication between processes. Processes communicate using shared memory, which is limited to only one parent-child relationship. In this example I have created three processes, one **primestart** which is the parent process, and two others **primecom** and **primecalc** which are child processes. The primestart process creates structures that are shared by the other two processes and reports them to the operating system. Primecom receives commands from the user and pushes the command ID into shared memory. Primecal computes prime numbers and writes them to shared memory. It also reads the commands sent by primecom and executes them.<br/>

Here is a more detailed explanation of the problem and how I solved it.<br/>

The parent process must tell the operating system which memory addresses to share, and all child processes should be given the same physical addresses in their virtual memory space by the operating system.<br/>

To enable memory sharing, I created a new structure called **shared** that contains metadata about the parts of memory to be shared, and added two additional attributes to the **proc** structure: Pointer to the **parent page directory** and the **array of shared structures**.<br/>

The system changes I made to support process communication functionality are:<br/>

1.  **fork() modification**:<br/>
    When the system call fork is invoked, the new process should inherit all common structures from its parent. At this point, the child has only one copy of the shared memory. This means that the child cannot yet communicate with its parent.<br/>

2.  **exec() modification**:<br/>
    I have limited the regular process size to a maximum of 1 GB. From 1 GB up to 2 GB, the physical addresses of the shared memory inherited from the parent are allocated. At this point, communication between child and parent is possible because the real physical addresses of the parent's shared memory are mapped in the child's virtual memory from 1 GB up to 2 GB.<br/><br/>

Additional changes to the xv6 OS:<br/>

1. Two system calls :<br/>
    
    -   **int share_mem(char \*name, void \*addr, int size)** :<br/>
        This system call is invoked by the parent process to report memory to be shared.<br/>
        @*param \*name* - unique name for the shared memory.<br/>
        @*param \*addr* - address of the shared memory.<br/>
        @*param size* - size of the shared memory.<br/>
        Returnig value is: 0\~9 if share went successfully, -1 - if any of parametars is wrong, -2 - shared structure with a 'name' already exist, -3 - if exist more than 10 shared structures.<br/>

    -   **int get_shared(char \*name, void \*\*addr)** :<br/>
        This system call is invoked by child processes to access a common structure.<br/>
        @*param \*name* - name of the shared structure which child process want to access.<br/>
        @*param \*\*addr* - pointer (passed by reference) which needs to point on shared memory after system call is over.<br/>
        Returning value is: 0 - if calls went successful, -1 - if any of parametars is wrong, -2 - if there is no shared structure named with 'name'.<br/><br/>

2. Three user programs:<br/>

    -   **primestart** :<br/>
        This program prepares structures to be passed on to its children..<br/>
        That structures are:<br/>
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
        This program calculates prime numbers. When a prime number is found, it writes the number to the common array and also increments the position counter of the array. Also, this program checks if the command some was sent by the program **primecom**.
