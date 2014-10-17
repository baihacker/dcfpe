#include "dpe_base/dpe_base.h"
#include "process/process.h"
#include "dpe_service/main/dpe_service.h"

#include <windows.h>
#include <iostream>
using namespace std;
ds::DPEService service;

void dpe_service_main()
{
  service.Start();
}

#if 0
int CALLBACK wWinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  wchar_t* lpCmdLine,
  int nCmdShow
)
{
  base::dpe_base_main(dpe_service_main);
  return 0;
}
#else
int main()
{
  base::dpe_base_main(dpe_service_main);
  return 0;
}
#endif