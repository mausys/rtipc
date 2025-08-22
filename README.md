## RTIPC

**RTIPC** is a zero-copy, wait-free inter-process communication C-library suited for real-time systems.

### Features
- Extremely fast: no data-copying and no syscalls are used for a data transfer.
- Deterministic: data updates don't affect the runtime of the remote process.
- Simple: No external dependency
- Optimized for SMP-systems: data buffers are cacheline aligned to avoid unneeded cache coherence transactions.
- Support for anonymous and named shared memory
- Multithreading support: multiple threads can communicate over different channels with each other.

### Design
The shared memory is divided into different channels. The core of the library is a single consumer single producer wait-free zero-copy circular message queue, that allows the producer to replace its oldest message with a new one.

#### Shared Memory Layout
|                 |
| --------------- |
| Header          |
|                 |
| Table           |
|                 |
| Channels        |

- Header: fixed size. Describes the memory layout. Written by the server during initization.
- Table: Each channel has a table entry. An entry contains the size and offset of the channel buffers. The table is written by the server during initialization.
- Channels: Each channel has at leas three equally sized data buffers plus the atomic variables for the exchange. For performance reasons 
The buffers are cacheline aligned.

