#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"
#include "filenodecmp.h"

void help(char *exe){
	char exeFilename[1024];
	File_GetName(exe,exeFilename);
	printf("Modo de uso:\n");
	printf("\t%s info [file] {[file] {..}}\n",exeFilename);
	printf("\t%s scan [dir] [tree] \n",exeFilename);
	printf("\t%s rescan [dir] [tree] \n",exeFilename);
	printf("\t%s read [file] [tree]\n",exeFilename);
	printf("\t%s dir [dir]\n",exeFilename);
	printf("\t%s sync [dirIzquierda] [dirDerecha]\n",exeFilename);
}

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
		char dirNodesFile[4092];
		FileNode *fn;

		printf("Checking Directory.. %s\n",path);
		if(File_ExistePath(path) && File_EsDirectorio(path)){
			// Get the FileNode from the dir
			snprintf(dirNodesFile,4092,"%s/"FileNode_Filename,path);
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
		// Leer informacion de dir
		char *pathIzquierda=argv[2];
		char *pathDerecha=argv[3];
		char dirNodesFileIzq[4092];
		char dirNodesFileDer[4092];
		FileNode *fnIzquierda,*fnDerecha;

		if(!File_ExistePath(pathIzquierda) || !File_EsDirectorio(pathIzquierda))
		{
			printf("Error, directory does not exist: %s\n",pathIzquierda);
			return 0;
		}
		if(!File_ExistePath(pathDerecha) || !File_EsDirectorio(pathDerecha))
		{
			printf("Error, directory does not exist: %s\n",pathDerecha);
			return 0;
		}

		// Comprobar directorio izquierdo
		printf("Checking Directory.. %s\n",pathIzquierda);
		snprintf(dirNodesFileIzq,4092,"%s/"FileNode_Filename,pathIzquierda);
		fnIzquierda=FileNode_Load(dirNodesFileIzq);
		if(fnIzquierda){
			fnIzquierda=FileNode_Refresh(fnIzquierda,pathIzquierda);
		}else{
			fnIzquierda=FileNode_Build(pathIzquierda);
		}

		// Comprobar directorui derecho
		printf("Checking Directory.. %s\n",pathDerecha);
		snprintf(dirNodesFileDer,4092,"%s/"FileNode_Filename,pathDerecha);
		fnDerecha=FileNode_Load(dirNodesFileDer);
		if(fnDerecha){
			fnDerecha=FileNode_Refresh(fnDerecha,pathDerecha);
		}else{
			fnDerecha=FileNode_Build(pathIzquierda);
		}

		// Construir acciones
		printf("Building action list.. \n");
		AccionFileNode *afn=NULL;
		afn=AccionFileNode_Build(fnIzquierda,fnDerecha);
		AccionFileNode_Print(afn);


	}else{
		help(argv[0]);
	}


/*
	if(argc<2){
		return(1);
	}

	f=fopen(argv[1],"rb");
	if(f){
		crc=CRC_File(f);
		fclose(f);
		printf("%s:\t%08X\n",argv[1],crc);
	}
*/

/*
	if(argc<2){
		return(1);
	}

	//printf("%d\n",FileTime_Get(argv[1]));
	FileTime ft;
	ft=FileTime_Get(argv[1]);
	FileTime_Print(ft);printf("\n");
	FileTime_Set(argv[1],ft+120);
	ft=FileTime_Get(argv[1]);
	FileTime_Print(ft);printf("\n");
*/


/*
	if(argc<2){
		return(1);
	}

	FileNode *fn;
	printf("Building FileNode..\n");
	fn=FileNode_Build(argv[1]);
	//printf("FileNode Contents:\n");
	//FileNode_Print(fn);
	extern int _n_filenode;
	printf("%d\n",_n_filenode);
	printf("END\n");

	FileNode_Save(fn,"test2.fs");
*/
/*

	FileNode *fn;
	fn=FileNode_Load("test2.fs");
	FileNode_Print(fn);
*/




	return(0);
}