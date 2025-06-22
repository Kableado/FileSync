// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez (existing license)
// Copyright (c) 2023 The Gemini Agent authors (for new P2P code)

#ifndef P2P_NODELIST_H
#define P2P_NODELIST_H

#include <stdint.h>
#include <pthread.h> // For pthread_mutex_t

// Include p2p_discovery.h for PeerList and PeerInfo definitions
// This creates a dependency. A common p2p_types.h might be better in the long run.
#include "p2p_discovery.h" // Provides PeerList, PeerInfo, MAX_NODE_ID_LEN, MAX_IP_STR_LEN

/**
 * @brief Loads a list of known peers from a specified file.
 *
 * Each line in the file should contain an IP address or a hostname.
 * Optionally, a port can be specified using host:port format.
 * If no port is specified, default_tcp_port is used.
 * Resolved peers are added to the provided known_peers_list.
 *
 * @param filepath Path to the node list file.
 * @param default_tcp_port The default TCP port to use if not specified in the file.
 * @param known_peers_list Pointer to the PeerList to populate.
 * @param peer_list_mutex Mutex to protect access to the known_peers_list.
 * @return 0 on success or if file not found (not an error), -1 on critical error (e.g., memory allocation).
 */
int load_known_peers_from_file(const char* filepath,
                               uint16_t default_tcp_port,
                               PeerList* known_peers_list,
                               pthread_mutex_t* peer_list_mutex);

#endif // P2P_NODELIST_H
