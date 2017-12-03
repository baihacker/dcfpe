#include "dpe_base/zmq_adapter.h"

#include <algorithm>
#include <process.h>
#include "dpe_base/thread_pool.h"

#include <zmq.h>

namespace base
{
ZMQClient::ZMQClient() :
  status_(STATUS_PREPARE),
  quit_flag_(0),
  thread_handle_(NULL),
  zmq_context_(NULL),
  zmq_ctrl_pub_(NULL),
  zmq_ctrl_sub_(NULL),
  weakptr_factory_(this)
{
  zmq_context_ = zmq_ctx_new();
  const int32_t ctrl_ip = AddressHelper::GetCtrlIP();
  for (;;)
  {
    ctrl_address_ = AddressHelper::MakeZMQTCPAddress(ctrl_ip, AddressHelper::GetNextAvailablePort());
    
    int ok = 0;
    do
    {
      zmq_ctrl_pub_ = zmq_socket(zmq_context_, ZMQ_PUB);
      if (!zmq_ctrl_pub_) break;
      int32_t rc = zmq_bind(zmq_ctrl_pub_, ctrl_address_.c_str());
      if (rc != 0) break;
      
      zmq_ctrl_sub_ = zmq_socket(zmq_context_, ZMQ_SUB);
      if (!zmq_ctrl_sub_) break;
      rc = zmq_connect(zmq_ctrl_sub_, ctrl_address_.c_str());
      if (rc != 0) break;
      rc = zmq_setsockopt(zmq_ctrl_sub_, ZMQ_SUBSCRIBE, "", 0);
      if (rc != 0) break;
      ok = 1;
    }while (false);
    
    if (ok)
    {
      break;
    }
    
    if (zmq_ctrl_pub_)
    {
      zmq_close(zmq_ctrl_pub_);
      zmq_ctrl_pub_ = NULL;
    }
    
    if (zmq_ctrl_sub_)
    {
      zmq_close(zmq_ctrl_sub_);
      zmq_ctrl_sub_ = NULL;
    }
  }
  start_event_ = ::CreateEvent(NULL, TRUE, FALSE, NULL);
  hello_event_ = ::CreateEvent(NULL, TRUE, FALSE, NULL);
}

ZMQClient::~ZMQClient()
{
  Stop();
  ::CloseHandle(start_event_);
  ::CloseHandle(hello_event_);
  zmq_close(zmq_ctrl_pub_);
  zmq_close(zmq_ctrl_sub_);
  zmq_ctx_term(zmq_context_);
}

bool ZMQClient::Start()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  
  if (status_ != STATUS_PREPARE) return false;
  
  ::ResetEvent(start_event_);
  ::ResetEvent(hello_event_);
  unsigned id = 0;
  thread_handle_ = (HANDLE)_beginthreadex(NULL, 0,
      &ZMQClient::ThreadMain, (void*)this, 0, &id);
  if (!thread_handle_)
  {
    return false;
  }
  
  // step 1: wait for Start event
  HANDLE handles[] = {thread_handle_, start_event_};
  DWORD result = ::WaitForMultipleObjects(2, handles, FALSE, -1);
  if (result != WAIT_OBJECT_0 + 1)
  {
    if (result != WAIT_OBJECT_0)
    {
      ::TerminateThread(thread_handle_, -1);
    }
    ::CloseHandle(thread_handle_);
    thread_handle_ = NULL;
    return false;
  }
  
  status_ = STATUS_RUNNING;
  
  // step 2: send hello message and wait for reply
  // 50ms * 60 tries
  for (int32_t tries = 0; tries < 60; ++tries)
  {
    int32_t msg = CMD_HELLO;
    SendCtrlMessage((const char*)&msg, sizeof (msg));
    
    HANDLE handles[] = {thread_handle_, hello_event_};
    DWORD result = ::WaitForMultipleObjects(2, handles, FALSE, 50);
    if (result == WAIT_OBJECT_0 + 1)
    {
      return true;
    }
    else if (result == WAIT_TIMEOUT)
    {
      continue;
    }
    else if (result == WAIT_OBJECT_0)
    {
      // thread stopped, it is unexpected
      ::CloseHandle(thread_handle_);
      thread_handle_ = NULL;
      return false;
    }
  }
  // thread is still running
  // it is possible that we can not send CMD_QUIT,
  // because we can not send CMD_HELLO
  Stop();
  status_ = STATUS_PREPARE;
  return true;
}

int32_t ZMQClient::SendCtrlMessage(const char* msg, int32_t length)
{
  return SendMessage(zmq_ctrl_pub_, msg, length);
}

int32_t ZMQClient::SendMessage(void* channel, const char* msg, int32_t length)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  if (status_ != STATUS_RUNNING) return -1;
  
  if (!msg || length <= 0) return -1;
  
  void* sender = channel;
  
  if (sender == zmq_ctrl_pub_)
  {
    int32_t rc = zmq_send(sender, (void*)msg, length, 0);
    return rc == 0 ? 0 : -1;
  }
  return -1;
}

bool ZMQClient::SendRequest(const std::string& address, const char* buffer, int32_t size, ZMQCallBack callback, int32_t timeout)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);

  {
    void* skt = NULL;
    skt = zmq_socket(zmq_context_, ZMQ_REQ);
    if (!skt) return false;
    
    int32_t value = 0;
    zmq_setsockopt(skt, ZMQ_LINGER, (const void*)&value, sizeof(value));

    int32_t rc = zmq_connect(skt, address.c_str());
    if (rc != 0)
    {
      zmq_close(skt);
      return false;
    }

    rc = zmq_send(skt, buffer, size, 0);
    if (rc < 0)
    {
      zmq_close(skt);
      return false;
    }

    std::lock_guard<std::mutex> lock(context_mutex_);
    RequestContext req;
    req.zmq_socket_ = skt;
    req.callback_ = callback;
    req.address_ = address;
    req.request_time_ = GetTickCount();
    req.time_out_ = timeout > 0 ? timeout : 0;
    context_.push_back(req);
  }
  for (int32_t id = 0; id < 3; ++id)
  {
    int32_t cmd = CMD_HELLO;
    SendCtrlMessage((const char*)&cmd, sizeof(cmd));
  }
  return true;
}

bool ZMQClient::Stop()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  
  if (status_ == STATUS_PREPARE) return false;
  if (status_ == STATUS_STOPPED) return true;
  
  // quit_flag_ = 1;
  int32_t cmd = CMD_QUIT;
  SendCtrlMessage((const char*)&cmd, sizeof(cmd));
  
  DWORD result = ::WaitForMultipleObjects(1, &thread_handle_, FALSE, 3000);
  
  if (result == WAIT_TIMEOUT)
  {
    ::TerminateThread(thread_handle_, -1);
    ::WaitForMultipleObjects(1, &thread_handle_, FALSE, 3000);
  }
  else if (result == WAIT_OBJECT_0)
  {
    //
  }
  ::CloseHandle(thread_handle_);
  thread_handle_ = NULL;
  status_ = STATUS_STOPPED;
  
  for (auto& iter: context_)
  {
    zmq_close(iter.zmq_socket_);
  }
  std::vector<RequestContext>().swap(context_);
  return true;
}

unsigned __stdcall ZMQClient::ThreadMain(void * arg)
{
  if (ZMQClient* pThis = (ZMQClient*)arg)
  {
    pThis->Run();
  }
  return 0;
}

unsigned ZMQClient::Run()
{

  for (int32_t id = 0; !quit_flag_; ++id)
  {
    context_mutex_.lock();
    std::vector<zmq_pollitem_t> items(context_.size() + 1);
    
    items[0].socket = zmq_ctrl_sub_;
    items[0].fd = NULL;
    items[0].events = ZMQ_POLLIN;
    
    int32_t top = 1;
    
    int32_t current_time = GetTickCount();
    int32_t timeout = -1;
    for (auto& it: context_)
    {
      items[top].socket = it.zmq_socket_;
      items[top].fd = NULL;
      items[top].events = ZMQ_POLLIN;
      if (it.time_out_ > 0)
      {
        int32_t t = it.request_time_ + it.time_out_ - current_time;
        if (t < 0) t = 0;
        if (timeout == -1 || t < timeout) timeout = t;
      }
      ++top;
    }
    context_mutex_.unlock();
    
    int32_t rc = zmq_poll(&items[0], top, id == 0 ? 1 : timeout);

    if (id == 0)
    {
      ::SetEvent(start_event_);
    }
    
    if (rc >= 0)
    {
      if (items[0].revents)
      {
        ProcessCtrlMessage();
      }

      if (quit_flag_)
      {
        break;
      }

      std::vector<void*> signal_sockets;
      for (int32_t i = 1; i < top; ++i) if (items[i].revents)
      {
        signal_sockets.push_back(items[i].socket);
      }
      ProcessEvent(signal_sockets);
      
    }
    else if (rc < 0)
    {
      int32_t error = zmq_errno();
      if (error == ETERM)
      {
        break;
      }
    }
  }
  
  return 0;
}

void ZMQClient::ProcessCtrlMessage()
{
  zmq_msg_t msg;
  zmq_msg_init(&msg);

  do
  {
    if (zmq_recvmsg(zmq_ctrl_sub_, &msg, ZMQ_DONTWAIT) <= 0) break;
    
    const char* buffer = static_cast<const char*>(zmq_msg_data(&msg));
    const int32_t size = static_cast<int32_t>(zmq_msg_size(&msg));
    if (size == sizeof(int32_t))
    {
      int32_t cmd = *(int32_t*)buffer;
       switch (cmd)
       {
         case CMD_QUIT: quit_flag_ = 1; break;
         case CMD_HELLO: ::SetEvent(hello_event_); break;
       }
    }
  } while (false);
  
  zmq_msg_close(&msg);
}

void ZMQClient::ProcessEvent(const std::vector<void*>& signal_sockets)
{
  std::map<void*, std::string> sockets;
  for (auto it: signal_sockets)
  {
    std::string data;

    zmq_msg_t msg;
    zmq_msg_init(&msg);

    do
    {
      if (zmq_recvmsg(it, &msg, ZMQ_DONTWAIT) <= 0) break;
      const char* buffer = static_cast<const char*>(zmq_msg_data(&msg));
      const int32_t size = static_cast<int32_t>(zmq_msg_size(&msg));
      std::string(buffer, buffer+size).swap(data);
    } while (false);

    zmq_msg_close(&msg);

    sockets[it] = std::move(data);
  }

  int32_t curr_time = GetTickCount();
  std::vector<std::pair<ZMQCallBack, scoped_refptr<ZMQResponse> > > responses;
  {
    std::lock_guard<std::mutex> lock(context_mutex_);
    for (auto iter = context_.begin(); iter != context_.end(); )
    {
      // case 1: have message
      if (sockets.count(iter->zmq_socket_))
      {
        zmq_close(iter->zmq_socket_);

        scoped_refptr<ZMQResponse> rep = new ZMQResponse();
        rep->error_code_ = ZMQResponse::ZMQ_REP_OK;
        rep->data_ = std::move(sockets[iter->zmq_socket_]);
        responses.push_back({iter->callback_, rep});

        iter = context_.erase(iter);
      }
      // case 2 time out
      else if (iter->time_out_ > 0 && curr_time > iter->request_time_ + iter->time_out_)
      {
        zmq_close(iter->zmq_socket_);

        scoped_refptr<ZMQResponse> rep = new ZMQResponse();
        rep->error_code_ = ZMQResponse::ZMQ_REP_TIME_OUT;
        std::string().swap(rep->data_);
        responses.push_back({iter->callback_, rep});

        iter = context_.erase(iter);
      }
      else
      {
        ++iter;
      }
    }
  }

  for (auto& it:responses)
  {
    base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(it.first, it.second)
    );
  }
}

}