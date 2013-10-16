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

void AccionFileNode_DeletePair(FileNode *fnIzq, FileNode *fnDer,
		AccionFileNode **afnCola) {
	AccionFileNode *afnNew = AccionFileNode_CreateNormal(fnIzq, fnDer);

	if (!fnIzq && !fnDer) {
		AccionFileNode_Destroy(afnNew);
		return;
	}
	if (!fnIzq && fnDer) {
		if (fnDer->flags & FileFlag_Directory) {
			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(afnNew, afnCola,
					AccionFileNode_DeletePair);
		}

		if (fnDer->estado != FileStatus_Deleted) {
			// Accion de borrado para el nodo
			afnNew->action = AccionFileCmp_DeleteRight;
			(*afnCola)->next = afnNew;
			(*afnCola) = afnNew;
		} else {
			AccionFileNode_Destroy(afnNew);
		}
	}
	if (fnIzq && !fnDer) {
		if (fnIzq->flags & FileFlag_Directory) {
			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(afnNew, afnCola,
					AccionFileNode_DeletePair);
		}

		if (fnIzq->estado != FileStatus_Deleted) {
			// Accion de borrado para el nodo
			afnNew->action = AccionFileCmp_DeleteLeft;
			(*afnCola)->next = afnNew;
			(*afnCola) = afnNew;
		} else {
			AccionFileNode_Destroy(afnNew);
		}
	}
	if (fnIzq && fnDer) {
		if ((fnIzq->flags & FileFlag_Directory)
				|| (fnDer->flags & FileFlag_Directory)) {
			// Alguno es directorio

			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(afnNew, afnCola,
					AccionFileNode_DeletePair);
		}

		if (fnIzq->estado != FileStatus_Deleted) {
			// Accion de borrado para el nodo izquierdo
			afnNew->action = AccionFileCmp_DeleteLeft;
			(*afnCola)->next = afnNew;
			(*afnCola) = afnNew;
			afnNew = NULL;
		}
		if (fnDer->estado != FileStatus_Deleted) {
			if (!afnNew) {
				afnNew = AccionFileNode_CreateNormal(fnIzq, fnDer);
			}
			// Accion de borrado para el nodo derecho
			afnNew->action = AccionFileCmp_DeleteRight;
			(*afnCola)->next = afnNew;
			(*afnCola) = afnNew;
			afnNew = NULL;
		}
		if (afnNew) {
			AccionFileNode_Destroy(afnNew);
		}
	}
}

void AccionFileNode_CheckPair(FileNode *fnIzq, FileNode *fnDer,
		AccionFileNode **afnCola) {
	AccionFileNode *afnNew = AccionFileNode_CreateNormal(fnIzq, fnDer);

	if (!fnIzq && !fnDer) {
		AccionFileNode_Destroy(afnNew);
		return;
	}
	if (!fnIzq && fnDer) {
		if (fnDer->flags & FileFlag_Directory) {
			// Directory
			if (fnDer->estado == FileStatus_Deleted) {
				afnNew->action = AccionFileCmp_Nothing;

				// Anhadir a la lista de acciones
				(*afnCola)->next = afnNew;
				(*afnCola) = afnNew;

			} else {
				afnNew->action = AccionFileCmp_MakeLeftDirectory;

				// Anhadir a la lista de acciones
				(*afnCola)->next = afnNew;
				(*afnCola) = afnNew;

				// Iterar hijos
				AccionFileNode_CompareChilds(afnNew, afnCola,
						AccionFileNode_CheckPair);

				// Crear nueva accion para copiar la fecha
				afnNew = AccionFileNode_CreateNormal(fnIzq, fnDer);
				afnNew->action = AccionFileCmp_DateRightToLeft;
				(*afnCola)->next = afnNew;
				(*afnCola) = afnNew;
			}
		} else {
			// File
			if (fnDer->estado == FileStatus_Deleted) {
				afnNew->action = AccionFileCmp_Nothing;
			} else {
				afnNew->action = AccionFileCmp_RightToLeft;
			}
			(*afnCola)->next = afnNew;
			(*afnCola) = afnNew;
		}
	}
	if (fnIzq && !fnDer) {
		if (fnIzq->flags & FileFlag_Directory) {
			// Directory
			if (fnIzq->estado == FileStatus_Deleted) {
				afnNew->action = AccionFileCmp_Nothing;

				// Anhadir a la lista de acciones
				(*afnCola)->next = afnNew;
				(*afnCola) = afnNew;

			} else {
				afnNew->action = AccionFileCmp_MakeRightDirectory;

				// Anhadir a la lista de acciones
				(*afnCola)->next = afnNew;
				(*afnCola) = afnNew;

				// Iterar hijos
				AccionFileNode_CompareChilds(afnNew, afnCola,
						AccionFileNode_CheckPair);

				// Crear nueva accion para copiar la fecha
				afnNew = AccionFileNode_CreateNormal(fnIzq, fnDer);
				afnNew->action = AccionFileCmp_DateLeftToRight;
				(*afnCola)->next = afnNew;
				(*afnCola) = afnNew;
			}
		} else {
			// File
			if (fnIzq->estado == FileStatus_Deleted) {
				afnNew->action = AccionFileCmp_Nothing;
			} else {
				afnNew->action = AccionFileCmp_LeftToRight;
			}
			(*afnCola)->next = afnNew;
			(*afnCola) = afnNew;
		}
	}
	if (fnIzq && fnDer) {
		if ((fnIzq->flags & FileFlag_Directory)
				&& (fnDer->flags & FileFlag_Directory)) {
			// Directorios

			// Preparar accion para el par de directorios
			if (abs(fnIzq->fileTime - fnDer->fileTime) <= 1) { // appoximadamente iguales
				if (fnDer->estado == FileStatus_Deleted
						&& fnIzq->estado == FileStatus_Deleted) {
					afnNew->action = AccionFileCmp_Nothing;
				} else if (fnDer->estado == FileStatus_Deleted) {
					afnNew->action = AccionFileCmp_DeleteLeft;
					if (fnIzq->estado == FileStatus_Deleted) {
						afnNew->action = AccionFileCmp_Nothing;
					}
				} else if (fnIzq->estado == FileStatus_Deleted) {
					afnNew->action = AccionFileCmp_DeleteRight;
					if (fnDer->estado == FileStatus_Deleted) {
						afnNew->action = AccionFileCmp_Nothing;
					}
				} else {
					afnNew->action = AccionFileCmp_Nothing;
				}
			} else if (fnIzq->fileTime < fnDer->fileTime) {
				afnNew->action = AccionFileCmp_DateRightToLeft;
				if (fnDer->estado == FileStatus_Deleted) {
					afnNew->action = AccionFileCmp_DeleteLeft;
					if (fnIzq->estado == FileStatus_Deleted) {
						afnNew->action = AccionFileCmp_Nothing;
					}
				}
			} else if (fnIzq->fileTime > fnDer->fileTime) {
				afnNew->action = AccionFileCmp_DateLeftToRight;
				if (fnIzq->estado == FileStatus_Deleted) {
					afnNew->action = AccionFileCmp_DeleteRight;
					if (fnDer->estado == FileStatus_Deleted) {
						afnNew->action = AccionFileCmp_Nothing;
					}
				}
			}

			// Procesar nodos hijos
			if (afnNew->action == AccionFileCmp_DeleteRight
					|| afnNew->action == AccionFileCmp_DeleteLeft
					|| (fnIzq->estado == FileStatus_Deleted
							&& fnDer->estado == FileStatus_Deleted)) {
				// Iterar nodos hijos para borrarlos
				AccionFileNode_CompareChilds(afnNew, afnCola,
						AccionFileNode_DeletePair);
			} else {
				AccionFileNode_CompareChilds(afnNew, afnCola,
						AccionFileNode_CheckPair);
			}

			// Encolar accion para el directorio padre
			(*afnCola)->next = afnNew;
			(*afnCola) = afnNew;

		} else if ((fnIzq->flags & FileFlag_Normal)
				&& (fnDer->flags & FileFlag_Normal)) {
			// Ficheros

			// Preparar accion para el par de ficheros
			if (abs(fnIzq->fileTime - fnDer->fileTime) <= 1) { // appoximadamente iguales
				if (fnDer->estado == FileStatus_Deleted
						&& fnIzq->estado == FileStatus_Deleted) {
					afnNew->action = AccionFileCmp_Nothing;
				} else if (fnDer->estado == FileStatus_Deleted) {
					afnNew->action = AccionFileCmp_DeleteLeft;
					if (fnIzq->estado == FileStatus_Deleted) {
						afnNew->action = AccionFileCmp_Nothing;
					}
				} else if (fnIzq->estado == FileStatus_Deleted) {
					afnNew->action = AccionFileCmp_DeleteRight;
					if (fnDer->estado == FileStatus_Deleted) {
						afnNew->action = AccionFileCmp_Nothing;
					}
				} else {
					afnNew->action = AccionFileCmp_Nothing;
				}
			} else if (fnIzq->fileTime < fnDer->fileTime) {
				afnNew->action = AccionFileCmp_RightToLeft;
				if (fnDer->estado == FileStatus_Deleted) {
					afnNew->action = AccionFileCmp_DeleteLeft;
					if (fnIzq->estado == FileStatus_Deleted) {
						afnNew->action = AccionFileCmp_Nothing;
					}
				}
			} else if (fnIzq->fileTime > fnDer->fileTime) {
				afnNew->action = AccionFileCmp_LeftToRight;
				if (fnIzq->estado == FileStatus_Deleted) {
					afnNew->action = AccionFileCmp_DeleteRight;
					if (fnDer->estado == FileStatus_Deleted) {
						afnNew->action = AccionFileCmp_Nothing;
					}
				}
			}

			// Encolar accion para el fichero
			(*afnCola)->next = afnNew;
			(*afnCola) = afnNew;

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
