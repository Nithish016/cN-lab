/**
 * ============================================================================
 * MULTI-CLIENT CHAT APPLICATION - COMPUTER NETWORKS LAB PROJECT
 * File: client/client.h
 * Description: Client function prototypes, UI routines, and thread headers.
 * Platform: Windows 10/11 (Winsock 2)
 * ============================================================================
 */

#ifndef CLIENT_H
#define CLIENT_H

#include "../shared/common.h"

/* Client Function Prototypes */
bool init_winsock(void);
void cleanup_winsock(void);
bool connect_to_server(const char *ip, int port, const char *username);
void disconnect_from_server(void);
unsigned __stdcall receive_thread(void *arg);

/* Command Processing & Messaging */
void handle_user_input(char *line);
void send_broadcast(const char *message);
void send_private(const char *recipient, const char *message);
void request_online_users(void);
void send_file_to_user(const char *recipient, const char *filepath);

/* UI & Display */
void print_startup_banner(void);
void print_connected_banner(const char *ip, int port, const char *username);
void print_help_menu(void);
void print_prompt(void);

#endif /* CLIENT_H */
