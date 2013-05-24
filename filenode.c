#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "crc.h"
#include "fileutil.h"
#include "filenode.h"



FileNode *_free_filenode=NULL;
int _n_filenode=0;
#define FileNode_Tocho 1024
FileNode *FileNode_New(){
	FileNode *fn;

	if(_free_filenode==NULL){
		FileNode *nodos;
		int i;
		// Reservar un tocho
		nodos=malloc(sizeof(FileNode)*FileNode_Tocho);
		for(i=0;i<FileNode_Tocho-1;i++){
			nodos[i].sig=&nodos[i+1];
		}
		nodos[FileNode_Tocho-1].sig=NULL;
		_free_filenode=&nodos[0];
	}

	// Obtener el primero libre
	fn=_free_filenode;
	_free_filenode=fn->sig;
	_n_filenode++;

	// Iniciar
	fn->name[0]=0;
	fn->flags=0;
	fn->estado=EstadoFichero_Nada;
	fn->size=0;
	fn->crc=0;
	fn->ft=0;
	fn->child=NULL;
	fn->n_childs=0;
	fn->sig=NULL;
	fn->padre=NULL;

	return(fn);
}

void FileNode_Delete(FileNode *fn){
	fn->sig=_free_filenode;
	_free_filenode=fn;
	_n_filenode--;
}

void FileNode_AddChild(FileNode *file,FileNode *file2){
	if(!file2 || !file)
		return;
	file2->sig=file->child;
	file->child=file2;
	file->n_childs++;
	file2->padre=file;
}


void FileNode_SetEstadoRec(FileNode *file,EstadoFichero estado){
	FileNode *fn_child;
	file->estado=estado;
	fn_child=file->child;
	while(fn_child!=NULL){
		FileNode_SetEstadoRec(fn_child,estado);
		fn_child=fn_child->sig;
	}
}

void FileNode_GetPath_Rec(FileNode *fn,char **pathnode){
	if(fn->padre){
		pathnode[0]=fn->padre->name;
		FileNode_GetPath_Rec(fn->padre,pathnode+1);
	}else{
		pathnode[0]=NULL;
	}
}
char temppath[1024];
char *FileNode_GetPath(FileNode *fn,char *path){
	char *pathnodes[100];
	int levels,i;
	char *pathptr=temppath;
	if(path)pathptr=path;

	FileNode_GetPath_Rec(fn,pathnodes);
	levels=0;while(pathnodes[levels]){levels++;}
	strcpy(pathptr,"");
	for(i=levels-1;i>=0;i--){
		strcat(pathptr,pathnodes[i]);
		strcat(pathptr,"/");
	}
	strcat(pathptr,fn->name);
}



void FileNode_GetTamanho(FileNode *fn,char *file){
	fn->flags|=FileFlag_TieneTamanho;
	fn->size=File_TamanhoFichero(file);
}

void FileNode_GetFecha(FileNode *fn,char *file){
	fn->flags|=FileFlag_TieneFecha;
	fn->ft=FileTime_Get(file);
}

void FileNode_GetCRC(FileNode *fn,char *file){
	FILE *f;
	f=fopen(file,"rb");
	if(!f){ return; }
	fn->flags|=FileFlag_TieneCRC;
	fn->crc=CRC_File(f);
	fclose(f);
}







void FileNode_SaveNode(FileNode *fn,FILE *file){
	short name_len;

	// Escribir nombre
	name_len=strlen(fn->name);
	fwrite((void *)&name_len,sizeof(name_len),1,file);
	fputs(fn->name,file);

	// Escribir flags
	fwrite((void *)&fn->flags,sizeof(fn->flags),1,file);

	// Escribir estado
	fputc((char)fn->estado,file);

	// Escribir tamanho
	if(fn->flags&FileFlag_TieneTamanho){
		fwrite((void *)&fn->size,sizeof(fn->size),1,file);
	}

	// Escribir fecha
	if(fn->flags&FileFlag_TieneFecha){
		fwrite((void *)&fn->ft,sizeof(fn->ft),1,file);
	}

	// Escribir CRC
	if(fn->flags&FileFlag_TieneCRC){
		fwrite((void *)&fn->crc,sizeof(fn->crc),1,file);
	}

	// Escribir ficheros del directorio
	if(fn->flags&FileFlag_Directorio){
		FileNode *fnc;
		fwrite((void *)&fn->n_childs,sizeof(fn->n_childs),1,file);
		fnc=fn->child;
		while(fnc){
			FileNode_SaveNode(fnc,file);
			fnc=fnc->sig;
		}
	}
}

void FileNode_Save(FileNode *fn,char *fichero){
	FILE *file;
	char marca[5];
	int version;

	if(!fn)
		return;
	file=fopen(fichero,"wb+");
	if(!file)
		return;

	// Escribir marca y version
	strcpy(marca,"sYnC");
	fwrite((void *)marca,sizeof(char),4,file);
	version=FileNode_Version;
	fwrite((void *)&version,sizeof(int),1,file);


	FileNode_SaveNode(fn,file);
	fclose(file);
}


FileNode *FileNode_LoadNode(FILE *file){
	short name_len;
	FileNode *fn;
	int i;

	fn=FileNode_New();

	// Leer el nombre
	fread((void *)&name_len,sizeof(name_len),1,file);
	fread((void *)fn->name,sizeof(char),name_len,file);
	fn->name[name_len]=0;

	// Leer vanderas
	fread((void *)&fn->flags,sizeof(fn->flags),1,file);

	// Leer estado
	fn->estado=fgetc(file);

	// Leer tamanho
	if(fn->flags&FileFlag_TieneTamanho){
		fread((void *)&fn->size,sizeof(fn->size),1,file);
	}

	// Leer fecha
	if(fn->flags&FileFlag_TieneFecha){
		fread((void *)&fn->ft,sizeof(fn->ft),1,file);
	}

	// Leer CRC
	if(fn->flags&FileFlag_TieneCRC){
		fread((void *)&fn->crc,sizeof(fn->crc),1,file);
	}

	// Leer ficheros del directorio
	if(fn->flags&FileFlag_Directorio){
		FileNode *fnca=NULL,*fnc;
		fread((void *)&fn->n_childs,sizeof(fn->n_childs),1,file);
		for(i=0;i<fn->n_childs;i++){
			fnc=FileNode_LoadNode(file);
			fnc->padre=fn;
			if(!fnca){
				fn->child=fnc;
			}else{
				fnca->sig=fnc;
			}
			fnca=fnc;
		}
	}

	return(fn);
}


FileNode *FileNode_Load(char *fichero){
	FILE *file;
	FileNode *fn;
	char marca[5];
	int version;

	file=fopen(fichero,"rb");
	if(!file)
		return(NULL);

	// Leer marca y version
	fread((void *)marca,sizeof(char),4,file);
	marca[4]=0;
	if(strcmp(marca,"sYnC")){
		// Marca incorrecta
		fclose(file);
		return(NULL);
	}
	fread((void *)&version,sizeof(int),1,file);
	if(version!=FileNode_Version){
		// Version incorrecta
		fclose(file);
		return(NULL);
	}


	fn=FileNode_LoadNode(file);
	fclose(file);

	return(fn);
}





void FileNode_Print(FileNode *fn){
	FileNode *padre;

	// Nombre
	printf(FileNode_GetPath(fn,NULL));
	if(fn->flags&FileFlag_Normal){
		printf(" File");
	}else{
		printf(" Dir");
	}
	printf(" %d",fn->estado);
	if(fn->estado==EstadoFichero_Nuevo){
		printf(" Nuevo");
	}
	if(fn->estado==EstadoFichero_Modificado){
		printf(" Modificado");
	}
	if(fn->estado==EstadoFichero_Borrado){
		printf(" Borrado!!!");
	}
	printf("\n");

/*
	// Tamanho
	if(fn->flags&FileFlag_TieneTamanho){
		printf("\\-Tamanho: %lld\n",fn->size);
	}

	// Fecha
	if(fn->flags&FileFlag_TieneFecha){
		printf("\\-Fecha  : ");FileTime_Print(fn->ft);printf("\n");
	}

	// CRC
	if(fn->flags&FileFlag_TieneCRC){
		printf("\\-CRC    : [%08X]\n",fn->crc);
	}
*/
	// Hijos
	if(fn->flags&FileFlag_Directorio){
		FileNode *fn2;
		fn2=fn->child;
		while(fn2){
			FileNode_Print(fn2);
			fn2=fn2->sig;
		}
	}
}
















int FileNode_Build_Iterate(char *path,char *name,void *d);

FileNode *FileNode_Build(char *path){
	FileNode *file;

	if(!File_ExistePath(path))
		return(NULL);

	// Crear el nodo
	file=FileNode_New();
	File_GetName(path,file->name);

	// Determinar si es un fichero o directorio
	if(File_EsDirectorio(path)){
		// Obtener datos para los directorios
		file->flags|=FileFlag_Directorio;
		FileNode_GetFecha(file,path);
		File_IterateDir(path,FileNode_Build_Iterate,file);
	}else{
		// Obtener datos para los ficheros
		file->flags|=FileFlag_Normal;
		FileNode_GetTamanho(file,path);
		FileNode_GetFecha(file,path);
	}

	return(file);
}


int FileNode_Build_Iterate(char *path,char *name,void *d){
	FileNode *file,*fn_padre=d;;

	if(!strcmp(name,FileNode_Filename)){
		return(0);
	}

	file=FileNode_Build(path);
	FileNode_AddChild(fn_padre,file);

	return(0);
}















int FileNode_Refresh_Iterate(char *path,char *name,void *d);

FileNode *FileNode_Refresh(FileNode *fn,char *path){


	if(!File_ExistePath(path)){
		// El fichero ha sido borrado
		if(!fn){
			fn=FileNode_New();
			File_GetName(path,fn->name);
		}
		FileNode_SetEstadoRec(fn,EstadoFichero_Borrado);
		return(fn);
	}
	if(!fn){
		// El fichero ha sido creado
		fn=FileNode_Build(path);
		FileNode_SetEstadoRec(fn,EstadoFichero_Nuevo);
	}else{
		// Comprobar si ha sido modificado
		FileTime ft;
		long long size;
		int crc;

		// Marcar normal
		fn->estado=EstadoFichero_Nada;
		fn->flags&=~FileFlag_MarcaRevision;

		// Determinar si es un fichero o directorio
		if(File_EsDirectorio(path)){
			FileNode *fn_child;

			// Comparar datos de los directorios
			if(!(fn->flags&FileFlag_Directorio)){
				fn->estado=EstadoFichero_Modificado;
				fn->flags|=FileFlag_Directorio;
				fn->flags&=~FileFlag_Normal;
			}
			ft=FileTime_Get(path);
			if(ft!=fn->ft){
				fn->estado=EstadoFichero_Modificado;
				fn->ft=ft;
			}

			// Marcar hijos para determinar cual es actualizado
			fn_child=fn->child;while(fn_child){
				fn_child->flags|=FileFlag_MarcaRevision;
				fn_child=fn_child->sig;
			}

			// Escanear subdirectorios
			File_IterateDir(path,FileNode_Refresh_Iterate,fn);

			// Buscar que sigan marcados (borrados)
			fn_child=fn->child;while(fn_child){
				if(fn_child->flags&FileFlag_MarcaRevision){
					fn_child->flags&=~FileFlag_MarcaRevision;
					fn_child->estado=EstadoFichero_Borrado;
				}
				fn_child=fn_child->sig;
			}
		}else{
			// Comprar datos de los ficheros
			if(!(fn->flags&FileFlag_Normal)){
				fn->estado=EstadoFichero_Modificado;
				fn->flags|=FileFlag_Normal;
				fn->flags&=~FileFlag_Directorio;
			}
			size=File_TamanhoFichero(path);
			if(size!=fn->size){
				fn->estado=EstadoFichero_Modificado;
				fn->size=size;
			}
			ft=FileTime_Get(path);
			if(ft!=fn->ft){
				fn->estado=EstadoFichero_Modificado;
				fn->ft=ft;
			}
			if(fn->estado==EstadoFichero_Modificado){
				fn->flags&=~FileFlag_TieneCRC;
			}
		}
	}
	return(fn);
}

int FileNode_Refresh_Iterate(char *path,char *name,void *d){
	FileNode *fn=d;
	FileNode *fn_child;

	if(!strcmp(name,FileNode_Filename)){
		return(0);
	}

	// Buscar el fichero entre los del arbol
	fn_child=fn->child;
	while(fn_child){
		if(!strcmp(fn_child->name,name)){
			break;
		}
		fn_child=fn_child->sig;
	}
	if(fn_child){
		// Existe, refrescar
		FileNode_Refresh(fn_child,path);
	}else{
		// Nuevo, construir
		fn_child=FileNode_Refresh(NULL,path);
		FileNode_AddChild(fn,fn_child);
	}

	return(0);
}










