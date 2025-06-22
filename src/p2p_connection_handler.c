// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez (existing license)
// Copyright (c) 2023 The Gemini Agent authors (for new P2P code)

#include "p2p_connection_handler.h"
#include "util.h" // For Print()

#include <stdio.h>
#include <string.h>
#include <unistd.h> // For close()

#if defined(_WIN32) || defined(_WIN64)
#include <winsock2.h> // send, recv are here for Windows
#else
#include <sys/socket.h> // For send(), recv() on POSIX
#endif

// Global WSAStartup/Cleanup is managed elsewhere (e.g. main or discovery for Winsock)

void handle_client_connection(int client_socket,
                              const char* client_ip,
                              const char* local_node_id,
                              const char* sync_directory_path,
                              volatile int* stop_flag) {

    Print("TCP Server: Accepted connection from %s by %s. SyncDir: %s. Socket: %d\n",
          client_ip, local_node_id, sync_directory_path, client_socket);

    // TODO: Implement P2P file synchronization protocol here.
    //       - Handshake (optional)
    //       - Message loop: receive command, process, send response.
    //       - Use stop_flag to allow graceful shutdown.

    // Example: Send a welcome message
    char welcome_message[256];
    snprintf(welcome_message, sizeof(welcome_message), "Welcome from %s! Ready to sync %s.\r\n",
             local_node_id, sync_directory_path);

    send(client_socket, welcome_message, strlen(welcome_message), 0);

    // Placeholder: Simple loop to keep connection open for a bit or until stop_flag
    // In a real scenario, this would be a recv() loop waiting for protocol messages.
    // For now, just wait a short period or until stop_flag is set.
    // A select() or poll() on the socket with a timeout would be better here
    // to respond to data and the stop_flag.

    long T0 = Time_GetTime();
    while(!(*stop_flag) && (Time_GetTime() - T0 < 30000) /* 30 sec timeout for idle */ ) {
        // Check if there's data to read to simulate activity or client closing
        // This is a very basic way; select/poll is better.
        char dummy_buffer[64];
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        struct timeval timeout;
        timeout.tv_sec = 1; // Check every 1 second
        timeout.tv_usec = 0;

        int activity = select(client_socket + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0) {
            #if defined(_WIN32) || defined(_WIN64)
            Print("TCP Handler (%s): select() error %d\n", client_ip, WSAGetLastError());
            #else
            perror("TCP Handler select()");
            #endif
            break; // Error
        }

        if (activity > 0 && FD_ISSET(client_socket, &read_fds)) {
            // Data available or connection closed/reset
            int bytes_received = recv(client_socket, dummy_buffer, sizeof(dummy_buffer) -1, 0);
            if (bytes_received <= 0) {
                if (bytes_received == 0) {
                    Print("TCP Handler (%s): Client closed connection gracefully.\n", client_ip);
                } else {
                    #if defined(_WIN32) || defined(_WIN64)
                    Print("TCP Handler (%s): recv error %d. Client likely disconnected.\n", client_ip, WSAGetLastError());
                    #else
                    Print("TCP Handler (%s): recv error. Client likely disconnected.\n", client_ip);
                    perror("recv");
                    #endif
                }
                break; // Connection closed or error
            }
            // Dummy: just log that we got something.
            dummy_buffer[bytes_received] = '\0';
            //Print("TCP Handler (%s): Received dummy data '%s'\n", client_ip, dummy_buffer);
            // Real protocol would parse this. For now, we do nothing with it.
            T0 = Time_GetTime(); // Reset idle timer on activity
        }
        // If activity == 0, it's a timeout from select(), loop and check stop_flag.
    }

    if (*stop_flag) {
         Print("TCP Handler (%s): Stop flag received, closing connection.\n", client_ip);
    } else {
         Print("TCP Handler (%s): Idle timeout or client disconnected, closing connection.\n", client_ip);
    }


#if defined(_WIN32) || defined(_WIN64)
    closesocket(client_socket);
#else
    close(client_socket);
#endif
    Print("TCP Server: Closed connection to %s (Socket: %d)\n", client_ip, client_socket);
}
