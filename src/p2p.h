// SPDX-License-Identifier: MIT
// Copyright (c) 2023 Jules

#ifndef P2P_H
#define P2P_H

#include <stdbool.h>
#include <stddef.h> // For size_t

// Define a structure to hold node information
typedef struct {
    char ip_address[16]; // Max IP address length (e.g., "255.255.255.255")
    int port;
    // Add other relevant node information here if needed
} NodeInfo;

// Function to initialize P2P node discovery
bool P2P_InitNodeDiscovery(int port, int rescan_interval);

// Function to stop P2P node discovery
void P2P_ShutdownNodeDiscovery();

// Function to get the list of discovered nodes
// Returns the number of nodes found, and fills the provided array
int P2P_GetDiscoveredNodes(NodeInfo *nodes, int max_nodes);

// Function to load nodes from a file
int P2P_LoadNodesFromFile(const char *filepath, NodeInfo *nodes, int max_nodes);

// TCP Communication functions
// Function to initialize the TCP server component for listening to incoming P2P connections
bool P2P_InitTCPServer(int port, char **configured_dirs, int num_configured_dirs);

// Function to shutdown the TCP server component
void P2P_ShutdownTCPServer();

// Function to connect to a remote node via TCP
// Returns a socket descriptor or -1 on error
int P2P_ConnectToNode(const char *ip_address, int port);

// Function to send data to a connected node
// Returns true on success, false on failure
bool P2P_SendData(int socket_fd, const void *data, size_t length);

// Function to receive data from a connected node
// Returns bytes received, or -1 on error, 0 on orderly shutdown by peer
int P2P_ReceiveData(int socket_fd, void *buffer, size_t buffer_length);

// Function to close a TCP connection
void P2P_CloseConnection(int socket_fd);

// Structure for representing a shareable directory over P2P
typedef struct {
    char name[256]; // Max Filename length (adjust as needed from fileutil.h)
    char path[1024]; // Max Path length
    // We might also need volume name, fs type if that's relevant for P2P decisions
    // char volumeName[256];
    // char volumeFsType[256];
} P2PShareableDir;

// Function to get locally defined shareable directories based on a list of input paths.
// Input: dir_paths - an array of directory path strings.
//        num_dirs - the number of paths in dir_paths.
// Output: out_dirs - will be allocated and should be freed by the caller.
// Returns: count of shareable directories found from the input list, or -1 on critical error.
int P2P_GetShareableDirsFromConfig(char **dir_paths, int num_dirs, P2PShareableDir **out_dirs);

// Define P2P message types
#define P2P_MSG_HELLO "HELLO"
#define P2P_MSG_REQ_SHARE_LIST "REQ_SHARE_LIST"
#define P2P_MSG_SHARE_LIST_RESP "SHARE_LIST_RESP"
#define P2P_MSG_ERROR "P2P_ERROR"
// TODO: Add more specific messages for FileNode and File transfer
// Example:
// #define P2P_MSG_REQ_FILENODE "REQ_FILENODE"
// #define P2P_MSG_FILENODE_RESP "FILENODE_RESP"
// #define P2P_MSG_REQ_FILE_CHUNK "REQ_FILECHUNK"
// #define P2P_MSG_FILE_CHUNK_RESP "FILECHUNK_RESP"


#endif // P2P_H
