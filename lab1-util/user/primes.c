#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void childFilter(int parent_read){
    int temp;
    if(read(parent_read,&temp,sizeof(temp))==0){ //no more prime
        close(parent_read);
	exit(0);
    }
    printf("prime %d\n",temp);
    //set pipe for child
    int newp[2];
    pipe(newp);
    int pid=fork();
    if(pid<0){
	fprintf(2,"fail to fork\n");
	exit(1);
    }
    else if(pid>0){
        close(newp[0]); //read parent and write child (close read child)
	int prime=temp; //the first is prime
	while(read(parent_read,&temp,sizeof(temp))>0){
	    if(temp%prime!=0){
	    write(newp[1],&temp,sizeof(temp));
	    }
	}
	close(parent_read);
	close(newp[1]);
	wait(0);
    }
    else{
	close(newp[1]);
	childFilter(newp[0]);
    }
}


int
main(int argc, char *argv[])
{
  if(argc != 1){
    fprintf(2, "Usage: primes\n");
    exit(1);
  }

  //create a pipe
  int p[2];
  pipe(p);

  int pid=fork();
  if(pid<0){
    fprintf(2,"fail to fork\n");
    exit(1);
  }
  else if(pid>0){ //use parent to write
    close(p[0]);
    for(int i=2;i<=35;i++){	
      write(p[1],&i,sizeof(i));
    }
    close(p[1]);
    wait(0);
  }
  else{ //use child to read and create its child
    close(p[1]);
    childFilter(p[0]);
  }

  exit(0);
}
