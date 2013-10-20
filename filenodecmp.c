#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"
#include "filenodecmp.h"

#define QueueNode(queue,node) (queue)->next = node; (queue) = node;

AccionFileNode *_actionFileNodeFree = NULL;
int _actionFileNodeFreeCount = 0;
#define AccionFileNode_Tocho 1024
AccionFileNode *AccionFileNode_Create() {
	AccionFileNode *actionFileNode;

	if (_actionFileNodeFree == NULL ) {
		AccionFileNode *actionFileNodeFreeAux;
		int i;
		// Reservar un tocho
		actionFileNodeFreeAux = malloc(
				sizeof(AccionFileNode) * AccionFileNode_Tocho);
		for (i = 0; i < AccionFileNode_Tocho - 1; i++) {
			actionFileNodeFreeAux[i].next = &actionFileNodeFreeAux[i + 1];
		}
		actionFileNodeFreeAux[AccionFileNode_Tocho - 1].next = NULL;
		_actionFileNodeFree = &actionFileNodeFreeAux[0];
	}

	// Obtener el primero libre
	actionFileNode = _actionFileNodeFree;
	_actionFileNodeFree = actionFileNode->next;
	_actionFileNodeFreeCount++;

	// Iniciar
	actionFileNode->action = AccionFileCmp_Nothing;
	actionFileNode->left = NULL;
	actionFileNode->right = NULL;
	actionFileNode->next = NULL;

	return (actionFileNode);
}

void AccionFileNode_Destroy(AccionFileNode *actionFileNode) {
	actionFileNode->next = _actionFileNodeFree;
	_actionFileNodeFree = actionFileNode;
	_actionFileNodeFreeCount--;
}

AccionFileNode *AccionFileNode_CreateNormal(FileNode *fileNodeLeft,
		FileNode *fileNodeRight) {
	AccionFileNode *actionFileNode;
	actionFileNode = AccionFileNode_Create();
	actionFileNode->action = AccionFileCmp_Nothing;
	actionFileNode->left = fileNodeLeft;
	actionFileNode->right = fileNodeRight;
	return actionFileNode;
}

void AccionFileNode_CompareChilds(AccionFileNode *actionFileNodeRoot,
		AccionFileNode **actionFileNodeQueue,
		void (*CheckPair)(FileNode *fileNodeLeft, FileNode *fileNodeRight,
				AccionFileNode **actionFileNodeQueue)) {
	FileNode *fileNodeLeft, *fileNodeRight;
	AccionFileNode *actionFileNodeQueueStart = (*actionFileNodeQueue);

	// Comprobar si hay algo que comparar
	if (!actionFileNodeRoot->left && !actionFileNodeRoot->right) {
		// Nada que hacer
		return;
	}

	// Iterar todos los nodos de la izquierda
	if (actionFileNodeRoot->left) {
		fileNodeLeft = actionFileNodeRoot->left->child;
		while (fileNodeLeft) {
			if (actionFileNodeRoot->right) {
				fileNodeRight = actionFileNodeRoot->right->child;
				while (fileNodeRight) {
					if (!strcmp(fileNodeLeft->name, fileNodeRight->name)) {
						break;
					} else {
						fileNodeRight = fileNodeRight->next;
					}
				}
			} else {
				fileNodeRight = NULL;
			}

			CheckPair(fileNodeLeft, fileNodeRight, actionFileNodeQueue);

			fileNodeLeft = fileNodeLeft->next;
		}
	}

	// Iterar todos los nodos de la derecha,
	//   ignorando las comparaciones ya realizadas
	if (actionFileNodeRoot->right) {
		fileNodeRight = actionFileNodeRoot->right->child;
		while (fileNodeRight) {
			int doCheck = 1;
			if (actionFileNodeRoot->left) {
				fileNodeLeft = actionFileNodeRoot->left->child;
				while (fileNodeLeft) {
					AccionFileNode *afnCheck = actionFileNodeQueueStart;
					while (afnCheck) {
						if (afnCheck->left == fileNodeLeft
								&& afnCheck->right == fileNodeRight) {
							break;
						} else {
							afnCheck = afnCheck->next;
						}
					}
					if (afnCheck) {
						doCheck = 0;
						break;
					}

					if (!strcmp(fileNodeLeft->name, fileNodeRight->name)) {
						break;
					} else {
						fileNodeLeft = fileNodeLeft->next;
					}
				}
			} else {
				fileNodeLeft = NULL;
			}

			if (doCheck) {
				CheckPair(fileNodeLeft, fileNodeRight, actionFileNodeQueue);
			}

			fileNodeRight = fileNodeRight->next;
		}
	}

}

void AccionFileNode_DeletePair(FileNode *fileNodeLeft, FileNode *fileNodeRight,
		AccionFileNode **actionFileNodeQueue) {
	AccionFileNode *actionFileNodeNew = AccionFileNode_CreateNormal(
			fileNodeLeft, fileNodeRight);

	if (!fileNodeLeft && !fileNodeRight) {
		AccionFileNode_Destroy(actionFileNodeNew);
		return;
	}
	if (!fileNodeLeft && fileNodeRight) {
		if (fileNodeRight->flags & FileFlag_Directory) {
			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(actionFileNodeNew, actionFileNodeQueue,
					AccionFileNode_DeletePair);
		}

		if (fileNodeRight->estado != FileStatus_Deleted) {
			// Accion de borrado para el nodo
			actionFileNodeNew->action = AccionFileCmp_DeleteRight;
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		} else {
			AccionFileNode_Destroy(actionFileNodeNew);
		}
	}
	if (fileNodeLeft && !fileNodeRight) {
		if (fileNodeLeft->flags & FileFlag_Directory) {
			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(actionFileNodeNew, actionFileNodeQueue,
					AccionFileNode_DeletePair);
		}

		if (fileNodeLeft->estado != FileStatus_Deleted) {
			// Accion de borrado para el nodo
			actionFileNodeNew->action = AccionFileCmp_DeleteLeft;
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		} else {
			AccionFileNode_Destroy(actionFileNodeNew);
		}
	}
	if (fileNodeLeft && fileNodeRight) {
		if ((fileNodeLeft->flags & FileFlag_Directory)
				|| (fileNodeRight->flags & FileFlag_Directory)) {
			// Alguno es directorio

			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(actionFileNodeNew, actionFileNodeQueue,
					AccionFileNode_DeletePair);
		}

		if (fileNodeLeft->estado != FileStatus_Deleted) {
			// Accion de borrado para el nodo izquierdo
			actionFileNodeNew->action = AccionFileCmp_DeleteLeft;
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			actionFileNodeNew = NULL;
		}
		if (fileNodeRight->estado != FileStatus_Deleted) {
			if (!actionFileNodeNew) {
				actionFileNodeNew = AccionFileNode_CreateNormal(fileNodeLeft,
						fileNodeRight);
			}
			// Accion de borrado para el nodo derecho
			actionFileNodeNew->action = AccionFileCmp_DeleteRight;
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			actionFileNodeNew = NULL;
		}
		if (actionFileNodeNew) {
			AccionFileNode_Destroy(actionFileNodeNew);
		}
	}
}

void AccionFileNode_CheckPair(FileNode *fileNodeLeft, FileNode *fileNodeRight,
		AccionFileNode **actionFileNodeQueue) {
	AccionFileNode *actionFileNodeNew = AccionFileNode_CreateNormal(
			fileNodeLeft, fileNodeRight);

	if (!fileNodeLeft && !fileNodeRight) {
		AccionFileNode_Destroy(actionFileNodeNew);
		return;
	}
	if (!fileNodeLeft && fileNodeRight) {
		if (fileNodeRight->flags & FileFlag_Directory) {
			// Directory
			if (fileNodeRight->estado == FileStatus_Deleted) {
				actionFileNodeNew->action = AccionFileCmp_Nothing;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			} else {
				actionFileNodeNew->action = AccionFileCmp_MakeLeftDirectory;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);

				// Iterar hijos
				AccionFileNode_CompareChilds(actionFileNodeNew,
						actionFileNodeQueue, AccionFileNode_CheckPair);

				// Crear nueva accion para copiar la fecha
				actionFileNodeNew = AccionFileNode_CreateNormal(fileNodeLeft,
						fileNodeRight);
				actionFileNodeNew->action = AccionFileCmp_DateRightToLeft;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			}
		} else {
			// File
			if (fileNodeRight->estado == FileStatus_Deleted) {
				actionFileNodeNew->action = AccionFileCmp_Nothing;
			} else {
				actionFileNodeNew->action = AccionFileCmp_RightToLeft;
			}
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		}
	}
	if (fileNodeLeft && !fileNodeRight) {
		if (fileNodeLeft->flags & FileFlag_Directory) {
			// Directory
			if (fileNodeLeft->estado == FileStatus_Deleted) {
				actionFileNodeNew->action = AccionFileCmp_Nothing;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			} else {
				actionFileNodeNew->action = AccionFileCmp_MakeRightDirectory;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);

				// Iterar hijos
				AccionFileNode_CompareChilds(actionFileNodeNew,
						actionFileNodeQueue, AccionFileNode_CheckPair);

				// Crear nueva accion para copiar la fecha
				actionFileNodeNew = AccionFileNode_CreateNormal(fileNodeLeft,
						fileNodeRight);
				actionFileNodeNew->action = AccionFileCmp_DateLeftToRight;
				QueueNode(*actionFileNodeQueue, actionFileNodeNew);
			}
		} else {
			// File
			if (fileNodeLeft->estado == FileStatus_Deleted) {
				actionFileNodeNew->action = AccionFileCmp_Nothing;
			} else {
				actionFileNodeNew->action = AccionFileCmp_LeftToRight;
			}
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		}
	}
	if (fileNodeLeft && fileNodeRight) {
		if ((fileNodeLeft->flags & FileFlag_Directory)
				&& (fileNodeRight->flags & FileFlag_Directory)) {
			// Directorios

			// Preparar accion para el par de directorios
			if (abs(fileNodeLeft->fileTime - fileNodeRight->fileTime) <= 1) { // appoximadamente iguales
				if (fileNodeRight->estado == FileStatus_Deleted
						&& fileNodeLeft->estado == FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_Nothing;
				} else if (fileNodeRight->estado == FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_DeleteLeft;
					if (fileNodeLeft->estado == FileStatus_Deleted) {
						actionFileNodeNew->action = AccionFileCmp_Nothing;
					}
				} else if (fileNodeLeft->estado == FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_DeleteRight;
					if (fileNodeRight->estado == FileStatus_Deleted) {
						actionFileNodeNew->action = AccionFileCmp_Nothing;
					}
				} else {
					actionFileNodeNew->action = AccionFileCmp_Nothing;
				}
			} else if (fileNodeLeft->fileTime < fileNodeRight->fileTime) {
				actionFileNodeNew->action = AccionFileCmp_DateRightToLeft;
				if (fileNodeRight->estado == FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_DeleteLeft;
					if (fileNodeLeft->estado == FileStatus_Deleted) {
						actionFileNodeNew->action = AccionFileCmp_Nothing;
					}
				}
			} else if (fileNodeLeft->fileTime > fileNodeRight->fileTime) {
				actionFileNodeNew->action = AccionFileCmp_DateLeftToRight;
				if (fileNodeLeft->estado == FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_DeleteRight;
					if (fileNodeRight->estado == FileStatus_Deleted) {
						actionFileNodeNew->action = AccionFileCmp_Nothing;
					}
				}
			}

			// Procesar nodos hijos
			if (actionFileNodeNew->action == AccionFileCmp_DeleteRight
					|| actionFileNodeNew->action == AccionFileCmp_DeleteLeft
					|| (fileNodeLeft->estado == FileStatus_Deleted
							&& fileNodeRight->estado == FileStatus_Deleted)) {
				// Iterar nodos hijos para borrarlos
				AccionFileNode_CompareChilds(actionFileNodeNew,
						actionFileNodeQueue, AccionFileNode_DeletePair);
			} else {
				AccionFileNode_CompareChilds(actionFileNodeNew,
						actionFileNodeQueue, AccionFileNode_CheckPair);
			}
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		} else if ((fileNodeLeft->flags & FileFlag_Normal)
				&& (fileNodeRight->flags & FileFlag_Normal)) {
			// Ficheros

			// Preparar accion para el par de ficheros
			if (abs(fileNodeLeft->fileTime - fileNodeRight->fileTime) <= 1) { // appoximadamente iguales
				if (fileNodeRight->estado == FileStatus_Deleted
						&& fileNodeLeft->estado == FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_Nothing;
				} else if (fileNodeRight->estado == FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_DeleteLeft;
					if (fileNodeLeft->estado == FileStatus_Deleted) {
						actionFileNodeNew->action = AccionFileCmp_Nothing;
					}
				} else if (fileNodeLeft->estado == FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_DeleteRight;
					if (fileNodeRight->estado == FileStatus_Deleted) {
						actionFileNodeNew->action = AccionFileCmp_Nothing;
					}
				} else {
					actionFileNodeNew->action = AccionFileCmp_Nothing;
				}
			} else if (fileNodeLeft->fileTime < fileNodeRight->fileTime) {
				// FIXME: Check size to determine y further checks are necessary
				actionFileNodeNew->action = AccionFileCmp_RightToLeft;
				if (fileNodeRight->estado == FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_DeleteLeft;
					if (fileNodeLeft->estado == FileStatus_Deleted) {
						actionFileNodeNew->action = AccionFileCmp_Nothing;
					}
				}
			} else if (fileNodeLeft->fileTime > fileNodeRight->fileTime) {
				// FIXME: Check size to determine y further checks are necessary
				actionFileNodeNew->action = AccionFileCmp_LeftToRight;
				if (fileNodeLeft->estado == FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_DeleteRight;
					if (fileNodeRight->estado == FileStatus_Deleted) {
						actionFileNodeNew->action = AccionFileCmp_Nothing;
					}
				}
			}
			QueueNode(*actionFileNodeQueue, actionFileNodeNew);
		} else {
			// FIXME: !!!!!
			// Directory vs File

		}
	}
}

AccionFileNode *AccionFileNode_BuildSync(FileNode *izquierda, FileNode *derecha) {
	AccionFileNode *actionFileNodeRoot = AccionFileNode_CreateNormal(izquierda,
			derecha);
	AccionFileNode *actionFileNodeQueue = actionFileNodeRoot;
	AccionFileNode_CompareChilds(actionFileNodeRoot, &actionFileNodeQueue,
			AccionFileNode_CheckPair);
	return actionFileNodeRoot;
}

void AccionFileNode_Copy(FileNode *fileNodeLeft, FileNode *fileNodeRight,
		AccionFileNode **actionFileNodeQueue) {
	AccionFileNode *actionFileNodeNew = AccionFileNode_CreateNormal(
			fileNodeLeft, fileNodeRight);

	if (!fileNodeLeft && !fileNodeRight) {
		AccionFileNode_Destroy(actionFileNodeNew);
		return;
	}
	if (!fileNodeLeft && fileNodeRight) {
		if (fileNodeRight->flags & FileFlag_Directory) {
			AccionFileNode_CompareChilds(actionFileNodeNew, actionFileNodeQueue,
					AccionFileNode_DeletePair);
		}

		if (fileNodeRight->estado != FileStatus_Deleted) {
			actionFileNodeNew->action = AccionFileCmp_DeleteRight;
		} else {
			actionFileNodeNew->action = AccionFileCmp_Nothing;
		}
		QueueNode(*actionFileNodeQueue, actionFileNodeNew);
	}
	if (fileNodeLeft && !fileNodeRight) {
		if (fileNodeLeft->estado != FileStatus_Deleted) {
			if (fileNodeLeft->flags & FileFlag_Directory) {
				actionFileNodeNew->action = AccionFileCmp_MakeRightDirectory;
				(*actionFileNodeQueue)->next = actionFileNodeNew;
				(*actionFileNodeQueue) = actionFileNodeNew;
				AccionFileNode_CompareChilds(actionFileNodeNew,
						actionFileNodeQueue, AccionFileNode_Copy);
				actionFileNodeNew = AccionFileNode_CreateNormal(fileNodeLeft,
						fileNodeRight);
				actionFileNodeNew->action = AccionFileCmp_DateLeftToRight;
			} else {
				actionFileNodeNew->action = AccionFileCmp_LeftToRight;
			}
		} else {
			actionFileNodeNew->action = AccionFileCmp_Nothing;
		}
		QueueNode(*actionFileNodeQueue, actionFileNodeNew);
	}
	if (fileNodeLeft && fileNodeRight) {
		if ((fileNodeLeft->flags & FileFlag_Directory)
				|| (fileNodeRight->flags & FileFlag_Directory)) {
			if (fileNodeLeft->estado != FileStatus_Deleted) {
				AccionFileNode_CompareChilds(actionFileNodeNew,
						actionFileNodeQueue, AccionFileNode_Copy);
				if (abs(fileNodeLeft->fileTime - fileNodeRight->fileTime)
						<= 1) { // appox. equal
					actionFileNodeNew->action = AccionFileCmp_Nothing;
				} else {
					actionFileNodeNew->action = AccionFileCmp_DateLeftToRight;
				}
			} else {
				AccionFileNode_CompareChilds(actionFileNodeNew,
						actionFileNodeQueue, AccionFileNode_DeletePair);
				if (fileNodeRight->estado != FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_DeleteRight;
				}
			}
		} else {
			if (fileNodeLeft->estado != FileStatus_Deleted) {
				if (abs(fileNodeLeft->fileTime - fileNodeRight->fileTime)
						<= 1) { // appox. equal
					actionFileNodeNew->action = AccionFileCmp_Nothing;
				} else {
					actionFileNodeNew->action = AccionFileCmp_LeftToRight;
				}
			} else {
				if (fileNodeRight->estado != FileStatus_Deleted) {
					actionFileNodeNew->action = AccionFileCmp_DeleteRight;
				}
			}
		}
		QueueNode(*actionFileNodeQueue, actionFileNodeNew);
	}
}

AccionFileNode *AccionFileNode_BuildCopy(FileNode *fileNodeLeft,
		FileNode *fileNodeRight) {
	AccionFileNode *actionFileNodeRoot = AccionFileNode_CreateNormal(
			fileNodeLeft, fileNodeRight);
	AccionFileNode *actionFileNodeQueue = actionFileNodeRoot;
	AccionFileNode_CompareChilds(actionFileNodeRoot, &actionFileNodeQueue,
			AccionFileNode_Copy);
	return actionFileNodeRoot;
}

void AccionFileNode_Statistics(AccionFileNode *actionFileNode,
		ActionQueueStatistics *statistics) {
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

	while (actionFileNode != NULL ) {

		switch (actionFileNode->action) {
		case AccionFileCmp_Nothing:
			break;
		case AccionFileCmp_LeftToRight:
			statistics->fullCopyCount++;
			statistics->readLeft += actionFileNode->left->size;
			statistics->writeRight += actionFileNode->left->size;
			break;
		case AccionFileCmp_RightToLeft:
			statistics->fullCopyCount++;
			statistics->writeLeft += actionFileNode->right->size;
			statistics->readRight += actionFileNode->right->size;
			break;
		case AccionFileCmp_DeleteLeft:
			statistics->deleteCount++;
			statistics->deleteLeft += actionFileNode->left->size;
			break;
		case AccionFileCmp_DeleteRight:
			statistics->deleteCount++;
			statistics->deleteRight += actionFileNode->right->size;
			break;
		case AccionFileCmp_DateLeftToRight:
			statistics->dateCopyCount++;
			break;
		case AccionFileCmp_DateRightToLeft:
			statistics->dateCopyCount++;
			break;
		case AccionFileCmp_MakeRightDirectory:
			statistics->directoryCount++;
			break;
		case AccionFileCmp_MakeLeftDirectory:
			statistics->directoryCount++;
			break;
		}

		actionFileNode = actionFileNode->next;
	}
}

void AccionFileNode_Print(AccionFileNode *actionFileNode) {
	char showPath[MaxPath];
	while (actionFileNode != NULL ) {
		if (actionFileNode->left) {
			FileNode_GetFullPath(actionFileNode->left, "", showPath);
		} else {
			FileNode_GetFullPath(actionFileNode->right, "", showPath);
		}

		switch (actionFileNode->action) {
		case AccionFileCmp_Nothing:
			//printf("%s == %s\n",pathIzq,pathDer);
			break;
		case AccionFileCmp_LeftToRight:
			printf(" => %s\n", showPath);
			break;
		case AccionFileCmp_RightToLeft:
			printf(" <= %s\n", showPath);
			break;
		case AccionFileCmp_DeleteLeft:
			printf(" *- %s\n", showPath);
			break;
		case AccionFileCmp_DeleteRight:
			printf(" -* %s\n", showPath);
			break;
		case AccionFileCmp_DateLeftToRight:
			printf(" -> %s\n", showPath);
			break;
		case AccionFileCmp_DateRightToLeft:
			printf(" <- %s\n", showPath);
			break;
		case AccionFileCmp_MakeRightDirectory:
			printf(" -D %s\n", showPath);
			break;
		case AccionFileCmp_MakeLeftDirectory:
			printf(" D- %s\n", showPath);
			break;
		}

		actionFileNode = actionFileNode->next;
	}
	printf("End\n");
}

void AccionFileNodeAux_CopyDate(char *pathOrig, char *pathDest) {
	FileTime ft = FileTime_Get(pathOrig);
	FileTime_Set(pathDest, ft);
}

void AccionFileNodeAux_Copy(char *pathOrig, char *pathDest) {
	if (File_Copy(pathOrig, pathDest)) {
		AccionFileNodeAux_CopyDate(pathOrig, pathDest);
	} else {
		File_Delete(pathDest);
	}
}
void AccionFileNodeAux_Delete(char *pathOrig, char *pathDest) {
	if (File_IsDirectory(pathDest)) {
		File_DeleteDirectory(pathDest);
	} else {
		File_Delete(pathDest);
	}
}
void AccionFileNodeAux_MakeDir(char *pathOrig, char *pathDest) {
	File_MakeDirectory(pathDest);
}

void AccionFileNode_RunList(AccionFileNode *actionFileNode, char *pathLeft,
		char *pathRight) {
	char fullPathLeft[MaxPath], fullPathRight[MaxPath], showPath[MaxPath];
	while (actionFileNode != NULL ) {
		if (actionFileNode->left) {
			FileNode_GetFullPath(actionFileNode->left, pathLeft, fullPathLeft);
		} else {
			FileNode_GetFullPath(actionFileNode->right, pathLeft, fullPathLeft);
		}
		if (actionFileNode->right) {
			FileNode_GetFullPath(actionFileNode->right, pathRight,
					fullPathRight);
		} else {
			FileNode_GetFullPath(actionFileNode->left, pathRight,
					fullPathRight);
		}
		if (actionFileNode->left) {
			FileNode_GetFullPath(actionFileNode->left, "", showPath);
		} else {
			FileNode_GetFullPath(actionFileNode->right, "", showPath);
		}

		switch (actionFileNode->action) {
		case AccionFileCmp_Nothing:
			//printf("%s == %s\n",pathIzq,pathDer);
			break;
		case AccionFileCmp_LeftToRight:
			printf(" => %s\n", showPath);
			AccionFileNodeAux_Copy(fullPathLeft, fullPathRight);
			break;
		case AccionFileCmp_RightToLeft:
			printf(" <= %s\n", showPath);
			AccionFileNodeAux_Copy(fullPathRight, fullPathLeft);
			break;
		case AccionFileCmp_DeleteLeft:
			printf(" *- %s\n", showPath);
			AccionFileNodeAux_Delete(fullPathRight, fullPathLeft);
			break;
		case AccionFileCmp_DeleteRight:
			printf(" -* %s\n", showPath);
			AccionFileNodeAux_Delete(fullPathLeft, fullPathRight);
			break;
		case AccionFileCmp_DateLeftToRight:
			printf(" -> %s\n", showPath);
			AccionFileNodeAux_CopyDate(fullPathLeft, fullPathRight);
			break;
		case AccionFileCmp_DateRightToLeft:
			printf(" <- %s\n", showPath);
			AccionFileNodeAux_CopyDate(fullPathRight, fullPathLeft);
			break;
		case AccionFileCmp_MakeRightDirectory:
			printf(" -D %s\n", showPath);
			AccionFileNodeAux_MakeDir(fullPathLeft, fullPathRight);
			break;
		case AccionFileCmp_MakeLeftDirectory:
			printf(" D- %s\n", showPath);
			AccionFileNodeAux_MakeDir(fullPathRight, fullPathLeft);
			break;
		}

		actionFileNode = actionFileNode->next;
	}
	printf("End\n");
}
