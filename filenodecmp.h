#ifndef _FILENODE_H_
#define _FILENODE_H_

#include "filenode.h"


typedef enum {
	AccionFileCmp_Nada,
	AccionFileCmp_IzquierdaADerecha,
	AccionFileCmp_DerechaAIzquierda,
	AccionFileCmp_BorrarIzquierda,
	AccionFileCmp_BorrarDerecha
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


#endif
