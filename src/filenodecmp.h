#ifndef _FILENODECMP_H_
#define _FILENODECMP_H_

#include "filenode.h"

typedef enum {
	ActionFileCmp_Nothing,
	ActionFileCmp_LeftToRight,
	ActionFileCmp_RightToLeft,
	ActionFileCmp_DeleteLeft,
	ActionFileCmp_DeleteRight,
	ActionFileCmp_DateLeftToRight,
	ActionFileCmp_DateRightToLeft,
	ActionFileCmp_MakeRightDirectory,
	ActionFileCmp_MakeLeftDirectory
} ActionFileCmp;

typedef struct SActionFileNode {
	ActionFileCmp action;
	FileNode *left;
	FileNode *right;
	struct SActionFileNode *next;
} ActionFileNode;

ActionFileNode *ActionFileNode_Create();
void AccionFileNode_Destroy(ActionFileNode *actionFileNode);
ActionFileNode *ActionFileNode_CreateNormal(FileNode *fileNodeLeft,
	FileNode *fileNodeRight);

ActionFileNode *ActionFileNode_BuildSync(FileNode *fileNodeLeft,
	FileNode *fileNodeRight);
ActionFileNode *ActionFileNode_BuildCopy(FileNode *fileNodeLeft,
	FileNode *fileNodeRight);

typedef struct SActionQueueStatistics {
	long long readLeft;
	long long writeLeft;
	long long readRight;
	long long writeRight;
	int fullCopyCount;
	int dateCopyCount;
	int directoryCount;
	int deleteCount;
	long long deleteLeft;
	long long deleteRight;
} ActionQueueStatistics;

void ActionFileNode_Statistics(ActionFileNode *actionFileNode,
	ActionQueueStatistics *statistics);

void ActionFileNode_Print(ActionFileNode *actionFileNode);

void ActionFileNode_RunList(ActionFileNode *actionFileNode, char *pathLeft,
	char *pathRight);

#endif
