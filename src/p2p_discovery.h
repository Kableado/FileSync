// SPDX-License-Identifier: MIT
// Copyright (c) 2014-2021 Valeriano Alfonso Rodriguez (existing license)
// Copyright (c) 2023 The Gemini Agent authors (for new P2P code)

#ifndef P2P_DISCOVERY_H
#define P2P_DISCOVERY_H

#include <stdint.h> // For uint16_t

// Max size for an announcement message.
// Example: {"node_id":"myhostname_or_custom_id","tcp_port":4856,"type":"announce"}
// Plus some buffer. Node ID could be up to MaxFilename (256 from util.h, but let's be generous for FQDNs)
#define MAX_ANNOUNCEMENT_MSG_SIZE 512
#define MAX_NODE_ID_LEN 256 // Should be consistent with util.h's MaxFilename or larger
#define MAX_IP_STR_LEN 46 // Sufficient for IPv6

// Forward declaration for pthread_mutex_t if needed, or include pthread.h
#include <pthread.h> // For pthread_mutex_t

/**
 * @brief Structure to hold information about a discovered peer.
 */
typedef struct {
    char ip_address[MAX_IP_STR_LEN];
    uint16_t tcp_port;
    char node_id[MAX_NODE_ID_LEN];
    time_t last_seen; // Timestamp of the last announcement received
} PeerInfo;

/**
 * @brief Structure to manage a list of discovered peers.
 */
typedef struct {
    PeerInfo* peers;
    int count;
    int capacity;
    // pthread_mutex_t* mutex; // Mutex should be managed by the caller that owns the list
} PeerList;

// Functions to manage PeerList (can be in a separate peer_list.c if it grows)
void PeerList_Init(PeerList* list);
void PeerList_AddOrUpdate(PeerList* list, const PeerInfo* new_peer, pthread_mutex_t* mutex);
void PeerList_Cleanup(PeerList* list);
// void PeerList_RemoveStale(PeerList* list, time_t stale_threshold_seconds, pthread_mutex_t* mutex); // Optional for later


/**
 * @brief Sends a UDP broadcast message to announce presence on the network.
 *
 * @param udp_port The UDP port to broadcast to (e.g., 4857).
 * @param tcp_port The TCP port this node is listening on for data (e.g., 4856).
 * @param node_id A unique identifier for this node (e.g., hostname).
 *                If NULL or empty, a default will be attempted (hostname).
 * @return 0 on success, -1 on error.
 */
int send_udp_announcement(uint16_t udp_port, uint16_t tcp_port, const char* node_id);


/**
 * @brief Arguments for the UDP listener thread.
 */
typedef struct {
    uint16_t udp_port;
    uint16_t own_tcp_port; // To ignore self-announcements
    char own_node_id[MAX_NODE_ID_LEN]; // To ignore self-announcements
    PeerList* discovered_peers_list;
    pthread_mutex_t* peer_list_mutex;
    volatile int* stop_flag; // Pointer to a flag to signal thread termination
} UdpListenerArgs;


/**
 * @brief Starts the UDP listener thread to discover other peers.
 *
 * The listener runs in a separate thread and calls the callback when a peer is found.
 *
 * @param args Pointer to UdpListenerArgs struct containing necessary parameters.
 * @return pthread_t The ID of the created listener thread, or 0 on error.
 */
pthread_t start_udp_listener(UdpListenerArgs* args);


#endif // P2P_DISCOVERY_H
