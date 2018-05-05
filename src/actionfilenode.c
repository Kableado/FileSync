#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "actionfilenode.h"
#include "crc.h"
#include "filenode.h"
#include "fileutil.h"
#include "util.h"

ActionFileNode _actionFileNodeFree = NULL;
#define AccionFileNode_Block 1024
ActionFileNode ActionFileNode_Create() {
	ActionFileNode actionFileNode;

	if (_actionFileNodeFree == NULL) {
		ActionFileNode actionFileNodeFreeAux;
		int i;
		// Allocate block
		actionFileNodeFreeAux =
			malloc(sizeof(TActionFileNode) * AccionFileNode_Block);
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
	actionFileNode->result = ActionFileNodeResult_Nothing;
	actionFileNode->next = NULL;

	return (actionFileNode);
}

void AccionFileNode_Destroy(ActionFileNode actionFileNode) {
	actionFileNode->next = _actionFileNodeFree;
	_actionFileNodeFree = actionFileNode;
}

ActionFileNode ActionFileNode_CreateNormal(FileNode fileNodeLeft,
										   FileNode fileNodeRight) {
	ActionFileNode actionFileNode;
	actionFileNode = ActionFileNode_Create();
	actionFileNode->action = ActionFileCmp_Nothing;
	actionFileNode->left = fileNodeLeft;
	actionFileNode->right = fileNodeRight;
	return actionFileNode;
}

void AccionFileNode_CompareChilds(
	ActionFileNode actionFileNodeRoot, ActionFileNode *actionFileNodeQueue,
	void (*CheckPair)(FileNode fileNodeLeft, FileNode fileNodeRight,
					  ActionFileNode *actionFileNodeQueue)) {
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
				} else {
					fileNodeRightQueue = fileNodeRight->next;
				}
				fileNodeRight->next = fileNodeRightProcessed;
				fileNodeRightProcessed = fileNodeRight;

				CheckPair(fileNodeLeft, fileNodeRight, actionFileNodeQueue);
				break;
			} else {
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

int ActionFileNode_Statistics(ActionFileNode actionFileNode,
							  ActionQueueStatistics *statistics,
							  ActionFileNodeResult result) {
	statistics->readLeft = 0;
	statistics->writeLeft = 0;
	statistics->readRight = 0;
	statistics->writeRight = 0;
	statistics->deleteLeft = 0;
	statistics->deleteRight = 0;

	statistics->fullCopyCount = 0;
	statistics->dateCopyCount = 0;
	statistics->directoryCount = 0;
	statistics->deleteCount = 0;

	while (actionFileNode != NULL) {
		if (actionFileNode->result != result) {
			actionFileNode = actionFileNode->next;
			continue;
		}

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
	return (statistics->fullCopyCount + statistics->dateCopyCount +
			statistics->directoryCount + statistics->deleteCount);
}

void ActionFileNode_Print(ActionFileNode actionFileNode) {
	char showPath[MaxPath];
	while (actionFileNode != NULL) {
		if (actionFileNode->left) {
			FileNode_GetFullPath(actionFileNode->left, "", showPath);
		} else {
			FileNode_GetFullPath(actionFileNode->right, "", showPath);
		}

		switch (actionFileNode->action) {
		case ActionFileCmp_Nothing:
			// printff("%s == %s\n",pathIzq,pathDer);
			break;
		case ActionFileCmp_LeftToRight:
			Print(" => %s\n", showPath);
			break;
		case ActionFileCmp_RightToLeft:
			Print(" <= %s\n", showPath);
			break;
		case ActionFileCmp_DeleteLeft:
			Print(" *- %s\n", showPath);
			break;
		case ActionFileCmp_DeleteRight:
			Print(" -* %s\n", showPath);
			break;
		case ActionFileCmp_DateLeftToRight:
			Print(" -> %s\n", showPath);
			break;
		case ActionFileCmp_DateRightToLeft:
			Print(" <- %s\n", showPath);
			break;
		case ActionFileCmp_MakeRightDirectory:
			Print(" -D %s\n", showPath);
			break;
		case ActionFileCmp_MakeLeftDirectory:
			Print(" D- %s\n", showPath);
			break;
		}

		actionFileNode = actionFileNode->next;
	}
	Print("End\n");
}

bool AccionFileNodeAux_CopyDate(char *pathOrig, char *pathDest) {
	FileTime ft = FileTime_Get(pathOrig);
	FileTime_Set(pathDest, ft);
	return true;
}

bool AccionFileNodeAux_Copy(char *pathOrig, char *pathDest) {
	if (File_Copy(pathOrig, pathDest)) {
		return AccionFileNodeAux_CopyDate(pathOrig, pathDest);
	} else {
		File_Delete(pathDest);
		Print("Error Copying to: %s, %s\n", pathDest, GetError());
		return false;
	}
}
bool AccionFileNodeAux_Delete(char *pathOrig, char *pathDest) {
	if (File_IsDirectory(pathDest)) {
		if (File_DeleteDirectory(pathDest) == 0) {
			Print("Error Deleting Directory: %s, %s\n", pathDest, GetError());
			return false;
		}
	} else {
		if (File_Delete(pathDest) == 0) {
			Print("Error Deleting File: %s, %s\n", pathDest, GetError());
			return false;
		}
	}
	return true;
}
bool AccionFileNodeAux_MakeDir(char *pathOrig, char *pathDest) {
	if (File_MakeDirectory(pathDest) == 0) {
		Print("Error Making Directory: %s, %s\n", pathDest, GetError());
		return false;
	}
}

int ActionFileNode_RunList(ActionFileNode actionFileNode, char *pathLeft,
						   char *pathRight) {
	int numActions = 0;
	char fullPathLeft[MaxPath], fullPathRight[MaxPath], showPath[MaxPath];
	while (actionFileNode != NULL) {
		if (actionFileNode->left) {
			FileNode_GetFullPath(actionFileNode->left, pathLeft, fullPathLeft);
			FileNode_GetFullPath(actionFileNode->left, "", showPath);
		} else {
			FileNode_GetFullPath(actionFileNode->right, pathLeft, fullPathLeft);
			FileNode_GetFullPath(actionFileNode->right, "", showPath);
		}
		if (actionFileNode->right) {
			FileNode_GetFullPath(actionFileNode->right, pathRight,
								 fullPathRight);
		} else {
			FileNode_GetFullPath(actionFileNode->left, pathRight,
								 fullPathRight);
		}

		switch (actionFileNode->action) {
		case ActionFileCmp_Nothing:
			// printff("%s == %s\n",pathIzq,pathDer);
			break;
		case ActionFileCmp_LeftToRight:
			Print(" => %s\n", showPath);
			if (AccionFileNodeAux_Copy(fullPathLeft, fullPathRight)) {
				actionFileNode->result = ActionFileNodeResult_Ok;
			} else {
				actionFileNode->result = ActionFileNodeResult_Error;
			}
			numActions++;
			break;
		case ActionFileCmp_RightToLeft:
			Print(" <= %s\n", showPath);
			if (AccionFileNodeAux_Copy(fullPathRight, fullPathLeft)) {
				actionFileNode->result = ActionFileNodeResult_Ok;
			} else {
				actionFileNode->result = ActionFileNodeResult_Error;
			}
			numActions++;
			break;
		case ActionFileCmp_DeleteLeft:
			Print(" *- %s\n", showPath);
			if (AccionFileNodeAux_Delete(fullPathRight, fullPathLeft)) {
				actionFileNode->result = ActionFileNodeResult_Ok;
			} else {
				actionFileNode->result = ActionFileNodeResult_Error;
			}
			numActions++;
			break;
		case ActionFileCmp_DeleteRight:
			Print(" -* %s\n", showPath);
			if (AccionFileNodeAux_Delete(fullPathLeft, fullPathRight)) {
				actionFileNode->result = ActionFileNodeResult_Ok;
			} else {
				actionFileNode->result = ActionFileNodeResult_Error;
			}
			numActions++;
			break;
		case ActionFileCmp_DateLeftToRight:
			Print(" -> %s\n", showPath);
			if (AccionFileNodeAux_CopyDate(fullPathLeft, fullPathRight)) {
				actionFileNode->result = ActionFileNodeResult_Ok;
			} else {
				actionFileNode->result = ActionFileNodeResult_Error;
			}
			numActions++;
			break;
		case ActionFileCmp_DateRightToLeft:
			Print(" <- %s\n", showPath);
			if (AccionFileNodeAux_CopyDate(fullPathRight, fullPathLeft)) {
				actionFileNode->result = ActionFileNodeResult_Ok;
			} else {
				actionFileNode->result = ActionFileNodeResult_Error;
			}
			numActions++;
			break;
		case ActionFileCmp_MakeRightDirectory:
			Print(" -D %s\n", showPath);
			if (AccionFileNodeAux_MakeDir(fullPathLeft, fullPathRight)) {
				actionFileNode->result = ActionFileNodeResult_Ok;
			} else {
				actionFileNode->result = ActionFileNodeResult_Error;
			}
			numActions++;
			break;
		case ActionFileCmp_MakeLeftDirectory:
			Print(" D- %s\n", showPath);
			if (AccionFileNodeAux_MakeDir(fullPathRight, fullPathLeft)) {
				actionFileNode->result = ActionFileNodeResult_Ok;
			} else {
				actionFileNode->result = ActionFileNodeResult_Error;
			}
			numActions++;
			break;
		}

		actionFileNode = actionFileNode->next;
	}
	Print("End\n");
	return numActions;
}

// ----------------------------------------------------------------------------
// Common utilities

#define QueueNode(queue, node)                                                 \
	(queue)->next = (node);                                                    \
	(queue) = (node);

void AccionFileNode_DeletePair(FileNode fileNodeLeft, FileNode fileNodeRight,
							   ActionFileNode *actionFileNodeQueue) {
	ActionFileNode actionFileNodeNew =
		ActionFileNode_CreateNormal(fileNodeLeft, fileNodeRight);

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
		} else {
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
		} else {
			AccionFileNode_Destroy(actionFileNodeNew);
		}
	}
	if (fileNodeLeft && fileNodeRight) {
		if ((fileNodeLeft->flags & FileFlag_Directory) ||
			(fileNodeRight->flags & FileFlag_Directory)) {
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
				actionFileNodeNew =
					ActionFileNode_CreateNormal(fileNodeLeft, fileNodeRight);
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