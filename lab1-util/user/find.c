#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


//get the filename in nowly path
char*
fmtname(char *path)
{
  static char buf[DIRSIZ+1];
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--);
  p++;

  // Return blank-padded name.
  if(strlen(p) >= DIRSIZ)
    return p;
  memmove(buf, p, strlen(p));
  memset(buf+strlen(p), '\0', DIRSIZ-strlen(p));
  return buf;
}
//find file
void find(char*path,char*filename)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if((fd=open(path,0))<0){
    fprintf(2,"find: cannot open%s\n",path);
    exit(1);
  }

  if(fstat(fd,&st)<0){
    fprintf(2,"find: cannot get state of %s\n",path);
    close(fd);
    exit(1);
  }

  switch(st.type){
    case T_FILE: //if file,then check the name
      if(strcmp(fmtname(path),filename)==0){
	printf("%s\n",path);
      }
      break;
    case T_DIR: //if dir,then call find recursively
      if(strlen(path)+1+DIRSIZ+1>sizeof(buf)){
	fprintf(2,"find: path too long\n");
	exit(1);
      }
      //copy path to buf and add /
      strcpy(buf,path);
      p=buf+strlen(buf);
      *p++='/';
      //read directory entry(dirent)
      while(read(fd,&de,sizeof(de))==sizeof(de)){
	if(de.inum==0){ //invalid directory entry
	  continue;
	}
	//add de.name to path in buf
	memmove(p,de.name,DIRSIZ);
	//add ending in string
	p[DIRSIZ]=0;
	//if de.name=="."or"..",then don't recurse
	if(!strcmp(de.name,".")||!strcmp(de.name,".."))continue;
	find(buf,filename);
      }
      break;
  }
  close(fd);
}
//use find()
int
main(int argc, char *argv[])
{
  if(argc!=3){
    fprintf(2,"usage: find <path> <filename>\n");
    exit(1);
  }
  find(argv[1],argv[2]);
  exit(0);
}
