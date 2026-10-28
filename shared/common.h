/**
 * ============================================================================
 * MULTI-CLIENT CHAT APPLICATION - COMPUTER NETWORKS LAB PROJECT
 * File: shared/common.h
 * Description: Common definitions, protocol packet formats, and helper
 *              functions for Windows Winsock 2 TCP communication.
 * Platform: Windows 10/11
 * ============================================================================
 */

#ifndef COMMON_H
#define COMMON_H

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS

/* Windows Sockets 2 and System Headers */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <direct.h>
#include <sys/stat.h>

/* Automatically link against Winsock 2 library on MSVC */
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

/* ------------------------------------------------------------------------- */
/* CONSTANTS & CONFIGURATION                                                 */
/* ------------------------------------------------------------------------- */
#define DEFAULT_PORT            8888
#define DEFAULT_IP              "127.0.0.1"
#define MAX_CLIENTS             30
#define BUFFER_SIZE             8192
#define MAX_USERNAME_LEN        32
#define MAX_FILENAME_LEN        256
#define MAX_MESSAGE_LEN         2048
#define FILE_CHUNK_SIZE         4096
#define PACKET_MAGIC            0x434E4C41  /* 'CNLA' - Computer Networks Lab App */

/* ------------------------------------------------------------------------- */
/* PACKET PROTOCOL ENUMERATION                                               */
/* ------------------------------------------------------------------------- */
typedef enum {
    PKT_CONNECT_REQ = 1,    /* Client -> Server: Register username request      */
    PKT_CONNECT_ACK,        /* Server -> Client: Registration success           */
    PKT_CONNECT_NACK,       /* Server -> Client: Registration rejected/duplicate*/
    PKT_BROADCAST,          /* Client <-> Server: Broadcast message to all      */
    PKT_PRIVATE_MSG,        /* Client <-> Server: Targeted unicast message      */
    PKT_USERS_REQ,          /* Client -> Server: Request list of online users   */
    PKT_USERS_RESP,         /* Server -> Client: Online users list              */
    PKT_FILE_START,         /* Sender -> Server -> Receiver: File info header   */
    PKT_FILE_DATA,          /* Sender -> Server -> Receiver: File binary chunk  */
    PKT_FILE_END,           /* Sender -> Server -> Receiver: File EOF notice    */
    PKT_FILE_ERROR,         /* Server -> Client: Recipient offline/error        */
    PKT_NOTIFICATION,       /* Server -> Client: Join/Leave/System announcement */
    PKT_DISCONNECT          /* Client -> Server: Graceful exit notification     */
} PacketType;

/* ------------------------------------------------------------------------- */
/* PACKET STRUCTURE DEFINITIONS (Byte-packed for cross-compiler safety)      */
/* ------------------------------------------------------------------------- */
#pragma pack(push, 1)

/**
 * Fixed-size packet header prepended to every transmitted packet.
 * Guarantees message framing and boundary preservation over TCP byte-streams.
 */
typedef struct {
    uint32_t magic;         /* Protocol verification magic number               */
    uint8_t  type;          /* PacketType value                                 */
    uint32_t payload_len;   /* Length of byte payload immediately following     */
} PacketHeader;

/**
 * Payload: Connection Request (PKT_CONNECT_REQ)
 */
typedef struct {
    char username[MAX_USERNAME_LEN];
} ConnectReqPayload;

/**
 * Payload: Connection Response (PKT_CONNECT_ACK / PKT_CONNECT_NACK)
 */
typedef struct {
    int  success;
    char message[256];
} ConnectAckPayload;

/**
 * Payload: Broadcast Message (PKT_BROADCAST)
 */
typedef struct {
    char sender[MAX_USERNAME_LEN];
    char message[MAX_MESSAGE_LEN];
} BroadcastPayload;

/**
 * Payload: Private Message (PKT_PRIVATE_MSG)
 */
typedef struct {
    char sender[MAX_USERNAME_LEN];
    char recipient[MAX_USERNAME_LEN];
    char message[MAX_MESSAGE_LEN];
} PrivateMsgPayload;

/**
 * Payload: Online Users Response (PKT_USERS_RESP)
 */
typedef struct {
    int  count;
    char user_list[BUFFER_SIZE];
} UsersRespPayload;

/**
 * Payload: File Transfer Header (PKT_FILE_START)
 */
typedef struct {
    char     sender[MAX_USERNAME_LEN];
    char     recipient[MAX_USERNAME_LEN];
    char     filename[MAX_FILENAME_LEN];
    uint64_t filesize;
    uint32_t total_chunks;
} FileStartPayload;

/**
 * Payload: File Transfer Data Chunk (PKT_FILE_DATA)
 */
typedef struct {
    char     sender[MAX_USERNAME_LEN];
    char     recipient[MAX_USERNAME_LEN];
    char     filename[MAX_FILENAME_LEN];
    uint32_t chunk_index;
    uint32_t chunk_size;
    char     data[FILE_CHUNK_SIZE];
} FileDataPayload;

/**
 * Payload: File Transfer Completion (PKT_FILE_END)
 */
typedef struct {
    char sender[MAX_USERNAME_LEN];
    char recipient[MAX_USERNAME_LEN];
    char filename[MAX_FILENAME_LEN];
    int  success;
    char message[256];
} FileEndPayload;

/**
 * Payload: System Notification (PKT_NOTIFICATION)
 */
typedef struct {
    char message[512];
} NotificationPayload;

#pragma pack(pop)

/* ------------------------------------------------------------------------- */
/* WINSOCK TCP STREAM FRAMING HELPERS                                        */
/* ------------------------------------------------------------------------- */

/**
 * Sends all requested bytes across a TCP socket stream, handling partial sends.
 * Returns: Number of bytes sent on success, or SOCKET_ERROR (-1) on failure.
 */
static inline int send_all(SOCKET sock, const void *buffer, int length) {
    const char *ptr = (const char *)buffer;
    int total_sent = 0;
    while (total_sent < length) {
        int bytes_sent = send(sock, ptr + total_sent, length - total_sent, 0);
        if (bytes_sent == SOCKET_ERROR || bytes_sent <= 0) {
            return SOCKET_ERROR;
        }
        total_sent += bytes_sent;
    }
    return total_sent;
}

/**
 * Receives exact requested number of bytes from a TCP socket stream.
 * Handles partial receives and stream segmentation.
 * Returns: Number of bytes received (equal to length), 0 on orderly close,
 *          or SOCKET_ERROR (-1) on error.
 */
static inline int recv_all(SOCKET sock, void *buffer, int length) {
    char *ptr = (char *)buffer;
    int total_received = 0;
    while (total_received < length) {
        int bytes_received = recv(sock, ptr + total_received, length - total_received, 0);
        if (bytes_received == 0) {
            /* Peer closed connection */
            return 0;
        }
        if (bytes_received == SOCKET_ERROR) {
            return SOCKET_ERROR;
        }
        total_received += bytes_received;
    }
    return total_received;
}

/**
 * Transmits a typed, framed protocol packet over TCP.
 * Returns: true on success, false on network failure.
 */
static inline bool send_packet(SOCKET sock, PacketType type, const void *payload, uint32_t payload_len) {
    PacketHeader header;
    header.magic = htonl(PACKET_MAGIC);
    header.type = (uint8_t)type;
    header.payload_len = htonl(payload_len);

    /* 1. Send Fixed-size Header */
    if (send_all(sock, &header, sizeof(PacketHeader)) == SOCKET_ERROR) {
        return false;
    }

    /* 2. Send Payload if present */
    if (payload_len > 0 && payload != NULL) {
        if (send_all(sock, payload, (int)payload_len) == SOCKET_ERROR) {
            return false;
        }
    }
    return true;
}

/**
 * Receives a framed packet header and payload from TCP socket stream.
 * Returns: 1 on complete packet read, 0 on client disconnect, -1 on protocol/network error.
 */
static inline int recv_packet(SOCKET sock, PacketHeader *header, void *payload_buf, uint32_t max_buf_len) {
    if (!header) return -1;

    /* 1. Receive Header */
    int res = recv_all(sock, header, sizeof(PacketHeader));
    if (res <= 0) {
        return res; /* 0 = disconnected, -1 = error */
    }

    /* Convert network byte order to host byte order */
    header->magic = ntohl(header->magic);
    header->payload_len = ntohl(header->payload_len);

    /* Verify Magic Number */
    if (header->magic != PACKET_MAGIC) {
        return -1; /* Malformed protocol stream */
    }

    /* Verify buffer size capacity */
    if (header->payload_len > max_buf_len) {
        return -1; /* Payload exceeds destination buffer */
    }

    /* 2. Receive Payload */
    if (header->payload_len > 0) {
        res = recv_all(sock, payload_buf, (int)header->payload_len);
        if (res <= 0) {
            return res;
        }
    }
    return 1;
}

/* ------------------------------------------------------------------------- */
/* UTILITY FUNCTIONS                                                         */
/* ------------------------------------------------------------------------- */

/**
 * Obtains current formatted timestamp string: [YYYY-MM-DD HH:MM:SS]
 */
static inline void get_current_timestamp(char *buffer, size_t size) {
    time_t raw_time;
    struct tm time_info;
    time(&raw_time);
    localtime_s(&time_info, &raw_time);
    strftime(buffer, size, "[%Y-%m-%d %H:%M:%S]", &time_info);
}

/**
 * Strips trailing newline and carriage return characters from a string.
 */
static inline void trim_newline(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\r' || str[len - 1] == '\n' || str[len - 1] == ' ')) {
        str[len - 1] = '\0';
        len--;
    }
}

/**
 * Safely extracts the base filename from a path (handles both '/' and '\').
 */
static inline const char *get_base_filename(const char *path) {
    const char *last_slash = strrchr(path, '/');
    const char *last_backslash = strrchr(path, '\\');
    const char *p = (last_slash > last_backslash) ? last_slash : last_backslash;
    return (p != NULL) ? (p + 1) : path;
}

/**
 * Ensures a directory exists on Windows.
 */
static inline void ensure_directory_exists(const char *dir_path) {
    struct _stat st;
    if (_stat(dir_path, &st) != 0) {
        _mkdir(dir_path);
    }
}

#endif /* COMMON_H */
