// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez (existing license)
// Copyright (c) 2023 The Gemini Agent authors (for new P2P code)

#include "p2p_nodelist.h"
#include "util.h" // For Print()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> // For time()

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
// Winsock already initialized by discovery module if it runs first,
// but good practice to ensure it for standalone use or different call order.
static int ensure_winsock_initialized_nodelist() {
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        Print("Nodelist: WSAStartup failed: %d\n", result);
        return -1;
    }
    return 0;
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> // For getaddrinfo
#include <arpa/inet.h> // For inet_ntop
#endif

#define MAX_LINE_LENGTH 512 // Max length for a line in the node list file

int load_known_peers_from_file(const char* filepath,
                               uint16_t default_tcp_port,
                               PeerList* known_peers_list,
                               pthread_mutex_t* peer_list_mutex) {
    if (!filepath || !known_peers_list || !peer_list_mutex) {
        Print("Nodelist: Invalid arguments to load_known_peers_from_file.\n");
        return -1;
    }

#if defined(_WIN32) || defined(_WIN64)
    // ensure_winsock_initialized_nodelist(); // Assuming discovery part initializes it.
                                          // If this module could be used totally independently, then yes.
#endif

    FILE* file = fopen(filepath, "r");
    if (!file) {
        // Print("Nodelist: File not found: %s (This is not an error if file is optional)\n", filepath);
        return 0; // Not finding the file is not necessarily a critical error.
    }

    char line[MAX_LINE_LENGTH];
    int line_num = 0;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        // Remove newline characters
        line[strcspn(line, "\r\n")] = 0;

        // Skip empty lines and comments
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        char* host_part = line;
        char* port_part_str = NULL;
        uint16_t current_port = default_tcp_port;

        // Check for port specification (e.g., "host:port" or "[ipv6]:port")
        char* last_colon = strrchr(line, ':');
        char* closing_bracket = strrchr(line, ']');

        if (last_colon) {
            // If ']' exists and ':' is after it, then it's a port for an IPv6 address
            // e.g. [::1]:8080
            // If ']' does not exist, and ':' exists, it could be IPv4:port or bare_ipv6::addr:port (less common for node lists)
            if (closing_bracket && last_colon > closing_bracket) { // Format: [host]:port
                port_part_str = last_colon + 1;
                *last_colon = '\0'; // Terminate host_part before the colon
                if (host_part[0] == '[') { // Remove '[' and ']' from host_part
                   host_part++;
                   *(last_colon-1) = '\0'; // remove ']'
                }
            } else if (!closing_bracket) { // No ']', so might be IPv4:port or a bare IPv6 that happens to have a colon
                // This is ambiguous for bare IPv6. Assume if last colon has digits after it, it's a port.
                // A more robust parser would be needed for all IPv6 cases without brackets.
                // For now, we try to parse port_part_str if it looks like a number.
                int potential_port = atoi(last_colon + 1);
                if (potential_port > 0 && potential_port <= 65535) {
                     port_part_str = last_colon + 1;
                     *last_colon = '\0'; // Terminate host_part
                }
            }
        }


        if (port_part_str) {
            long port_val = strtol(port_part_str, NULL, 10);
            if (port_val > 0 && port_val <= 65535) {
                current_port = (uint16_t)port_val;
            } else {
                Print("Nodelist: Invalid port '%s' in %s line %d. Using default %u.\n",
                      port_part_str, filepath, line_num, default_tcp_port);
            }
        }

        // Resolve hostname/IP
        struct addrinfo hints, *res, *p;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC; // Allow IPv4 or IPv6
        hints.ai_socktype = SOCK_STREAM;

        int status = getaddrinfo(host_part, NULL, &hints, &res); // Port not needed for getaddrinfo if resolving host
        if (status != 0) {
            Print("Nodelist: Failed to resolve '%s' from %s line %d: %s\n",
                  host_part, filepath, line_num, gai_strerror(status));
            continue;
        }

        for (p = res; p != NULL; p = p->ai_next) {
            PeerInfo peer;
            memset(&peer, 0, sizeof(PeerInfo));

            if (p->ai_family == AF_INET) { // IPv4
                struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
                inet_ntop(AF_INET, &(ipv4->sin_addr), peer.ip_address, sizeof(peer.ip_address));
            } else { // IPv6
                struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
                inet_ntop(AF_INET6, &(ipv6->sin6_addr), peer.ip_address, sizeof(peer.ip_address));
            }

            strncpy(peer.node_id, host_part, MAX_NODE_ID_LEN - 1); // Use original entry as node_id initially
            peer.node_id[MAX_NODE_ID_LEN - 1] = '\0';
            peer.tcp_port = current_port;
            peer.last_seen = time(NULL); // Mark as seen now

            PeerList_AddOrUpdate(known_peers_list, &peer, peer_list_mutex);
            // Typically, take the first resolved address. Could add all, but might create duplicates if not careful.
            break;
        }
        freeaddrinfo(res);
    }

    fclose(file);
    return 0;
}
