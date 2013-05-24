#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"

void help(char *exe){
	printf("%s info [file] {[file] {..}}\n",exe);
	printf("%s scan [dir] [tree] \n",exe);
	printf("%s rescan [dir] [tree] \n",exe);
	printf("%s read [file] [tree]\n",exe);
	printf("%s dir [dir]\n",exe);
}

int main(int argc,char *argv[]){
	FILE *f;
	unsigned long crc;
	FileTime ft;
	int i;

	if(argc<2){
		help(argv[0]);
	}else
	if(!strcmp(argv[1],"info") && argc>=3){
		// Informacion de ficheros
		for(i=2;i<argc;i++){
			f=fopen(argv[i],"rb");
			if(f){
				crc=CRC_File(f);
				fclose(f);
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