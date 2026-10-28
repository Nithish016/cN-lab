# Multi-Client Chat Application using TCP Socket Programming in C (Windows)

**Computer Networks Laboratory Project**  
**Department of Information Technology**  
**Operating System:** Windows 10 / Windows 11  
**Networking API:** Windows Sockets 2 (Winsock 2 - `ws2_32.lib`)  
**Language:** C (Standard C99 / C11)

---

## 1. Project Overview

This project is a multi-threaded, multi-client chat and file transfer system developed natively for the **Windows** operating system using **C** and the **Winsock 2 API** (`winsock2.h`). 

The application utilizes a robust client-server architecture over reliable TCP sockets. The server handles multiple concurrent client connections asynchronously using native Windows multi-threading (`_beginthreadex`), synchronizing shared client registries with Windows Critical Sections (`CRITICAL_SECTION`). In addition to multi-user broadcast messaging and direct private messaging, the system implements a framed binary file transfer protocol allowing users to send files of any size directly across clients through the TCP stream.

---

## 2. System Architecture

```text
                           +-------------------------------------+
                           |            CHAT SERVER              |
                           |       (Winsock 2 / TCP:8080)        |
                           |  - Accept Loop                      |
                           |  - Thread Pool / Client Workers     |
                           |  - Synchronized Registry (Mutex/CS) |
                           |  - Activity Logger (server.log)     |
                           +------------------+------------------+
                                              |
               +------------------------------+------------------------------+
               | (TCP Connection 1)           | (TCP Connection 2)           | (TCP Connection 3)
               v                              v                              v
      +-----------------+            +-----------------+            +-----------------+
      |    CLIENT 1     |            |    CLIENT 2     |            |    CLIENT 3     |
      |   (e.g. Nithish)|            |   (e.g. Rahul)  |            |   (e.g. Arjun)  |
      |  - Main Input   |            |  - Main Input   |            |  - Main Input   |
      |  - Recv Thread  |            |  - Recv Thread  |            |  - Recv Thread  |
      +--------+--------+            +--------+--------+            +--------+--------+
               |                              |                              |
         [Chat / Files]                 [Chat / Files]                 [Chat / Files]
```

### Protocol Layering and Data Flow

```text
IP Address (IPv4: e.g. 192.168.1.10 / 127.0.0.1)
     |
Port Number (e.g. TCP 8080)
     |
Socket Endpoint (socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))
     |
TCP Connection (Three-Way Handshake: SYN -> SYN+ACK -> ACK)
     |
Framed Packet Transmission (Magic 'CNLA' + Type + Payload Length + Data)
     |
Connection Termination (FIN / PKT_DISCONNECT -> closesocket() -> WSACleanup())
```

---

## 3. Repository Structure

```text
MultiClientChat/
│
├── server/
│   ├── server.c             # Multi-client TCP server implementation
│   ├── server.h             # Server headers, client structures, prototypes
│   └── server.log           # Persistent server activity log
│
├── client/
│   ├── client.c             # Client implementation (UI, background receiver thread)
│   └── client.h             # Client prototypes and headers
│
├── shared/
│   └── common.h             # Winsock packet framing, packet types, network helpers
│
├── files/
│   ├── test_files/          # Files ready to be sent with /sendfile
│   │   ├── sample.txt
│   │   ├── notes.txt
│   │   └── lab_manual.txt
│   └── received_files/      # Download directory for received files
│
├── received_files/          # Root download fallback directory
├── build.bat                # One-click Windows build script (Auto-detects GCC/MSVC)
├── run_server.bat           # Starts server on port 8080 in a new window
├── run_client.bat           # Starts client instance
├── Makefile                 # Build automation for MinGW Make / MSYS2
├── test_suite.ps1           # Automated test suite validating all 14 test cases
├── README.md                # Project documentation and guide
└── documentation/
    └── project_report.md    # Formal college laboratory project report (27 sections)
```

---

## 4. Key Winsock 2 Functions Used

| Function | Purpose in Project |
| :--- | :--- |
| `WSAStartup()` | Initializes the Winsock 2.2 DLL runtime and populates `WSADATA`. |
| `socket()` | Allocates a stream socket descriptor configured for IPv4 TCP (`AF_INET, SOCK_STREAM, IPPROTO_TCP`). |
| `bind()` | Binds the listening socket to `INADDR_ANY` and designated TCP port (e.g., 8080). |
| `listen()` | Places the server socket into passive state to queue incoming connection attempts (`SOMAXCONN`). |
| `accept()` | Extracts the next pending client connection request from the queue and creates a dedicated client socket. |
| `connect()` | Establishes the TCP connection from client to server (initiates TCP 3-way handshake). |
| `send()` | Transmits byte stream data across an active TCP socket. Encapsulated in `send_all()` to handle partial writes. |
| `recv()` | Receives byte stream data from socket. Encapsulated in `recv_all()` to guarantee complete packet header framing. |
| `closesocket()` | Closes the socket handle, transmits TCP FIN to terminate connection, and frees kernel resources. |
| `WSACleanup()` | Deregisters the application from Winsock DLL and deallocates network subsystem memory. |

---

## 5. Development Environment & Compilation

### Option A: Using MinGW GCC (Recommended for College Labs)

If MinGW GCC is installed, simply compile from the command line:

```bash
# Compile Server
gcc server/server.c -o server.exe -lws2_32 -Wall -Wextra

# Compile Client
gcc client/client.c -o client.exe -lws2_32 -Wall -Wextra
```

*Note: If you do not have GCC on Windows, you can install it via Windows Package Manager:*
```powershell
winget install MartinStorsjo.LLVM-MinGW.UCRT
```

### Option B: Using Visual Studio / MSVC (`cl.exe`)

Open the **Developer Command Prompt for VS** and run:

```cmd
cl.exe /W3 /O2 /MD /Fe:server.exe server\server.c ws2_32.lib
cl.exe /W3 /O2 /MD /Fe:client.exe client\client.c ws2_32.lib
```

Or open Visual Studio -> Create New Empty C++ Project -> Add existing files -> Under Linker Input add `ws2_32.lib`.

### Option C: Using One-Click Build Script (`build.bat`)

Double-click `build.bat` in File Explorer or run in terminal:

```cmd
build.bat
```
`build.bat` automatically detects your installed compiler (MinGW GCC, WinGet LLVM-MinGW, MSYS2, or Visual Studio MSVC) and builds both `server.exe` and `client.exe`.

---

## 6. How to Run and Test

### Mode 1 — Same Computer (Loopback Testing)

1. **Start Server:**
   Double-click `run_server.bat` (or execute `.\server.exe 8080`).
   The server console displays:
   ```text
   ========================================================
                MULTI-CLIENT CHAT SERVER
                COMPUTER NETWORKS LAB
   ========================================================
     Host Name: DESKTOP-LAB
     Available IP Addresses:
       -> 127.0.0.1 (Loopback)
       -> 192.168.1.10 (LAN / Wi-Fi)
     Port     : 8080
     Status   : RUNNING (Press Ctrl+C to stop)
   ========================================================
   ```

2. **Start Client 1 (Nithish):**
   Open a Command Prompt and run:
   ```cmd
   client.exe
   ```
   - Enter Server IP: `127.0.0.1` (or press Enter for default)
   - Enter Port: `8080` (or press Enter for default)
   - Enter Username: `Nithish`

3. **Start Client 2 (Rahul):**
   Open a second Command Prompt and run `client.exe`:
   - Enter Server IP: `127.0.0.1`
   - Enter Port: `8080`
   - Enter Username: `Rahul`

4. **Start Client 3 (Arjun):**
   Open a third Command Prompt and run `client.exe`:
   - Enter Server IP: `127.0.0.1`
   - Enter Port: `8080`
   - Enter Username: `Arjun`

---

### Mode 2 — Same Wi-Fi / Local Area Network (LAN)

To allow a classmate or separate laptop on the same Wi-Fi network to connect:

1. **Find Server IP:**
   On the host PC running the server, open Command Prompt and type:
   ```cmd
   ipconfig
   ```
   Look for **IPv4 Address** under `Wireless LAN adapter Wi-Fi` or `Ethernet adapter` (e.g., `192.168.1.28`).

2. **Windows Firewall Rule (If prompted):**
   When `server.exe` starts, Windows Defender Firewall may display an alert. Check both **Private networks** and click **Allow access**.
   Alternatively, add a temporary firewall port rule in administrative PowerShell:
   ```powershell
   New-NetFirewallRule -DisplayName "ChatServer8080" -Direction Inbound -LocalPort 8080 -Protocol TCP -Action Allow
   ```

3. **Connect from Client PC:**
   On the client PC:
   ```cmd
   client.exe
   ```
   - Enter Server IP: `192.168.1.28` (Server PC's LAN IP)
   - Enter Port: `8080`
   - Enter Username: `Rahul`

---

## 7. Chat Commands Reference

| Command | Syntax | Description | Example |
| :--- | :--- | :--- | :--- |
| **Online Users** | `/users` | Lists all active connected clients with uptime and IP. | `/users` |
| **Private Message** | `/msg <user> <message>` | Unicast private message to specific user only. | `/msg Rahul Hello Rahul!` |
| **Broadcast Message** | `/all <message>` | Transmits message to all other connected clients. | `/all Hello everyone` |
| **Normal Message** | `<message>` | Plain text without `/` is automatically broadcast. | `Good morning class` |
| **File Transfer** | `/sendfile <user> <file>` | Transfers file safely via chunked TCP packets. | `/sendfile Rahul sample.txt` |
| **Help Menu** | `/help` | Displays available command list. | `/help` |
| **Quit** | `/quit` | Sends disconnect packet and exits safely. | `/quit` |

---

## 8. File Transfer Operation

Files to be sent can be placed in `files/test_files/` (e.g., `sample.txt`, `notes.txt`, `lab_manual.txt`) or referenced by relative/absolute path.

Example:
```text
Nithish > /sendfile Rahul sample.txt
[FILE] Sending 'sample.txt' (572 bytes in 1 chunks) to Rahul...
[FILE] File 'sample.txt' successfully sent to Rahul!
```

On Rahul's terminal:
```text
[FILE] Receiving 'sample.txt' (572 bytes) from Nithish...
[FILE] File 'sample.txt' successfully received from Nithish!
       Saved to: files/received_files/sample.txt (572 of 572 bytes)
Rahul >
```

Received files are automatically stored into `files/received_files/` with exact byte integrity.

---

## 9. Automated Test Suite

An automated test script (`test_suite.ps1`) is provided to validate all 14 required lab test cases. To run the test suite:

```powershell
powershell -ExecutionPolicy Bypass -File test_suite.ps1
```

### Verified Test Results:
- [x] Test 1: Server Startup & Port Binding
- [x] Test 2: Single Client Connection Handshake
- [x] Test 3: Multiple Simultaneous Clients (5+ connected)
- [x] Test 4: Broadcast Messaging
- [x] Test 5: Unicast Private Messaging
- [x] Test 6: Online Users Directory Query
- [x] Test 7: Duplicate Username Rejection
- [x] Test 8: Invalid Command Handling
- [x] Test 9: Graceful Client Disconnection
- [x] Test 10: Server Unavailable Handling
- [x] Test 11: Binary File Transfer
- [x] Test 12: High-Volume Simultaneous Messages
- [x] Test 13: Large Multi-Chunk File Stream
- [x] Test 14: Unexpected Client Disconnection (Abrupt Kill)

---

## 10. License

Developed for Academic and Laboratory Educational Purposes in Computer Networks (B.Tech IT). Open-source under MIT License.

---

## 👨‍💻 Developer & Author

- **Name:** Gubba Nithish
- **GitHub:** [@Nithish016](https://github.com/Nithish016)
- **Email:** [gubbanithish9@gmail.com](mailto:gubbanithish9@gmail.com)
- **Program:** B.Tech — Information Technology
- **Course:** Computer Networks Laboratory Project

---
*Developed natively for Windows using C and Winsock 2 (`ws2_32.lib`).*
