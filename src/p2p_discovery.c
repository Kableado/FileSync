// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez (existing license)
// Copyright (c) 2023 The Gemini Agent authors (for new P2P code)

#include "p2p_discovery.h"
#include "util.h" // For Print() and MaxFilename (though MAX_NODE_ID_LEN is local)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For close(), gethostname()
#include <time.h>   // For time()
#include <errno.h>  // For errno

#if defined(_WIN32) || defined(_WIN64)
// Windows specific includes and setup
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
// Helper to initialize Winsock
static int ensure_winsock_initialized() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        Print("WSAStartup failed: %d\n", result);
        return -1;
    }
    return 0;
}
#else
// POSIX specific includes
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // For inet_addr, htons
#include <sys/utsname.h> // For gethostname alternative if needed, though unistd.h usually has it
#endif


int send_udp_announcement(uint16_t udp_port, uint16_t tcp_port, const char* node_id_param) {
    int sockfd;
    struct sockaddr_in broadcast_addr;
    char message[MAX_ANNOUNCEMENT_MSG_SIZE];
    char current_node_id[MAX_NODE_ID_LEN];

    // Determine Node ID
    if (node_id_param != NULL && strlen(node_id_param) > 0) {
        strncpy(current_node_id, node_id_param, MAX_NODE_ID_LEN - 1);
        current_node_id[MAX_NODE_ID_LEN - 1] = '\0';
    } else {
        if (gethostname(current_node_id, MAX_NODE_ID_LEN - 1) != 0) {
            Print("P2P Discovery: Failed to get hostname. Using 'unknown_node'.\n");
            strncpy(current_node_id, "unknown_node", MAX_NODE_ID_LEN - 1);
        }
        current_node_id[MAX_NODE_ID_LEN - 1] = '\0';
    }

    // Construct the announcement message (simple key-value like for now, not JSON yet)
    // Example: "node_id=myhost;tcp_port=4856"
    // For robustness, especially with varied node_ids, JSON would be better later.
    snprintf(message, MAX_ANNOUNCEMENT_MSG_SIZE, "p2p_node_id=%s;p2p_tcp_port=%u", current_node_id, tcp_port);

#if defined(_WIN32) || defined(_WIN64)
    if (ensure_winsock_initialized() != 0) return -1;
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == INVALID_SOCKET) {
        Print("P2P Discovery: Failed to create UDP socket. Error: %d\n", WSAGetLastError());
        return -1;
    }
#else
    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        Print("P2P Discovery: Failed to create UDP socket.\n");
        perror("socket");
        return -1;
    }
#endif

    // Enable broadcasting
    int broadcast_enable = 1;
#if defined(_WIN32) || defined(_WIN64)
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (char*)&broadcast_enable, sizeof(broadcast_enable)) < 0) {
        Print("P2P Discovery: Failed to set SO_BROADCAST. Error: %d\n", WSAGetLastError());
        closesocket(sockfd);
        WSACleanup();
        return -1;
    }
#else
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) < 0) {
        Print("P2P Discovery: Failed to set SO_BROADCAST.\n");
        perror("setsockopt SO_BROADCAST");
        close(sockfd);
        return -1;
    }
#endif

    // Configure broadcast address
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(udp_port);
    // Using INADDR_BROADCAST (255.255.255.255) is generally preferred for simplicity
    // unless specific network interface broadcast is required.
    broadcast_addr.sin_addr.s_addr = inet_addr("255.255.255.255");
    // On some systems, you might need to iterate interfaces and send to each interface's broadcast address.
    // For now, 255.255.255.255 should work on most LAN setups.

    // Send the message
    if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&broadcast_addr, sizeof(broadcast_addr)) < 0) {
        Print("P2P Discovery: Failed to send broadcast message.\n");
#if defined(_WIN32) || defined(_WIN64)
        Print("P2P Discovery: sendto error: %d\n", WSAGetLastError());
        closesocket(sockfd);
        WSACleanup();
#else
        perror("sendto");
        close(sockfd);
#endif
        return -1;
    }

    Print("P2P Discovery: Sent announcement: '%s' to 255.255.255.255:%u\n", message, udp_port);

#if defined(_WIN32) || defined(_WIN64)
    closesocket(sockfd);
    // WSACleanup(); // Cleanup might be done globally at program exit
#else
    close(sockfd);
#endif

    return 0;
}


// --- PeerList Management Functions ---

void PeerList_Init(PeerList* list) {
    if (!list) return;
    list->peers = NULL;
    list->count = 0;
    list->capacity = 0;
}

void PeerList_AddOrUpdate(PeerList* list, const PeerInfo* new_peer, pthread_mutex_t* mutex) {
    if (!list || !new_peer) return;

    pthread_mutex_lock(mutex);

    // Try to find if peer already exists (by IP and Node ID, port can change but less likely for main TCP)
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->peers[i].ip_address, new_peer->ip_address) == 0 &&
            strcmp(list->peers[i].node_id, new_peer->node_id) == 0) {
            // Update existing peer
            list->peers[i].tcp_port = new_peer->tcp_port; // Update port just in case
            list->peers[i].last_seen = new_peer->last_seen;
            // Potentially update node_id if it can change and old one was placeholder
            // strncpy(list->peers[i].node_id, new_peer->node_id, MAX_NODE_ID_LEN -1);
            // list->peers[i].node_id[MAX_NODE_ID_LEN -1] = '\0';
            pthread_mutex_unlock(mutex);
            Print("P2P Discovery: Updated peer %s (%s:%u)\n", new_peer->node_id, new_peer->ip_address, new_peer->tcp_port);
            return;
        }
    }

    // Add new peer if not found
    if (list->count >= list->capacity) {
        int new_capacity = (list->capacity == 0) ? 10 : list->capacity * 2;
        PeerInfo* new_peers_array = (PeerInfo*)realloc(list->peers, new_capacity * sizeof(PeerInfo));
        if (!new_peers_array) {
            Print("P2P Discovery: Failed to realloc PeerList.\n");
            pthread_mutex_unlock(mutex);
            return; // Out of memory
        }
        list->peers = new_peers_array;
        list->capacity = new_capacity;
    }

    list->peers[list->count] = *new_peer; // Struct copy
    list->count++;
    Print("P2P Discovery: Added new peer %s (%s:%u)\n", new_peer->node_id, new_peer->ip_address, new_peer->tcp_port);

    pthread_mutex_unlock(mutex);
}

void PeerList_Cleanup(PeerList* list) {
    if (!list) return;
    if (list->peers) {
        free(list->peers);
        list->peers = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}


// --- UDP Listener Implementation ---

// Helper to parse the simple announcement message: "p2p_node_id=...;p2p_tcp_port=..."
static int parse_announcement_message(const char* msg_buffer, char* out_node_id, int max_node_id_len, uint16_t* out_tcp_port) {
    if (!msg_buffer || !out_node_id || !out_tcp_port) return -1;

    const char* id_key = "p2p_node_id=";
    const char* port_key = "p2p_tcp_port=";

    const char* id_start = strstr(msg_buffer, id_key);
    const char* port_start = strstr(msg_buffer, port_key);

    if (!id_start || !port_start) {
        // Print("P2P Discovery: Malformed message - missing keys: '%s'\n", msg_buffer);
        return -1; // Keys not found
    }

    id_start += strlen(id_key);
    port_start += strlen(port_key);

    const char* id_end = strchr(id_start, ';');
    if (!id_end) { // Node ID might be the last part if port came first or malformed
         id_end = msg_buffer + strlen(msg_buffer); // Assume it goes to end of string
    }
    int id_len = id_end - id_start;
    if (id_len <= 0 || id_len >= max_node_id_len) {
        // Print("P2P Discovery: Malformed message - invalid node_id length: %d for '%s'\n", id_len, msg_buffer);
        return -1; // Invalid length
    }
    strncpy(out_node_id, id_start, id_len);
    out_node_id[id_len] = '\0';

    // TCP port parsing
    char port_str[10]; // Max 5 digits for uint16_t + null
    const char* port_end = strchr(port_start, ';');
    if (!port_end) { // Port might be the last part
        port_end = msg_buffer + strlen(msg_buffer);
    }
    int port_len = port_end - port_start;
    if (port_len <=0 || port_len >= sizeof(port_str)) {
        // Print("P2P Discovery: Malformed message - invalid port length: %d for '%s'\n", port_len, msg_buffer);
        return -1;
    }
    strncpy(port_str, port_start, port_len);
    port_str[port_len] = '\0';

    long port_val = strtol(port_str, NULL, 10);
    if (port_val <= 0 || port_val > 65535) {
        // Print("P2P Discovery: Malformed message - invalid port value: %ld for '%s'\n", port_val, msg_buffer);
        return -1; // Invalid port number
    }
    *out_tcp_port = (uint16_t)port_val;

    return 0; // Success
}


static void* udp_listener_thread_func(void* thread_args) {
    UdpListenerArgs* args = (UdpListenerArgs*)thread_args;
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[MAX_ANNOUNCEMENT_MSG_SIZE];
    socklen_t client_addr_len = sizeof(client_addr);

#if defined(_WIN32) || defined(_WIN64)
    if (ensure_winsock_initialized() != 0) {
        Print("P2P Discovery Listener: Winsock init failed in thread.\n");
        return NULL;
    }
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd == INVALID_SOCKET) {
        Print("P2P Discovery Listener: Failed to create UDP socket. Error: %d\n", WSAGetLastError());
        return NULL;
    }
#else
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        Print("P2P Discovery Listener: Failed to create UDP socket.\n");
        perror("socket listener");
        return NULL;
    }
#endif

    // Enable SO_REUSEADDR to allow multiple instances on the same machine (for testing)
    // or rapid restarts.
    int reuse_addr = 1;
#if defined(_WIN32) || defined(_WIN64)
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse_addr, sizeof(reuse_addr)) < 0) {
        Print("P2P Discovery Listener: Failed to set SO_REUSEADDR. Error: %d\n", WSAGetLastError());
    }
#else
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0) {
        Print("P2P Discovery Listener: Failed to set SO_REUSEADDR.\n");
        perror("setsockopt SO_REUSEADDR");
        // Non-fatal, continue
    }
#endif

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all interfaces
    server_addr.sin_port = htons(args->udp_port);

#if defined(_WIN32) || defined(_WIN64)
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        Print("P2P Discovery Listener: Failed to bind socket to port %u. Error: %d\n", args->udp_port, WSAGetLastError());
        closesocket(sockfd);
        return NULL;
    }
#else
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        Print("P2P Discovery Listener: Failed to bind socket to port %u.\n", args->udp_port);
        perror("bind listener");
        close(sockfd);
        return NULL;
    }
#endif

    Print("P2P Discovery Listener: Listening for announcements on UDP port %u\n", args->udp_port);

    // Set a timeout for recvfrom so the loop can check stop_flag
#if defined(_WIN32) || defined(_WIN64)
    DWORD timeout = 1000; // 1 second in milliseconds
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
#endif


    while (!(*(args->stop_flag))) {
        memset(buffer, 0, MAX_ANNOUNCEMENT_MSG_SIZE);
        int recv_len = recvfrom(sockfd, buffer, MAX_ANNOUNCEMENT_MSG_SIZE - 1, 0,
                                (struct sockaddr*)&client_addr, &client_addr_len);

        if (recv_len < 0) {
#if defined(_WIN32) || defined(_WIN64)
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                continue; // Timeout, check stop_flag and loop
            } else if (!(*(args->stop_flag))) { // Don't log error if we are stopping
                 Print("P2P Discovery Listener: recvfrom error: %d\n", error);
            }
#else
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue; // Timeout, check stop_flag and loop
            } else if (!(*(args->stop_flag))) { // Don't log error if we are stopping
                perror("recvfrom listener");
            }
#endif
            if (*(args->stop_flag)) break; // Break if stopping
            continue;
        }
        buffer[recv_len] = '\0'; // Null-terminate the received data

        char peer_ip_str[MAX_IP_STR_LEN];
        inet_ntop(AF_INET, &(client_addr.sin_addr), peer_ip_str, MAX_IP_STR_LEN);

        PeerInfo discovered_peer;
        if (parse_announcement_message(buffer, discovered_peer.node_id, MAX_NODE_ID_LEN, &discovered_peer.tcp_port) == 0) {
            // Check if it's not a self-announcement
            // This check could be more robust by comparing against all local IPs.
            // For now, simple check against own_node_id and own_tcp_port.
            // Also, if source IP is 127.0.0.1 and matches own port, likely self.
            int is_loopback_self = (strcmp(peer_ip_str, "127.0.0.1") == 0 && discovered_peer.tcp_port == args->own_tcp_port);
            int is_known_self = (strcmp(discovered_peer.node_id, args->own_node_id) == 0 && discovered_peer.tcp_port == args->own_tcp_port);

            // A more robust check for self involves iterating through local interface IPs.
            // For now, if the node_id matches and the IP is a known local IP (like 127.0.0.1 or gethostname result)
            // it's likely self. The discovery module in Python had a gethostbyname check.
            // Here, we rely on the provided own_node_id and own_tcp_port.

            if (is_loopback_self || is_known_self) {
                // Print("P2P Discovery Listener: Ignored self-announcement from %s (%s:%u)\n", discovered_peer.node_id, peer_ip_str, discovered_peer.tcp_port);
                continue;
            }

            strncpy(discovered_peer.ip_address, peer_ip_str, MAX_IP_STR_LEN -1);
            discovered_peer.ip_address[MAX_IP_STR_LEN -1] = '\0';
            discovered_peer.last_seen = time(NULL);

            // Add to shared list (thread-safe)
            PeerList_AddOrUpdate(args->discovered_peers_list, &discovered_peer, args->peer_list_mutex);

        } else {
             Print("P2P Discovery Listener: Received malformed message from %s: '%s'\n", peer_ip_str, buffer);
        }
    }

    Print("P2P Discovery Listener: Stopping...\n");
#if defined(_WIN32) || defined(_WIN64)
    closesocket(sockfd);
    // WSACleanup(); // Global cleanup
#else
    close(sockfd);
#endif
    return NULL;
}


pthread_t start_udp_listener(UdpListenerArgs* args) {
    pthread_t thread_id = 0;
    if (!args || !args->discovered_peers_list || !args->peer_list_mutex || !args->stop_flag) {
        Print("P2P Discovery: Invalid arguments for start_udp_listener.\n");
        return 0; // Using 0 as an invalid thread ID indicator
    }

    if (pthread_create(&thread_id, NULL, udp_listener_thread_func, (void*)args) != 0) {
        Print("P2P Discovery: Failed to create UDP listener thread.\n");
        perror("pthread_create listener");
        return 0; // Error
    }
    return thread_id;
}
