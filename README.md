## RTIPC

**RTIPC** is a zero-copy, wait-free inter-process communication C-library suited for real-time systems.

### Features
- Extremely fast: no data-copying and no syscalls are used for a data transfer.
- Deterministic: data updates don't affect the runtime of the remote process.
- Simple: No external dependency
- Optimized for SMP-systems: data buffers are cacheline aligned to avoid unneeded cache coherence transactions.
- Simple Object Mapper
- Support for anonymous and named shared memory
- Multithreading support: multiple threads can communicate over different channels with each other.

### Design
The shared memory is divided into different channels. Each channel consists of an atomic exchange variable, three equally sized data buffers and optional meta data.

#### Shared Memory Layout
|                 |
| --------------- |
| Header          |
|                 |
| Table           |
|                 |
| Channel Buffers |
|                 |
| Meta Data       |

- Header: fixed size. Describes the memory layout. Written by the server during initization.
- Table: Each channel has a table entry. An entry contains the size and offset of the channel buffers and the meta data.
Also, the channel atomic exchange variable is located in the table entry. Because the atomic exchange variable is accessed by both producer and consumer,
Each table entry is cacheline aligned. The table is written by the server during initialization, but the exchange variable is accessed by both the server and client during data exchange.
- Channel Buffers: Each channel has three equally sized data buffers. For performance reasons 
The buffers are cacheline aligned. Depending on the direction of the channel, its buffers are either written by the server and read by the client or written by the client and read by the server.
- Meta Data: The server can store additional channel information in the meta data. The object mapper uses the meta data for the object information (alignment, size, id). 

