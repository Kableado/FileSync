// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez (existing license)
// Copyright (c) 2023 The Gemini Agent authors (for new P2P code)

#ifndef P2P_TCP_SERVER_H
#define P2P_TCP_SERVER_H

#include <stdint.h>
#include <pthread.h> // For pthread_t

#include "p2p_discovery.h" // For MAX_NODE_ID_LEN (used in args)
#include "util.h" // For MaxPath


/**
 * @brief Arguments for the TCP server listener thread.
 */
typedef struct {
    uint16_t tcp_port;
    char local_node_id[MAX_NODE_ID_LEN];
    char sync_dir_path[MaxPath];
    volatile int* stop_flag; // Pointer to a global stop flag for graceful shutdown
} TcpServerArgs;

/**
 * @brief Starts the TCP server to listen for incoming P2P connections.
 *
 * The server runs its main listening loop in a separate thread.
 * For each accepted connection, it spawns another new thread to handle
 * communication with that specific client using `handle_client_connection`.
 *
 * @param args Pointer to TcpServerArgs struct containing necessary parameters.
 * @return pthread_t The ID of the created listener thread, or 0 on error.
 */
pthread_t start_tcp_server(TcpServerArgs* args);


#endif // P2P_TCP_SERVER_H
