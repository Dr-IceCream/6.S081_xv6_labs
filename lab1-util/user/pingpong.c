#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  if(argc != 1){
    fprintf(2, "Usage: pingpong\n");
    exit(1);
  }

  //define two pipes.
  int prcw[2]; //parent read and children write
  int pwcr[2]; //parent write and children read
  pipe(prcw);
  pipe(pwcr);

  char buf[5]; //used in read() to store inf

  int pid=fork();
  if(pid<0){
	  fprintf(2,"fail to fork");
	  exit(1);
  }
  else if(pid>0){ //now process is parent process
	  close(pwcr[0]);
	  close(prcw[1]);
	  write(pwcr[1],"ping",4);
	  close(pwcr[1]);
	  read(prcw[0],buf,4);
	  printf("%d: received %s\n",getpid(),buf);
  }
  else{  //now process is children process
	  close(pwcr[1]);
          close(prcw[0]);
          read(pwcr[0],buf,4);  //before write use read for pingpong order
          printf("%d: received %s\n",getpid(),buf);
          write(prcw[1],"pong",4);
          close(prcw[1]);
  }


  exit(0);
}
