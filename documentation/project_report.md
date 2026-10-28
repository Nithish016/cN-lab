# COMPUTER NETWORKS LABORATORY PROJECT REPORT

---

# Multi-Client Chat Application using TCP Socket Programming in C — Windows

**Course:** Computer Networks Laboratory  
**Degree:** Bachelor of Technology (B.Tech) — Information Technology  
**Student Name / Developer:** Gubba Nithish  
**GitHub Profile:** [@Nithish016](https://github.com/Nithish016)  
**Project Repository:** [https://github.com/Nithish016/cN-lab](https://github.com/Nithish016/cN-lab)  
**Platform:** Windows 10 / Windows 11  
**Development Language:** C (Standard C99/C11)  
**Networking Framework:** Microsoft Windows Sockets 2 (Winsock 2 — `Ws2_32.lib`)  

---

## 1. Title

**Design and Implementation of a Multi-Threaded, Multi-Client Chat and File Transfer Application Using TCP Socket Programming in C on Windows (Winsock 2)**

---

## 2. Abstract

In modern computer networking, understanding the foundational mechanics of reliable end-to-end transport communication is paramount. This project presents the complete design, implementation, and empirical evaluation of a multi-threaded client-server chat application implemented natively for the Microsoft Windows environment using Standard C and the Windows Sockets 2 (Winsock 2) Application Programming Interface (`winsock2.h`, `Ws2_32.lib`). 

The architecture features a central high-concurrency TCP server capable of handling multiple simultaneous clients using dedicated Windows worker threads (`_beginthreadex`). Shared data structures, including active client tables and activity logging systems, are synchronized using Windows Critical Sections (`CRITICAL_SECTION`) to ensure deterministic, race-condition-free operation. A custom byte-level framing protocol (`PACKET_MAGIC = 0x434E4C41`) is designed to overcome TCP stream concatenation and fragmentation, supporting multi-user broadcast messaging, private unicast messaging, active user enumeration, and binary file transfer partitioned into 4096-byte segments. The project is validated across 14 comprehensive test cases, demonstrating fault-tolerant socket cleanup, graceful disconnect handling, and reliable data transport across both loopback (`127.0.0.1`) and Local Area Networks (LAN / Wi-Fi).

---

## 3. Introduction

Computer networks facilitate communication between distinct physical or virtual computing machines by establishing standard communication protocols organized in hierarchical layers. At the core of the Internet Protocol Suite (TCP/IP) is the Transport Layer, responsible for host-to-host communication services for applications. 

Socket programming provides the programmer interface (API) between user-space application logic and the underlying transport layer implementations embedded inside the operating system kernel. While Unix and Linux systems leverage POSIX socket APIs (`sys/socket.h`), the Microsoft Windows operating system provides the Windows Sockets specification (Winsock), which introduces specific runtime lifecycle paradigms (`WSAStartup()`, `WSACleanup()`, `SOCKET` handles, Windows error codes, and Windows synchronization primitives).

This project demonstrates the complete engineering lifecycle of a production-quality, multi-client, multi-threaded networked communication system written natively for Windows. It provides practical insight into socket creation, local binding, passive listening, client connection acceptance, thread management, stream framing, and graceful shutdown.

---

## 4. Problem Statement

Standard networking lab exercises frequently rely on trivial, single-threaded echo servers or blocking single-client implementations. Such implementations suffer from critical deficiencies:
1. **Concurrency Bottlenecks:** A single-threaded server blocks in synchronous `recv()` or `accept()` calls, rendering the entire system unresponsive to other clients.
2. **Platform Incompatibility:** Linux-specific POSIX headers (`unistd.h`, `pthread.h`, `arpa/inet.h`, `fork()`) fail to compile natively on Windows workstations.
3. **Stream Boundary Confusion:** Because TCP is a continuous byte-stream protocol devoid of intrinsic record boundaries, simplistic line-reading functions often fail when messages or file bytes arrive fragmented or coalesced.
4. **Data Race Hazards:** Multiple threads concurrently accessing shared client registries without synchronization induce race conditions, corrupted buffers, and segmentation faults.

The objective is to formulate a clean, portable, and robust Windows-native application resolving all the aforementioned issues.

---

## 5. Objectives

The core objectives of this project are:
1. **Native Windows Architecture:** Develop client and server modules using purely Windows-compliant APIs (`winsock2.h`, `windows.h`, `process.h`, `Ws2_32.lib`), strictly omitting Linux-only headers.
2. **Multi-Client Concurrency:** Enable the server to accept and service at least 5 (up to 30) concurrent client connections simultaneously without starvation or blocking.
3. **Thread Safety:** Implement Windows Critical Sections (`CRITICAL_SECTION`) to synchronize client registry modifications, message broadcasts, and file activity logging.
4. **Duplicate Prevention:** Enforce unique username registration during connection handshakes.
5. **Rich Feature Set:** Provide multi-user broadcast chat, unicast private messaging (`/msg`), online user queries (`/users`), chunked binary file transfer (`/sendfile`), and graceful teardown (`/quit`).
6. **Dual Mode Testing:** Support seamless operation across single-machine loopback (`127.0.0.1`) and multi-machine Local Area Networks (Wi-Fi / Ethernet).
7. **Empirical Verification:** Formulate an automated test suite verifying 14 distinct test cases covering functional, concurrency, error, and stress conditions.

---

## 6. Existing System

Existing rudimentary lab socket demonstrations typically possess the following attributes:
- **Iterative (Single-Client):** The server processes one client request to completion before calling `accept()` for subsequent clients. Other clients experience indefinite connection timeouts.
- **Console-Only Echo:** Simply prints whatever text is received without distinguishing between message types, commands, or binary files.
- **No Framing:** Relies on newline characters (`\n`). If a user sends a binary file or multi-line text containing newlines, the stream framing breaks catastrophically.
- **Uncontrolled Crashes:** If a client abruptly terminates (e.g., closing the command prompt), the server throws a socket exception (Winsock error 10054: `WSAECONNRESET`) and terminates abnormally.

---

## 7. Proposed System

The proposed system introduces an enterprise-grade academic implementation:
- **Asynchronous Multi-Threading:** The server main thread solely handles listening and accepting connections. Dedicated worker threads are spawned for each connected client via `_beginthreadex()`.
- **Binary Stream Framing Protocol:** Every transmission is wrapped in a structured packet comprising a fixed 9-byte header (`magic`, `type`, `payload_len`) followed by typed binary payloads.
- **Thread-Safe Synchronization:** Critical sections protect the active client array and log outputs, eliminating race conditions.
- **Robust Exception Handling:** Network calls evaluate `WSAGetLastError()` and gracefully clean up disconnected sockets while maintaining uninterrupted service for all remaining users.
- **Dual Console and File Logging:** Server events are time-stamped and recorded in real time to both stdout and a persistent `server.log` file.
- **Integrated File Transfer:** Supports binary and ASCII file transfers with chunking and client progress notification.

---

## 8. Technologies Used

| Component | Technology / Specification | Purpose |
| :--- | :--- | :--- |
| **Language** | C (C99 / C11 Standard) | Systems programming, socket manipulation, memory efficiency |
| **Operating System** | Windows 10 / Windows 11 (64-bit) | Target development and runtime host environment |
| **Networking API** | Microsoft Winsock 2 (`winsock2.h`) | Socket creation, address binding, TCP data streaming |
| **TCP/IP Helper** | `ws2tcpip.h` | Modern address resolution (`inet_ntop`, `inet_pton`, `getaddrinfo`) |
| **Threading** | Windows C Runtime (`process.h`) | `_beginthreadex` for thread-safe CRT state handling |
| **Synchronization** | Windows Kernel (`synchapi.h`) | `CRITICAL_SECTION` for lightweight user-space mutual exclusion |
| **Linker Library** | `Ws2_32.lib` | Windows Sockets 2 dynamic link library import library |
| **Build Systems** | MinGW GCC (`gcc.exe`) / MSVC (`cl.exe`) | Cross-toolchain compatibility for college lab environments |

---

## 9. Hardware Requirements

- **Processor:** Intel Core i3 / AMD Ryzen 3 or higher (Dual-Core or Quad-Core recommended for multi-threading).
- **RAM:** Minimum 2 GB RAM (4 GB+ recommended).
- **Disk Space:** 50 MB free disk space for source files, compiled binaries, and log archives.
- **Network Interface Card (NIC):** Standard 10/100/1000 Mbps Ethernet adapter or 802.11b/g/n/ac Wi-Fi adapter.

---

## 10. Software Requirements

- **Operating System:** Windows 10 (Version 1903+) or Windows 11.
- **C Compiler:** Any of the following:
  - MinGW-w64 GCC (via WinGet: `MartinStorsjo.LLVM-MinGW.UCRT` or MSYS2)
  - Microsoft Visual Studio 2019 / 2022 (with "Desktop development with C++")
  - Visual Studio Code with C/C++ extension and MinGW GCC.
- **Shell / Terminal:** Windows Command Prompt (`cmd.exe`) or Windows PowerShell 5.1 / 7+.
- **Network Utilities:** `ipconfig`, `netstat`, Windows Defender Firewall.

---

## 11. System Architecture

```text
+=============================================================================+
|                                CHAT SERVER                                  |
|                                                                             |
|  [Main Thread]                                                              |
|        |                                                                    |
|        +---> WSAStartup() -> socket() -> bind() -> listen()                 |
|        |                                                                    |
|        +---> While(running):                                                |
|                   accept() ---> Spawns Worker Thread via _beginthreadex()   |
|                                                                             |
|  [Worker Threads (1 per client)]                                            |
|        |                                                                    |
|        +---> Handshake (PKT_CONNECT_REQ -> Check Uniqueness -> ACK/NACK)    |
|        +---> Message Loop: recv_packet()                                    |
|        |        ├── PKT_BROADCAST    --> Broadcast to all clients           |
|        |        ├── PKT_PRIVATE_MSG  --> Route to target recipient socket   |
|        |        ├── PKT_USERS_REQ    --> Build user list & return           |
|        |        ├── PKT_FILE_START   --> Initiate file stream               |
|        |        ├── PKT_FILE_DATA    --> Forward file chunks                |
|        |        ├── PKT_FILE_END     --> Signal file completion             |
|        |        └── PKT_DISCONNECT   --> Clean shutdown                     |
|        |                                                                    |
|  [Shared Resources Protected by CRITICAL_SECTION]                           |
|        ├── ClientSession g_clients[MAX_CLIENTS]                             |
|        ├── int g_client_count                                               |
|        └── FILE *g_log_file (Console + server.log)                          |
+=============================================================================+
                                 ▲
               TCP Connections   │  Winsock 2 Framed Packets
                                 ▼
+=============================================================================+
|                                CHAT CLIENT                                  |
|                                                                             |
|  [Main Thread]                                                              |
|        ├── WSAStartup() -> socket() -> connect()                            |
|        ├── Send PKT_CONNECT_REQ -> Await PKT_CONNECT_ACK                    |
|        ├── Spawn Asynchronous Receiver Thread                               |
|        └── Input Loop: fgets(stdin) -> Parse Commands (/all, /msg, etc.)   |
|                                                                             |
|  [Background Receiver Thread]                                               |
|        └── Loop: recv_packet()                                              |
|                 ├── PKT_BROADCAST   --> Display "[MSG] user: text"          |
|                 ├── PKT_PRIVATE_MSG --> Display "[PRIVATE from user]: text" |
|                 ├── PKT_USERS_RESP  --> Display formatted online users      |
|                 ├── PKT_FILE_START  --> Open destination file               |
|                 ├── PKT_FILE_DATA   --> Write chunk to disk                 |
|                 └── PKT_FILE_END    --> Close file & confirm size           |
+=============================================================================+
```

---

## 12. Network Architecture

The network communication model operates according to standard TCP/IP protocol mapping:

```text
OSI Reference Layer           TCP/IP Architecture           Chat Application Implementation
-------------------           -------------------           -------------------------------
7. Application Layer    ->    Application Layer     ->     Custom Framing Protocol ('CNLA')
                                                           Commands: /users, /msg, /sendfile
6. Presentation Layer   ->    (Framing / Packing)   ->     #pragma pack(1), Endian (htonl/ntohl)
5. Session Layer        ->    (Connection State)    ->     ClientSession table, Handshake
4. Transport Layer      ->    Transport Layer       ->     TCP (Transmission Control Protocol)
                                                           Reliable, in-order, stream sockets
3. Network Layer        ->    Internet Layer        ->     IPv4 Address Resolution (INADDR_ANY,
                                                           127.0.0.1, LAN 192.168.x.x)
2. Data Link Layer      ->    Network Interface     ->     Ethernet (802.3) / Wi-Fi (802.11)
1. Physical Layer       ->    Physical Layer        ->     Network Card / RJ45 / RF Antenna
```

### End-to-End Address Resolution

```text
IP Address (Logical Host Address)
     ↓
Port Number (Logical Service Endpoint: TCP 8080)
     ↓
Socket Descriptor (Kernel File Handle / SOCKET)
     ↓
TCP Connection (Full-Duplex Established State)
     ↓
Data Transmission (send_packet / recv_packet)
     ↓
Orderly Connection Termination (FIN / closesocket)
```

---

## 13. TCP Communication and The Three-Way Handshake

TCP (Transmission Control Protocol) is a connection-oriented, reliable, stream-based transport layer protocol. Before any chat message or file payload can be transmitted, a connection must be established using the **TCP Three-Way Handshake**:

```text
Client (Active Open)                                 Server (Passive Open)
  [CLOSED]                                                 [LISTEN]
     |                                                         |
     | ------------------- 1. SYN (seq = x) -----------------> |  (Client calls connect())
     |                                                         |  Server receives SYN,
     |                                                         |  allocates TCB resources
     | <----------- 2. SYN + ACK (seq = y, ack = x + 1) ------ |  (Server creates new socket)
     |                                                         |
     | ------------------- 3. ACK (ack = y + 1) -------------> |  (connect() completes)
     |                                                         |  accept() unblocks!
  [ESTABLISHED]                                             [ESTABLISHED]
     |                                                         |
     | <================ Full Duplex Chat Data ===============> |  (send() / recv())
     |                                                         |
```

### Handshake Sequence Breakdown:
1. **SYN (Synchronize):** The client transmits a TCP segment with the `SYN` control bit set, specifying its Initial Sequence Number (`ISN_c = x`). This is triggered when the client invokes `connect()`.
2. **SYN + ACK:** The server detects the `SYN`, allocates transmission control block buffers, and replies with a segment containing both its own `SYN` flag (`ISN_s = y`) and an acknowledgment (`ACK = x + 1`).
3. **ACK:** The client acknowledges the server's sequence number by transmitting `ACK = y + 1`. Both endpoints enter the `ESTABLISHED` state. `accept()` returns the active client socket.

---

## 14. Socket Programming Fundamentals

A socket is an abstraction representing a bidirectional communication endpoint. In Windows, a socket is represented by the `SOCKET` type (an unsigned integer handle managed by kernel space).

### The Five Essential Parameters of a TCP Connection (Socket Quintuple):
1. **Source IP Address:** Client host IP.
2. **Source Port Number:** Ephemeral port allocated dynamically by the operating system kernel (e.g., 54321).
3. **Destination IP Address:** Server host IP (e.g., 192.168.1.10).
4. **Destination Port Number:** Server listening port (e.g., 8080).
5. **Transport Protocol:** TCP (`IPPROTO_TCP`).

---

## 15. Winsock 2 API (Windows Sockets)

Winsock 2 is the standard Windows networking API. Unlike POSIX systems where network sockets are treated identically to Unix file descriptors (`int fd`), Winsock requires explicit subsystem initialization and specific error retrieval mechanisms:

### Winsock Initialization Lifecycle
```text
      WSAStartup(MAKEWORD(2,2), &wsaData)
                     │
          (Socket Operations Loop)
                     │
                WSACleanup()
```

### Detailed Winsock API Operations in this Project:
1. **`WSAStartup(WORD wVersionRequired, LPWSADATA lpWSAData)`:**  
   Initiates use of the Winsock DLL (`Ws2_32.dll`) by the calling process. `MAKEWORD(2, 2)` requests Winsock version 2.2.
2. **`socket(int af, int type, int protocol)`:**  
   Allocates a socket endpoint. `socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)` creates an IPv4 stream socket.
3. **`bind(SOCKET s, const struct sockaddr *name, int namelen)`:**  
   Associates the local address structure (`sockaddr_in`) with the socket handle.
4. **`listen(SOCKET s, int backlog)`:**  
   Marks the socket as a passive listener. `SOMAXCONN` instructs the underlying service provider to permit maximum queued pending connections.
5. **`accept(SOCKET s, struct sockaddr *addr, int *addrlen)`:**  
   Extracts the first pending connection from the listen backlog, creates a brand-new socket descriptor for that client, and populates the client's network address.
6. **`connect(SOCKET s, const struct sockaddr *name, int namelen)`:**  
   Initiates active TCP three-way handshake with the remote server.
7. **`send(SOCKET s, const char *buf, int len, int flags)`:**  
   Copies user payload bytes into the transport provider's outgoing buffer.
8. **`recv(SOCKET s, char *buf, int len, int flags)`:**  
   Extracts available received bytes from the transport incoming queue.
9. **`closesocket(SOCKET s)`:**  
   Closes the socket handle, flushes lingering data, and initiates TCP `FIN` teardown.
10. **`WSAGetLastError()`:**  
    Retrieves the specific error code associated with the most recently failed Winsock call (e.g., 10061: `WSAECONNREFUSED`, 10054: `WSAECONNRESET`).
11. **`WSACleanup()`:**  
    Deregisters the process from the Winsock DLL and releases memory allocations.

---

## 16. Server Module Architecture

The server implementation (`server/server.c`, `server/server.h`) is organized around modular components:

### 1. Connection Acceptance
The main server thread executes an infinite accept loop:
```c
SOCKET client_sock = accept(g_listen_socket, (struct sockaddr *)&client_addr, &client_addr_len);
```
Upon a successful connection, a dynamic `ClientThreadParam` structure is allocated and passed to a newly spawned Windows worker thread.

### 2. Client Registry and Synchronization
The server maintains an in-memory session array:
```c
static ClientSession g_clients[MAX_CLIENTS];
static CRITICAL_SECTION g_clients_lock;
```
Every lookup, addition, status inquiry, and removal is synchronized with:
```c
EnterCriticalSection(&g_clients_lock);
/* Access or modify client table */
LeaveCriticalSection(&g_clients_lock);
```

### 3. Duplicate Username Prevention
When a client requests registration, the server scans the registry:
- If a match is detected (case-insensitive via `_stricmp`), the server responds with `PKT_CONNECT_NACK` and closes the socket.
- If unique, the client is added, marked active, and sent `PKT_CONNECT_ACK`.

### 4. Activity Logger
Events are logged with millisecond precision to both the terminal and `server/server.log`:
```text
[2026-10-28 19:40:57] [+] Client connected : Nithish [127.0.0.1:63007]
[2026-10-28 19:40:57] [MSG] Nithish -> ALL : Hello everyone
[2026-10-28 19:40:57] [FILE] Transfer initiating: 'sample.txt' from Nithish to Rahul
[2026-10-28 19:40:59] [-] Client disconnected : Nithish
```

---

## 17. Client Module Architecture

The client implementation (`client/client.c`, `client/client.h`) features an asynchronous, dual-threaded architecture:

1. **Main Input Thread:**
   - Prompts the user for Server IP, Port, and Username.
   - Executes `connect()` and exchanges the `PKT_CONNECT_REQ` handshake.
   - Spawns the background receiver thread.
   - Blocks on `fgets(stdin)` reading user terminal input.
   - Parses slash commands (`/users`, `/msg`, `/all`, `/sendfile`, `/help`, `/quit`) or defaults to broadcast.

2. **Background Receiver Thread:**
   - Runs `receive_thread()` continuously in a loop calling `recv_packet()`.
   - Asynchronously receives incoming broadcasts, private messages, system notifications, user lists, and file data packets without interrupting the user's typing prompt.
   - Synchronizes console output using `g_console_lock`.

---

## 18. Multi-Threading and Synchronization

### Why `_beginthreadex` over `CreateThread` or `pthread`?
In standard C on Windows, `_beginthreadex()` is the recommended thread creation API. Unlike raw Windows `CreateThread()`, `_beginthreadex()` properly initializes thread-local C Runtime (CRT) data structures (`errno`, string buffers, formatting state), preventing memory leaks when CRT functions (`printf`, `malloc`, `strtok`) are utilized inside worker threads.

### Thread Synchronization Primitive: Windows Critical Section
The project employs Windows `CRITICAL_SECTION` objects.
- **Advantage over Kernel Mutex:** A Critical Section operates primarily in user space using atomic compare-and-swap (CAS) instructions. It only transitions to kernel-mode wait if thread contention occurs. This yields substantially higher throughput and lower CPU overhead compared to heavy kernel mutexes.
- **Race Condition Prevention:** Eliminates simultaneous write hazards when multiple client threads attempt to broadcast messages or modify the online users table simultaneously.

---

## 19. Binary File Transfer Mechanism

File transfer is implemented through framed packet chunking:

```text
Sender Client                         Server                         Receiver Client
     |                                   |                                   |
     | ----- 1. PKT_FILE_START --------> | ----- 1. PKT_FILE_START --------> |
     |    (filename, size, chunks)       |    (Lookup receiver socket)       | (Opens destination file)
     |                                   |                                   |
     | ----- 2. PKT_FILE_DATA Chunk 0 -> | ----- 2. PKT_FILE_DATA Chunk 0 -> | (Writes chunk to disk)
     | ----- 2. PKT_FILE_DATA Chunk 1 -> | ----- 2. PKT_FILE_DATA Chunk 1 -> | (Writes chunk to disk)
     | ----- ... (Repeat for all) -----> | ----- ... (Repeat for all) -----> | (Writes chunk to disk)
     |                                   |                                   |
     | ----- 3. PKT_FILE_END ----------> | ----- 3. PKT_FILE_END ----------> | (Closes file & verifies)
     |                                   |                                   |
```

### Steps:
1. **Pre-check:** The sender checks whether the requested file exists locally and measures its size via `_fseeki64` and `_ftelli64`.
2. **Initiation:** The sender transmits `PKT_FILE_START` containing the base filename, target recipient, and total chunk count.
3. **Chunk Streaming:** The sender reads the file in 4096-byte buffers (`FILE_CHUNK_SIZE`) and streams `PKT_FILE_DATA` packets sequentially.
4. **Completion:** Upon reaching EOF, the sender transmits `PKT_FILE_END`. The receiver closes the file handle and outputs a verified completion summary.
5. **Storage:** Files are saved into `received_files/` with exact byte-for-byte fidelity.

---

## 20. Command Processing Engine

| Command | Action | Payload Transmitted |
| :--- | :--- | :--- |
| `/users` | Queries connected users | `PKT_USERS_REQ` -> Server returns formatted string table in `PKT_USERS_RESP` |
| `/msg <user> <text>` | Direct private message | `PKT_PRIVATE_MSG` (sender, recipient, text) |
| `/all <text>` | Broadcast message | `PKT_BROADCAST` (sender, text) |
| `<plain text>` | Default broadcast | Automatically forwarded as `PKT_BROADCAST` |
| `/sendfile <user> <file>` | Starts file transfer | `PKT_FILE_START` -> `PKT_FILE_DATA` (chunks) -> `PKT_FILE_END` |
| `/help` | Displays command reference | Handled locally on client |
| `/quit` | Clean disconnect | `PKT_DISCONNECT` -> closesocket -> WSACleanup |

---

## 21. Error Handling and Resilience

The application is engineered to resist and recover from network errors:
- **`WSAStartup` failure:** Reports precise error code and exits cleanly.
- **Port already in use (`WSAEADDRINUSE` / 10048):** Informs user that another application is bound to the port.
- **Connection Refused (`WSAECONNREFUSED` / 10061):** Detects offline server and terminates without hanging.
- **Abrupt Connection Reset (`WSAECONNRESET` / 10054):** When a client window is abruptly terminated, `recv_packet()` returns `<= 0`. The server catches this, invokes `remove_client()`, reclaims the slot, logs the event, and continues servicing other users.
- **File Not Found:** Informs sender immediately without transmitting invalid packets.
- **Offline Recipient:** When sending private messages or files to an offline user, the server intercepts the request and alerts the sender with `PKT_NOTIFICATION` / `PKT_FILE_ERROR`.

---

## 22. Empirical Testing and Validation

All 14 required test cases were executed and validated using the automated test suite (`test_suite.ps1`):

| Test ID | Test Scenario | Input / Action | Expected Result | Actual Result | Status |
| :---: | :--- | :--- | :--- | :--- | :---: |
| **1** | Server Startup | Launch `server.exe 8995` | Binds to port, logs local IPs, enters listen state | Server started, bound to 8995 | **PASSED** |
| **2** | Single Client Connection | Connect "Nithish" | Handshake acknowledged (`PKT_CONNECT_ACK`) | Connected successfully | **PASSED** |
| **3** | Multiple Clients (5+) | Connect Rahul, Arjun, Sneha, Kiran | All 5 sessions established concurrently | 5 concurrent clients active | **PASSED** |
| **4** | Broadcast Messaging | Nithish sends broadcast | All other clients receive `[MSG] Nithish: ...` | Delivered to all clients | **PASSED** |
| **5** | Private Messaging | Nithish sends `/msg Rahul Hi` | Only Rahul receives; others do not see | Unicast routed to Rahul | **PASSED** |
| **6** | Online Users Directory | Send `/users` | Returns list of all 5 active usernames | Correct list returned | **PASSED** |
| **7** | Duplicate Username | Register existing "Nithish" | Server rejects with `PKT_CONNECT_NACK` | Registration rejected | **PASSED** |
| **8** | Invalid Command | Enter `/unknown_cmd` | Client displays error and suggests `/help` | Error printed | **PASSED** |
| **9** | Orderly Disconnect | Kiran sends `/quit` | Server closes socket, notifies remaining | Slot freed cleanly | **PASSED** |
| **10** | Server Unavailable | Connect to dead port | Client outputs error code 10061 and exits | Detected offline server | **PASSED** |
| **11** | File Transfer (Standard) | Send `sample.txt` to Rahul | File chunks routed; saved in `received_files/` | File received & verified | **PASSED** |
| **12** | Concurrent Burst Stress | Simultaneous burst messages | Server processes burst without socket stall | Handled without deadlock | **PASSED** |
| **13** | Large File Streaming | 12 KB stream in 3 chunks | All chunks delivered in order with EOF marker | 100% byte integrity | **PASSED** |
| **14** | Unexpected Kill | Abruptly kill client process | Server catches EOF, frees slot, remains alive | Server unharmed | **PASSED** |

---

## 23. Advantages of the System

1. **Deterministic Concurrency:** True non-blocking client interaction via native multi-threading.
2. **Platform Native:** Built specifically for Windows without external emulation layers or Linux compatibility shims.
3. **Framing Integrity:** Binary packet header design prevents message concatenation and truncations.
4. **Memory Safety:** Bounded static buffers, safe string functions (`strncpy_s`, `snprintf`), and byte packing (`#pragma pack(1)`).
5. **Dual LAN & Loopback Support:** Usable in both standalone individual study and multi-computer laboratory environments.

---

## 24. Limitations

1. **Unencrypted Transport:** Data travels as unencrypted TCP plaintext. (Production systems would incorporate TLS / OpenSSL / SChannel).
2. **No Persistent Database:** Client sessions and chat histories are kept in volatile memory and text logs rather than an SQL database.
3. **No NAT Traversal:** Requires clients to be on the same Local Area Network (or have public port forwarding configured).

---

## 25. Future Enhancements

1. **Transport Layer Security (TLS):** Integrate Microsoft SChannel or OpenSSL for encrypted communication.
2. **GUI Interface:** Develop a modern Windows desktop frontend using Win32 API or WPF / C#.
3. **Group Channels / Rooms:** Implement named channels (e.g., `#general`, `#study-group`) with `/join` and `/leave`.
4. **Voice over IP (VoIP):** Incorporate UDP socket streaming for real-time audio transmission.

---

## 26. Conclusion

The Multi-Client Chat Application using TCP Socket Programming in C for Windows demonstrates the practical application of transport layer networking, Windows Sockets (Winsock 2), thread synchronization, and binary protocol engineering. By adhering to strict Windows architecture standards and implementing structured packet framing, the application guarantees reliable, concurrent, and fault-tolerant communication suitable for academic study and engineering defense.

---

## 27. Viva Questions and Answers (25 Comprehensive Questions)

### Q1: What is a computer network?
**Answer:** A computer network is an interconnected collection of autonomous computing devices capable of exchanging data and sharing resources over communication links using established transmission protocols.

### Q2: What is the Transmission Control Protocol (TCP)?
**Answer:** TCP is a connection-oriented, reliable transport layer protocol that provides guaranteed, in-order, error-checked delivery of a stream of octets between applications running on hosts communicating over an IP network.

### Q3: What is the User Datagram Protocol (UDP)?
**Answer:** UDP is a connectionless, lightweight transport protocol that transmits independent datagram packets without establishing a connection, sequence acknowledgment, flow control, or delivery guarantees.

### Q4: Why is TCP preferred over UDP for a chat application?
**Answer:** Chat messaging and file transfers require 100% data integrity, guaranteed delivery, and strict message ordering. With UDP, packets could be dropped, duplicated, or arrive out of sequence, resulting in missing messages and corrupted files.

### Q5: What is a socket in computer networking?
**Answer:** A socket is a software abstraction acting as an endpoint for bidirectional communication between two processes across an IP network. It binds an application process to an IP address and port number.

### Q6: What is an IP address?
**Answer:** An Internet Protocol (IP) address is a unique numerical identifier assigned to each device connected to a computer network, utilized for routing and host addressing (e.g., IPv4: 32-bit `192.168.1.10`).

### Q7: What is a port number?
**Answer:** A port number is a 16-bit unsigned integer (ranging from 0 to 65535) used by the transport layer to identify the specific application or process service executing on a given host.

### Q8: What is client-server architecture?
**Answer:** It is a distributed computing model where centralized service providers (servers) manage resources and listen for incoming connection requests from multiple distributed service requesters (clients).

### Q9: What is Winsock (Windows Sockets)?
**Answer:** Winsock is a Microsoft Windows programming interface and runtime dynamic-link library (`ws2_32.dll`) that enables Windows applications to utilize standard Berkeley socket-style networking functions natively.

### Q10: What does `WSAStartup()` do?
**Answer:** `WSAStartup()` initiates the Windows Sockets DLL for the calling process, negotiates the requested Winsock version (e.g., 2.2 via `MAKEWORD(2,2)`), and initializes internal system data structures. It must be called prior to any socket function.

### Q11: What does `socket()` do?
**Answer:** `socket()` creates an unbound socket descriptor by specifying the address family (`AF_INET` for IPv4), socket type (`SOCK_STREAM` for streaming), and transport protocol (`IPPROTO_TCP` for TCP).

### Q12: What does `bind()` do?
**Answer:** `bind()` associates a created socket handle with a specific local network interface IP address and port number encapsulated in a `sockaddr_in` structure.

### Q13: What does `listen()` do?
**Answer:** `listen()` converts an active socket into a passive listening socket, configuring the maximum backlog queue length of incoming connection requests awaiting acceptance.

### Q14: What does `accept()` do?
**Answer:** `accept()` extracts the first connection request from the listening socket's pending queue, creates a new active socket descriptor dedicated to that specific client connection, and returns it while leaving the listening socket untouched.

### Q15: What does `connect()` do?
**Answer:** `connect()` is invoked by a client application to initiate an active TCP connection to a specified server IP and port, triggering the TCP three-way handshake.

### Q16: What does `send()` do, and why did you wrap it in `send_all()`?
**Answer:** `send()` transfers bytes to the outgoing network buffer. Because TCP is a stream protocol, `send()` might transmit fewer bytes than requested (partial send). `send_all()` loops repeatedly until every requested byte is transmitted.

### Q17: What does `recv()` do, and why is `recv_all()` necessary?
**Answer:** `recv()` reads available incoming bytes from the socket buffer. Because network packets may arrive fragmented across TCP segments, `recv_all()` repeatedly calls `recv()` until the exact requested packet header or payload size has been fully accumulated.

### Q18: What does `closesocket()` do?
**Answer:** `closesocket()` closes a socket descriptor on Windows, frees the socket handle, and transmits a TCP `FIN` packet to initiate graceful connection termination with the remote host.

### Q19: What does `WSACleanup()` do?
**Answer:** `WSACleanup()` terminates all usage of the Winsock 2 dynamic-link library for the calling process, releasing all allocated networking resources and socket handles.

### Q20: Explain the TCP Three-Way Handshake.
**Answer:** To establish a reliable TCP connection:
1. Client sends `SYN` (Synchronize) with an initial sequence number ($x$).
2. Server responds with `SYN + ACK` containing its sequence number ($y$) and acknowledges $x+1$.
3. Client sends `ACK` containing $y+1$. Both endpoints transition to `ESTABLISHED`.

### Q21: Why are threads necessary in a multi-client server?
**Answer:** Network socket operations like `recv()` and `accept()` are blocking calls. In a single-threaded server, waiting for data from Client A completely freezes the server, preventing Client B from connecting or sending messages. Threads allow independent, simultaneous handling of each client.

### Q22: Why did you use `_beginthreadex()` instead of `CreateThread()`?
**Answer:** `_beginthreadex()` initializes thread-local C Runtime (CRT) memory structures, ensuring thread-safe usage of standard C library functions (`printf`, `malloc`, `strtok`) without resource leaks.

### Q23: How are shared resources protected in your server?
**Answer:** Shared data structures (the active client array and log file) are protected using Windows Critical Sections (`CRITICAL_SECTION`). When a thread executes `EnterCriticalSection()`, no other thread can read or write the client table until `LeaveCriticalSection()` is executed.

### Q24: How does private messaging work?
**Answer:** The sender transmits a `PKT_PRIVATE_MSG` packet identifying the recipient. The server enters the critical section, looks up the recipient's username in the active client table, and routes the packet directly to the recipient's dedicated socket descriptor.

### Q25: How does the chunked file transfer work?
**Answer:** The sender reads the file into 4096-byte buffers. It first transmits a `PKT_FILE_START` header with the filename and size. It then loops transmitting `PKT_FILE_DATA` packets containing each chunk. Finally, it sends a `PKT_FILE_END` packet. The receiver writes chunks directly to disk in `received_files/`.

---

## 28. College Lab Demonstration Plan (5–10 Minutes)

| Time | Step | Action | Command / Visual |
| :---: | :--- | :--- | :--- |
| **0:00 - 1:00** | **1. Start Server** | Launch `run_server.bat` | Displays server banner, host name, IP (`127.0.0.1` and LAN IP), and status `RUNNING`. |
| **1:00 - 2:00** | **2. Connect Client 1** | Open Terminal 1 -> `client.exe` | Enter Username: `Nithish`. Server logs `[+] Client connected: Nithish`. |
| **2:00 - 3:00** | **3. Connect Client 2 & 3** | Open Terminal 2 & 3 | Connect `Rahul` and `Arjun`. Server logs connections; clients receive join notifications. |
| **3:00 - 4:00** | **4. Show Online Users** | In Nithish's window: `/users` | Displays table showing Nithish, Rahul, and Arjun with connection uptimes. |
| **4:00 - 5:00** | **5. Broadcast Message** | In Nithish's window: `/all Hello everyone!` | Terminal 2 (Rahul) and Terminal 3 (Arjun) instantly display `[MSG] Nithish: Hello everyone!`. |
| **5:00 - 6:00** | **6. Private Message** | In Rahul's window: `/msg Nithish Hi Nithish!` | Only Nithish receives `[PRIVATE from Rahul]: Hi Nithish!`. Arjun sees nothing. |
| **6:00 - 7:30** | **7. File Transfer** | In Nithish's window: `/sendfile Rahul sample.txt` | Nithish sends chunk; Rahul receives notification and file saved in `files/received_files/sample.txt`. |
| **7:30 - 8:30** | **8. Duplicate Name Test** | Open Terminal 4 -> Enter `Nithish` | Server rejects with `PKT_CONNECT_NACK` stating username is already taken. |
| **8:30 - 9:30** | **9. Disconnect Client** | In Arjun's window: `/quit` | Arjun exits cleanly. Server and remaining clients receive leave notification. |
| **9:30 - 10:00** | **10. View Activity Log** | Open `server/server.log` | Review complete, timestamped forensic log of all connections, chats, and transfers. |
