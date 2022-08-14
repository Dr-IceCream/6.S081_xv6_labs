#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char*params[MAXARG];
  char line[1024];
  int anchor=0;

  //copy argvs to params
  char*cmd=argv[1];
  for(int i=1;i<argc;i++){
    params[anchor++]=argv[i];
  }
  int free_anchor=anchor;
  int num=0;
  while((num=read(0,line,1024))>0){
    int pid=fork();
    if(pid<0){
      fprintf(2,"fail to fork\n");
      exit(1);
    }
    else if(pid==0){ //child
      char*arg=(char*)malloc(sizeof(line));
      int index=0;
      for(int i=0;i<num;i++){ //get complete statement
	if(line[i]!=' '&&line[i]!='\n'){
	  arg[index++]=line[i];
	}
	else{
	  arg[index]=0;
	  params[anchor++]=arg;
	  index=0;
	  arg=(char*)malloc(sizeof(line));
	}
      }
      params[anchor]=0;
      exec(cmd,params);
      for(int i=free_anchor;i<anchor;i++){
	      free(params[i]);
      }
    }
    else{
      wait((int*)(0));
    }
  }

  exit(0);
}
