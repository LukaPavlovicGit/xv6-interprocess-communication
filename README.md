# Adding Process Communication Support to the XV6 Operating System

# [Video Demonstration](https://drive.google.com/file/d/1XphjlcFuXzx-v_BjJrwVY3OILzRrkXUP/view?usp=drive_link)
# [Project Specification](OS-Domaći3.pdf)

Xv6 has been modified to enable interprocess communication (IPC) using shared memory, restricted to a single parent-child relationship. In this example, there are three processes:
- **primestart** (the parent process)
- **primecom** (child process)
- **primecalc** (child process)

The **primestart** process creates and registers the shared memory structures with the operating system. **primecom** receives commands from the user, places their command ID in the shared memory, and **primecalc** computes prime numbers and writes them to the shared memory. Additionally, **primecalc** reads and executes commands sent by **primecom**.

Below is a more detailed explanation of the problem and how it was solved.

---

## Overview

The parent process must inform the operating system which memory regions should be shared. The operating system must then ensure each child process receives the same physical addresses mapped into its virtual memory space.

To enable memory sharing, a new structure called **shared** was created to hold metadata about the memory to be shared. Two additional attributes were also added to the **proc** structure:
1. A pointer to the **parent page directory**  
2. An **array of shared structures**

---

## System Changes

1. **`fork()` Modification**  
   When the `fork()` system call is invoked, the new (child) process inherits all common structures from its parent. At this stage, the child has only a single copy of the shared memory, which means the child cannot yet communicate with its parent.

2. **`exec()` Modification**  
   The regular process size is limited to a maximum of 1 GB. From 1 GB to 2 GB, physical addresses of the shared memory inherited from the parent are allocated. At this point, communication between child and parent becomes possible because the parent’s physical shared memory addresses are mapped into the child’s virtual address space within the 1 GB to 2 GB range.

---

## Additional Changes to XV6

1. **Two System Calls**

   - **`int share_mem(char *name, void *addr, int size)`**  
     This system call is invoked by the parent process to register a shared memory region.  
     - **`name`**: Unique identifier for the shared memory.  
     - **`addr`**: Address of the shared memory.  
     - **`size`**: Size of the shared memory region (in bytes).  
     - **Return value**:  
       - `0~9` if sharing succeeds,  
       - `-1` if any parameter is invalid,  
       - `-2` if a shared structure with the same name already exists,  
       - `-3` if there are more than 10 shared structures.

   - **`int get_shared(char *name, void **addr)`**  
     This system call is invoked by child processes to access a shared memory region.  
     - **`name`**: Name of the shared memory region the child wants to access.  
     - **`addr`**: A pointer (passed by reference) which should point to the shared memory region after the call completes.  
     - **Return value**:  
       - `0` if the call succeeds,  
       - `-1` if any parameter is invalid,  
       - `-2` if no shared structure with the given name exists.

2. **Three User Programs**

   - **`primestart`**  
     This program prepares the shared structures to pass to its children. The structures include:  
       - An empty array of 400,000 bytes (used as `int` array).  
       - A counter (an `int`) indicating the current position in the array.  
       - A command indicator (an `int`) used to signal commands.

   - **`primecom`**  
     This program receives commands from the user. Possible commands include:  
       - **`prime <n>`**: Prints the *n*-th prime number. If the *n*-th prime is not yet calculated, an error message is displayed.  
       - **`latest`**: Prints the most recently calculated prime number.  
       - **`pause`**: Pauses prime number calculation.  
       - **`resume`**: Resumes prime number calculation.  
       - **`end`**: Notifies **primecalc** to terminate, then ends its own execution.

   - **`primecalc`**  
     This program calculates prime numbers. When it finds a new prime, it writes it to the shared array and increments the array position counter. **primecalc** also monitors for commands from **primecom**, and executes them as needed.
