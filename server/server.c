/**
 * ============================================================================
 * MULTI-CLIENT CHAT APPLICATION - COMPUTER NETWORKS LAB PROJECT
 * File: server/server.c
 * Description: Multi-Client TCP Chat Server Implementation using Windows Winsock 2.
 * Platform: Windows 10/11
 *
 * Key Winsock Functions Demonstrated:
 *   - WSAStartup()   : Initializes Winsock library runtime.
 *   - socket()       : Creates TCP endpoint for network communication.
 *   - bind()         : Associates socket with local IP address and port number.
 *   - listen()       : Marks socket as passive listening socket for client connections.
 *   - accept()       : Extracts first connection request on listening queue.
 *   - send()         : Sends formatted byte stream data across TCP socket.
 *   - recv()         : Receives byte stream data from TCP socket.
 *   - closesocket()  : Deallocates socket descriptor and closes TCP connection.
 *   - WSACleanup()   : Terminates Winsock DLL usage and frees resources.
 * ============================================================================
 */

#include "server.h"

/* ------------------------------------------------------------------------- */
/* GLOBAL SERVER STATE                                                       */
/* ------------------------------------------------------------------------- */
static ClientSession    g_clients[MAX_CLIENTS];
static int              g_client_count = 0;
static CRITICAL_SECTION g_clients_lock;
static CRITICAL_SECTION g_log_lock;
static SOCKET           g_listen_socket = INVALID_SOCKET;
static volatile bool    g_server_running = false;
static FILE            *g_log_file = NULL;

/* ------------------------------------------------------------------------- */
/* LOGGING IMPLEMENTATION                                                    */
/* ------------------------------------------------------------------------- */

/**
 * Thread-safe logging function that writes events simultaneously to:
 * 1. Standard console output (stdout)
 * 2. Activity log file (server.log)
 */
void log_event(const char *tag, const char *format, ...) {
    char timestamp[32];
    get_current_timestamp(timestamp, sizeof(timestamp));

    char message_buffer[BUFFER_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(message_buffer, sizeof(message_buffer), format, args);
    va_end(args);

    EnterCriticalSection(&g_log_lock);

    /* Console output */
    printf("%s %-7s %s\n", timestamp, tag, message_buffer);
    fflush(stdout);

    /* File output */
    if (g_log_file != NULL) {
        fprintf(g_log_file, "%s %-7s %s\n", timestamp, tag, message_buffer);
        fflush(g_log_file);
    }

    LeaveCriticalSection(&g_log_lock);
}

/* ------------------------------------------------------------------------- */
/* NETWORK ENUMERATION & BANNER                                              */
/* ------------------------------------------------------------------------- */

/**
 * Enumerates and displays local host IPv4 addresses for LAN and loopback.
 */
void display_local_ips(void) {
    char host_name[256];
    if (gethostname(host_name, sizeof(host_name)) == SOCKET_ERROR) {
        printf("  Local IP : 127.0.0.1 (Loopback)\n");
        return;
    }

    struct addrinfo hints, *res, *p;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET; /* IPv4 */
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host_name, NULL, &hints, &res) != 0) {
        printf("  Local IP : 127.0.0.1 (Loopback)\n");
        return;
    }

    printf("  Host Name: %s\n", host_name);
    printf("  Available IP Addresses:\n");
    for (p = res; p != NULL; p = p->ai_next) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ipv4->sin_addr), ip_str, sizeof(ip_str));
        printf("    -> %s%s\n", ip_str, (strcmp(ip_str, "127.0.0.1") == 0) ? " (Loopback)" : " (LAN / Wi-Fi)");
    }
    freeaddrinfo(res);
}

/**
 * Prints the professional Command Prompt banner for the server.
 */
void print_server_banner(int port) {
    printf("\n");
    printf("========================================================\n");
    printf("             MULTI-CLIENT CHAT SERVER                   \n");
    printf("             COMPUTER NETWORKS LAB                      \n");
    printf("========================================================\n");
    display_local_ips();
    printf("  Port     : %d\n", port);
    printf("  Status   : RUNNING (Press Ctrl+C to stop)\n");
    printf("========================================================\n\n");
    fflush(stdout);
}

/* ------------------------------------------------------------------------- */
/* WINSOCK INITIALIZATION & TEARDOWN                                         */
/* ------------------------------------------------------------------------- */

/**
 * Initializes the Windows Sockets DLL (Winsock 2.2).
 */
bool init_winsock(void) {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        fprintf(stderr, "[ERROR] WSAStartup failed with error code: %d\n", result);
        return false;
    }
    return true;
}

/**
 * Cleans up Winsock and flushes resources.
 */
void cleanup_winsock(void) {
    WSACleanup();
}

/* ------------------------------------------------------------------------- */
/* CLIENT REGISTRY MANAGEMENT (CRITICAL SECTION SYNCHRONIZED)               */
/* ------------------------------------------------------------------------- */

/**
 * Checks if a username is already active among connected clients (case-insensitive).
 * Must be called with g_clients_lock held or will acquire it.
 */
bool is_username_taken(const char *username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].is_active) {
            if (_stricmp(g_clients[i].username, username) == 0) {
                return true;
            }
        }
    }
    return false;
}

/**
 * Adds a verified client to the global client array.
 */
bool add_client(SOCKET sock, const char *username, const char *ip, int port, int *out_index) {
    EnterCriticalSection(&g_clients_lock);

    if (g_client_count >= MAX_CLIENTS || is_username_taken(username)) {
        LeaveCriticalSection(&g_clients_lock);
        return false;
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_clients[i].is_active) {
            g_clients[i].socket = sock;
            strncpy_s(g_clients[i].username, MAX_USERNAME_LEN, username, _TRUNCATE);
            strncpy_s(g_clients[i].ip_address, INET_ADDRSTRLEN, ip, _TRUNCATE);
            g_clients[i].port = port;
            g_clients[i].is_active = true;
            g_clients[i].connected_at = time(NULL);
            g_client_count++;
            *out_index = i;
            LeaveCriticalSection(&g_clients_lock);
            return true;
        }
    }

    LeaveCriticalSection(&g_clients_lock);
    return false;
}

/**
 * Removes a client from the active array and closes their socket.
 */
void remove_client(int client_index) {
    char username_copy[MAX_USERNAME_LEN] = {0};
    SOCKET sock_to_close = INVALID_SOCKET;

    EnterCriticalSection(&g_clients_lock);
    if (client_index >= 0 && client_index < MAX_CLIENTS && g_clients[client_index].is_active) {
        strncpy_s(username_copy, MAX_USERNAME_LEN, g_clients[client_index].username, _TRUNCATE);
        sock_to_close = g_clients[client_index].socket;
        g_clients[client_index].is_active = false;
        g_clients[client_index].socket = INVALID_SOCKET;
        g_clients[client_index].username[0] = '\0';
        g_client_count--;
    }
    LeaveCriticalSection(&g_clients_lock);

    if (sock_to_close != INVALID_SOCKET) {
        closesocket(sock_to_close);
    }

    if (strlen(username_copy) > 0) {
        log_event("[-]", "Client disconnected : %s", username_copy);
        char leave_msg[256];
        snprintf(leave_msg, sizeof(leave_msg), "[-] %s has left the chat.", username_copy);
        broadcast_notification(leave_msg);
    }
}

/**
 * Finds client index by username (case-insensitive). Returns -1 if not found.
 */
int find_client_by_username(const char *username) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].is_active && _stricmp(g_clients[i].username, username) == 0) {
            return i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------------- */
/* MESSAGING OPERATIONS                                                      */
/* ------------------------------------------------------------------------- */

/**
 * Broadcasts a chat message to all connected clients except the sender.
 */
void broadcast_chat(const char *sender, const char *message) {
    BroadcastPayload payload;
    strncpy_s(payload.sender, MAX_USERNAME_LEN, sender, _TRUNCATE);
    strncpy_s(payload.message, MAX_MESSAGE_LEN, message, _TRUNCATE);

    EnterCriticalSection(&g_clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].is_active && strcmp(g_clients[i].username, sender) != 0) {
            send_packet(g_clients[i].socket, PKT_BROADCAST, &payload, sizeof(payload));
        }
    }
    LeaveCriticalSection(&g_clients_lock);
}

/**
 * Broadcasts a server system notification to all connected clients.
 */
void broadcast_notification(const char *notification) {
    NotificationPayload payload;
    strncpy_s(payload.message, sizeof(payload.message), notification, _TRUNCATE);

    EnterCriticalSection(&g_clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].is_active) {
            send_packet(g_clients[i].socket, PKT_NOTIFICATION, &payload, sizeof(payload));
        }
    }
    LeaveCriticalSection(&g_clients_lock);
}

/**
 * Transmits a private unicast message from sender to recipient.
 */
bool send_private_chat(const char *sender, const char *recipient, const char *message) {
    PrivateMsgPayload payload;
    strncpy_s(payload.sender, MAX_USERNAME_LEN, sender, _TRUNCATE);
    strncpy_s(payload.recipient, MAX_USERNAME_LEN, recipient, _TRUNCATE);
    strncpy_s(payload.message, MAX_MESSAGE_LEN, message, _TRUNCATE);

    bool delivered = false;

    EnterCriticalSection(&g_clients_lock);
    int target_idx = find_client_by_username(recipient);
    if (target_idx != -1 && g_clients[target_idx].is_active) {
        delivered = send_packet(g_clients[target_idx].socket, PKT_PRIVATE_MSG, &payload, sizeof(payload));
    }
    LeaveCriticalSection(&g_clients_lock);

    return delivered;
}

/**
 * Builds formatted listing of all currently connected clients.
 */
void build_online_users_string(char *buffer, size_t max_size) {
    EnterCriticalSection(&g_clients_lock);

    snprintf(buffer, max_size, "\n--- Online Users (%d/%d) ---\n", g_client_count, MAX_CLIENTS);
    char line[128];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].is_active) {
            time_t now = time(NULL);
            long uptime_sec = (long)(now - g_clients[i].connected_at);
            snprintf(line, sizeof(line), "  * %-16s [%s:%d] (Online %ldm %lds)\n",
                     g_clients[i].username, g_clients[i].ip_address,
                     g_clients[i].port, uptime_sec / 60, uptime_sec % 60);
            strncat_s(buffer, max_size, line, _TRUNCATE);
        }
    }
    strncat_s(buffer, max_size, "---------------------------\n", _TRUNCATE);

    LeaveCriticalSection(&g_clients_lock);
}

/* ------------------------------------------------------------------------- */
/* CLIENT WORKER THREAD                                                      */
/* ------------------------------------------------------------------------- */

typedef struct {
    SOCKET socket;
    char   client_ip[INET_ADDRSTRLEN];
    int    client_port;
} ClientThreadParam;

/**
 * Dedicated thread function servicing a single connected client.
 * Handles handshake, message dispatching, file transfers, and graceful exit.
 */
unsigned __stdcall client_handler(void *arg) {
    ClientThreadParam *param = (ClientThreadParam *)arg;
    SOCKET client_sock = param->socket;
    char client_ip[INET_ADDRSTRLEN];
    int client_port = param->client_port;
    strncpy_s(client_ip, sizeof(client_ip), param->client_ip, _TRUNCATE);
    free(param); /* Dynamically allocated parameter */

    int client_index = -1;
    char username[MAX_USERNAME_LEN] = {0};

    /* --------------------------------------------------------------------- */
    /* STEP 1: CLIENT HANDSHAKE & AUTHENTICATION                             */
    /* --------------------------------------------------------------------- */
    PacketHeader header;
    char payload_buf[BUFFER_SIZE];

    int res = recv_packet(client_sock, &header, payload_buf, sizeof(payload_buf));
    if (res <= 0 || header.type != PKT_CONNECT_REQ) {
        closesocket(client_sock);
        return 0;
    }

    ConnectReqPayload *conn_req = (ConnectReqPayload *)payload_buf;
    trim_newline(conn_req->username);

    /* Validate Username format */
    if (strlen(conn_req->username) == 0 || strlen(conn_req->username) >= MAX_USERNAME_LEN) {
        ConnectAckPayload ack;
        ack.success = 0;
        snprintf(ack.message, sizeof(ack.message), "Invalid username. Must be 1-31 characters.");
        send_packet(client_sock, PKT_CONNECT_NACK, &ack, sizeof(ack));
        closesocket(client_sock);
        return 0;
    }

    EnterCriticalSection(&g_clients_lock);

    /* Check for duplicate username */
    if (is_username_taken(conn_req->username)) {
        LeaveCriticalSection(&g_clients_lock);
        ConnectAckPayload ack;
        ack.success = 0;
        snprintf(ack.message, sizeof(ack.message), "Username '%s' is already taken. Please choose another.", conn_req->username);
        send_packet(client_sock, PKT_CONNECT_NACK, &ack, sizeof(ack));
        closesocket(client_sock);
        return 0;
    }

    /* Check server capacity */
    if (g_client_count >= MAX_CLIENTS) {
        LeaveCriticalSection(&g_clients_lock);
        ConnectAckPayload ack;
        ack.success = 0;
        snprintf(ack.message, sizeof(ack.message), "Server is full (Capacity: %d clients).", MAX_CLIENTS);
        send_packet(client_sock, PKT_CONNECT_NACK, &ack, sizeof(ack));
        closesocket(client_sock);
        return 0;
    }

    /* Register client session */
    strncpy_s(username, MAX_USERNAME_LEN, conn_req->username, _TRUNCATE);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!g_clients[i].is_active) {
            g_clients[i].socket = client_sock;
            strncpy_s(g_clients[i].username, MAX_USERNAME_LEN, username, _TRUNCATE);
            strncpy_s(g_clients[i].ip_address, INET_ADDRSTRLEN, client_ip, _TRUNCATE);
            g_clients[i].port = client_port;
            g_clients[i].is_active = true;
            g_clients[i].connected_at = time(NULL);
            g_clients[i].thread_handle = GetCurrentThread();
            g_clients[i].thread_id = GetCurrentThreadId();
            g_client_count++;
            client_index = i;
            break;
        }
    }

    LeaveCriticalSection(&g_clients_lock);

    /* Send successful acknowledgment */
    ConnectAckPayload ack;
    ack.success = 1;
    snprintf(ack.message, sizeof(ack.message), "Welcome to the chat server, %s!", username);
    send_packet(client_sock, PKT_CONNECT_ACK, &ack, sizeof(ack));

    log_event("[+]", "Client connected : %s [%s:%d]", username, client_ip, client_port);

    /* Notify other clients */
    char join_notification[256];
    snprintf(join_notification, sizeof(join_notification), "[+] %s has joined the chat!", username);
    broadcast_notification(join_notification);

    /* --------------------------------------------------------------------- */
    /* STEP 2: PACKET PROCESSING LOOP                                        */
    /* --------------------------------------------------------------------- */
    while (g_server_running) {
        res = recv_packet(client_sock, &header, payload_buf, sizeof(payload_buf));
        if (res <= 0) {
            /* Peer closed connection or socket read error */
            break;
        }

        switch (header.type) {
            case PKT_BROADCAST: {
                BroadcastPayload *bcast = (BroadcastPayload *)payload_buf;
                trim_newline(bcast->message);
                if (strlen(bcast->message) > 0) {
                    log_event("[MSG]", "%s -> ALL : %s", username, bcast->message);
                    broadcast_chat(username, bcast->message);
                }
                break;
            }

            case PKT_PRIVATE_MSG: {
                PrivateMsgPayload *pmsg = (PrivateMsgPayload *)payload_buf;
                trim_newline(pmsg->recipient);
                trim_newline(pmsg->message);

                if (strlen(pmsg->recipient) == 0 || strlen(pmsg->message) == 0) {
                    NotificationPayload notif;
                    snprintf(notif.message, sizeof(notif.message), "Usage: /msg <username> <message>");
                    send_packet(client_sock, PKT_NOTIFICATION, &notif, sizeof(notif));
                    break;
                }

                if (_stricmp(pmsg->recipient, username) == 0) {
                    NotificationPayload notif;
                    snprintf(notif.message, sizeof(notif.message), "[-] You cannot send a private message to yourself.");
                    send_packet(client_sock, PKT_NOTIFICATION, &notif, sizeof(notif));
                    break;
                }

                bool sent = send_private_chat(username, pmsg->recipient, pmsg->message);
                if (sent) {
                    log_event("[MSG]", "%s -> %s : %s", username, pmsg->recipient, pmsg->message);
                    /* Echo confirmation back to sender */
                    PrivateMsgPayload echo;
                    strncpy_s(echo.sender, MAX_USERNAME_LEN, username, _TRUNCATE);
                    strncpy_s(echo.recipient, MAX_USERNAME_LEN, pmsg->recipient, _TRUNCATE);
                    strncpy_s(echo.message, MAX_MESSAGE_LEN, pmsg->message, _TRUNCATE);
                    send_packet(client_sock, PKT_PRIVATE_MSG, &echo, sizeof(echo));
                } else {
                    NotificationPayload notif;
                    snprintf(notif.message, sizeof(notif.message), "[-] User '%s' is not online.", pmsg->recipient);
                    send_packet(client_sock, PKT_NOTIFICATION, &notif, sizeof(notif));
                }
                break;
            }

            case PKT_USERS_REQ: {
                UsersRespPayload resp;
                build_online_users_string(resp.user_list, sizeof(resp.user_list));
                EnterCriticalSection(&g_clients_lock);
                resp.count = g_client_count;
                LeaveCriticalSection(&g_clients_lock);

                send_packet(client_sock, PKT_USERS_RESP, &resp, sizeof(resp));
                log_event("[REQ]", "%s requested online users list", username);
                break;
            }

            case PKT_FILE_START: {
                FileStartPayload *fstart = (FileStartPayload *)payload_buf;
                EnterCriticalSection(&g_clients_lock);
                int dest_idx = find_client_by_username(fstart->recipient);
                SOCKET dest_sock = (dest_idx != -1) ? g_clients[dest_idx].socket : INVALID_SOCKET;
                LeaveCriticalSection(&g_clients_lock);

                if (dest_sock != INVALID_SOCKET) {
                    log_event("[FILE]", "Transfer initiating: '%s' (%llu bytes) from %s to %s",
                              fstart->filename, (unsigned long long)fstart->filesize, username, fstart->recipient);
                    /* Forward to recipient */
                    send_packet(dest_sock, PKT_FILE_START, fstart, sizeof(FileStartPayload));
                } else {
                    /* Notify sender of recipient unavailability */
                    FileEndPayload ferr;
                    strncpy_s(ferr.sender, MAX_USERNAME_LEN, username, _TRUNCATE);
                    strncpy_s(ferr.recipient, MAX_USERNAME_LEN, fstart->recipient, _TRUNCATE);
                    strncpy_s(ferr.filename, MAX_FILENAME_LEN, fstart->filename, _TRUNCATE);
                    ferr.success = 0;
                    snprintf(ferr.message, sizeof(ferr.message), "Recipient '%s' is not online.", fstart->recipient);
                    send_packet(client_sock, PKT_FILE_ERROR, &ferr, sizeof(FileEndPayload));
                }
                break;
            }

            case PKT_FILE_DATA: {
                FileDataPayload *fdata = (FileDataPayload *)payload_buf;
                EnterCriticalSection(&g_clients_lock);
                int dest_idx = find_client_by_username(fdata->recipient);
                SOCKET dest_sock = (dest_idx != -1) ? g_clients[dest_idx].socket : INVALID_SOCKET;
                LeaveCriticalSection(&g_clients_lock);

                if (dest_sock != INVALID_SOCKET) {
                    send_packet(dest_sock, PKT_FILE_DATA, fdata, sizeof(FileDataPayload));
                }
                break;
            }

            case PKT_FILE_END: {
                FileEndPayload *fend = (FileEndPayload *)payload_buf;
                EnterCriticalSection(&g_clients_lock);
                int dest_idx = find_client_by_username(fend->recipient);
                SOCKET dest_sock = (dest_idx != -1) ? g_clients[dest_idx].socket : INVALID_SOCKET;
                LeaveCriticalSection(&g_clients_lock);

                if (dest_sock != INVALID_SOCKET) {
                    send_packet(dest_sock, PKT_FILE_END, fend, sizeof(FileEndPayload));
                    log_event("[FILE]", "Transfer complete: '%s' from %s to %s",
                              fend->filename, username, fend->recipient);
                }
                break;
            }

            case PKT_DISCONNECT: {
                log_event("[-]", "%s initiated graceful disconnect", username);
                goto cleanup;
            }

            default:
                log_event("[WARN]", "Received unknown packet type %d from %s", header.type, username);
                break;
        }
    }

cleanup:
    /* Clean up client registration and state */
    remove_client(client_index);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* SERVER MAIN ENGINE                                                        */
/* ------------------------------------------------------------------------- */

/**
 * Initializes listening TCP socket and enters connection accept loop.
 */
bool start_server(int port) {
    /* 1. Initialize Critical Sections */
    InitializeCriticalSection(&g_clients_lock);
    InitializeCriticalSection(&g_log_lock);

    /* 2. Open Server Activity Log File */
    fopen_s(&g_log_file, "server.log", "a");
    if (!g_log_file) {
        /* If running from another directory, create inside server directory if needed */
        fopen_s(&g_log_file, "server/server.log", "a");
    }

    log_event("[SYS]", "Starting Multi-Client TCP Chat Server on Port %d...", port);

    /* 3. Create TCP Stream Socket */
    g_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen_socket == INVALID_SOCKET) {
        log_event("[ERR]", "socket() creation failed with error: %d", WSAGetLastError());
        return false;
    }

    /* 4. Configure Socket Options (SO_REUSEADDR) */
    BOOL optval = TRUE;
    if (setsockopt(g_listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof(optval)) == SOCKET_ERROR) {
        log_event("[WARN]", "setsockopt(SO_REUSEADDR) failed with error: %d", WSAGetLastError());
    }

    /* 5. Bind Socket to Local Address and Port */
    struct sockaddr_in server_addr;
    ZeroMemory(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; /* Accept connections on all network interfaces */
    server_addr.sin_port = htons((u_short)port);

    if (bind(g_listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        int err = WSAGetLastError();
        log_event("[ERR]", "bind() failed on port %d with error: %d.", port, err);
        /* If port was 8080 and blocked (e.g., by Tomcat/HTTP service), try alternate port 8888 */
        if (port == 8080 && (err == 10013 || err == 10048)) {
            port = 8888;
            server_addr.sin_port = htons((u_short)port);
            log_event("[SYS]", "Port 8080 in use by another Windows service. Falling back to port %d...", port);
            if (bind(g_listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
                log_event("[ERR]", "Fallback bind() failed on port %d with error: %d.", port, WSAGetLastError());
                closesocket(g_listen_socket);
                return false;
            }
        } else {
            closesocket(g_listen_socket);
            return false;
        }
    }

    /* 6. Mark Socket as Listening */
    if (listen(g_listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        log_event("[ERR]", "listen() failed with error: %d", WSAGetLastError());
        closesocket(g_listen_socket);
        return false;
    }

    g_server_running = true;
    print_server_banner(port);
    log_event("[SYS]", "Server listening successfully. Ready for client connections.");

    /* 7. Accept Connection Loop */
    while (g_server_running) {
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);

        SOCKET client_sock = accept(g_listen_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_sock == INVALID_SOCKET) {
            if (!g_server_running) break; /* Server stopped intentionally */
            log_event("[ERR]", "accept() failed with error: %d", WSAGetLastError());
            continue;
        }

        /* Extract Client IP and Port */
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        /* Allocate thread parameter */
        ClientThreadParam *param = (ClientThreadParam *)malloc(sizeof(ClientThreadParam));
        if (!param) {
            log_event("[ERR]", "Out of memory allocating thread param for client %s:%d", client_ip, client_port);
            closesocket(client_sock);
            continue;
        }
        param->socket = client_sock;
        strncpy_s(param->client_ip, sizeof(param->client_ip), client_ip, _TRUNCATE);
        param->client_port = client_port;

        /* Spawn dedicated Windows thread using _beginthreadex */
        uintptr_t thread_handle = _beginthreadex(NULL, 0, client_handler, (void *)param, 0, NULL);
        if (thread_handle == 0) {
            log_event("[ERR]", "_beginthreadex failed to create client handler thread for %s:%d", client_ip, client_port);
            free(param);
            closesocket(client_sock);
        } else {
            /* Close the thread handle immediately since thread will terminate independently */
            CloseHandle((HANDLE)thread_handle);
        }
    }

    return true;
}

/**
 * Cleanly shuts down the server, closes all client sockets, and releases locks.
 */
void stop_server(void) {
    g_server_running = false;

    if (g_listen_socket != INVALID_SOCKET) {
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
    }

    EnterCriticalSection(&g_clients_lock);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_clients[i].is_active) {
            closesocket(g_clients[i].socket);
            g_clients[i].is_active = false;
        }
    }
    LeaveCriticalSection(&g_clients_lock);

    log_event("[SYS]", "Server shut down cleanly.");

    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    DeleteCriticalSection(&g_clients_lock);
    DeleteCriticalSection(&g_log_lock);
}

/* ------------------------------------------------------------------------- */
/* CONSOLE CONTROL HANDLER (Ctrl+C Graceful Shutdown)                        */
/* ------------------------------------------------------------------------- */
static BOOL WINAPI console_ctrl_handler(DWORD ctrl_type) {
    switch (ctrl_type) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            printf("\n[SYS] Shutdown signal received. Closing server sockets...\n");
            stop_server();
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
    int port = DEFAULT_PORT;

    /* Optional command-line port override */
    if (argc >= 2) {
        int input_port = atoi(argv[1]);
        if (input_port > 0 && input_port <= 65535) {
            port = input_port;
        } else {
            fprintf(stderr, "[WARN] Invalid port '%s' provided. Falling back to default %d.\n", argv[1], DEFAULT_PORT);
        }
    }

    /* Register Console Ctrl Handler for clean Ctrl+C shutdown */
    SetConsoleCtrlHandler(console_ctrl_handler, TRUE);

    /* 1. Initialize Winsock */
    if (!init_winsock()) {
        fprintf(stderr, "[FATAL] Failed to initialize Winsock 2.2.\n");
        return 1;
    }

    /* 2. Start TCP Chat Server */
    if (!start_server(port)) {
        fprintf(stderr, "[FATAL] Failed to start server on port %d.\n", port);
        cleanup_winsock();
        return 1;
    }

    /* 3. Cleanup on Exit */
    stop_server();
    cleanup_winsock();
    return 0;
}
