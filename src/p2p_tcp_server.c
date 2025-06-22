// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez (existing license)
// Copyright (c) 2023 The Gemini Agent authors (for new P2P code)

#include "p2p_tcp_server.h"
#include "p2p_connection_handler.h" // For handle_client_connection
#include "util.h"                   // For Print()

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For close()

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
// Global Winsock initialization assumed to be handled by main or discovery module
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // For inet_ntop
#include <pthread.h>
#endif

// Max number of client handler threads we might keep track of (optional)
#define MAX_CLIENT_HANDLER_THREADS 128
static pthread_t client_handler_threads[MAX_CLIENT_HANDLER_THREADS];
static int client_thread_count = 0;
// Mutex for client_handler_threads array if needed for cleanup management,
// but simple increment might be fine if join is on global stop.
// pthread_mutex_t client_threads_mutex = PTHREAD_MUTEX_INITIALIZER;


typedef struct {
    int client_socket;
    char client_ip_str[INET6_ADDRSTRLEN]; // Max length for IPv6 string
    char local_node_id[MAX_NODE_ID_LEN];
    char sync_dir_path[MaxPath];
    volatile int* stop_flag;
} ClientHandlerArgs;


static void* client_handler_thread_func(void* thread_args) {
    ClientHandlerArgs* args = (ClientHandlerArgs*)thread_args;

    // Call the actual handler function
    handle_client_connection(args->client_socket, args->client_ip_str,
                             args->local_node_id, args->sync_dir_path, args->stop_flag);

    // Cleanup for this thread's resources
    // Socket is closed by handle_client_connection
    free(args); // Free the dynamically allocated args
    pthread_detach(pthread_self()); // Detach thread so resources are reclaimed on exit
    return NULL;
}


static void* tcp_server_listener_thread_func(void* thread_args) {
    TcpServerArgs* server_args = (TcpServerArgs*)thread_args;
    int server_sockfd;
    struct sockaddr_in6 server_addr; // Use sockaddr_in6 for IPv4/IPv6 compatibility
    struct sockaddr_in6 client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

#if defined(_WIN32) || defined(_WIN64)
    // Assuming Winsock is initialized by main or discovery
    server_sockfd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
    if (server_sockfd == INVALID_SOCKET) {
        Print("TCP Server: Failed to create socket. Error: %d\n", WSAGetLastError());
        return NULL;
    }
#else
    server_sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_sockfd < 0) {
        Print("TCP Server: Failed to create socket.\n");
        perror("tcp socket");
        return NULL;
    }
#endif

    int reuse_addr = 1;
#if defined(_WIN32) || defined(_WIN64)
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse_addr, sizeof(reuse_addr)) < 0) {
         Print("TCP Server: setsockopt(SO_REUSEADDR) failed. Error: %d\n", WSAGetLastError());
    }
    // For IPv6/IPv4 dual stack, IPV6_V6ONLY should be set to 0 (false)
    int no = 0;
    if (setsockopt(server_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&no, sizeof(no)) < 0) {
        Print("TCP Server: setsockopt(IPV6_V6ONLY) failed. Error: %d\n", WSAGetLastError());
        // This might not be critical on all systems, or if only IPv6 is intended.
    }
#else
    if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0) {
        perror("TCP Server setsockopt(SO_REUSEADDR)");
    }
    // For IPv6/IPv4 dual stack, IPV6_V6ONLY should be set to 0 (false)
    int no = 0;
    if (setsockopt(server_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&no, sizeof(no)) < 0) {
        perror("TCP Server setsockopt(IPV6_V6ONLY)");
        // Non-fatal on some systems or if only IPv6 is fine.
    }
#endif

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr = in6addr_any; // Listen on all interfaces (0.0.0.0 or ::)
    server_addr.sin6_port = htons(server_args->tcp_port);

#if defined(_WIN32) || defined(_WIN64)
    if (bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        Print("TCP Server: Failed to bind socket to port %u. Error: %d\n", server_args->tcp_port, WSAGetLastError());
        closesocket(server_sockfd);
        return NULL;
    }
    if (listen(server_sockfd, SOMAXCONN) == SOCKET_ERROR) {
        Print("TCP Server: Listen failed. Error: %d\n", WSAGetLastError());
        closesocket(server_sockfd);
        return NULL;
    }
#else
    if (bind(server_sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        Print("TCP Server: Failed to bind socket to port %u.\n", server_args->tcp_port);
        perror("tcp bind");
        close(server_sockfd);
        return NULL;
    }
    if (listen(server_sockfd, 10) < 0) { // 10 is a common backlog size
        Print("TCP Server: Listen failed.\n");
        perror("tcp listen");
        close(server_sockfd);
        return NULL;
    }
#endif

    Print("TCP Server (%s): Listening on port %u\n", server_args->local_node_id, server_args->tcp_port);

    // Set a timeout for accept() so the loop can check stop_flag
    // This is tricky with accept(). A common way is to use select/poll on the listening socket.
    fd_set read_fds;
    struct timeval timeout_select;

    while (!(*(server_args->stop_flag))) {
        FD_ZERO(&read_fds);
        FD_SET(server_sockfd, &read_fds);
        timeout_select.tv_sec = 1; // 1 second timeout for select
        timeout_select.tv_usec = 0;

        int activity = select(server_sockfd + 1, &read_fds, NULL, NULL, &timeout_select);

        if (activity < 0) {
            if (*(server_args->stop_flag)) break; // Stopping, select might be interrupted
            #if defined(_WIN32) || defined(_WIN64)
            Print("TCP Server: select() error: %d\n", WSAGetLastError());
            #else
            perror("TCP Server select()");
            #endif
            // Consider a short sleep before retrying select on error
            usleep(100000); // 100ms
            continue;
        }

        if (activity > 0 && FD_ISSET(server_sockfd, &read_fds)) { // New connection
            int client_sockfd;
#if defined(_WIN32) || defined(_WIN64)
            client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
            if (client_sockfd == INVALID_SOCKET) {
                if (!(*(server_args->stop_flag))) { // Don't log if we are stopping
                    Print("TCP Server: accept() failed. Error: %d\n", WSAGetLastError());
                }
                continue;
            }
#else
            client_sockfd = accept(server_sockfd, (struct sockaddr*)&client_addr, &client_addr_len);
            if (client_sockfd < 0) {
                if (!(*(server_args->stop_flag))) { // Don't log if we are stopping
                    perror("TCP Server accept()");
                }
                continue;
            }
#endif
            ClientHandlerArgs* handler_args = (ClientHandlerArgs*)malloc(sizeof(ClientHandlerArgs));
            if (!handler_args) {
                Print("TCP Server: Failed to allocate memory for client handler args.\n");
#if defined(_WIN32) || defined(_WIN64)
                closesocket(client_sockfd);
#else
                close(client_sockfd);
#endif
                continue;
            }

            // Get client IP string
            if (client_addr.sin6_family == AF_INET) { // IPv4 mapped to IPv6
                 struct sockaddr_in *v4Addr = (struct sockaddr_in *)&client_addr;
                 inet_ntop(AF_INET, &v4Addr->sin_addr, handler_args->client_ip_str, sizeof(handler_args->client_ip_str));
            } else { // IPv6
                 inet_ntop(AF_INET6, &client_addr.sin6_addr, handler_args->client_ip_str, sizeof(handler_args->client_ip_str));
            }

            handler_args->client_socket = client_sockfd;
            strncpy(handler_args->local_node_id, server_args->local_node_id, MAX_NODE_ID_LEN -1);
            handler_args->local_node_id[MAX_NODE_ID_LEN-1] = '\0';
            strncpy(handler_args->sync_dir_path, server_args->sync_dir_path, MaxPath -1);
            handler_args->sync_dir_path[MaxPath-1] = '\0';
            handler_args->stop_flag = server_args->stop_flag;

            pthread_t client_thread_id;
            if (pthread_create(&client_thread_id, NULL, client_handler_thread_func, (void*)handler_args) != 0) {
                Print("TCP Server: Failed to create client handler thread for %s.\n", handler_args->client_ip_str);
                perror("pthread_create client handler");
                free(handler_args);
#if defined(_WIN32) || defined(_WIN64)
                closesocket(client_sockfd);
#else
                close(client_sockfd);
#endif
            } else {
                // Optionally keep track of client_thread_id if robust joining is needed later
                // For now, they are detached in client_handler_thread_func.
                 Print("TCP Server: Dispatched handler for client %s on socket %d\n", handler_args->client_ip_str, client_sockfd);
            }
        }
        // If activity == 0, it's a timeout from select(), loop and check stop_flag.
    }

    Print("TCP Server (%s): Stopping listener thread...\n", server_args->local_node_id);
#if defined(_WIN32) || defined(_WIN64)
    closesocket(server_sockfd);
    // Global WSACleanup
#else
    close(server_sockfd);
#endif
    return NULL;
}


pthread_t start_tcp_server(TcpServerArgs* args) {
    pthread_t thread_id = 0;
    if (!args || !args->stop_flag) {
        Print("TCP Server: Invalid arguments for start_tcp_server.\n");
        return 0;
    }

    // Clear any tracked client threads (if we were tracking them for joining)
    client_thread_count = 0;

    if (pthread_create(&thread_id, NULL, tcp_server_listener_thread_func, (void*)args) != 0) {
        Print("TCP Server: Failed to create listener thread.\n");
        perror("pthread_create tcp server");
        return 0;
    }
    return thread_id;
}
