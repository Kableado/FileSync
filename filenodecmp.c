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


void AccionFileNode_CompareChilds(
	AccionFileNode *afnRaiz,
	AccionFileNode **afnCola);


void AccionFileNode_CheckPair(
	FileNode *fnIzq,FileNode *fnDer,AccionFileNode **afnCola)
{
	AccionFileNode *afnNew;
	afnNew=AccionFileNode_Crear();
	afnNew->izquierda=fnIzq;
	afnNew->derecha=fnDer;
	int doChilds=0;

	if(!fnIzq && !fnDer){
		AccionFileNode_Destruir(afnNew);
		return;
	}
	if(!fnIzq && fnDer){
		afnNew->accion=AccionFileCmp_DerechaAIzquierda;
		if(fnDer->child){
			doChilds=1;
		}
	}
	if(fnIzq && !fnDer){
		afnNew->accion=AccionFileCmp_IzquierdaADerecha;
		if(fnIzq->child){
			doChilds=1;
		}
	}
	if(fnIzq && fnDer){
		// Realizar comparacion completa
		if(fnIzq->child){
			doChilds=1;
		}
		if(fnIzq->child){
			doChilds=1;
		}

		// Comparacion mediante fechas
		if(fnIzq->ft==fnDer->ft){
			afnNew->accion=AccionFileCmp_Nada;
		}else
		if(fnIzq->ft<fnDer->ft){
			afnNew->accion=AccionFileCmp_DerechaAIzquierda;
		}else
		if(fnIzq->ft>fnDer->ft){
			afnNew->accion=AccionFileCmp_IzquierdaADerecha;
		}
	}

	// Anhadir a la lista de acciones
	(*afnCola)->sig=afnNew;
	(*afnCola)=afnNew;

	if(doChilds){
		AccionFileNode_CompareChilds(afnNew,afnCola);
	}
}


void AccionFileNode_CompareChilds(
	AccionFileNode *afnRaiz,
	AccionFileNode **afnCola)
{
	FileNode *fnIzq,*fnDer;
	AccionFileNode *afnColaStart=(*afnCola);

	// Comprobar si hay algo que comparar
	if(!afnRaiz->izquierda || !afnRaiz->derecha){
		// Nada que hacer
		return;
	}

	// Iterar todos los nodos de la izquierda
	fnIzq=afnRaiz->izquierda->child;
	while(fnIzq){
		fnDer=afnRaiz->derecha->child;
		while(fnDer){
			if(!strcmp(fnIzq->name,fnDer->name)){
				break;
			}else{
				fnDer=fnDer->sig;
			}
		}

		AccionFileNode_CheckPair(fnIzq,fnDer,afnCola);

		fnIzq=fnIzq->sig;
	}


	// Iterar todos los nodos de la derecha,
	//   ignorando las comparaciones ya realizadas
	fnDer=afnRaiz->derecha->child;
	while(fnDer){
		int doCheck=1;
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

		if(doCheck){
			AccionFileNode_CheckPair(fnIzq,fnDer,afnCola);
		}

		fnDer=fnDer->sig;
	}

}

AccionFileNode *AccionFileNode_Build(
	FileNode *izquierda,FileNode *derecha)
{
	AccionFileNode *afnRaiz=AccionFileNode_Crear();
	AccionFileNode *afnCola=afnRaiz;

	afnRaiz->izquierda=izquierda;
	afnRaiz->derecha=derecha;

	AccionFileNode_CompareChilds(afnRaiz,&afnCola);

	return afnRaiz;
}



void AccionFileNode_Print(AccionFileNode *afn){
	char pathIzq[4096],pathDer[4096];
	while(afn!=NULL){
		if(afn->izquierda){
			FileNode_GetPath(afn->izquierda,pathIzq);
		}else{
			strcpy(pathIzq,"(null)");
		}
		if(afn->derecha){
			FileNode_GetPath(afn->derecha,pathDer);
		}else{
			strcpy(pathDer,"(null)");
		}

		switch(afn->accion){
			case AccionFileCmp_Nada:
				//printf("%s == %s\n",pathIzq,pathDer);
				break;
			case AccionFileCmp_IzquierdaADerecha:
				printf("%s => %s\n",pathIzq,pathDer);break;
			case AccionFileCmp_DerechaAIzquierda:
				printf("%s <= %s\n",pathIzq,pathDer);break;
			case AccionFileCmp_BorrarIzquierda:
				printf("%s *- %s\n",pathIzq,pathDer);break;
			case AccionFileCmp_BorrarDerecha:
				printf("%s -* %s\n",pathIzq,pathDer);break;
			case AccionFileCmp_FechaIzquierdaADerecha:
				printf("%s -> %s\n",pathIzq,pathDer);break;
			case AccionFileCmp_FechaDerechaAIzquierda:
				printf("%s <- %s\n",pathIzq,pathDer);break;
		}

		afn=afn->sig;
	}
}
