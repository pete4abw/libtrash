#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "trash.h"

int main(void)
{
   fopen("", "r");
   fopen64("", "r");
   freopen("", "r", NULL);
   freopen64("", "r", NULL);
   open("", O_RDONLY);
   open64("", O_RDONLY);
   creat("", S_IRWXU);
   creat64("", S_IRWXU);
   
   unlink("");
   rename("", "");
   
#ifdef AT_FUNCTIONS

   unlinkat(0, "", 0);
   renameat(0, "", 0, "");
   openat(0, "", 0);
   openat64(0, "", 0);

#endif
     
   return 0;
}
