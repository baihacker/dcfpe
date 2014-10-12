#include "dpe_base/utility_interface.h"
#include "dpe_base/thread_pool.h"
#include "dpe_base/chromium_base.h"
#include <iostream>
#include <windows.h>
#include <process.h>
using namespace std;

void SayHello()
{
  cerr << "hello world" << endl;
}
void will_quit()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  base::MessageLoop::current()->Quit();
}
int main()
{
  //auto x = logging::LoggingSettings();
  //x.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  //logging::BaseInitLoggingImpl(x);
  DLOG(INFO) << "orz";
  base::ThreadPool::InitializeThreadPool();
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(SayHello));
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(will_quit));
  base::ThreadPool::RunMainLoop();
  base::ThreadPool::DeinitializeThreadPool();
  return 0;
}