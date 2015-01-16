#include "dpe_base/dpe_base.h"
#include "dpe_base/support_wtl_message_loop.h"
#include "process/process.h"
#include "dpe_service/main/dpe_service.h"
#include "dpe_service/main/dpe_model/dpe_shell.h"

#include <windows.h>
#include <Objbase.h>

ds::DPEService service;
CAppModule _Module;
void dpe_service_main()
{
  service.Start();
}

#if 1
int CALLBACK wWinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  wchar_t* lpCmdLine,
  int nCmdShow
)
{
  HRESULT hRes = ::CoInitialize(NULL);
  ::DefWindowProc(NULL, 0, 0, 0L);
  AtlInitCommonControls(ICC_BAR_CLASSES);
  hRes = _Module.Init(NULL, hInstance);
  CChromiumMessageLoop theLoop;
  _Module.AddMessageLoop(&theLoop);
  AllocConsole();
  freopen("CONOUT$","w+t",stdout);
  freopen("CONOUT$","w+t",stderr);
  freopen("CONIN$","r+t",stdin);
  
  CDPEShellDlg dlg;
  dlg.Create(NULL);
  dlg.ShowWindow(SW_SHOW);

  base::dpe_base_main(dpe_service_main, &theLoop);
  _Module.RemoveMessageLoop();

  return 0;
}
#else
int main()
{
  base::dpe_base_main(dpe_service_main);
  return 0;
}
#endif