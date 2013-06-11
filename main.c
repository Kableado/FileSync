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
	printf("    %s info [file] {[file] {..}}\n",exeFilename);
	printf("    %s scan [dir] [tree] \n",exeFilename);
	printf("    %s rescan [dir] [tree] \n",exeFilename);
	printf("    %s read [file] [tree]\n",exeFilename);
	printf("    %s dir [dir]\n",exeFilename);
	printf("\n");
	printf("    %s sync [dirIzquierda] [dirDerecha]\n",exeFilename);
	printf("    %s resync [dirIzquierda] [dirDerecha]\n",exeFilename);
	printf("    %s synctest [dirIzquierda] [dirDerecha]\n",exeFilename);
	printf("    %s resynctest [dirIzquierda] [dirDerecha]\n",exeFilename);
	printf("\n");
	printf("    %s copy [dirIzquierda] [dirDerecha]\n",exeFilename);
	printf("    %s recopy [dirIzquierda] [dirDerecha]\n",exeFilename);
	printf("    %s copytest [dirIzquierda] [dirDerecha]\n",exeFilename);
	printf("    %s recopytest [dirIzquierda] [dirDerecha]\n",exeFilename);
}


FileNode *checkDir(char *path,int recheck);
int sync(char *pathIzquierda,char *pathDerecha,int recheck,int dryrun);


int main(int argc,char *argv[]){
	FILE *f;
	unsigned long crc=0;
	FileTime ft;
	int i;

	if(argc<2){
		help(argv[0]);
		return 0;
	}

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

		fn=checkDir(path,1);
		if(fn){
			FileNode_Print(fn);
		}
	}else
	if(!strcmp(argv[1],"sync") && argc==4){
		// Sincronizar dos directorios
		char *pathIzquierda=argv[2];
		char *pathDerecha=argv[3];
		sync(pathIzquierda,pathDerecha,1,0);
	}else
	if(!strcmp(argv[1],"resync") && argc==4){
		// Sincronizar dos directorios
		char *pathIzquierda=argv[2];
		char *pathDerecha=argv[3];
		sync(pathIzquierda,pathDerecha,0,0);
	}else
	if(!strcmp(argv[1],"synctest") && argc==4){
		// Sincronizar dos directorios
		char *pathIzquierda=argv[2];
		char *pathDerecha=argv[3];
		sync(pathIzquierda,pathDerecha,1,1);
	}else
	if(!strcmp(argv[1],"resynctest") && argc==4){
		// Sincronizar dos directorios
		char *pathIzquierda=argv[2];
		char *pathDerecha=argv[3];
		sync(pathIzquierda,pathDerecha,0,1);


	}else
	if(!strcmp(argv[1],"copy") && argc==4){
		// Sincronizar dos directorios
		char *pathIzquierda=argv[2];
		char *pathDerecha=argv[3];
		copy(pathIzquierda,pathDerecha,1,0);
	}else
	if(!strcmp(argv[1],"recopy") && argc==4){
		// Sincronizar dos directorios
		char *pathIzquierda=argv[2];
		char *pathDerecha=argv[3];
		copy(pathIzquierda,pathDerecha,0,0);
	}else
	if(!strcmp(argv[1],"copytest") && argc==4){
		// Sincronizar dos directorios
		char *pathIzquierda=argv[2];
		char *pathDerecha=argv[3];
		copy(pathIzquierda,pathDerecha,1,1);
	}else
	if(!strcmp(argv[1],"recopytest") && argc==4){
		// Sincronizar dos directorios
		char *pathIzquierda=argv[2];
		char *pathDerecha=argv[3];
		copy(pathIzquierda,pathDerecha,0,1);


	}else{
		help(argv[0]);
	}

	return(0);
}



FileNode *checkDir(char *path,int recheck){
	char dirNodesFile[MaxPath];
	FileNode *fn;
	// Comprobar directorio
	snprintf(dirNodesFile,MaxPath,"%s/"FileNode_Filename,path);
	if(recheck){
		printf("Checking Directory.. %s\n",path);
		fn=FileNode_Load(dirNodesFile);
		if(fn){
			fn=FileNode_Refresh(fn,path);
		}else{
			fn=FileNode_Build(path);
		}
		FileNode_Save(fn,dirNodesFile);
	}else{
		printf("Loading Directory.. %s\n",path);
		fn=FileNode_Load(dirNodesFile);
		if(!fn){
			printf("Error, no nodesFile.fs\n");
			return NULL;
		}
	}
	return fn;
}

int sync(char *pathIzquierda,char *pathDerecha,int recheck,int dryrun){
	char dirNodesFileIzq[MaxPath];
	char dirNodesFileDer[MaxPath];
	FileNode *fnIzquierda,*fnDerecha;

	// Comprobar y cargar directorios
	if(!File_ExistePath(pathIzquierda) || !File_EsDirectorio(pathIzquierda)){
		printf("Error, directory does not exist: %s\n",pathIzquierda);
		return 0;
	}
	if(!File_ExistePath(pathDerecha) || !File_EsDirectorio(pathDerecha)){
		printf("Error, directory does not exist: %s\n",pathDerecha);
		return 0;
	}
	fnIzquierda=checkDir(pathIzquierda,recheck);
	if(!fnIzquierda){return 0;}
	fnDerecha=checkDir(pathDerecha,recheck);
	if(!fnDerecha){return 0;}


	// Construir acciones
	printf("Building action list.. \n");
	AccionFileNode *afn=NULL;
	afn=AccionFileNode_BuildSync(fnIzquierda,fnDerecha);

	if(dryrun){
		// Mostrar lista de acciones
		AccionFileNode_Print(afn);
	}else{
		// Ejecutar lista de acciones
		AccionFileNode_RunList(afn,pathIzquierda,pathDerecha);
	}

	return(1);
}

int copy(char *pathIzquierda,char *pathDerecha,int recheck,int dryrun){
	char dirNodesFileIzq[MaxPath];
	char dirNodesFileDer[MaxPath];
	FileNode *fnIzquierda,*fnDerecha;

	// Comprobar y cargar directorios
	if(!File_ExistePath(pathIzquierda) || !File_EsDirectorio(pathIzquierda)){
		printf("Error, directory does not exist: %s\n",pathIzquierda);
		return 0;
	}
	if(!File_ExistePath(pathDerecha) || !File_EsDirectorio(pathDerecha)){
		printf("Error, directory does not exist: %s\n",pathDerecha);
		return 0;
	}
	fnIzquierda=checkDir(pathIzquierda,recheck);
	if(!fnIzquierda){return 0;}
	fnDerecha=checkDir(pathDerecha,recheck);
	if(!fnDerecha){return 0;}


	// Construir acciones
	printf("Building action list.. \n");
	AccionFileNode *afn=NULL;
	afn=AccionFileNode_BuildCopy(fnIzquierda,fnDerecha);

	if(dryrun){
		// Mostrar lista de acciones
		AccionFileNode_Print(afn);
	}else{
		// Ejecutar lista de acciones
		AccionFileNode_RunList(afn,pathIzquierda,pathDerecha);
	}

	return(1);
}
