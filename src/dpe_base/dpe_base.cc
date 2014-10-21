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

static MessageCenter* msg_center_impl = NULL;
static ZMQServer* zmq_server_impl = NULL;
static ZMQClient* zmq_client_impl = NULL;

int32_t dpe_base_main(void (*logic_main)())
{
  {
    auto x = logging::LoggingSettings();
    x.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
    logging::BaseInitLoggingImpl(x);
    LOG(INFO) << "dpe base main running";
  }
  LOG(INFO) << "InitializeThreadPool";
  base::ThreadPool::InitializeThreadPool();
  
  msg_center_impl = new MessageCenter();
  msg_center_impl->Start();
  
  zmq_server_impl = new ZMQServer();
  zmq_server_impl->Start();
  
  zmq_client_impl = new ZMQClient();
  zmq_client_impl->Start();
  
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
        base::Bind(logic_main));
  
  LOG(INFO) << "RunMainLoop";
  base::ThreadPool::RunMainLoop();
  
  delete zmq_client_impl;
  zmq_client_impl = NULL;
  
  delete zmq_server_impl;
  zmq_server_impl = NULL;
  
  delete msg_center_impl;
  msg_center_impl = NULL;
  
  LOG(INFO) << "DeinitializeThreadPool";
  base::ThreadPool::DeinitializeThreadPool();
  
  return 0;
}

MessageCenter* zmq_message_center()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  return msg_center_impl;
}

ZMQServer* zmq_server()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  return zmq_server_impl;
}

ZMQClient* zmq_client()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  return zmq_client_impl;
}

}

namespace base
{
bool StringEqualCaseInsensitive(const std::wstring& x, const std::wstring& y)
{
  const int n = x.size();
  const int m = y.size();
  if (n != m) return false;
  for (int i = 0; i < n; ++i)
  if (tolower(x[i]) != tolower(y[i]))
    return false;
  return true;
}
bool StringEqualCaseInsensitive(const std::string& x, const std::string& y)
{
  const int n = x.size();
  const int m = y.size();
  if (n != m) return false;
  for (int i = 0; i < n; ++i)
  if (tolower(x[i]) != tolower(y[i]))
    return false;
  return true;
}
#if defined(OS_WIN)
  std::wstring NativeToWide(const NativeString& x) {return x;}
  std::string NativeToUTF8(const NativeString& x) {return base::SysWideToUTF8(x);}
  NativeString WideToNative(const std::wstring& x) {return x;}
  NativeString UTF8ToNative(const std::string& x) {return base::SysUTF8ToWide(x);}
#elif defined(OS_POSIX)
  std::wstring NativeToWide(const NativeString& x) {return base::SysUTF8ToWide(x);}
  std::string NativeToUTF8(const NativeString& x) {return x;}
  NativeString WideToNative(const std::wstring& x) {return base::SysWideToUTF8(x);}
  NativeString UTF8ToNative(const std::string& x) {return x;}
#endif
}
