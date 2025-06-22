// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez (existing license)
// Copyright (c) 2023 The Gemini Agent authors (for new P2P code)

#ifndef P2P_CONNECTION_HANDLER_H
#define P2P_CONNECTION_HANDLER_H

// Potentially include p2p_types.h or other common headers if needed in the future.
// For now, it's self-contained or uses basic types.

/**
 * @brief Handles an individual client connection on the server side.
 *
 * This function is intended to be run in a separate thread for each client.
 * It will manage the P2P file synchronization protocol with the connected client.
 *
 * @param client_socket The socket descriptor for the connected client.
 * @param client_ip A string representation of the client's IP address.
 * @param local_node_id The node ID of this server instance.
 * @param sync_directory_path The path to the directory being synchronized.
 * @param stop_flag A pointer to a volatile integer flag. If this flag becomes non-zero,
 *                  the handler should attempt to gracefully terminate.
 */
void handle_client_connection(int client_socket,
                              const char* client_ip,
                              const char* local_node_id,
                              const char* sync_directory_path,
                              volatile int* stop_flag);

#endif // P2P_CONNECTION_HANDLER_H
