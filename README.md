## RTIPC

**RTIPC** is a zero-copy wait-free inter-process communication C-library suited for real-time systems.

### Features
- Extremly fast: no data-copying and no syscall is used for a data transfer
- Deterministic: data updates doesn't affect runtime of remote process
- Optimized for SMP-Systems: data buffers are cacheline aligned to avoid unneeded cache coherence transactions
- Simple Object Mapper
- Support for anonymous and named shared memory
- Multithreading support: multiple threads can communicate over different channels with each other
- No external dependency
