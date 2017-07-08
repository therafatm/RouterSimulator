# RouterSimulator
A multithreaded scheduler written in C using pthreads.

### Description
The tool reads in abstracted flows (or packets) from an input text file, that describe meta information about the packets
such as arrival time, transmission time, and packet priority, etc. The life cycle of these packets are then simulated by instantiating
individual threads per packet, which are then stored and processed within a general priority queue. Mutexes and Convars are also used
where needed to maintain synchronization of each individual thread and the main thread.

### General Algorithm
  - Read command line arguments
  - Initialize flow waiting `queue`, `flow` array, `pthread_t` array to store threads
  - Read appropriate file, and store flows in flow array
  - Create N threads for simulating N flow’s as read from the input file
  - For each thread, start simulation, sleep for appropriate time, then
    prints arrival statement
  - For each thread, request access for transmission:
    - `Mutex` is locked if available
    - If waiting queue is empty and no transmission is going on, begin transmission. 
      Else, add to waiting queue, sort waiting queue according to given rules, and wait 
      (using convar) until no transmission is running and current flow thread is on top of queue.
    - Set variable to indicate transmission, i.e. `isTransmitting` to true
    - Deque current transmitting flow from queue.
  - After having permission to transmit, sleep for transmission time.
  - Release access for transmission:
    - Set variable to indicate transmission completion, i.e. `isTransmitting` to false
    - Broadcast to change convar value i.e. `pthread_cond_broadcast()`
- Wait for each flow thread to finish lifecycle in main using `pthread_join()`
- Release dynamically allocated memory, destroy mutexes and convars, return
 
### To Build
  - Use the `make` utility to build the executable
  - `./MFS flows.txt`
