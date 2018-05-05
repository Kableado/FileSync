#ifndef _FILENODE_H_
#define _FILENODE_H_

#include "fileutil.h"

#define FileNode_Filename "nodesFile.fs"

#define FileNode_Version 5

#define FileFlag_Root 1
#define FileFlag_Normal 2
#define FileFlag_Directory 4
#define FileFlag_HasSize 8
#define FileFlag_HasTime 16
#define FileFlag_HasCRC 32
#define FileFlag_PlaceHolder 512
#define FileFlag_MarkerForReview 1024

typedef enum EFileStatus {
	FileStatus_None,
	FileStatus_New,
	FileStatus_Modified,
	FileStatus_Deleted,
	FileStatus_Undeleted
} FileStatus;

typedef struct TFileNode TFileNode, *FileNode;
struct TFileNode {
	char name[MaxFilename];
	int flags;
	FileStatus status;

	long long size;
	unsigned long crc;
	FileTime fileTime;

	FileNode child;
	int childCount;

	FileNode next;
	FileNode parent;
};

FileNode FileNode_Create();
FileNode FileNode_Copy(FileNode fileNode);
void FileNode_Delete(FileNode fileNode);
void FileNode_AddChild(FileNode file, FileNode file2);
FileNode FileNode_GetRoot(FileNode fileNode);

char *FileNode_GetFullPath(FileNode fileNode, char *basePath, char *path);

void FileNode_LoadSize(FileNode fileNode, char *file);
void FileNode_LoadTime(FileNode fileNode, char *file);
void FileNode_LoadSizeAndTime(FileNode fileNode, char *file);
void FileNode_LoadCRC(FileNode fileNode, char *file);

void FileNode_Save(FileNode fileNode, char *fichero);
FileNode FileNode_Load(char *fichero);

void FileNode_PrintNode(FileNode fileNode);
void FileNode_Print(FileNode fileNode);

FileNode FileNode_Build(char *path);

FileNode FileNode_Refresh(FileNode file, char *path);

#endif
