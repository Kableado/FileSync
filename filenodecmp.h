#ifndef _FILENODECMP_H_
#define _FILENODECMP_H_

#include "filenode.h"


typedef enum {
	AccionFileCmp_Nada,
	AccionFileCmp_IzquierdaADerecha,
	AccionFileCmp_DerechaAIzquierda,
	AccionFileCmp_BorrarIzquierda,
	AccionFileCmp_BorrarDerecha,
	AccionFileCmp_FechaIzquierdaADerecha,
	AccionFileCmp_FechaDerechaAIzquierda
} AccionFileCmp;


typedef struct Tag_AccionFileNode {
	AccionFileCmp accion;
	FileNode *izquierda;
	FileNode *derecha;
	struct Tag_AccionFileNode *sig;
} AccionFileNode;


AccionFileNode *AccionFileNode_Crear();
void AccionFileNode_Destruir(AccionFileNode *afn);

AccionFileNode *AccionFileNode_Build(FileNode *izquierda,FileNode *derecha);

void AccionFileNode_Print(AccionFileNode *afn);

#endif
