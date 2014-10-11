#include "dpe_base/utility_interface.h"
#include "dpe_base/chromium_base.h"
#include <iostream>
#include <windows.h>
#include <process.h>
using namespace std;

unsigned __stdcall ThreadMain(void * arg)
{
  MSG msg;
  BOOL bRet;
  while((bRet=GetMessage(&msg,NULL,0,0))!=0)
  {
    if(bRet==-1)
    {
      
    }
    else
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  return 0;
}

VOID CALLBACK APCProc(
  _In_  ULONG_PTR dwParam
)
{
  cerr << "apc invoked" << endl;
}

InterfacePtr<IMessageCenter>  msg_center;

struct MessageHandler : public DPESingleInterfaceObjectRoot<IMessageHandler, RefCountedObjectModelEx>
{
  MessageHandler()
  {
    test_data = 0;
  }
  ~MessageHandler()
  {
    test_data = -1;
  }
  int32_t handle_message(
        int32_t channel_id, int32_t is_ctrl, const char* msg, int32_t length) override
  {
    cerr << endl << "begin_message" << endl;
    cerr << GetCurrentThreadId() << endl;
    cerr << is_ctrl << endl;
    cerr << msg << endl;
    cerr << test_data << endl;
    cerr << "end_message" << endl << endl;
    return 1;
  }
private:
  int test_data;
};

int main()
{
  cerr << GetCurrentThreadId() << endl;
  CreateUtility(INTERFACE_MESSAGE_CENTER, msg_center.storage());
  InterfacePtr<IMessageHandler> handler = new MessageHandler();
  msg_center->add_message_handler(handler);
  
  msg_center->start();
  
  msg_center->send_ctrl_message("123", 4);
  Sleep(16);
  handler = NULL;
  
  
  #if 0
    unsigned id = 0;
    HANDLE thread_handle_ = (HANDLE)_beginthreadex(NULL, 0,
        &ThreadMain, (void*)0, 0, &id);
    Sleep(1000);
    QueueUserAPC(APCProc, thread_handle_, NULL);
    Sleep(3000);
     // Sleep(10000000);
     ::WaitForSingleObject(reinterpret_cast<HANDLE>(msg_center->worker_handle()), -1);
     QueueUserAPC(APCProc, reinterpret_cast<HANDLE>(msg_center->worker_handle()), NULL);
     Sleep(30);
  #endif
  msg_center->stop();
  return 0;
}