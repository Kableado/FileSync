#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"
#include "filenodecmp.h"

AccionFileNode *_free_accionfilenode=NULL;
int _n_accionfilenode=0;
#define AccionFileNode_Tocho 1024
AccionFileNode *AccionFileNode_Crear(){
	AccionFileNode *afn;

	if(_free_accionfilenode==NULL){
		AccionFileNode *nodos;
		int i;
		// Reservar un tocho
		nodos=malloc(sizeof(AccionFileNode)*AccionFileNode_Tocho);
		for(i=0;i<AccionFileNode_Tocho-1;i++){
			nodos[i].sig=&nodos[i+1];
		}
		nodos[AccionFileNode_Tocho-1].sig=NULL;
		_free_accionfilenode=&nodos[0];
	}

	// Obtener el primero libre
	afn=_free_accionfilenode;
	_free_accionfilenode=afn->sig;
	_n_accionfilenode++;

	// Iniciar
	afn->accion=AccionFileCmp_Nada;
	afn->izquierda=NULL;
	afn->derecha=NULL;
	afn->sig=NULL;

	return(afn);
}


void AccionFileNode_Destruir(AccionFileNode *afn){
	afn->sig=_free_accionfilenode;
	_free_accionfilenode=afn;
	_n_accionfilenode--;
}

AccionFileNode *AccionFileNode_CrearNormal(FileNode *fnIzq,FileNode *fnDer)
{
	AccionFileNode *afnNew;
	afnNew=AccionFileNode_Crear();
	afnNew->accion=AccionFileCmp_Nada;
	afnNew->izquierda=fnIzq;
	afnNew->derecha=fnDer;
	return afnNew;
}

void AccionFileNode_CompareChilds(
	AccionFileNode *afnRaiz,
	AccionFileNode **afnCola,
	void (*CheckPair)(FileNode *fnIzq,FileNode *fnDer,AccionFileNode **afnCola))
{
	FileNode *fnIzq,*fnDer;
	AccionFileNode *afnColaStart=(*afnCola);

	// Comprobar si hay algo que comparar
	if(!afnRaiz->izquierda && !afnRaiz->derecha){
		// Nada que hacer
		return;
	}

	// Iterar todos los nodos de la izquierda
	if(afnRaiz->izquierda){
		fnIzq=afnRaiz->izquierda->child;
		while(fnIzq){
			if(afnRaiz->derecha){
				fnDer=afnRaiz->derecha->child;
				while(fnDer){
					if(!strcmp(fnIzq->name,fnDer->name)){
						break;
					}else{
						fnDer=fnDer->sig;
					}
				}
			}else{
				fnDer=NULL;
			}

			CheckPair(fnIzq,fnDer,afnCola);

			fnIzq=fnIzq->sig;
		}
	}


	// Iterar todos los nodos de la derecha,
	//   ignorando las comparaciones ya realizadas
	if(afnRaiz->derecha){
		fnDer=afnRaiz->derecha->child;
		while(fnDer){
			int doCheck=1;
			if(afnRaiz->izquierda){
				fnIzq=afnRaiz->izquierda->child;
				while(fnIzq){
					AccionFileNode *afnCheck=afnColaStart;
					while(afnCheck){
						if(afnCheck->izquierda==fnIzq && afnCheck->derecha==fnDer){
							break;
						}else{
							afnCheck=afnCheck->sig;
						}
					}
					if(afnCheck){
						doCheck=0;
						break;
					}

					if(!strcmp(fnIzq->name,fnDer->name)){
						break;
					}else{
						fnIzq=fnIzq->sig;
					}
				}
			}else{
				fnIzq=NULL;
			}

			if(doCheck){
				CheckPair(fnIzq,fnDer,afnCola);
			}

			fnDer=fnDer->sig;
		}
	}

}



void AccionFileNode_DeletePair(
	FileNode *fnIzq,FileNode *fnDer,AccionFileNode **afnCola)
{
	AccionFileNode *afnNew=AccionFileNode_CrearNormal(fnIzq,fnDer);

	if(!fnIzq && !fnDer){
		AccionFileNode_Destruir(afnNew);
		return;
	}
	if(!fnIzq && fnDer){
		if(fnDer->flags&FileFlag_Directorio){
			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(afnNew,afnCola,
				AccionFileNode_DeletePair);
		}

		if(fnDer->estado!=EstadoFichero_Borrado){
			// Accion de borrado para el nodo
			afnNew->accion=AccionFileCmp_BorrarDerecha;
			(*afnCola)->sig=afnNew;
			(*afnCola)=afnNew;
		}else{
			AccionFileNode_Destruir(afnNew);
		}
	}
	if(fnIzq && !fnDer){
		if(fnIzq->flags&FileFlag_Directorio){
			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(afnNew,afnCola,
				AccionFileNode_DeletePair);
		}

		if(fnIzq->estado!=EstadoFichero_Borrado){
			// Accion de borrado para el nodo
			afnNew->accion=AccionFileCmp_BorrarIzquierda;
			(*afnCola)->sig=afnNew;
			(*afnCola)=afnNew;
		}else{
			AccionFileNode_Destruir(afnNew);
		}
	}
	if(fnIzq && fnDer){
		if(fnIzq->flags&FileFlag_Directorio ||
			fnDer->flags&FileFlag_Directorio)
		{
			// Alguno es directorio

			// Iterar hijos para borrarlos
			AccionFileNode_CompareChilds(afnNew,afnCola,
				AccionFileNode_DeletePair);
		}

		if(fnIzq->estado!=EstadoFichero_Borrado){
			// Accion de borrado para el nodo izquierdo
			afnNew->accion=AccionFileCmp_BorrarIzquierda;
			(*afnCola)->sig=afnNew;
			(*afnCola)=afnNew;
			afnNew=NULL;
		}
		if(fnDer->estado!=EstadoFichero_Borrado){
			if(!afnNew){ afnNew=AccionFileNode_CrearNormal(fnIzq,fnDer); }
			// Accion de borrado para el nodo derecho
			afnNew->accion=AccionFileCmp_BorrarDerecha;
			(*afnCola)->sig=afnNew;
			(*afnCola)=afnNew;
			afnNew=NULL;
		}
		if(afnNew){
			AccionFileNode_Destruir(afnNew);
		}
	}
}

void AccionFileNode_CheckPair(
	FileNode *fnIzq,FileNode *fnDer,AccionFileNode **afnCola)
{
	AccionFileNode *afnNew=AccionFileNode_CrearNormal(fnIzq,fnDer);

	if(!fnIzq && !fnDer){
		AccionFileNode_Destruir(afnNew);
		return;
	}
	if(!fnIzq && fnDer){
		if(fnDer->flags&FileFlag_Directorio){
			// Directory
			if(fnDer->estado==EstadoFichero_Borrado){
				afnNew->accion=AccionFileCmp_Nada;

				// Anhadir a la lista de acciones
				(*afnCola)->sig=afnNew;
				(*afnCola)=afnNew;

			}else{
				afnNew->accion=AccionFileCmp_CrearDirIzquierda;

				// Anhadir a la lista de acciones
				(*afnCola)->sig=afnNew;
				(*afnCola)=afnNew;

				// Iterar hijos
				AccionFileNode_CompareChilds(afnNew,afnCola,
					AccionFileNode_CheckPair);

				// Crear nueva accion para copiar la fecha
				afnNew=AccionFileNode_CrearNormal(fnIzq,fnDer);
				afnNew->accion=AccionFileCmp_FechaDerechaAIzquierda;
				(*afnCola)->sig=afnNew;
				(*afnCola)=afnNew;
			}
		}else{
			// File
			if(fnDer->estado==EstadoFichero_Borrado){
				afnNew->accion=AccionFileCmp_Nada;
			}else{
				afnNew->accion=AccionFileCmp_DerechaAIzquierda;
			}
			(*afnCola)->sig=afnNew;
			(*afnCola)=afnNew;
		}
	}
	if(fnIzq && !fnDer){
		if(fnIzq->flags&FileFlag_Directorio){
			// Directory
			if(fnIzq->estado==EstadoFichero_Borrado){
				afnNew->accion=AccionFileCmp_Nada;

				// Anhadir a la lista de acciones
				(*afnCola)->sig=afnNew;
				(*afnCola)=afnNew;

			}else{
				afnNew->accion=AccionFileCmp_CrearDirDerecha;

				// Anhadir a la lista de acciones
				(*afnCola)->sig=afnNew;
				(*afnCola)=afnNew;

				// Iterar hijos
				AccionFileNode_CompareChilds(afnNew,afnCola,
					AccionFileNode_CheckPair);

				// Crear nueva accion para copiar la fecha
				afnNew=AccionFileNode_CrearNormal(fnIzq,fnDer);
				afnNew->accion=AccionFileCmp_FechaIzquierdaADerecha;
				(*afnCola)->sig=afnNew;
				(*afnCola)=afnNew;
			}
		}else{
			// File
			if(fnIzq->estado==EstadoFichero_Borrado){
				afnNew->accion=AccionFileCmp_Nada;
			}else{
				afnNew->accion=AccionFileCmp_IzquierdaADerecha;
			}
			(*afnCola)->sig=afnNew;
			(*afnCola)=afnNew;
		}
	}
	if(fnIzq && fnDer){
		if(fnIzq->flags&FileFlag_Directorio &&
			fnDer->flags&FileFlag_Directorio)
		{
			// Directorios

			// Preparar accion para el par de directorios
			if(abs(fnIzq->ft-fnDer->ft)<=1){ // appoximadamente iguales
				if(fnDer->estado==EstadoFichero_Borrado &&
					fnIzq->estado==EstadoFichero_Borrado)
				{
					afnNew->accion=AccionFileCmp_Nada;
				}else
				if(fnDer->estado==EstadoFichero_Borrado){
					afnNew->accion=AccionFileCmp_BorrarIzquierda;
					if(fnIzq->estado==EstadoFichero_Borrado){
						afnNew->accion=AccionFileCmp_Nada;
					}
				}else
				if(fnIzq->estado==EstadoFichero_Borrado){
					afnNew->accion=AccionFileCmp_BorrarDerecha;
					if(fnDer->estado==EstadoFichero_Borrado){
						afnNew->accion=AccionFileCmp_Nada;
					}
				}else{
					afnNew->accion=AccionFileCmp_Nada;
				}
			}else
			if(fnIzq->ft<fnDer->ft){
				afnNew->accion=AccionFileCmp_FechaDerechaAIzquierda;
				if(fnDer->estado==EstadoFichero_Borrado){
					afnNew->accion=AccionFileCmp_BorrarIzquierda;
					if(fnIzq->estado==EstadoFichero_Borrado){
						afnNew->accion=AccionFileCmp_Nada;
					}
				}
			}else
			if(fnIzq->ft>fnDer->ft){
				afnNew->accion=AccionFileCmp_FechaIzquierdaADerecha;
				if(fnIzq->estado==EstadoFichero_Borrado){
					afnNew->accion=AccionFileCmp_BorrarDerecha;
					if(fnDer->estado==EstadoFichero_Borrado){
						afnNew->accion=AccionFileCmp_Nada;
					}
				}
			}

			// Procesar nodos hijos
			if(afnNew->accion==AccionFileCmp_BorrarDerecha ||
				afnNew->accion==AccionFileCmp_BorrarIzquierda ||
				(fnIzq->estado==EstadoFichero_Borrado &&
				 fnDer->estado==EstadoFichero_Borrado))
			{
				// Iterar nodos hijos para borrarlos
				AccionFileNode_CompareChilds(afnNew,afnCola,
					AccionFileNode_DeletePair);
			}else{
				AccionFileNode_CompareChilds(afnNew,afnCola,
					AccionFileNode_CheckPair);
			}

			// Encolar accion para el directorio padre
			(*afnCola)->sig=afnNew;
			(*afnCola)=afnNew;

		}else
		if(fnIzq->flags&FileFlag_Normal &&
			fnDer->flags&FileFlag_Normal)
		{
			// Ficheros


			// Preparar accion para el par de ficheros
			if(abs(fnIzq->ft-fnDer->ft)<=1){ // appoximadamente iguales
				if(fnDer->estado==EstadoFichero_Borrado &&
					fnIzq->estado==EstadoFichero_Borrado)
				{
					afnNew->accion=AccionFileCmp_Nada;
				}else
				if(fnDer->estado==EstadoFichero_Borrado){
					afnNew->accion=AccionFileCmp_BorrarIzquierda;
					if(fnIzq->estado==EstadoFichero_Borrado){
						afnNew->accion=AccionFileCmp_Nada;
					}
				}else
				if(fnIzq->estado==EstadoFichero_Borrado){
					afnNew->accion=AccionFileCmp_BorrarDerecha;
					if(fnDer->estado==EstadoFichero_Borrado){
						afnNew->accion=AccionFileCmp_Nada;
					}
				}else{
					afnNew->accion=AccionFileCmp_Nada;
				}
			}else
			if(fnIzq->ft<fnDer->ft){
				afnNew->accion=AccionFileCmp_DerechaAIzquierda;
				if(fnDer->estado==EstadoFichero_Borrado){
					afnNew->accion=AccionFileCmp_BorrarIzquierda;
					if(fnIzq->estado==EstadoFichero_Borrado){
						afnNew->accion=AccionFileCmp_Nada;
					}
				}
			}else
			if(fnIzq->ft>fnDer->ft){
				afnNew->accion=AccionFileCmp_IzquierdaADerecha;
				if(fnIzq->estado==EstadoFichero_Borrado){
					afnNew->accion=AccionFileCmp_BorrarDerecha;
					if(fnDer->estado==EstadoFichero_Borrado){
						afnNew->accion=AccionFileCmp_Nada;
					}
				}
			}

			// Encolar accion para el fichero
			(*afnCola)->sig=afnNew;
			(*afnCola)=afnNew;

		}else{
			// FIXME: !!!!!
			// Directory vs File

		}
	}
}


AccionFileNode *AccionFileNode_BuildSync(
	FileNode *izquierda,FileNode *derecha)
{
	AccionFileNode *afnRaiz=AccionFileNode_CrearNormal(izquierda,derecha);
	AccionFileNode *afnCola=afnRaiz;

	AccionFileNode_CompareChilds(afnRaiz,&afnCola,
		AccionFileNode_CheckPair);

	return afnRaiz;
}








void AccionFileNode_Copy(
	FileNode *fnIzq,FileNode *fnDer,AccionFileNode **afnCola)
{
	AccionFileNode *afnNew=AccionFileNode_CrearNormal(fnIzq,fnDer);

	if(!fnIzq && !fnDer){
		AccionFileNode_Destruir(afnNew);
		return;
	}
	if(!fnIzq && fnDer){
		if(fnDer->flags&FileFlag_Directorio){
			AccionFileNode_CompareChilds(afnNew,afnCola,
				AccionFileNode_DeletePair);
		}

		if(fnDer->estado!=EstadoFichero_Borrado){
			afnNew->accion=AccionFileCmp_BorrarDerecha;
		}else{
			afnNew->accion=AccionFileCmp_Nada;
		}
		(*afnCola)->sig=afnNew;
		(*afnCola)=afnNew;
	}
	if(fnIzq && !fnDer){
		if(fnIzq->estado!=EstadoFichero_Borrado){
			if(fnIzq->flags&FileFlag_Directorio){
				afnNew->accion=AccionFileCmp_CrearDirDerecha;
				(*afnCola)->sig=afnNew;
				(*afnCola)=afnNew;
				AccionFileNode_CompareChilds(afnNew,afnCola,
					AccionFileNode_Copy);
				afnNew=AccionFileNode_CrearNormal(fnIzq,fnDer);
				if(abs(fnIzq->ft-fnDer->ft)<=1){ // appox. equal
					afnNew->accion=AccionFileCmp_Nada;
				}else{
					afnNew->accion=AccionFileCmp_FechaIzquierdaADerecha;
				}
			}else{
				afnNew->accion=AccionFileCmp_IzquierdaADerecha;
			}
		}else{
			afnNew->accion=AccionFileCmp_Nada;
		}
		(*afnCola)->sig=afnNew;
		(*afnCola)=afnNew;
	}
	if(fnIzq && fnDer){
		if(fnIzq->flags&FileFlag_Directorio ||
			fnDer->flags&FileFlag_Directorio)
		{
			if(fnIzq->estado!=EstadoFichero_Borrado){
				AccionFileNode_CompareChilds(afnNew,afnCola,
					AccionFileNode_Copy);
				if(abs(fnIzq->ft-fnDer->ft)<=1){ // appox. equal
					afnNew->accion=AccionFileCmp_Nada;
				}else{
					afnNew->accion=AccionFileCmp_FechaIzquierdaADerecha;
				}
			}else{
				AccionFileNode_CompareChilds(afnNew,afnCola,
					AccionFileNode_DeletePair);
				afnNew->accion=AccionFileCmp_BorrarDerecha;
			}
		}else{
			if(fnIzq->estado!=EstadoFichero_Borrado){
				if(abs(fnIzq->ft-fnDer->ft)<=1){ // appox. equal
					afnNew->accion=AccionFileCmp_Nada;
				}else{
					afnNew->accion=AccionFileCmp_IzquierdaADerecha;
				}
			}else{
				if(fnDer->estado!=EstadoFichero_Borrado){
					afnNew->accion=AccionFileCmp_BorrarDerecha;
				}
			}
		}
		(*afnCola)->sig=afnNew;
		(*afnCola)=afnNew;
	}
}







AccionFileNode *AccionFileNode_BuildCopy(
	FileNode *izquierda,FileNode *derecha)
{
	AccionFileNode *afnRaiz=AccionFileNode_CrearNormal(izquierda,derecha);
	AccionFileNode *afnCola=afnRaiz;

	AccionFileNode_CompareChilds(afnRaiz,&afnCola,
		AccionFileNode_Copy);

	return afnRaiz;
}


void AccionFileNode_Print(AccionFileNode *afn){
	char showPath[MaxPath];
	while(afn!=NULL){
		if(afn->izquierda){
			FileNode_GetFullPath(afn->izquierda,"",showPath);
		}else{
			FileNode_GetFullPath(afn->derecha,"",showPath);
		}

		switch(afn->accion){
			case AccionFileCmp_Nada:
				//printf("%s == %s\n",pathIzq,pathDer);
				break;
			case AccionFileCmp_IzquierdaADerecha:
				printf(" => %s\n",showPath);break;
			case AccionFileCmp_DerechaAIzquierda:
				printf(" <= %s\n",showPath);break;
			case AccionFileCmp_BorrarIzquierda:
				printf(" *- %s\n",showPath);break;
			case AccionFileCmp_BorrarDerecha:
				printf(" -* %s\n",showPath);break;
			case AccionFileCmp_FechaIzquierdaADerecha:
				printf(" -> %s\n",showPath);break;
			case AccionFileCmp_FechaDerechaAIzquierda:
				printf(" <- %s\n",showPath);break;
			case AccionFileCmp_CrearDirDerecha:
				printf(" -D %s\n",showPath);break;
			case AccionFileCmp_CrearDirIzquierda:
				printf(" D- %s\n",showPath);break;
		}

		afn=afn->sig;
	}
	printf("End\n");
}





void AccionFileNodeAux_CopyDate(char *pathOrig,char *pathDest){
	FileTime ft=FileTime_Get(pathOrig);
	FileTime_Set(pathDest,ft);
}

void AccionFileNodeAux_Copy(char *pathOrig,char *pathDest){
	if(File_Copiar(pathOrig,pathDest)){
		AccionFileNodeAux_CopyDate(pathOrig,pathDest);
	}else{
		File_Borrar(pathDest);
	}
}
void AccionFileNodeAux_Delete(char *pathOrig,char *pathDest){
	if(File_EsDirectorio(pathDest)){
		File_BorrarDirectorio(pathDest);
	}else{
		File_Borrar(pathDest);
	}
}
void AccionFileNodeAux_MakeDir(char *pathOrig,char *pathDest){
	File_CrearDir(pathDest);
}

void AccionFileNode_RunList(AccionFileNode *afn,char *pathIzquierda,char *pathDerecha){
	char pathIzq[MaxPath],pathDer[MaxPath],showPath[MaxPath];
	while(afn!=NULL){
		if(afn->izquierda){
			FileNode_GetFullPath(afn->izquierda,pathIzquierda,pathIzq);
		}else{
			FileNode_GetFullPath(afn->derecha,pathIzquierda,pathIzq);
		}
		if(afn->derecha){
			FileNode_GetFullPath(afn->derecha,pathDerecha,pathDer);
		}else{
			FileNode_GetFullPath(afn->izquierda,pathDerecha,pathDer);
		}
		if(afn->izquierda){
			FileNode_GetFullPath(afn->izquierda,"",showPath);
		}else{
			FileNode_GetFullPath(afn->derecha,"",showPath);
		}


		switch(afn->accion){
			case AccionFileCmp_Nada:
				//printf("%s == %s\n",pathIzq,pathDer);
				break;
			case AccionFileCmp_IzquierdaADerecha:
				printf(" => %s\n",showPath);
				AccionFileNodeAux_Copy(pathIzq,pathDer);
				break;
			case AccionFileCmp_DerechaAIzquierda:
				printf(" <= %s\n",showPath);
				AccionFileNodeAux_Copy(pathDer,pathIzq);
				break;
			case AccionFileCmp_BorrarIzquierda:
				printf(" *- %s\n",showPath);
				AccionFileNodeAux_Delete(pathDer,pathIzq);
				break;
			case AccionFileCmp_BorrarDerecha:
				printf(" -* %s\n",showPath);
				AccionFileNodeAux_Delete(pathIzq,pathDer);
				break;
			case AccionFileCmp_FechaIzquierdaADerecha:
				printf(" -> %s\n",showPath);
				AccionFileNodeAux_CopyDate(pathIzq,pathDer);
				break;
			case AccionFileCmp_FechaDerechaAIzquierda:
				printf(" <- %s\n",showPath);
				AccionFileNodeAux_CopyDate(pathDer,pathIzq);
				break;
			case AccionFileCmp_CrearDirDerecha:
				printf(" -D %s\n",showPath);
				AccionFileNodeAux_MakeDir(pathIzq,pathDer);
				break;
			case AccionFileCmp_CrearDirIzquierda:
				printf(" D- %s\n",showPath);
				AccionFileNodeAux_MakeDir(pathDer,pathIzq);
				break;
		}

		afn=afn->sig;
	}
	printf("End\n");
}
