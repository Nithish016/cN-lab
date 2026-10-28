/**
 * ============================================================================
 * MULTI-CLIENT CHAT APPLICATION - COMPUTER NETWORKS LAB PROJECT
 * File: server/server.h
 * Description: Header definitions and function prototypes for TCP Chat Server.
 * Platform: Windows 10/11 (Winsock 2)
 * ============================================================================
 */

#ifndef SERVER_H
#define SERVER_H

#include "../shared/common.h"

/* Structure to maintain state for each connected client session */
typedef struct {
    SOCKET       socket;
    char         username[MAX_USERNAME_LEN];
    char         ip_address[INET_ADDRSTRLEN];
    int          port;
    bool         is_active;
    HANDLE       thread_handle;
    unsigned int thread_id;
    time_t       connected_at;
} ClientSession;

/* Function Declarations */
bool init_winsock(void);
void cleanup_winsock(void);
bool start_server(int port);
void stop_server(void);
unsigned __stdcall client_handler(void *arg);

/* Client management routines (Thread-safe via Critical Section) */
bool add_client(SOCKET sock, const char *username, const char *ip, int port, int *out_index);
void remove_client(int client_index);
bool is_username_taken(const char *username);
int  find_client_by_username(const char *username);

/* Messaging operations */
void broadcast_chat(const char *sender, const char *message);
void broadcast_notification(const char *notification);
bool send_private_chat(const char *sender, const char *recipient, const char *message);
void build_online_users_string(char *buffer, size_t max_size);

/* Logging & UI */
void log_event(const char *tag, const char *format, ...);
void print_server_banner(int port);
void display_local_ips(void);

#endif /* SERVER_H */
