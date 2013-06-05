#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"
#include "filenodecmp.h"

void help(char *exe){
	char exeFilename[MaxPath];
	File_GetName(exe,exeFilename);
	printf("Modo de uso:\n");
	printf("\t%s info [file] {[file] {..}}\n",exeFilename);
	printf("\t%s scan [dir] [tree] \n",exeFilename);
	printf("\t%s rescan [dir] [tree] \n",exeFilename);
	printf("\t%s read [file] [tree]\n",exeFilename);
	printf("\t%s dir [dir]\n",exeFilename);
	printf("\t%s sync [dirIzquierda] [dirDerecha]\n",exeFilename);
}


int sync(char *pathIzquierda,char *pathDerecha,int dryrun);


int main(int argc,char *argv[]){
	FILE *f;
	unsigned long crc=0;
	FileTime ft;
	int i;

	if(!strcmp(argv[1],"info") && argc>=3){
		// Informacion de ficheros
		for(i=2;i<argc;i++){
			if(File_ExistePath(argv[i])){
				f=fopen(argv[i],"rb");
				if(f){
					crc=CRC_File(f);
					fclose(f);
				}
				ft=FileTime_Get(argv[i]);
				printf("%s:\t[%08X]\t",argv[i],crc);
				FileTime_Print(ft);printf("\n");
			}
		}
	}else
	if(!strcmp(argv[1],"scan") && argc==4){
		// Scanear informacion de directorio y guardar arbol
		FileNode *fn;
		printf("Building FileNode..\n");
		fn=FileNode_Build(argv[2]);
		FileNode_Save(fn,argv[3]);
	}else
	if(!strcmp(argv[1],"rescan") && argc==4){
		// Scanear informacion de directorio y guardar arbol
		FileNode *fn;
		printf("Loading FileNode..\n");
		fn=FileNode_Load(argv[3]);
		if(fn){
			printf("Rebuilding FileNode..\n");
			fn=FileNode_Refresh(fn,argv[2]);
			FileNode_Save(fn,argv[3]);
		}
	}else
	if(!strcmp(argv[1],"read") && argc==3){
		// Leer informacion de arbol
		FileNode *fn;
		fn=FileNode_Load(argv[2]);
		if(fn)FileNode_Print(fn);
	}else
	if(!strcmp(argv[1],"dir") && argc==3){
		// Leer informacion de dir
		char *path=argv[2];
		char dirNodesFile[MaxPath];
		FileNode *fn;

		printf("Checking Directory.. %s\n",path);
		if(File_ExistePath(path) && File_EsDirectorio(path)){
			// Get the FileNode from the dir
			snprintf(dirNodesFile,MaxPath,"%s/"FileNode_Filename,path);
			fn=FileNode_Load(dirNodesFile);
			if(fn){
				fn=FileNode_Refresh(fn,path);
			}else{
				fn=FileNode_Build(path);
			}
			if(fn){
				FileNode_Print(fn);
				FileNode_Save(fn,dirNodesFile);
			}
		}
	}else
	if(!strcmp(argv[1],"sync") && argc==4){
		// Sincronizar dos directorios
		char *pathIzquierda=argv[2];
		char *pathDerecha=argv[3];
		sync(pathIzquierda,pathDerecha,0);

	}else
	if(!strcmp(argv[1],"synctest") && argc==4){
		// Sincronizar dos directorios
		char *pathIzquierda=argv[2];
		char *pathDerecha=argv[3];
		sync(pathIzquierda,pathDerecha,1);

	}else{
		help(argv[0]);
	}

	return(0);
}


int sync(char *pathIzquierda,char *pathDerecha,int dryrun){
	char dirNodesFileIzq[MaxPath];
	char dirNodesFileDer[MaxPath];
	FileNode *fnIzquierda,*fnDerecha;

	if(!File_ExistePath(pathIzquierda) || !File_EsDirectorio(pathIzquierda)){
		printf("Error, directory does not exist: %s\n",pathIzquierda);
		return 0;
	}
	if(!File_ExistePath(pathDerecha) || !File_EsDirectorio(pathDerecha)){
		printf("Error, directory does not exist: %s\n",pathDerecha);
		return 0;
	}

	// Comprobar directorio izquierdo
	printf("Checking Directory.. %s\n",pathIzquierda);
	snprintf(dirNodesFileIzq,MaxPath,"%s/"FileNode_Filename,
		pathIzquierda);
	fnIzquierda=FileNode_Load(dirNodesFileIzq);
	if(fnIzquierda){
		fnIzquierda=FileNode_Refresh(fnIzquierda,pathIzquierda);
	}else{
		fnIzquierda=FileNode_Build(pathIzquierda);
	}
	FileNode_Save(fnIzquierda,dirNodesFileIzq);

	// Comprobar directorui derecho
	printf("Checking Directory.. %s\n",pathDerecha);
	snprintf(dirNodesFileDer,MaxPath,"%s/"FileNode_Filename,
		pathDerecha);
	fnDerecha=FileNode_Load(dirNodesFileDer);
	if(fnDerecha){
		fnDerecha=FileNode_Refresh(fnDerecha,pathDerecha);
	}else{
		fnDerecha=FileNode_Build(pathDerecha);
	}
	FileNode_Save(fnDerecha,dirNodesFileDer);

	// Construir acciones
	printf("Building action list.. \n");
	AccionFileNode *afn=NULL;
	afn=AccionFileNode_Build(fnIzquierda,fnDerecha);

	if(dryrun){
		// Mostrar lista de acciones
		AccionFileNode_Print(afn);
	}else{
		// Ejecutar lista de acciones
		AccionFileNode_RunList(afn,pathIzquierda,pathDerecha);
	}

	return(1);
}

