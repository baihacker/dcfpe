#include "dpe_base/zmq_adapter.h"

#include <algorithm>
//#include <iostream>

#include <process.h>

#include "dpe_base/thread_pool.h"

//using namespace std;

namespace base
{
int32_t MessageCenter::next_available_port_ = MIN_PORT;

MessageCenter::MessageCenter() :
  status_(STATUS_PREPARE),
  quit_flag_(0),
  zmq_context_(NULL),
  zmq_ctrl_pub_(NULL),
  zmq_ctrl_sub_(NULL),
  weakptr_factory_(this)
{
  zmq_context_ = zmq_ctx_new();
  const int32_t ctrl_ip = GetCtrlIP();
  for (;;)
  {
    ctrl_address_ = GetAddress(ctrl_ip, GetNextAvailablePort());
    
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

MessageCenter::~MessageCenter()
{
  Stop();
  ::CloseHandle(start_event_);
  ::CloseHandle(hello_event_);
}

bool MessageCenter::AddMessageHandler(MessageHandler* handler)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  
  for (auto it: handlers_)
  if (it == handler)
  {
    return true;
  }
  if (handler)
  {
    handlers_.push_back(handler);
  }
  return true;
}

bool MessageCenter::RemoveMessageHandler(MessageHandler* handler)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  
  std::remove_if(handlers_.begin(), handlers_.end(), [=](MessageHandler* x)
  {
    return x == handler;
  });
  return true;
}

int32_t MessageCenter::RegisterChannel(const std::string& address, bool is_sender, bool is_bind)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  
  if (is_sender)
  {
    for (auto& it: publishers_) if (it.second == address)
    {
      return reinterpret_cast<int32_t>(it.first);
    }
    void* publisher = zmq_socket(zmq_context_, ZMQ_PUB);
    if (!publisher) return INVALID_CHANNEL_ID;
    
    int32_t rc = is_bind ?
                  zmq_bind(publisher, address.c_str()):
                  zmq_connect(publisher, address.c_str());
    if (rc != 0)
    {
      zmq_close(publisher);
      return INVALID_CHANNEL_ID;
    }
    
    publishers_.push_back({publisher, address});
    socket_address_.push_back({publisher, address});
    return reinterpret_cast<int32_t>(publisher);
  }
  else
  {
    void* subscriber = NULL;
    {
      std::lock_guard<std::mutex> lock(zmq_mutex_);
      for (auto& it: subscribers_) if (it.second == address)
      {
        return reinterpret_cast<int32_t>(it.first);
      }
      
      subscriber = zmq_socket(zmq_context_, ZMQ_SUB);
      if (!subscriber) return INVALID_CHANNEL_ID;
      
      int32_t rc = is_bind ?
                    zmq_bind(subscriber, address.c_str()) : 
                    zmq_connect(subscriber, address.c_str());
      if (rc != 0)
      {
        zmq_close(subscriber);
        return INVALID_CHANNEL_ID;
      }
      
      rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
      if (rc != 0)
      {
        zmq_close(subscriber);
        return INVALID_CHANNEL_ID;
      }
      subscribers_.push_back({subscriber, address});
    }
    socket_address_.push_back({subscriber, address});
    
    for (int32_t id = 0; id < 3; ++id)
    {
      int32_t cmd = CMD_HELLO;
      SendCtrlMessage((const char*)&cmd, sizeof(cmd));
    }
    return reinterpret_cast<int32_t>(subscriber);
  }
}

bool MessageCenter::RemoveChannel(int32_t channel_id)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  
  std::remove_if(publishers_.begin(), publishers_.end(),
    [=](std::pair<void*, std::string>& x)
    {
      return reinterpret_cast<int32_t>(x.first) == channel_id;
    });
  
  zmq_mutex_.lock();
  std::remove_if(subscribers_.begin(), subscribers_.end(),
    [=](std::pair<void*, std::string>& x)
    {
      return reinterpret_cast<int32_t>(x.first) == channel_id;
    });
  zmq_mutex_.unlock();
  
  std::remove_if(socket_address_.begin(), socket_address_.end(),
    [=](std::pair<void*, std::string>& x)
    {
      return reinterpret_cast<int32_t>(x.first) == channel_id;
    });
  
  Sleep(0);
  
  return true;
}

const char* MessageCenter::GetAddressByHandle(int32_t channel_id)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  
  void* channel = reinterpret_cast<void*>(channel_id);
  
  if (channel == zmq_ctrl_pub_ || channel == zmq_ctrl_pub_)
  {
    return ctrl_address_.c_str();
  }
  
  for (auto& it: socket_address_)
  if (it.first == channel)
  {
    return it.second.c_str();
  }
  
  return "";
}

int32_t MessageCenter::SendCtrlMessage(const char* msg, int32_t length)
{
  return SendMessage(reinterpret_cast<int32_t>(zmq_ctrl_pub_), msg, length);
}

int32_t MessageCenter::SendMessage(int32_t channel_id, const char* msg, int32_t length)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  if (status_ != STATUS_RUNNING) return -1;
  
  if (!msg || length <= 0) return -1;
  
  void* sender = reinterpret_cast<void*>(channel_id);
  
  if (sender == zmq_ctrl_pub_)
  {
    int32_t rc = zmq_send(sender, (void*)msg, length, 0);
    return rc == 0 ? 0 : -1;
  }
  
  int32_t rc = -1;
  for (auto& it: publishers_)
  if (it.first == sender)
  {
    rc = zmq_send(sender, (void*)msg, length, 0);
    break;
  }
  
  return rc;
}

bool MessageCenter::Start()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  
  if (status_ != STATUS_PREPARE) return false;
  
  ::ResetEvent(start_event_);
  ::ResetEvent(hello_event_);
  unsigned id = 0;
  thread_handle_ = (HANDLE)_beginthreadex(NULL, 0,
      &MessageCenter::ThreadMain, (void*)this, 0, &id);
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

bool   MessageCenter::Stop()
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
  return true;
}

base::WeakPtr<MessageCenter> MessageCenter::GetWeakPtr()
{
  return weakptr_factory_.GetWeakPtr();
}

unsigned __stdcall MessageCenter::ThreadMain(void * arg)
{
  if (MessageCenter* pThis = (MessageCenter*)arg)
  {
    pThis->Run();
  }
  return 0;
}

unsigned    MessageCenter::Run()
{
  zmq_pollitem_t  items[128];
  
  for (int32_t id = 0; !quit_flag_; ++id)
  {
    items[0].socket = zmq_ctrl_sub_;
    items[0].fd = NULL;
    items[0].events = ZMQ_POLLIN;
    
    int32_t top = 1;
    
    zmq_mutex_.lock();
    for (auto& it: subscribers_)
    {
      items[top].socket = it.first;
      items[top].fd = NULL;
      items[top].events = ZMQ_POLLIN;
      ++top;
    }
    zmq_mutex_.unlock();
    
    int32_t rc = zmq_poll(items, top, id == 0 ? 1 : -1);

    if (id == 0)
    {
      ::SetEvent(start_event_);
    }
    
    if (rc > 0)
    {
      for (int32_t i = 0; i < top; ++i)
      {
        ProcessEvent(items+i);
        if (quit_flag_)
        {
          break;
        }
      }
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

void MessageCenter::ProcessEvent(zmq_pollitem_t* item)
{
  if (!item) return;
  if (!item->revents) return;
  
  zmq_msg_t* msg = new zmq_msg_t;
  zmq_msg_init(msg);
  bool should_destroy = true;
  do
  {
    if (zmq_recvmsg(item->socket, msg, ZMQ_DONTWAIT) <= 0) break;
    
    if (item->socket == zmq_ctrl_sub_)
    {
      const char* buffer = static_cast<const char*>(zmq_msg_data(msg));
      const int32_t size = static_cast<int32_t>(zmq_msg_size(msg));
      HandleCtrlMessage(buffer, size);
      if (quit_flag_) break;
    }
    else
    {
      base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
          base::Bind(&MessageCenter::HandleMessage, weakptr_factory_.GetWeakPtr(),
          reinterpret_cast<int32_t>(item->socket), msg));
      should_destroy = false;
    }
  } while (false);
  
  if (should_destroy)
  {
    zmq_msg_close(msg);
    delete msg;
  }
}

void MessageCenter::HandleCtrlMessage(const char* msg, int32_t length)
{
  if (length == sizeof(int32_t))
  {
    int32_t cmd = *(int32_t*)msg;
    switch (cmd)
    {
      case CMD_QUIT: quit_flag_ = 1; break;
      case CMD_HELLO: ::SetEvent(hello_event_); break;
    }
  }
}

void  MessageCenter::HandleMessage(base::WeakPtr<MessageCenter> center, int32_t socket, zmq_msg_t* msg)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);
  
  if (MessageCenter* pThis = center.get())
  {
    pThis->HandleMessageImpl(socket, msg);
  }
  zmq_msg_close(msg);
  delete msg;
}

void  MessageCenter::HandleMessageImpl(int32_t socket, zmq_msg_t* msg)
{
  for (auto it: handlers_)
  {
    if (it->handle_message(
          socket,
          static_cast<const char*>(zmq_msg_data(msg)),
          static_cast<int32_t>(zmq_msg_size(msg))
        ))
    {
      break;
    }
  }
}

std::string MessageCenter::GetAddress(int32_t ip, int32_t port)
{
  char address[64];
  sprintf(address, "tcp://%d.%d.%d.%d:%d",
      ip>>24, (ip>>16)&255, (ip>>8)&255, ip&255, port);
  return address;
}

int32_t     MessageCenter::GetProcessIP()
{
  static const int32_t offset = ::GetCurrentProcessId() % 65535 + 1;
  static const int32_t basic_ip = MAKE_IP(127, 233, 0, 0) | offset;
  return basic_ip;
}

int32_t     MessageCenter::GetCtrlIP()
{
  static const int32_t offset = ::GetCurrentProcessId() % 65535 + 1;
  static const int32_t basic_ip = MAKE_IP(127, 234, 0, 0) | offset;
  return basic_ip;
}

int32_t    MessageCenter::GetNextAvailablePort()
{
  int32_t ret = next_available_port_;
  if (++next_available_port_ == MAX_PORT)
  {
    next_available_port_ = MIN_PORT;
  }
  return ret;
}
}
