// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Jules

#include "p2p.h"
#include "util.h" // For Print function and potentially others
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h> // For isspace, isdigit
#include <pthread.h> // For pthread functions

// Basic networking includes - these might need to be adjusted based on OS
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h> // For inet_pton, InetPtonA, etc.
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // For close()
#include <errno.h>
#endif

// Include headers needed for FindShareableDirectories and related structs/defines
#include "fileutil.h" // For VolumeInfo, MaxPath, MaxFilename etc.
#include "filenode.h"   // For FileNode_Filename

// P2P message types are now in p2p.h

#define MAX_NODES 100 // Maximum number of nodes to keep track of
static NodeInfo discovered_nodes[MAX_NODES];
static int num_discovered_nodes = 0;

// UDP Socket for discovery
static int udp_socket = -1;
static int p2p_udp_port_internal = 0; // Renamed to clarify this is for UDP
static int rescan_interval_internal = 30; // Default rescan interval

// Placeholder for thread that handles UDP listening and periodic announcements
#ifdef _WIN32
static HANDLE discovery_thread_handle = NULL;
#else
static pthread_t discovery_thread_handle;
#endif
static bool discovery_active = false;
static bool discovery_thread_created = false; // General flag

// TCP Server Socket
static int tcp_listen_socket = -1;
static int p2p_tcp_port_internal = 0; // Store the port used for TCP server

#ifdef _WIN32
static HANDLE tcp_server_thread_handle = NULL;
#else
static pthread_t tcp_server_thread_handle; // POSIX thread for accepting connections
#endif
static bool tcp_server_active = false;
static bool tcp_server_thread_created = false; // General flag

// Static storage for the shares this daemon instance will publish
static P2PShareableDir* local_published_shares = NULL;
static int num_local_published_shares = 0;


// Helper to initialize Winsock on Windows
#ifdef _WIN32
bool InitializeWindowsSockets() {
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        Print("WSAStartup failed: %d\n", iResult);
        return false;
    }
    return true;
}
void CleanupWindowsSockets() {
    WSACleanup();
}
#endif

// Thread function for TCP server (accepting connections)
#ifdef _WIN32
DWORD WINAPI TCPServerThreadFunc(LPVOID lpParam) {
#else
void* TCPServerThreadFunc(void* lpParam) {
#endif
    Print("P2P TCP Server Thread Started. Listening on port: %d\n", p2p_tcp_port_internal);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (tcp_server_active) {
        // Set socket to non-blocking or use select/poll to allow graceful shutdown
        // For this example, we'll make it simple and rely on socket closure to break accept
        // A short timeout for accept could also be used.
        fd_set readfds;
        struct timeval tv;
        int activity;

        FD_ZERO(&readfds);
        FD_SET(tcp_listen_socket, &readfds);

        tv.tv_sec = 1; // 1 second timeout for select
        tv.tv_usec = 0;

        if (!tcp_server_active) break; // Check before select

        activity = select(tcp_listen_socket + 1, &readfds, NULL, NULL, &tv);

        if (!tcp_server_active) break; // Check after select

        if ((activity < 0) && (errno!=EINTR)) {
#ifdef _WIN32
            Print("TCP Server: select error %d\n", WSAGetLastError());
#else
            Print("TCP Server: select error %s\n", strerror(errno));
#endif
            continue;
        }

        if (FD_ISSET(tcp_listen_socket, &readfds)) {
            int client_socket = accept(tcp_listen_socket, (struct sockaddr *)&client_addr, &client_len);
            if (client_socket < 0) {
                if (tcp_server_active) { // Only print error if we are supposed to be active
    #ifdef _WIN32
                    if (WSAGetLastError() != WSAEINTR && WSAGetLastError() != WSAEWOULDBLOCK && WSAGetLastError() != WSAECONNABORTED && WSAGetLastError() != WSAEINVAL ) {
                         Print("TCP Server: accept failed with error: %d\n", WSAGetLastError());
                    }
    #else
                    if (errno != EINTR && errno != EWOULDBLOCK && errno != ECONNABORTED && errno != EINVAL) { // EINVAL can happen if socket is closed
                        Print("TCP Server: accept failed: %s\n", strerror(errno));
                    }
    #endif
                }
                if (!tcp_server_active) break; // Exit if server is no longer active
                continue;
            }

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            Print("P2P TCP Server: Accepted connection from %s:%d on socket %d\n", client_ip, ntohs(client_addr.sin_port), client_socket);

            // TODO: Handle the connection (e.g., create a new thread or add to a pool)
            // Basic request/response for "HELLO"
            char buffer[1024];
            int bytes_received = P2P_ReceiveData(client_socket, buffer, sizeof(buffer) - 1);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                Print("P2P TCP Server: Received from %s:%d on socket %d: '%s'\n", client_ip, ntohs(client_addr.sin_port), client_socket, buffer);

                if (strcmp(buffer, "HELLO_FROM_DAEMON") == 0) {
                    const char *response = "HELLO_ACK_FROM_SERVER";
                    if (P2P_SendData(client_socket, response, strlen(response))) {
                        Print("P2P TCP Server: Sent '%s' back to client.\n", response);
                    } else {
                        Print("P2P TCP Server: Failed to send ACK to client.\n");
                    }
                } else if (strcmp(buffer, P2P_MSG_REQ_SHARE_LIST) == 0) {
                    Print("P2P TCP Server: Received REQ_SHARE_LIST from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
                    char response_buffer[4096]; // Buffer for the response message

                    // Use the statically populated list of published shares
                    if (num_local_published_shares >= 0) { // Even 0 is a valid response (empty list)
                        Print("P2P TCP Server: Reporting %d configured local shareable directories.\n", num_local_published_shares);
                        int offset = snprintf(response_buffer, sizeof(response_buffer), "%s %d", P2P_MSG_SHARE_LIST_RESP, num_local_published_shares);
                        for (int i = 0; i < num_local_published_shares; i++) {
                            // Use local_published_shares[i].name and local_published_shares[i].path
                            int len_name = strlen(local_published_shares[i].name);
                            int len_path = strlen(local_published_shares[i].path);

                            // Ensure null terminators are not part of the length for network transfer, but account for them in buffer checks if sending them.
                            // Here, we send exact lengths and then the string data.
                            if (offset + (sizeof(int) * 2) + len_name + len_path < sizeof(response_buffer)) {
                                memcpy(response_buffer + offset, &len_name, sizeof(int));
                                offset += sizeof(int);
                                memcpy(response_buffer + offset, local_published_shares[i].name, len_name);
                                offset += len_name;

                                memcpy(response_buffer + offset, &len_path, sizeof(int));
                                offset += sizeof(int);
                                memcpy(response_buffer + offset, local_published_shares[i].path, len_path);
                                offset += len_path;
                            } else {
                                Print("P2P TCP Server: Response buffer too small for all shareable dirs. Sent %d out of %d.\n", i, num_local_published_shares);
                                // Update the count in the message to reflect how many were actually written
                                // This is tricky; for now, client must be robust or we send an error if list is too long.
                                // For simplicity, we send what fits, the initial count might be higher.
                                snprintf(response_buffer + strlen(P2P_MSG_SHARE_LIST_RESP) + 1, sizeof(response_buffer) - (strlen(P2P_MSG_SHARE_LIST_RESP) + 1), "%d", i);
                                break;
                            }
                        }
                        P2P_SendData(client_socket, response_buffer, offset);
                        Print("P2P TCP Server: Sent SHARE_LIST_RESP with (up to) %d dirs.\n", num_local_published_shares);
                    } else { // This case should ideally not be hit if num_local_published_shares is >= 0 from init.
                             // If P2P_GetShareableDirsFromConfig returned error, num_local_published_shares would be 0.
                        Print("P2P TCP Server: No local shares configured or error during init. Reporting 0 shares.\n");
                        int offset_err = snprintf(response_buffer, sizeof(response_buffer), "%s 0", P2P_MSG_SHARE_LIST_RESP);
                        P2P_SendData(client_socket, response_buffer, offset_err);
                    }
                    // No need to free local_published_shares here, it's static for the server lifetime
                } else {
                    Print("P2P TCP Server: Received unknown message: %s\n", buffer);
                    // Optionally send an error or generic response like:
                    // snprintf(response_buffer, sizeof(response_buffer), "%s Unknown command", P2P_MSG_ERROR);
                    // P2P_SendData(client_socket, response_buffer, strlen(response_buffer));
                }
            } else if (bytes_received == 0) {
                Print("P2P TCP Server: Client %s:%d on socket %d closed connection gracefully.\n", client_ip, ntohs(client_addr.sin_port), client_socket);
            } else {
                Print("P2P TCP Server: Error receiving data from %s:%d on socket %d.\n", client_ip, ntohs(client_addr.sin_port), client_socket);
            }
            P2P_CloseConnection(client_socket); // Close after handling
        }
    }
    Print("P2P TCP Server Thread Exiting.\n");
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}


// Thread function for UDP discovery (very basic placeholder)
#ifdef _WIN32
DWORD WINAPI DiscoveryThreadFunc(LPVOID lpParam) {
#else
void* DiscoveryThreadFunc(void* lpParam) {
#endif
    Print("P2P Discovery Thread Started. UDP Port: %d, Interval: %d\n", p2p_udp_port_internal, rescan_interval_internal);

    // TODO: Implement actual UDP broadcasting/multicasting for announcements
    // TODO: Implement UDP listening to discover other nodes

    while (discovery_active) {
        // Print("P2P Discovery Thread: Scanning for nodes...\n"); // Reduce verbosity for now
        // TODO: Send out announcement packet (broadcast/multicast)
        // TODO: Listen for responses / announcements from other nodes

        // Sleep for the rescan interval
#ifdef _WIN32
        Sleep(rescan_interval_internal * 1000);
#else
        sleep(rescan_interval_internal);
#endif
    }
    Print("P2P Discovery Thread Exiting.\n");
#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}


bool P2P_InitNodeDiscovery(int port, int rescan_interval) {
    if (discovery_active) {
        Print("P2P Node Discovery is already active.\n");
        return true;
    }

#ifdef _WIN32
    if (!InitializeWindowsSockets()) {
        return false;
    }
#endif

    p2p_udp_port_internal = port; // Corrected variable name
    rescan_interval_internal = rescan_interval;
    num_discovered_nodes = 0; // Reset node list
    discovery_thread_created = false; // Reset flag

    // TODO: Setup UDP socket for broadcasting/listening
    // For now, we are just starting the thread concept

    discovery_active = true;
#ifdef _WIN32
    discovery_thread_handle = CreateThread(NULL, 0, DiscoveryThreadFunc, NULL, 0, NULL);
    if (discovery_thread_handle == NULL) {
        Print("Error creating P2P discovery thread: %ld\n", GetLastError());
        discovery_active = false;
        // No WSACleanup here if TCP thread might be running
        return false;
    }
    discovery_thread_created = true;
#else // POSIX
    if (pthread_create(&discovery_thread_handle, NULL, DiscoveryThreadFunc, NULL) != 0) {
        Print("Error creating P2P discovery thread: %s\n", strerror(errno));
        discovery_active = false;
        return false;
    }
    discovery_thread_created = true;
#endif

    Print("P2P Node Discovery initialized. UDP Port: %d, Rescan Interval: %d seconds.\n", p2p_udp_port_internal, rescan_interval_internal);
    return true;
}

void P2P_ShutdownNodeDiscovery() {
    if (!discovery_active && !discovery_thread_created) { // Check if it was ever active or thread created
        // Print("P2P Node Discovery was not active or thread not created.\n");
        return;
    }

    if(discovery_active){
        Print("Shutting down P2P Node Discovery (active)...\n");
        discovery_active = false; // Signal thread to stop
    } else if (discovery_thread_created){
         Print("Shutting down P2P Node Discovery (inactive but thread exists)...\n");
    }


#ifdef _WIN32
    if (discovery_thread_created && discovery_thread_handle != NULL) {
        WaitForSingleObject(discovery_thread_handle, INFINITE);
        CloseHandle(discovery_thread_handle);
        discovery_thread_handle = NULL;
    }
#else
    if (discovery_thread_created) { // Only join if created
        pthread_join(discovery_thread_handle, NULL);
    }
#endif
    discovery_thread_created = false; // Mark as joined/cleaned

    if (udp_socket != -1) {
#ifdef _WIN32
        closesocket(udp_socket);
#else
        close(udp_socket);
#endif
        udp_socket = -1;
    }
    // Final WSACleanup() should be done when both UDP and TCP systems are down, typically in main.
    Print("P2P Node Discovery shut down.\n");
}

int P2P_GetDiscoveredNodes(NodeInfo *nodes, int max_nodes) {
    if (!discovery_active && num_discovered_nodes == 0) {
        // To allow testing without fully active discovery if nodes were loaded from file
        // Print("P2P Discovery not active or no nodes found yet.\n");
    }
    int count_to_copy = (num_discovered_nodes < max_nodes) ? num_discovered_nodes : max_nodes;
    memcpy(nodes, discovered_nodes, count_to_copy * sizeof(NodeInfo));
    return count_to_copy;
}

int P2P_LoadNodesFromFile(const char *filepath, NodeInfo *nodes_output_array, int max_nodes_to_load) {
    if (!filepath) {
        Print("Node list filepath is NULL.\n");
        return 0;
    }

    FILE *file = fopen(filepath, "r");
    if (!file) {
        Print("Error opening node list file: %s\n", filepath);
        return 0;
    }

    char line[256];
    int loaded_count = 0;
    while (fgets(line, sizeof(line), file) && loaded_count < max_nodes_to_load) {
        // Remove newline character
        line[strcspn(line, "\r\n")] = 0;

        // Trim leading/trailing whitespace
        char* start = line;
        while (isspace((unsigned char)*start)) start++;
        char* end = line + strlen(line) - 1;
        while (end > start && isspace((unsigned char)*end)) end--;
        *(end + 1) = '\0';

        if (start[0] == '#' || start[0] == '\0') { // Skip comments and empty lines
            continue;
        }

        char *processed_line = start; // Use the trimmed line

        char *port_str_separator = strrchr(processed_line, ':');
        int port_val = p2p_tcp_port_internal; // Default to the global TCP data port

        char host_part[256]; // Buffer for the host part
        strncpy(host_part, processed_line, sizeof(host_part)-1);
        host_part[sizeof(host_part)-1] = '\0';

        if (port_str_separator != NULL) {
            // Check if what follows the colon is a number (potential port)
            // and not just another segment of an IPv6 address.
            bool is_port_segment = true;
            char *check_ptr = port_str_separator + 1;
            if (*check_ptr == '\0') { // e.g. "1.2.3.4:"
                is_port_segment = true; // Will use default port
            } else {
                while(*check_ptr) {
                    if (!isdigit((unsigned char)*check_ptr)) {
                        is_port_segment = false; // Found non-digit, likely part of IPv6 or invalid
                        break;
                    }
                    check_ptr++;
                }
            }

            // If there's only one colon, or if the segment after the last colon is numeric,
            // it's likely a port definition.
            if (strchr(processed_line, ':') == port_str_separator || is_port_segment) {
                // Null-terminate the host part at the last colon
                *port_str_separator = '\0';
                strncpy(host_part, processed_line, sizeof(host_part)-1); // Recopy host part without port
                host_part[sizeof(host_part)-1] = '\0';

                char* p_num = port_str_separator + 1;
                if (*p_num != '\0' && is_port_segment) { // If port string is not empty and was numeric
                    port_val = atoi(p_num);
                    if (port_val <= 0 || port_val > 65535) {
                        Print("Invalid port '%s' in node list for host '%s', using default TCP port %d\n", p_num, host_part, p2p_tcp_port_internal);
                        port_val = p2p_tcp_port_internal;
                    }
                } else {
                     // Empty port string (e.g. "host:") or non-numeric port segment that we initially thought was port.
                     // If it was non-numeric, host_part already holds the full string due to earlier logic.
                     // If it was "host:", use default port.
                     port_val = p2p_tcp_port_internal;
                }
            }
            // If it has multiple colons and the last segment isn't numeric,
            // host_part remains the full processed_line (IPv6) and default port is used.
        }

        const int max_ip_field_len = sizeof(nodes_output_array[loaded_count].ip_address) -1;
        if (strlen(host_part) > 0) {
            strncpy(nodes_output_array[loaded_count].ip_address, host_part, max_ip_field_len);
            nodes_output_array[loaded_count].ip_address[max_ip_field_len] = '\0';
            nodes_output_array[loaded_count].port = port_val;
            loaded_count++;
        } else {
            Print("Skipping empty or invalid host part in node list after processing: %s\n", processed_line);
        }
    }

    fclose(file);
    Print("Loaded %d nodes from %s.\n", loaded_count, filepath);

    // Optionally, add these to the main discovered_nodes list or manage separately
    // For now, let's add them to the main list if discovery is not yet active,
    // or if we want to merge them.
    // This simple merge assumes no duplicates.
    int current_total_nodes = num_discovered_nodes;
    for(int i = 0; i < loaded_count && (current_total_nodes + i) < MAX_NODES; ++i) {
        bool exists = false;
        for(int j=0; j < num_discovered_nodes; ++j) {
            if(strcmp(discovered_nodes[j].ip_address, nodes_output_array[i].ip_address) == 0 &&
               discovered_nodes[j].port == nodes_output_array[i].port) {
                exists = true;
                break;
            }
        }
        if(!exists) {
            if(num_discovered_nodes < MAX_NODES) {
                discovered_nodes[num_discovered_nodes] = nodes_output_array[i];
                num_discovered_nodes++;
            } else {
                Print("Max discovered nodes limit reached, cannot add more from file.\n");
                break;
            }
        }
    }

    return loaded_count; // Return count of nodes read from file, not total
}


// Internal callback for P2P_GetLocalShareableDirectories, similar to ListDirectoriesCallback in main.c
typedef struct {
    char **potentialPaths;
    int *pathCount;
    int pathCapacity;
} P2PDirScanData;

static int P2PListDirectoriesCallback(char *itemPath, char *itemName, void *data) {
    P2PDirScanData *scanData = (P2PDirScanData *)data;
    if (File_IsDirectory(itemPath)) {
        if (*scanData->pathCount >= scanData->pathCapacity) {
            scanData->pathCapacity = (scanData->pathCapacity == 0) ? 10 : scanData->pathCapacity * 2;
            char **tempPaths = (char **)realloc(scanData->potentialPaths, scanData->pathCapacity * sizeof(char *));
            if (!tempPaths) {
                Print("Error: Memory reallocation failed in P2PListDirectoriesCallback for potentialPaths\n");
                for(int i=0; i < *scanData->pathCount; i++) { if(scanData->potentialPaths[i]) free(scanData->potentialPaths[i]); }
                free(scanData->potentialPaths);
                scanData->potentialPaths = NULL;
				*scanData->pathCount = 0;
                return 1; // Stop iteration
            }
            scanData->potentialPaths = tempPaths;
        }
        scanData->potentialPaths[*scanData->pathCount] = strdup(itemPath);
        if (!scanData->potentialPaths[*scanData->pathCount]) {
             Print("Error: strdup failed in P2PListDirectoriesCallback\n");
             return 1; // Stop iteration
        }
        (*scanData->pathCount)++;
    }
    return 0; // Continue
}

// Function to find shareable directories from a given list of directory paths.
// Returns the count of found directories, or -1 on critical error.
// out_dirs will be allocated and should be freed by the caller.
int P2P_GetShareableDirsFromConfig(char **dir_paths, int num_dirs, P2PShareableDir **out_p2p_dirs) {
    P2PShareableDir *foundDirs = NULL;
    int foundCount = 0;
    int foundCapacity = 0;

    *out_p2p_dirs = NULL;

    if (!dir_paths || num_dirs <= 0) {
        return 0; // No paths provided
    }

    Print("P2P_GetShareableDirsFromConfig: Checking %d configured paths.\n", num_dirs);

    for (int i = 0; i < num_dirs; i++) {
        if (dir_paths[i] == NULL) continue; // Should not happen if num_dirs is accurate

        Print("P2P_GetShareableDirsFromConfig: Checking configured path: %s\n", dir_paths[i]);
        char markerFilePath[MaxPath];
        snprintf(markerFilePath, MaxPath, "%s/%s", dir_paths[i], FileNode_Filename);
        Print("P2P_GetShareableDirsFromConfig: Looking for marker file: %s\n", markerFilePath);

        if (File_ExistsPath(markerFilePath)) {
            Print("P2P_GetShareableDirsFromConfig: Found marker file for %s\n", dir_paths[i]);
            if (foundCount >= foundCapacity) {
                foundCapacity = (foundCapacity == 0) ? 2 : foundCapacity * 2; // Start with 2, grow as needed
                 P2PShareableDir *temp_foundDirs = (P2PShareableDir *)realloc(foundDirs, foundCapacity * sizeof(P2PShareableDir));
                if (!temp_foundDirs) {
                    Print("Error: Memory reallocation failed for foundDirs in P2P_GetShareableDirsFromConfig\n");
                    // Free any already found P2PShareableDir items if they had allocated members (none currently)
                    free(foundDirs);
                    *out_p2p_dirs = NULL;
                    return -1; // Critical error
                }
                foundDirs = temp_foundDirs;
            }

            P2PShareableDir *currentEntry = &foundDirs[foundCount];
            strncpy(currentEntry->path, dir_paths[i], sizeof(currentEntry->path) - 1);
            currentEntry->path[sizeof(currentEntry->path) - 1] = '\0';

            File_GetName(currentEntry->path, currentEntry->name); // Get base name
            currentEntry->name[sizeof(currentEntry->name) -1] = '\0';

            Print("P2P_GetShareableDirsFromConfig: Added share: Name='%s', Path='%s'\n", currentEntry->name, currentEntry->path);
            foundCount++;
        } else {
             Print("P2P_GetShareableDirsFromConfig: No marker file at %s\n", markerFilePath);
        }
    }

    *out_p2p_dirs = foundDirs;
    Print("P2P_GetShareableDirsFromConfig: Found %d shareable directories from config.\n", foundCount);
    return foundCount;
}

// Helper function to free the published shares list
void P2P_ClearPublishedShares() {
    if (local_published_shares != NULL) {
        // If P2PShareableDir members dynamically allocated memory, free them here first
        free(local_published_shares);
        local_published_shares = NULL;
    }
    num_local_published_shares = 0;
}

// Modified P2P_InitTCPServer to accept ApplicationConfiguration
// to setup its published shares list.
bool P2P_InitTCPServer(int port, char **configured_dirs, int num_configured_dirs) {
    if (tcp_server_active) {
        Print("P2P TCP Server is already active.\n");
        return true;
    }

    // Clear any previous published shares (e.g., if re-initializing)
    P2P_ClearPublishedShares();

    // Populate the list of shares this server instance will publish
    if (configured_dirs != NULL && num_configured_dirs > 0) {
        num_local_published_shares = P2P_GetShareableDirsFromConfig(configured_dirs, num_configured_dirs, &local_published_shares);
        if (num_local_published_shares < 0) {
            Print("P2P_InitTCPServer: Error getting shareable dirs from config. Aborting TCP server init.\n");
            num_local_published_shares = 0; // Ensure it's zero on error
            return false; // Critical error during share list population
        }
        Print("P2P_InitTCPServer: Will publish %d shares.\n", num_local_published_shares);
    } else {
        Print("P2P_InitTCPServer: No directories configured to share via P2P.\n");
        num_local_published_shares = 0;
    }


#ifdef _WIN32
    // Winsock should be initialized by P2P_InitNodeDiscovery or here if called first
    // Call it here to be safe, it has checks to prevent multiple initializations.
    if (!InitializeWindowsSockets()) { // Ensure Winsock is up
        P2P_ClearPublishedShares(); // Clean up if we fail after populating
        return false;
    }
#endif

    p2p_tcp_port_internal = port;
    tcp_listen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_listen_socket < 0) {
        P2P_ClearPublishedShares(); // Clean up
#ifdef _WIN32
        Print("P2P TCP Server: Failed to create socket: %d\n", WSAGetLastError());
#else
        Print("P2P TCP Server: Failed to create socket: %s\n", strerror(errno));
#endif
        return false;
    }

    // Set socket to be reusable
    int optval = 1;
#ifdef _WIN32
    if (setsockopt(tcp_listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval)) < 0) {
      Print("P2P TCP Server: setsockopt(SO_REUSEADDR) failed: %d\n", WSAGetLastError());
      closesocket(tcp_listen_socket);
      return false;
    }
#else
    if (setsockopt(tcp_listen_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
      Print("P2P TCP Server: setsockopt(SO_REUSEADDR) failed: %s\n", strerror(errno));
      close(tcp_listen_socket);
      return false;
    }
#endif

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(p2p_tcp_port_internal);

    if (bind(tcp_listen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
#ifdef _WIN32
        Print("P2P TCP Server: Bind failed on port %d: %d\n", p2p_tcp_port_internal, WSAGetLastError());
        closesocket(tcp_listen_socket);
#else
        Print("P2P TCP Server: Bind failed on port %d: %s\n", p2p_tcp_port_internal, strerror(errno));
        close(tcp_listen_socket);
#endif
        tcp_listen_socket = -1;
        return false;
    }

    if (listen(tcp_listen_socket, 5) < 0) { // Listen with a backlog of 5 connections
#ifdef _WIN32
        Print("P2P TCP Server: Listen failed: %d\n", WSAGetLastError());
        closesocket(tcp_listen_socket);
#else
        Print("P2P TCP Server: Listen failed: %s\n", strerror(errno));
        close(tcp_listen_socket);
#endif
        tcp_listen_socket = -1;
        return false;
    }

    tcp_server_active = true;
    tcp_server_thread_created = false; // Reset flag
#ifdef _WIN32
    tcp_server_thread_handle = CreateThread(NULL, 0, TCPServerThreadFunc, NULL, 0, NULL);
    if (tcp_server_thread_handle == NULL) {
        Print("Error creating P2P TCP server thread: %ld\n", GetLastError());
        tcp_server_active = false;
        closesocket(tcp_listen_socket);
        tcp_listen_socket = -1;
        return false;
    }
    tcp_server_thread_created = true;
#else // POSIX
    if (pthread_create(&tcp_server_thread_handle, NULL, TCPServerThreadFunc, NULL) != 0) {
        Print("Error creating P2P TCP server thread: %s\n", strerror(errno));
        tcp_server_active = false;
        close(tcp_listen_socket);
        tcp_listen_socket = -1;
        return false;
    }
    tcp_server_thread_created = true;
#endif

    Print("P2P TCP Server initialized. Listening on port %d.\n", p2p_tcp_port_internal);
    return true;
}

void P2P_ShutdownTCPServer() {
    if (!tcp_server_active && !tcp_server_thread_created) {
        // Print("P2P TCP Server was not active or thread not created.\n");
        return;
    }

    if(tcp_server_active){
        Print("Shutting down P2P TCP Server (active)...\n");
        tcp_server_active = false; // Signal the thread to stop
    } else if (tcp_server_thread_created){
        Print("Shutting down P2P TCP Server (inactive but thread exists)...\n");
    }


    // Close the listening socket to interrupt the accept() call in the server thread
    // This needs to happen *before* joining the thread.
    if (tcp_listen_socket != -1) {
#ifdef _WIN32
        shutdown(tcp_listen_socket, SD_BOTH);
        closesocket(tcp_listen_socket);
#else
        shutdown(tcp_listen_socket, SHUT_RDWR);
        close(tcp_listen_socket);
#endif
        tcp_listen_socket = -1;
    }

#ifdef _WIN32
    if (tcp_server_thread_created && tcp_server_thread_handle != NULL) {
        WaitForSingleObject(tcp_server_thread_handle, INFINITE);
        CloseHandle(tcp_server_thread_handle);
        tcp_server_thread_handle = NULL;
    }
#else
    if (tcp_server_thread_created) { // Only join if created
        pthread_join(tcp_server_thread_handle, NULL);
    }
#endif
    tcp_server_thread_created = false; // Mark as joined/cleaned

    // WSACleanup should be called when ALL socket operations are done (UDP + TCP)
    // This might be better handled in main.c after both P2P_ShutdownNodeDiscovery and P2P_ShutdownTCPServer
    // For now, if discovery is also off, then cleanup.
    // if (!discovery_active) {
    // #ifdef _WIN32
    //    CleanupWindowsSockets();
    // #endif
    // }
    P2P_ClearPublishedShares(); // Clean up the published shares list
    Print("P2P TCP Server shut down.\n");
}

int P2P_ConnectToNode(const char *ip_address, int port) {
    int sock_fd = -1;
    struct sockaddr_in server_addr;

#ifdef _WIN32
    // Ensure Winsock is initialized
    if (!InitializeWindowsSockets()) return -1;
#endif

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
#ifdef _WIN32
        Print("P2P_ConnectToNode: Failed to create socket. Error: %d\n", WSAGetLastError());
#else
        Print("P2P_ConnectToNode: Failed to create socket. Error: %s\n", strerror(errno));
#endif
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    // Convert IP address string to network address structure
#ifdef _WIN32
    if (InetPtonA(AF_INET, ip_address, &server_addr.sin_addr) <= 0) {
        Print("P2P_ConnectToNode: Invalid address or address not supported for %s. Error: %d\n", ip_address, WSAGetLastError());
        closesocket(sock_fd);
        return -1;
    }
#else
    if (inet_pton(AF_INET, ip_address, &server_addr.sin_addr) <= 0) {
        Print("P2P_ConnectToNode: Invalid address or address not supported for %s. Error: %s\n", ip_address, strerror(errno));
        close(sock_fd);
        return -1;
    }
#endif

    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
#ifdef _WIN32
        Print("P2P_ConnectToNode: Connection failed to %s:%d. Error: %d\n", ip_address, port, WSAGetLastError());
        closesocket(sock_fd);
#else
        Print("P2P_ConnectToNode: Connection failed to %s:%d. Error: %s\n", ip_address, port, strerror(errno));
        close(sock_fd);
#endif
        return -1;
    }

    Print("P2P_ConnectToNode: Successfully connected to %s:%d on socket %d\n", ip_address, port, sock_fd);
    return sock_fd;
}

bool P2P_SendData(int socket_fd, const void *data, size_t length) {
    if (socket_fd < 0 || data == NULL || length == 0) {
        return false;
    }
    ssize_t bytes_sent = send(socket_fd, (const char*)data, length, 0);
    if (bytes_sent < 0) {
#ifdef _WIN32
        Print("P2P_SendData: send() to socket %d failed. Error: %d\n", socket_fd, WSAGetLastError());
#else
        Print("P2P_SendData: send() to socket %d failed. Error: %s\n", socket_fd, strerror(errno));
#endif
        return false;
    }
    if ((size_t)bytes_sent < length) {
        Print("P2P_SendData: Warning, not all data sent to socket %d. Sent %zd of %zu bytes.\n", socket_fd, bytes_sent, length);
        // Depending on protocol, this might be a hard error or might need retry logic.
        // For now, treating partial sends as failure for simplicity of higher-level logic.
        return false;
    }
    // Print("P2P_SendData: Successfully sent %zu bytes to socket %d\n", length, socket_fd);
    return true;
}

int P2P_ReceiveData(int socket_fd, void *buffer, size_t buffer_length) {
    if (socket_fd < 0 || buffer == NULL || buffer_length == 0) {
        Print("P2P_ReceiveData: Invalid arguments (socket: %d, buffer: %p, length: %zu)\n", socket_fd, buffer, buffer_length);
        return -1;
    }
    ssize_t bytes_received = recv(socket_fd, (char*)buffer, buffer_length, 0);
    if (bytes_received < 0) {
#ifdef _WIN32
        // Common non-fatal errors during async operations or graceful close by peer.
        // WSAEWOULDBLOCK is for non-blocking sockets, not expected here with blocking recv.
        // WSAECONNRESET can happen if peer closes abruptly.
        int error_code = WSAGetLastError();
        if (error_code == WSAECONNRESET || error_code == WSAESHUTDOWN || error_code == WSAENOTCONN) {
             Print("P2P_ReceiveData: recv() from socket %d indicated connection closed or reset by peer. Error: %d\n", socket_fd, error_code);
        } else {
             Print("P2P_ReceiveData: recv() from socket %d failed. Error: %d\n", socket_fd, error_code);
        }
#else
        // EWOULDBLOCK/EAGAIN for non-blocking. ENOTCONN, ECONNRESET for closed connections.
        if (errno == ECONNRESET || errno == ENOTCONN || errno == EPIPE) {
             Print("P2P_ReceiveData: recv() from socket %d indicated connection closed or reset by peer. Error: %s\n", socket_fd, strerror(errno));
        } else {
             Print("P2P_ReceiveData: recv() from socket %d failed. Error: %s\n", socket_fd, strerror(errno));
        }
#endif
        return -1; // Error
    } else if (bytes_received == 0) {
        Print("P2P_ReceiveData: Socket %d peer closed connection gracefully (recv returned 0).\n", socket_fd);
    }
    // Print("P2P_ReceiveData: Received %zd bytes from socket %d\n", bytes_received, socket_fd);
    return bytes_received; // Can be 0 if peer closed connection gracefully
}

void P2P_CloseConnection(int socket_fd) {
    if (socket_fd >= 0) {
        Print("Closing P2P TCP connection socket %d\n", socket_fd);
#ifdef _WIN32
        shutdown(socket_fd, SD_BOTH);
        closesocket(socket_fd);
#else
        shutdown(socket_fd, SHUT_RDWR);
        close(socket_fd);
#endif
    }
}
