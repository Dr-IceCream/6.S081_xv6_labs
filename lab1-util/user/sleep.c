#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;

  if(argc != 2){
    fprintf(2, "Usage: sleep <duration>\n");
    exit(1);
  }

  for(i = 1; i < argc; i++){
    int duration=atoi(argv[i]);
    if(duration==0){
      fprintf(2,"please input a valid duration\n");
      break;
    }
    if(sleep(duration) < 0){
      fprintf(2, "sleep: %s failed to execute\n", argv[i]);
      break;
    }
  }

  exit(0);
}
