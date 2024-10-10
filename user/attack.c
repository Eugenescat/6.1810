#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user/user.h"
#include "kernel/riscv.h"

int
main(int argc, char *argv[])
{
  char *end = sbrk(PGSIZE*32);
  end = end + 8 * PGSIZE;

  char *secret = end + 16;

  write(2, secret, 8);

  exit(0);
}
