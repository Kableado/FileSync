#ifndef _ACTIONFILENODE_H_
#define _ACTIONFILENODE_H_

#include "filenode.h"

typedef enum EActionFileCmp {
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

typedef struct TActionFileNode TActionFileNode, *ActionFileNode;
struct TActionFileNode {
	ActionFileCmp action;
	FileNode left;
	FileNode right;
	ActionFileNode next;
};

ActionFileNode ActionFileNode_Create();
void AccionFileNode_Destroy(ActionFileNode actionFileNode);
ActionFileNode ActionFileNode_CreateNormal(FileNode fileNodeLeft,
	FileNode fileNodeRight);

void AccionFileNode_CompareChilds(
	ActionFileNode actionFileNodeRoot,
	ActionFileNode *actionFileNodeQueue,
	void(*CheckPair)(FileNode fileNodeLeft, FileNode fileNodeRight,
		ActionFileNode *actionFileNodeQueue));

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

int ActionFileNode_Statistics(ActionFileNode actionFileNode,
	ActionQueueStatistics *statistics);

void ActionFileNode_Print(ActionFileNode actionFileNode);

int ActionFileNode_RunList(ActionFileNode actionFileNode, char *pathLeft,
	char *pathRight);

// Common utilities

void AccionFileNode_DeletePair(FileNode fileNodeLeft, FileNode fileNodeRight,
	ActionFileNode *actionFileNodeQueue);

#endif
