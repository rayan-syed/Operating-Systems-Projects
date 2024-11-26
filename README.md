# Operating-Systems
This repository contains four major Operating Systems projects I completed in C:

## Simple Shell
- Implemented a command parser for a simple shell, handling &, |, <, and > tokens
- Managed process handling including forking, executing commands with execvp, and collecting exit statuses with waitpid
- Developed piping functionality to seamlessly connect multiple commands and manage their input/output
- Employed a two-phase lexing and parsing approach for accurate command interpretation
- Created and used numerous test cases in Bash to ensure parser functionality

## Threading Library
- Developed user-level threading library based on POSIX pthreads, including functions for thread creation, termination, joining, and self-identification
- Engineered TCB to manage threads, CPU registers, and stacks, supporting up to 128 threads
- Utilize round-robin scheduler, mutex operations and barrier functions to synchronize thread execution
- Debugged thoroughly using test cases via GDB (GNU debugger)

## Thread Local Storage (TLS)
- Implemented a Copy On Write (COW) TLS library using block-based memory management techniques
- Developed create, write, read, destroy, and clone functions to manage thread-specific memory regions
- Utilized mmap for allocating memory aligned and mprotect for controlling read/write permissions
- Ensured thread safety through page fault handling and signal handling (SIGSEGV)
- Created test-cases in Bash and performed extensive debugging with GDB (GNU Debugger)
- Ran Valgrind on code to test for memory leaks

## File System
- Designed and implemented file operations from scratch (create, delete, read, write) using inode-based structures
- Managed file system meta-information and ensured persistent storage across mounts and unmounts
- Optimized data storage by implementing efficient block allocation and deallocation strategies
- Gained deeper understanding of file system architecture and low-level programming
- Created test-cases in bash and performed extensive debugging using GDB (GNU debugger)
