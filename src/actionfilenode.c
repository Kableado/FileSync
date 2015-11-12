#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"
#include "actionfilenode.h"

ActionFileNode _actionFileNodeFree = NULL;
#define AccionFileNode_Block 1024
ActionFileNode ActionFileNode_Create() {
	ActionFileNode actionFileNode;

	if (_actionFileNodeFree == NULL) {
		ActionFileNode actionFileNodeFreeAux;
		int i;
		// Allocate block
		actionFileNodeFreeAux = malloc(
			sizeof(TActionFileNode) * AccionFileNode_Block);
		if (actionFileNodeFreeAux == NULL) {
			return NULL;
		}
		for (i = 0; i < AccionFileNode_Block - 1; i++) {
			actionFileNodeFreeAux[i].next = &actionFileNodeFreeAux[i + 1];
		}
		actionFileNodeFreeAux[AccionFileNode_Block - 1].next = NULL;
		_actionFileNodeFree = &actionFileNodeFreeAux[0];
	}

	// Get the first free
	actionFileNode = _actionFileNodeFree;
	_actionFileNodeFree = actionFileNode->next;

	// Initialize
	actionFileNode->action = ActionFileCmp_Nothing;
	actionFileNode->left = NULL;
	actionFileNode->right = NULL;
	actionFileNode->next = NULL;

	return (actionFileNode);
}

void AccionFileNode_Destroy(ActionFileNode actionFileNode) {
	actionFileNode->next = _actionFileNodeFree;
	_actionFileNodeFree = actionFileNode;
}

ActionFileNode ActionFileNode_CreateNormal(FileNode fileNodeLeft,
	FileNode fileNodeRight)
{
	ActionFileNode actionFileNode;
	actionFileNode = ActionFileNode_Create();
	actionFileNode->action = ActionFileCmp_Nothing;
	actionFileNode->left = fileNodeLeft;
	actionFileNode->right = fileNodeRight;
	return actionFileNode;
}


void AccionFileNode_CompareChilds(
	ActionFileNode actionFileNodeRoot,
	ActionFileNode *actionFileNodeQueue,
	void(*CheckPair)(FileNode fileNodeLeft, FileNode fileNodeRight,
		ActionFileNode *actionFileNodeQueue))
{
	FileNode fileNodeLeft;
	FileNode fileNodeRight;
	FileNode fileNodeRightQueue;
	FileNode fileNodeRightProcessed;
	FileNode fileNodeRightPrevious;

	// Check if there is something to do
	if (!actionFileNodeRoot->left && !actionFileNodeRoot->right) {
		return;
	}

	// There is no left part
	if (!actionFileNodeRoot->left) {
		fileNodeRight = actionFileNodeRoot->right->child;
		while (fileNodeRight) {

			CheckPair(NULL, fileNodeRight, actionFileNodeQueue);

			fileNodeRight = fileNodeRight->next;
		}
		return;
	}

	// There is no right part
	if (!actionFileNodeRoot->right) {
		fileNodeLeft = actionFileNodeRoot->left->child;
		while (fileNodeLeft) {

			CheckPair(fileNodeLeft, NULL, actionFileNodeQueue);

			fileNodeLeft = fileNodeLeft->next;
		}
		return;
	}

	// Prepare chains
	fileNodeRightQueue = actionFileNodeRoot->right->child;
	fileNodeRightProcessed = NULL;

	// Iterate left child FileNodes
	fileNodeLeft = actionFileNodeRoot->left->child;
	while (fileNodeLeft) {
		fileNodeRightPrevious = NULL;
		fileNodeRight = fileNodeRightQueue;
		while (fileNodeRight) {
			if (!strcmp(fileNodeLeft->name, fileNodeRight->name)) {
				// Match, extract right child FileNode to the processed chain
				if (fileNodeRightPrevious) {
					fileNodeRightPrevious->next = fileNodeRight->next;
				}
				else {
					fileNodeRightQueue = fileNodeRight->next;
				}
				fileNodeRight->next = fileNodeRightProcessed;
				fileNodeRightProcessed = fileNodeRight;

				CheckPair(fileNodeLeft, fileNodeRight, actionFileNodeQueue);
				break;
			}
			else {
				// Next right child
				fileNodeRightPrevious = fileNodeRight;
				fileNodeRight = fileNodeRight->next;
			}
		}
		if (!fileNodeRight) {
			CheckPair(fileNodeLeft, NULL, actionFileNodeQueue);
		}
		fileNodeLeft = fileNodeLeft->next;
	}

	// Iterate unprocessed right childs
	fileNodeRight = fileNodeRightQueue;
	while (fileNodeRight) {
		CheckPair(NULL, fileNodeRight, actionFileNodeQueue);
		fileNodeRightPrevious = fileNodeRight;
		fileNodeRight = fileNodeRight->next;
		fileNodeRightPrevious->next = fileNodeRightProcessed;
		fileNodeRightProcessed = fileNodeRightPrevious;
	}
	actionFileNodeRoot->right->child = fileNodeRightProcessed;

}

void ActionFileNode_Statistics(ActionFileNode actionFileNode,
	ActionQueueStatistics *statistics)
{
	statistics->readLeft = 0;
	statistics->writeLeft = 0;
	statistics->readRight = 0;
	statistics->writeRight = 0;
	statistics->fullCopyCount = 0;
	statistics->dateCopyCount = 0;
	statistics->directoryCount = 0;
	statistics->deleteCount = 0;
	statistics->deleteLeft = 0;
	statistics->deleteRight = 0;

	while (actionFileNode != NULL) {

		switch (actionFileNode->action) {
		case ActionFileCmp_Nothing:
			break;
		case ActionFileCmp_LeftToRight:
			statistics->fullCopyCount++;
			statistics->readLeft += actionFileNode->left->size;
			statistics->writeRight += actionFileNode->left->size;
			break;
		case ActionFileCmp_RightToLeft:
			statistics->fullCopyCount++;
			statistics->writeLeft += actionFileNode->right->size;
			statistics->readRight += actionFileNode->right->size;
			break;
		case ActionFileCmp_DeleteLeft:
			statistics->deleteCount++;
			statistics->deleteLeft += actionFileNode->left->size;
			break;
		case ActionFileCmp_DeleteRight:
			statistics->deleteCount++;
			statistics->deleteRight += actionFileNode->right->size;
			break;
		case ActionFileCmp_DateLeftToRight:
			statistics->dateCopyCount++;
			break;
		case ActionFileCmp_DateRightToLeft:
			statistics->dateCopyCount++;
			break;
		case ActionFileCmp_MakeRightDirectory:
			statistics->directoryCount++;
			break;
		case ActionFileCmp_MakeLeftDirectory:
			statistics->directoryCount++;
			break;
		}

		actionFileNode = actionFileNode->next;
	}
}

void ActionFileNode_Print(ActionFileNode actionFileNode) {
	char showPath[MaxPath];
	while (actionFileNode != NULL) {
		if (actionFileNode->left) {
			FileNode_GetFullPath(actionFileNode->left, "", showPath);
		}
		else {
			FileNode_GetFullPath(actionFileNode->right, "", showPath);
		}

		switch (actionFileNode->action) {
		case ActionFileCmp_Nothing:
			//printff("%s == %s\n",pathIzq,pathDer);
			break;
		case ActionFileCmp_LeftToRight:
			printff(" => %s\n", showPath);
			break;
		case ActionFileCmp_RightToLeft:
			printff(" <= %s\n", showPath);
			break;
		case ActionFileCmp_DeleteLeft:
			printff(" *- %s\n", showPath);
			break;
		case ActionFileCmp_DeleteRight:
			printff(" -* %s\n", showPath);
			break;
		case ActionFileCmp_DateLeftToRight:
			printff(" -> %s\n", showPath);
			break;
		case ActionFileCmp_DateRightToLeft:
			printff(" <- %s\n", showPath);
			break;
		case ActionFileCmp_MakeRightDirectory:
			printff(" -D %s\n", showPath);
			break;
		case ActionFileCmp_MakeLeftDirectory:
			printff(" D- %s\n", showPath);
			break;
		}

		actionFileNode = actionFileNode->next;
	}
	printff("End\n");
}

void AccionFileNodeAux_CopyDate(char *pathOrig, char *pathDest) {
	FileTime ft = FileTime_Get(pathOrig);
	FileTime_Set(pathDest, ft);
}

void AccionFileNodeAux_Copy(char *pathOrig, char *pathDest) {
	if (File_Copy(pathOrig, pathDest)) {
		AccionFileNodeAux_CopyDate(pathOrig, pathDest);
	}
	else {
		File_Delete(pathDest);
	}
}
void AccionFileNodeAux_Delete(char *pathOrig, char *pathDest) {
	if (File_IsDirectory(pathDest)) {
		File_DeleteDirectory(pathDest);
	}
	else {
		File_Delete(pathDest);
	}
}
void AccionFileNodeAux_MakeDir(char *pathOrig, char *pathDest) {
	File_MakeDirectory(pathDest);
}

void ActionFileNode_RunList(ActionFileNode actionFileNode, char *pathLeft,
	char *pathRight)
{
	char fullPathLeft[MaxPath], fullPathRight[MaxPath], showPath[MaxPath];
	while (actionFileNode != NULL) {
		if (actionFileNode->left) {
			FileNode_GetFullPath(actionFileNode->left, pathLeft, fullPathLeft);
			FileNode_GetFullPath(actionFileNode->left, "", showPath);
		}
		else {
			FileNode_GetFullPath(actionFileNode->right, pathLeft, fullPathLeft);
			FileNode_GetFullPath(actionFileNode->right, "", showPath);
		}
		if (actionFileNode->right) {
			FileNode_GetFullPath(actionFileNode->right, pathRight,
				fullPathRight);
		}
		else {
			FileNode_GetFullPath(actionFileNode->left, pathRight,
				fullPathRight);
		}

		switch (actionFileNode->action) {
		case ActionFileCmp_Nothing:
			//printff("%s == %s\n",pathIzq,pathDer);
			break;
		case ActionFileCmp_LeftToRight:
			printff(" => %s\n", showPath);
			AccionFileNodeAux_Copy(fullPathLeft, fullPathRight);
			break;
		case ActionFileCmp_RightToLeft:
			printff(" <= %s\n", showPath);
			AccionFileNodeAux_Copy(fullPathRight, fullPathLeft);
			break;
		case ActionFileCmp_DeleteLeft:
			printff(" *- %s\n", showPath);
			AccionFileNodeAux_Delete(fullPathRight, fullPathLeft);
			break;
		case ActionFileCmp_DeleteRight:
			printff(" -* %s\n", showPath);
			AccionFileNodeAux_Delete(fullPathLeft, fullPathRight);
			break;
		case ActionFileCmp_DateLeftToRight:
			printff(" -> %s\n", showPath);
			AccionFileNodeAux_CopyDate(fullPathLeft, fullPathRight);
			break;
		case ActionFileCmp_DateRightToLeft:
			printff(" <- %s\n", showPath);
			AccionFileNodeAux_CopyDate(fullPathRight, fullPathLeft);
			break;
		case ActionFileCmp_MakeRightDirectory:
			printff(" -D %s\n", showPath);
			AccionFileNodeAux_MakeDir(fullPathLeft, fullPathRight);
			break;
		case ActionFileCmp_MakeLeftDirectory:
			printff(" D- %s\n", showPath);
			AccionFileNodeAux_MakeDir(fullPathRight, fullPathLeft);
			break;
		}

		actionFileNode = actionFileNode->next;
	}
	printff("End\n");
}


// ----------------------------------------------------------------------------
// Common utilities

#define QueueNode(queue,node) (queue)->next = (node); (queue) = (node);

void AccionFileNode_DeletePair(FileNode fileNodeLeft, FileNode fileNodeRight,
	ActionFileNode *actionFileNodeQueue)
{
	ActionFileNode actionFileNodeNew = ActionFileNode_CreateNormal(
		fileNodeLeft, fileNodeRight);

	if (!fileNodeLeft && !fileNodeRight) {
		AccionFileNode_Destroy(actionFileNodeNew);
		return;
	}
	if (!fileNodeLeft && fileNodeRight) {
		if (fileNodeRight->flags & FileFlag_Directory) {
			// Iterate childs for deletion
			AccionFileNode_CompareChilds(actionFileNodeNew, actionFileNodeQueue,
				AccionFileNode_DeletePair);
		}

		if (fileNodeRight->status != FileStatus_Deleted) {
			// Node delete action
			actionFileNodeNew->action = ActionFileCmp_DeleteRight;
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		}
		else {
			AccionFileNode_Destroy(actionFileNodeNew);
		}
	}
	if (fileNodeLeft && !fileNodeRight) {
		if (fileNodeLeft->flags & FileFlag_Directory) {
			// Iterate childs for deletion
			AccionFileNode_CompareChilds(actionFileNodeNew, actionFileNodeQueue,
				AccionFileNode_DeletePair);
		}

		if (fileNodeLeft->status != FileStatus_Deleted) {
			// Node delete action
			actionFileNodeNew->action = ActionFileCmp_DeleteLeft;
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		}
		else {
			AccionFileNode_Destroy(actionFileNodeNew);
		}
	}
	if (fileNodeLeft && fileNodeRight) {
		if ((fileNodeLeft->flags & FileFlag_Directory)
			|| (fileNodeRight->flags & FileFlag_Directory)) {
			// One is Directory

			// Iterate childs for deletion
			AccionFileNode_CompareChilds(actionFileNodeNew, actionFileNodeQueue,
				AccionFileNode_DeletePair);
		}

		if (fileNodeLeft->status != FileStatus_Deleted) {
			// Left node delete action
			actionFileNodeNew->action = ActionFileCmp_DeleteLeft;
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			actionFileNodeNew = NULL;
		}
		if (fileNodeRight->status != FileStatus_Deleted) {
			if (!actionFileNodeNew) {
				actionFileNodeNew = ActionFileNode_CreateNormal(fileNodeLeft,
					fileNodeRight);
			}
			// Right node delete action
			actionFileNodeNew->action = ActionFileCmp_DeleteRight;
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			actionFileNodeNew = NULL;
		}
		if (actionFileNodeNew) {
			AccionFileNode_Destroy(actionFileNodeNew);
		}
	}
}