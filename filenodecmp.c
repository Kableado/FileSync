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

}


void AccionFileNode_Destruir(AccionFileNode *afn){

}

AccionFileNode *AccionFileNode_Build(FileNode *izquierda,FileNode *derecha){

}
