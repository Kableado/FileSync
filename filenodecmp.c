#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"
#include "filenodecmp.h"

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
	AccionFileNode *afnNew = AccionFileNode_CreateNormal(fileNodeLeft,
			fileNodeRight);

	if (!fileNodeLeft && !fileNodeRight) {
		AccionFileNode_Destroy(afnNew);
		return;
	}
	if (!fileNodeLeft && fileNodeRight) {
		if (fileNodeRight->flags & FileFlag_Directory) {
			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(afnNew, actionFileNodeQueue,
					AccionFileNode_DeletePair);
		}

		if (fileNodeRight->estado != FileStatus_Deleted) {
			// Accion de borrado para el nodo
			afnNew->action = AccionFileCmp_DeleteRight;
			(*actionFileNodeQueue)->next = afnNew;
			(*actionFileNodeQueue) = afnNew;
		} else {
			AccionFileNode_Destroy(afnNew);
		}
	}
	if (fileNodeLeft && !fileNodeRight) {
		if (fileNodeLeft->flags & FileFlag_Directory) {
			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(afnNew, actionFileNodeQueue,
					AccionFileNode_DeletePair);
		}

		if (fileNodeLeft->estado != FileStatus_Deleted) {
			// Accion de borrado para el nodo
			afnNew->action = AccionFileCmp_DeleteLeft;
			(*actionFileNodeQueue)->next = afnNew;
			(*actionFileNodeQueue) = afnNew;
		} else {
			AccionFileNode_Destroy(afnNew);
		}
	}
	if (fileNodeLeft && fileNodeRight) {
		if ((fileNodeLeft->flags & FileFlag_Directory)
				|| (fileNodeRight->flags & FileFlag_Directory)) {
			// Alguno es directorio

			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(afnNew, actionFileNodeQueue,
					AccionFileNode_DeletePair);
		}

		if (fileNodeLeft->estado != FileStatus_Deleted) {
			// Accion de borrado para el nodo izquierdo
			afnNew->action = AccionFileCmp_DeleteLeft;
			(*actionFileNodeQueue)->next = afnNew;
			(*actionFileNodeQueue) = afnNew;
			afnNew = NULL;
		}
		if (fileNodeRight->estado != FileStatus_Deleted) {
			if (!afnNew) {
				afnNew = AccionFileNode_CreateNormal(fileNodeLeft,
						fileNodeRight);
			}
			// Accion de borrado para el nodo derecho
			afnNew->action = AccionFileCmp_DeleteRight;
			(*actionFileNodeQueue)->next = afnNew;
			(*actionFileNodeQueue) = afnNew;
			afnNew = NULL;
		}
		if (afnNew) {
			AccionFileNode_Destroy(afnNew);
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

				// Anhadir a la lista de acciones
				(*actionFileNodeQueue)->next = actionFileNodeNew;
				(*actionFileNodeQueue) = actionFileNodeNew;

			} else {
				actionFileNodeNew->action = AccionFileCmp_MakeLeftDirectory;

				// Anhadir a la lista de acciones
				(*actionFileNodeQueue)->next = actionFileNodeNew;
				(*actionFileNodeQueue) = actionFileNodeNew;

				// Iterar hijos
				AccionFileNode_CompareChilds(actionFileNodeNew,
						actionFileNodeQueue, AccionFileNode_CheckPair);

				// Crear nueva accion para copiar la fecha
				actionFileNodeNew = AccionFileNode_CreateNormal(fileNodeLeft,
						fileNodeRight);
				actionFileNodeNew->action = AccionFileCmp_DateRightToLeft;
				(*actionFileNodeQueue)->next = actionFileNodeNew;
				(*actionFileNodeQueue) = actionFileNodeNew;
			}
		} else {
			// File
			if (fileNodeRight->estado == FileStatus_Deleted) {
				actionFileNodeNew->action = AccionFileCmp_Nothing;
			} else {
				actionFileNodeNew->action = AccionFileCmp_RightToLeft;
			}
			(*actionFileNodeQueue)->next = actionFileNodeNew;
			(*actionFileNodeQueue) = actionFileNodeNew;
		}
	}
	if (fileNodeLeft && !fileNodeRight) {
		if (fileNodeLeft->flags & FileFlag_Directory) {
			// Directory
			if (fileNodeLeft->estado == FileStatus_Deleted) {
				actionFileNodeNew->action = AccionFileCmp_Nothing;

				// Anhadir a la lista de acciones
				(*actionFileNodeQueue)->next = actionFileNodeNew;
				(*actionFileNodeQueue) = actionFileNodeNew;

			} else {
				actionFileNodeNew->action = AccionFileCmp_MakeRightDirectory;

				// Anhadir a la lista de acciones
				(*actionFileNodeQueue)->next = actionFileNodeNew;
				(*actionFileNodeQueue) = actionFileNodeNew;

				// Iterar hijos
				AccionFileNode_CompareChilds(actionFileNodeNew,
						actionFileNodeQueue, AccionFileNode_CheckPair);

				// Crear nueva accion para copiar la fecha
				actionFileNodeNew = AccionFileNode_CreateNormal(fileNodeLeft,
						fileNodeRight);
				actionFileNodeNew->action = AccionFileCmp_DateLeftToRight;
				(*actionFileNodeQueue)->next = actionFileNodeNew;
				(*actionFileNodeQueue) = actionFileNodeNew;
			}
		} else {
			// File
			if (fileNodeLeft->estado == FileStatus_Deleted) {
				actionFileNodeNew->action = AccionFileCmp_Nothing;
			} else {
				actionFileNodeNew->action = AccionFileCmp_LeftToRight;
			}
			(*actionFileNodeQueue)->next = actionFileNodeNew;
			(*actionFileNodeQueue) = actionFileNodeNew;
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

			// Encolar accion para el directorio padre
			(*actionFileNodeQueue)->next = actionFileNodeNew;
			(*actionFileNodeQueue) = actionFileNodeNew;

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

			// Encolar accion para el fichero
			(*actionFileNodeQueue)->next = actionFileNodeNew;
			(*actionFileNodeQueue) = actionFileNodeNew;

		} else {
			// FIXME: !!!!!
			// Directory vs File

		}
	}
}

AccionFileNode *AccionFileNode_BuildSync(FileNode *izquierda, FileNode *derecha) {
	AccionFileNode *afnRaiz = AccionFileNode_CreateNormal(izquierda, derecha);
	AccionFileNode *afnCola = afnRaiz;

	AccionFileNode_CompareChilds(afnRaiz, &afnCola, AccionFileNode_CheckPair);

	return afnRaiz;
}

void AccionFileNode_Copy(FileNode *fnIzq, FileNode *fnDer,
		AccionFileNode **afnCola) {
	AccionFileNode *afnNew = AccionFileNode_CreateNormal(fnIzq, fnDer);

	if (!fnIzq && !fnDer) {
		AccionFileNode_Destroy(afnNew);
		return;
	}
	if (!fnIzq && fnDer) {
		if (fnDer->flags & FileFlag_Directory) {
			AccionFileNode_CompareChilds(afnNew, afnCola,
					AccionFileNode_DeletePair);
		}

		if (fnDer->estado != FileStatus_Deleted) {
			afnNew->action = AccionFileCmp_DeleteRight;
		} else {
			afnNew->action = AccionFileCmp_Nothing;
		}
		(*afnCola)->next = afnNew;
		(*afnCola) = afnNew;
	}
	if (fnIzq && !fnDer) {
		if (fnIzq->estado != FileStatus_Deleted) {
			if (fnIzq->flags & FileFlag_Directory) {
				afnNew->action = AccionFileCmp_MakeRightDirectory;
				(*afnCola)->next = afnNew;
				(*afnCola) = afnNew;
				AccionFileNode_CompareChilds(afnNew, afnCola,
						AccionFileNode_Copy);
				afnNew = AccionFileNode_CreateNormal(fnIzq, fnDer);
				afnNew->action = AccionFileCmp_DateLeftToRight;
			} else {
				afnNew->action = AccionFileCmp_LeftToRight;
			}
		} else {
			afnNew->action = AccionFileCmp_Nothing;
		}
		(*afnCola)->next = afnNew;
		(*afnCola) = afnNew;
	}
	if (fnIzq && fnDer) {
		if ((fnIzq->flags & FileFlag_Directory)
				|| (fnDer->flags & FileFlag_Directory)) {
			if (fnIzq->estado != FileStatus_Deleted) {
				AccionFileNode_CompareChilds(afnNew, afnCola,
						AccionFileNode_Copy);
				if (abs(fnIzq->fileTime - fnDer->fileTime) <= 1) { // appox. equal
					afnNew->action = AccionFileCmp_Nothing;
				} else {
					afnNew->action = AccionFileCmp_DateLeftToRight;
				}
			} else {
				AccionFileNode_CompareChilds(afnNew, afnCola,
						AccionFileNode_DeletePair);
				afnNew->action = AccionFileCmp_DeleteRight;
			}
		} else {
			if (fnIzq->estado != FileStatus_Deleted) {
				if (abs(fnIzq->fileTime - fnDer->fileTime) <= 1) { // appox. equal
					afnNew->action = AccionFileCmp_Nothing;
				} else {
					afnNew->action = AccionFileCmp_LeftToRight;
				}
			} else {
				if (fnDer->estado != FileStatus_Deleted) {
					afnNew->action = AccionFileCmp_DeleteRight;
				}
			}
		}
		(*afnCola)->next = afnNew;
		(*afnCola) = afnNew;
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
