#include <iostream>
#include <windows.h>
#include <process.h>
#include "dpe_base/dpe_base.h"
using namespace std;

void SayHello()
{
  cerr << "hello world" << endl;
  base::quit_main_loop();
}

int main()
{
  base::dpe_base_main(SayHello);
  return 0;
}