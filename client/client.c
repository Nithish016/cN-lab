/**
 * ============================================================================
 * MULTI-CLIENT CHAT APPLICATION - COMPUTER NETWORKS LAB PROJECT
 * File: client/client.c
 * Description: Client Implementation using Windows Winsock 2 and Multi-threading.
 * Platform: Windows 10/11
 *
 * Demonstrates:
 *   - Winsock 2 runtime initialization (WSAStartup)
 *   - TCP Socket creation (socket)
 *   - Connection establishment to Server (connect)
 *   - Bi-directional framing and messaging (send/recv)
 *   - Dedicated background receiver thread (_beginthreadex)
 *   - Binary File Transfer with chunking
 *   - Graceful termination (closesocket, WSACleanup)
 * ============================================================================
 */

#include "client.h"

/* ------------------------------------------------------------------------- */
/* GLOBAL CLIENT STATE                                                       */
/* ------------------------------------------------------------------------- */
/* Global connection info */
static SOCKET         g_socket = INVALID_SOCKET;
static char           g_username[MAX_USERNAME_LEN] = {0};
static char           g_server_ip[INET_ADDRSTRLEN] = {0};
static int            g_server_port = DEFAULT_PORT;
static volatile bool  g_connected = false;
static HANDLE         g_recv_thread_handle = NULL;
static CRITICAL_SECTION g_console_lock;

/* File reception state */
static FILE          *g_incoming_file = NULL;
static char           g_incoming_filename[MAX_FILENAME_LEN] = {0};
static char           g_incoming_sender[MAX_USERNAME_LEN] = {0};
static uint64_t       g_incoming_filesize = 0;
static uint64_t       g_incoming_bytes_received = 0;

/* ------------------------------------------------------------------------- */
/* UI & BANNER DISPLAY                                                       */
/* ------------------------------------------------------------------------- */

void print_startup_banner(void) {
    printf("==================================================\n");
    printf("       MULTI-CLIENT CHAT APPLICATION              \n");
    printf("       COMPUTER NETWORKS LAB                      \n");
    printf("==================================================\n\n");
    fflush(stdout);
}

void print_connected_banner(const char *ip, int port, const char *username) {
    EnterCriticalSection(&g_console_lock);
    printf("\n========================================================\n");
    printf("             CONNECTED TO SERVER                        \n");
    printf("========================================================\n");
    printf("  Username : %s\n", username);
    printf("  Server   : %s\n", ip);
    printf("  Port     : %d\n", port);
    printf("  Status   : CONNECTED\n\n");
    printf("Available Commands:\n");
    printf("  /users\n");
    printf("  /msg <username> <message>\n");
    printf("  /all <message> (or plain text)\n");
    printf("  /sendfile <username> <filename>\n");
    printf("  /help\n");
    printf("  /quit\n");
    printf("========================================================\n\n");
    fflush(stdout);
    LeaveCriticalSection(&g_console_lock);
}

void print_help_menu(void) {
    EnterCriticalSection(&g_console_lock);
    printf("\n--- Available Chat Commands ---\n");
    printf("  /users                          - List all currently online users\n");
    printf("  /msg <username> <message>       - Send a private message to a specific user\n");
    printf("  /all <message>                  - Broadcast a message to everyone\n");
    printf("  <plain message>                 - Broadcast a message (shortcut)\n");
    printf("  /sendfile <username> <filepath> - Transfer a file directly to a user\n");
    printf("  /help                           - Show this help menu\n");
    printf("  /quit                           - Safely disconnect from the chat server\n");
    printf("-------------------------------\n\n");
    fflush(stdout);
    LeaveCriticalSection(&g_console_lock);
}

void print_prompt(void) {
    if (g_connected) {
        printf("%s > ", g_username);
        fflush(stdout);
    }
}

/* ------------------------------------------------------------------------- */
/* WINSOCK INITIALIZATION & TEARDOWN                                         */
/* ------------------------------------------------------------------------- */

bool init_winsock(void) {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        fprintf(stderr, "[ERROR] WSAStartup failed with code: %d\n", result);
        return false;
    }
    return true;
}

void cleanup_winsock(void) {
    WSACleanup();
}

/* ------------------------------------------------------------------------- */
/* BACKGROUND RECEIVER THREAD                                                */
/* ------------------------------------------------------------------------- */

/**
 * Worker thread that continuously listens for incoming packets from the server.
 * Handles broadcasts, private messages, notifications, and file chunks asynchronously.
 */
unsigned __stdcall receive_thread(void *arg) {
    (void)arg;
    PacketHeader header;
    char payload_buf[BUFFER_SIZE];

    while (g_connected) {
        int res = recv_packet(g_socket, &header, payload_buf, sizeof(payload_buf));
        if (res <= 0) {
            /* Connection broken or server shut down */
            if (g_connected) {
                EnterCriticalSection(&g_console_lock);
                printf("\n[-] Disconnected from server. Server may have shut down.\n");
                fflush(stdout);
                LeaveCriticalSection(&g_console_lock);
                g_connected = false;
            }
            break;
        }

        EnterCriticalSection(&g_console_lock);

        switch (header.type) {
            case PKT_BROADCAST: {
                BroadcastPayload *bcast = (BroadcastPayload *)payload_buf;
                printf("\r\033[K[MSG] %s: %s\n", bcast->sender, bcast->message);
                print_prompt();
                break;
            }

            case PKT_PRIVATE_MSG: {
                PrivateMsgPayload *pmsg = (PrivateMsgPayload *)payload_buf;
                if (strcmp(pmsg->sender, g_username) == 0) {
                    /* Confirmation of private message sent */
                    printf("\r\033[K[PRIVATE to %s]: %s\n", pmsg->recipient, pmsg->message);
                } else {
                    /* Inbound private message received */
                    printf("\r\033[K[PRIVATE from %s]: %s\n", pmsg->sender, pmsg->message);
                }
                print_prompt();
                break;
            }

            case PKT_NOTIFICATION: {
                NotificationPayload *notif = (NotificationPayload *)payload_buf;
                printf("\r\033[K%s\n", notif->message);
                print_prompt();
                break;
            }

            case PKT_USERS_RESP: {
                UsersRespPayload *users = (UsersRespPayload *)payload_buf;
                printf("\r\033[K%s\n", users->user_list);
                print_prompt();
                break;
            }

            case PKT_FILE_START: {
                FileStartPayload *fstart = (FileStartPayload *)payload_buf;
                strncpy_s(g_incoming_filename, MAX_FILENAME_LEN, fstart->filename, _TRUNCATE);
                strncpy_s(g_incoming_sender, MAX_USERNAME_LEN, fstart->sender, _TRUNCATE);
                g_incoming_filesize = fstart->filesize;
                g_incoming_bytes_received = 0;

                /* Ensure received_files/ directory exists */
                ensure_directory_exists("received_files");
                ensure_directory_exists("files/received_files");

                /* Prefer files/received_files/ if files/ exists, otherwise received_files/ */
                char dest_filepath[512];
                struct _stat st;
                if (_stat("files", &st) == 0 && (st.st_mode & _S_IFDIR)) {
                    snprintf(dest_filepath, sizeof(dest_filepath), "files/received_files/%s", fstart->filename);
                } else {
                    snprintf(dest_filepath, sizeof(dest_filepath), "received_files/%s", fstart->filename);
                }

                fopen_s(&g_incoming_file, dest_filepath, "wb");
                if (g_incoming_file) {
                    printf("\r\033[K[FILE] Receiving '%s' (%llu bytes) from %s...\n",
                           fstart->filename, (unsigned long long)fstart->filesize, fstart->sender);
                } else {
                    printf("\r\033[K[-] Error creating destination file '%s'.\n", dest_filepath);
                }
                print_prompt();
                break;
            }

            case PKT_FILE_DATA: {
                FileDataPayload *fdata = (FileDataPayload *)payload_buf;
                if (g_incoming_file) {
                    size_t written = fwrite(fdata->data, 1, fdata->chunk_size, g_incoming_file);
                    g_incoming_bytes_received += written;
                }
                break;
            }

            case PKT_FILE_END: {
                FileEndPayload *fend = (FileEndPayload *)payload_buf;
                if (g_incoming_file) {
                    fclose(g_incoming_file);
                    g_incoming_file = NULL;

                    char dest_filepath[512];
                    struct _stat st;
                    if (_stat("files", &st) == 0 && (st.st_mode & _S_IFDIR)) {
                        snprintf(dest_filepath, sizeof(dest_filepath), "files/received_files/%s", fend->filename);
                    } else {
                        snprintf(dest_filepath, sizeof(dest_filepath), "received_files/%s", fend->filename);
                    }

                    printf("\r\033[K[FILE] File '%s' successfully received from %s!\n",
                           fend->filename, fend->sender);
                    printf("       Saved to: %s (%llu of %llu bytes)\n",
                           dest_filepath, (unsigned long long)g_incoming_bytes_received,
                           (unsigned long long)g_incoming_filesize);
                }
                print_prompt();
                break;
            }

            case PKT_FILE_ERROR: {
                FileEndPayload *ferr = (FileEndPayload *)payload_buf;
                printf("\r\033[K[-] File transfer failed: %s\n", ferr->message);
                print_prompt();
                break;
            }

            default:
                break;
        }

        LeaveCriticalSection(&g_console_lock);
    }

    if (g_incoming_file) {
        fclose(g_incoming_file);
        g_incoming_file = NULL;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
/* CONNECTION SETUP                                                          */
/* ------------------------------------------------------------------------- */

/**
 * Connects TCP socket to server and completes registration handshake.
 */
bool connect_to_server(const char *ip, int port, const char *username) {
    /* 1. Create Socket */
    g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_socket == INVALID_SOCKET) {
        fprintf(stderr, "[ERROR] socket() failed with error code: %d\n", WSAGetLastError());
        return false;
    }

    /* 2. Configure Server Address Structure */
    struct sockaddr_in server_addr;
    ZeroMemory(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((u_short)port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "[ERROR] Invalid IP address format: '%s'\n", ip);
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        return false;
    }

    /* 3. Connect to Server */
    printf("[*] Connecting to %s:%d...\n", ip, port);
    if (connect(g_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "[ERROR] Connection failed (Error: %d). Is the server running?\n", WSAGetLastError());
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        return false;
    }

    /* 4. Send Registration Handshake (PKT_CONNECT_REQ) */
    ConnectReqPayload req;
    strncpy_s(req.username, MAX_USERNAME_LEN, username, _TRUNCATE);
    if (!send_packet(g_socket, PKT_CONNECT_REQ, &req, sizeof(req))) {
        fprintf(stderr, "[ERROR] Failed to send handshake request to server.\n");
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        return false;
    }

    /* 5. Await Server Response (PKT_CONNECT_ACK or PKT_CONNECT_NACK) */
    PacketHeader header;
    ConnectAckPayload ack;
    int res = recv_packet(g_socket, &header, &ack, sizeof(ack));
    if (res <= 0 || header.type != PKT_CONNECT_ACK) {
        if (header.type == PKT_CONNECT_NACK) {
            fprintf(stderr, "\n[-] Connection Rejected: %s\n\n", ack.message);
        } else {
            fprintf(stderr, "\n[-] Handshake failed or server terminated connection.\n\n");
        }
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        return false;
    }

    /* Handshake verified */
    strncpy_s(g_username, MAX_USERNAME_LEN, username, _TRUNCATE);
    strncpy_s(g_server_ip, INET_ADDRSTRLEN, ip, _TRUNCATE);
    g_server_port = port;
    g_connected = true;

    /* 6. Launch Asynchronous Receiver Thread */
    uintptr_t thread_handle = _beginthreadex(NULL, 0, receive_thread, NULL, 0, NULL);
    if (thread_handle == 0) {
        fprintf(stderr, "[ERROR] Failed to create background receiver thread.\n");
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
        g_connected = false;
        return false;
    }
    g_recv_thread_handle = (HANDLE)thread_handle;

    print_connected_banner(ip, port, username);
    return true;
}

/**
 * Disconnects safely from the chat server.
 */
void disconnect_from_server(void) {
    if (!g_connected && g_socket == INVALID_SOCKET) {
        return;
    }

    g_connected = false;

    if (g_socket != INVALID_SOCKET) {
        /* Send graceful disconnect notification */
        send_packet(g_socket, PKT_DISCONNECT, NULL, 0);
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
    }

    if (g_recv_thread_handle != NULL) {
        WaitForSingleObject(g_recv_thread_handle, 1000);
        CloseHandle(g_recv_thread_handle);
        g_recv_thread_handle = NULL;
    }

    printf("\n[SYS] Disconnected safely from server %s:%d. Goodbye!\n", g_server_ip, g_server_port);
}

/* ------------------------------------------------------------------------- */
/* COMMANDS & MESSAGING IMPLEMENTATION                                       */
/* ------------------------------------------------------------------------- */

void send_broadcast(const char *message) {
    if (!g_connected) return;
    BroadcastPayload payload;
    strncpy_s(payload.sender, MAX_USERNAME_LEN, g_username, _TRUNCATE);
    strncpy_s(payload.message, MAX_MESSAGE_LEN, message, _TRUNCATE);
    send_packet(g_socket, PKT_BROADCAST, &payload, sizeof(payload));
}

void send_private(const char *recipient, const char *message) {
    if (!g_connected) return;
    PrivateMsgPayload payload;
    strncpy_s(payload.sender, MAX_USERNAME_LEN, g_username, _TRUNCATE);
    strncpy_s(payload.recipient, MAX_USERNAME_LEN, recipient, _TRUNCATE);
    strncpy_s(payload.message, MAX_MESSAGE_LEN, message, _TRUNCATE);
    send_packet(g_socket, PKT_PRIVATE_MSG, &payload, sizeof(payload));
}

void request_online_users(void) {
    if (!g_connected) return;
    send_packet(g_socket, PKT_USERS_REQ, NULL, 0);
}

/**
 * Transfers a file chunk-by-chunk to the specified user over TCP.
 */
void send_file_to_user(const char *recipient, const char *filepath) {
    if (!g_connected) return;

    /* Check local file existence */
    char resolved_path[512];
    strncpy_s(resolved_path, sizeof(resolved_path), filepath, _TRUNCATE);

    FILE *file = NULL;
    fopen_s(&file, resolved_path, "rb");

    /* Fallback search in test folders if relative path wasn't found directly */
    if (!file) {
        snprintf(resolved_path, sizeof(resolved_path), "files/test_files/%s", filepath);
        fopen_s(&file, resolved_path, "rb");
    }
    if (!file) {
        snprintf(resolved_path, sizeof(resolved_path), "test_files/%s", filepath);
        fopen_s(&file, resolved_path, "rb");
    }

    if (!file) {
        printf("[-] Error: File '%s' does not exist or could not be opened for reading.\n", filepath);
        return;
    }

    /* Compute File Size */
    _fseeki64(file, 0, SEEK_END);
    uint64_t filesize = _ftelli64(file);
    _fseeki64(file, 0, SEEK_SET);

    const char *base_name = get_base_filename(filepath);
    uint32_t total_chunks = (uint32_t)((filesize + FILE_CHUNK_SIZE - 1) / FILE_CHUNK_SIZE);
    if (total_chunks == 0) total_chunks = 1;

    printf("[FILE] Sending '%s' (%llu bytes in %u chunks) to %s...\n",
           base_name, (unsigned long long)filesize, total_chunks, recipient);

    /* 1. Transmit File Transfer Header (PKT_FILE_START) */
    FileStartPayload fstart;
    strncpy_s(fstart.sender, MAX_USERNAME_LEN, g_username, _TRUNCATE);
    strncpy_s(fstart.recipient, MAX_USERNAME_LEN, recipient, _TRUNCATE);
    strncpy_s(fstart.filename, MAX_FILENAME_LEN, base_name, _TRUNCATE);
    fstart.filesize = filesize;
    fstart.total_chunks = total_chunks;

    if (!send_packet(g_socket, PKT_FILE_START, &fstart, sizeof(fstart))) {
        printf("[-] Failed to send file start header to server.\n");
        fclose(file);
        return;
    }

    /* 2. Transmit Binary File Chunks (PKT_FILE_DATA) */
    FileDataPayload fdata;
    strncpy_s(fdata.sender, MAX_USERNAME_LEN, g_username, _TRUNCATE);
    strncpy_s(fdata.recipient, MAX_USERNAME_LEN, recipient, _TRUNCATE);
    strncpy_s(fdata.filename, MAX_FILENAME_LEN, base_name, _TRUNCATE);

    uint32_t chunk_idx = 0;
    size_t bytes_read = 0;
    while ((bytes_read = fread(fdata.data, 1, FILE_CHUNK_SIZE, file)) > 0) {
        fdata.chunk_index = chunk_idx++;
        fdata.chunk_size = (uint32_t)bytes_read;

        if (!send_packet(g_socket, PKT_FILE_DATA, &fdata, sizeof(FileDataPayload))) {
            printf("[-] Network error while transmitting file chunk %u.\n", chunk_idx);
            fclose(file);
            return;
        }

        /* Brief yield to avoid starving receiver buffer on fast localhost */
        Sleep(5);
    }
    fclose(file);

    /* 3. Transmit Completion Marker (PKT_FILE_END) */
    FileEndPayload fend;
    strncpy_s(fend.sender, MAX_USERNAME_LEN, g_username, _TRUNCATE);
    strncpy_s(fend.recipient, MAX_USERNAME_LEN, recipient, _TRUNCATE);
    strncpy_s(fend.filename, MAX_FILENAME_LEN, base_name, _TRUNCATE);
    fend.success = 1;
    snprintf(fend.message, sizeof(fend.message), "File successfully transmitted.");
    send_packet(g_socket, PKT_FILE_END, &fend, sizeof(fend));

    printf("[FILE] File '%s' successfully sent to %s!\n", base_name, recipient);
}

/**
 * Parses user input from console and dispatches to appropriate command handler.
 */
void handle_user_input(char *line) {
    trim_newline(line);
    if (strlen(line) == 0) {
        return;
    }

    if (line[0] == '/') {
        if (_strnicmp(line, "/users", 6) == 0) {
            request_online_users();
        } else if (_strnicmp(line, "/help", 5) == 0) {
            print_help_menu();
        } else if (_strnicmp(line, "/quit", 5) == 0) {
            disconnect_from_server();
        } else if (_strnicmp(line, "/all ", 5) == 0) {
            const char *msg = line + 5;
            while (*msg == ' ') msg++;
            if (strlen(msg) > 0) {
                send_broadcast(msg);
            } else {
                printf("Usage: /all <message>\n");
            }
        } else if (_strnicmp(line, "/msg ", 5) == 0) {
            char recipient[MAX_USERNAME_LEN] = {0};
            const char *ptr = line + 5;
            while (*ptr == ' ') ptr++;

            int scanned = sscanf_s(ptr, "%31s", recipient, (unsigned)sizeof(recipient));
            if (scanned == 1) {
                const char *msg_ptr = ptr + strlen(recipient);
                while (*msg_ptr == ' ') msg_ptr++;
                if (strlen(msg_ptr) > 0) {
                    send_private(recipient, msg_ptr);
                } else {
                    printf("Usage: /msg <username> <message>\n");
                }
            } else {
                printf("Usage: /msg <username> <message>\n");
            }
        } else if (_strnicmp(line, "/sendfile ", 10) == 0) {
            char recipient[MAX_USERNAME_LEN] = {0};
            char filename[MAX_FILENAME_LEN] = {0};
            const char *ptr = line + 10;
            while (*ptr == ' ') ptr++;

            int scanned = sscanf_s(ptr, "%31s %255s", recipient, (unsigned)sizeof(recipient),
                                   filename, (unsigned)sizeof(filename));
            if (scanned == 2) {
                send_file_to_user(recipient, filename);
            } else {
                printf("Usage: /sendfile <username> <filename>\n");
            }
        } else {
            printf("[-] Unknown command: '%s'. Type /help for available commands.\n", line);
        }
    } else {
        /* Normal text treated as broadcast message */
        send_broadcast(line);
    }
}

/* ------------------------------------------------------------------------- */
/* CONSOLE CTRL HANDLER                                                      */
/* ------------------------------------------------------------------------- */
static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            disconnect_from_server();
            cleanup_winsock();
            ExitProcess(0);
            return TRUE;
        default:
            return FALSE;
    }
}

/* ------------------------------------------------------------------------- */
/* MAIN ENTRY POINT                                                          */
/* ------------------------------------------------------------------------- */
int main(int argc, char *argv[]) {
    char server_ip[INET_ADDRSTRLEN] = DEFAULT_IP;
    int port = DEFAULT_PORT;
    char username[MAX_USERNAME_LEN] = {0};

    InitializeCriticalSection(&g_console_lock);
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    if (!init_winsock()) {
        fprintf(stderr, "[FATAL] Failed to initialize Winsock 2.2.\n");
        return 1;
    }

    print_startup_banner();

    /* Optional CLI argument overrides: client.exe [ip] [port] [username] */
    if (argc >= 2) {
        strncpy_s(server_ip, sizeof(server_ip), argv[1], _TRUNCATE);
    } else {
        printf("Enter Server IP [Default: %s]: ", DEFAULT_IP);
        char input_ip[INET_ADDRSTRLEN];
        if (fgets(input_ip, sizeof(input_ip), stdin)) {
            trim_newline(input_ip);
            if (strlen(input_ip) > 0) {
                strncpy_s(server_ip, sizeof(server_ip), input_ip, _TRUNCATE);
            }
        }
    }

    if (argc >= 3) {
        port = atoi(argv[2]);
        if (port <= 0 || port > 65535) port = DEFAULT_PORT;
    } else {
        printf("Enter Port [Default: %d]: ", DEFAULT_PORT);
        char input_port[16];
        if (fgets(input_port, sizeof(input_port), stdin)) {
            trim_newline(input_port);
            if (strlen(input_port) > 0) {
                int p = atoi(input_port);
                if (p > 0 && p <= 65535) port = p;
            }
        }
    }

    while (strlen(username) == 0) {
        if (argc >= 4 && strlen(username) == 0) {
            strncpy_s(username, sizeof(username), argv[3], _TRUNCATE);
        } else {
            printf("Enter Username: ");
            if (fgets(username, sizeof(username), stdin)) {
                trim_newline(username);
            }
        }

        if (strlen(username) == 0) {
            printf("[-] Username cannot be empty. Please enter a valid username.\n");
            argc = 0; /* Reset CLI flag to prompt on next loop */
        }
    }

    /* Connect to Server with Handshake */
    if (!connect_to_server(server_ip, port, username)) {
        cleanup_winsock();
        DeleteCriticalSection(&g_console_lock);
        if (argc < 4) {
            printf("\nPress Enter to exit...");
            getchar();
        }
        return 1;
    }

    /* Main Console Input Loop */
    char input_line[MAX_MESSAGE_LEN];
    while (g_connected) {
        print_prompt();
        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            break;
        }
        handle_user_input(input_line);
    }

    disconnect_from_server();
    cleanup_winsock();
    DeleteCriticalSection(&g_console_lock);
    return 0;
}
