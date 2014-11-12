#include <cstdio>

int main(int x, char* argv[])
{
  printf("%s\n", argv[1]);
  printf("work now\n");
  fflush(stdout);
  int n;
  scanf("%d", &n);
  printf("n=%d\n", n);
  fflush(stdout);
  int s = 0;
  for (int i = 1; i <= n; ++i)
  {
    printf("i=%d\n", i);
    fflush(stdout);
    int x;
    scanf("%d", &x);
    s += x;
    printf("x=%d\n", x);
    fflush(stdout);
  }
  printf("%d\n", s);
  return 0;
}