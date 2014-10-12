#include "dpe_base/utility_interface.h"
#include "dpe_base/thread_pool.h"
#include "dpe_base/chromium_base.h"
#include <iostream>
#include <windows.h>
#include <process.h>
using namespace std;
void SayHello()
{
  cerr << GetCurrentThreadId() << endl;
  cerr << "hello world" << endl;
}
void will_quit()
{
  cerr << GetCurrentThreadId() << endl;
  base::MessageLoop::current()->Quit();
}
int main()
{
  cerr << GetCurrentThreadId() << endl;
  base::ThreadPool::InitializeThreadPool();
  base::ThreadPool::PostTask(base::ThreadPool::FILE, FROM_HERE,
    base::Bind(SayHello));
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(will_quit));
  base::ThreadPool::RunMainLoop();
  base::ThreadPool::DeinitializeThreadPool();
  return 0;
}