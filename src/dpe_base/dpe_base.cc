#include "dpe_base/dpe_base.h"

namespace base
{
static void will_quit()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  base::MessageLoop::current()->Quit();
}
void quit_main_loop()
{
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(will_quit));
}
static MessageCenter* zmq = NULL;
int32_t dpe_base_main(void (*logic_main)())
{
  {
    auto x = logging::LoggingSettings();
    x.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
    logging::BaseInitLoggingImpl(x);
    PLOG(INFO) << "dpe base main running";
  }
  PLOG(INFO) << "InitializeThreadPool";
  base::ThreadPool::InitializeThreadPool();
  
  zmq = new MessageCenter();
  zmq->Start();
  
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
        base::Bind(logic_main));
  
  PLOG(INFO) << "RunMainLoop";
  base::ThreadPool::RunMainLoop();
  
  delete zmq;
  zmq = NULL;
  
  PLOG(INFO) << "DeinitializeThreadPool";
  base::ThreadPool::DeinitializeThreadPool();
  
  return 0;
}
MessageCenter* zmq_message_center()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  return zmq;
}
}
