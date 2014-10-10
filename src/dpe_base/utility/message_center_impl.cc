#include "message_center_impl.h"

#include <algorithm>
#include <process.h>
#include <iostream>
using namespace std;
namespace utility_impl
{
int32_t MessageCenterImpl::next_ctrl_port_ = DEFAULT_CTRL_PORT;

MessageCenterImpl::MessageCenterImpl() :
  status_(STATUS_PREPARE),
  quit_flag_(0),
  zmq_context_(NULL),
  zmq_ctrl_pub_(NULL),
  zmq_ctrl_sub_(NULL)
{
  zmq_context_ = zmq_ctx_new();
  const int32_t ctrl_ip = GetCtrlIP();
  for (;;)
  {
    ctrl_address_ = GetAddress(ctrl_ip, GetNextCtrlPort());
    
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

MessageCenterImpl::~MessageCenterImpl()
{
  stop();
  ::CloseHandle(start_event_);
  ::CloseHandle(hello_event_);
}

int32_t MessageCenterImpl::add_message_handler(IMessageHandler* handler)
{
  if (status_ == STATUS_PREPARE)
  {
    for (auto it: handlers_)
    if (it == handler)
    {
      return DPE_OK;
    }
    if (handler)
    {
      handlers_.push_back(handler);
    }
    return DPE_OK;
  }
  return DPE_FAILED;
}

int32_t MessageCenterImpl::remove_message_handler(IMessageHandler* handler)
{
  if (status_ == STATUS_RUNNING)
  {
    return DPE_FAILED;
  }
  std::remove_if(handlers_.begin(), handlers_.end(), [=](IMessageHandler* x)
  {
    return x == handler;
  });
  return DPE_OK;
}

int32_t MessageCenterImpl::add_sender_address(const char* address)
{
  if (status_ == STATUS_RUNNING) return INVALID_CHANNEL_ID;
  if (!address || !address[0]) return INVALID_CHANNEL_ID;
  
  for (auto& it: publishers_) if (it.second == address)
  {
    return reinterpret_cast<int32_t>(it.first);
  }
  
  void* publisher = zmq_socket(zmq_context_, ZMQ_PUB);
  if (!publisher) return INVALID_CHANNEL_ID;
  int32_t rc = zmq_bind(publisher, address);
  if (rc != 0)
  {
    zmq_close(publisher);
    return INVALID_CHANNEL_ID;
  }
  
  publishers_.push_back({publisher, address});
  return reinterpret_cast<int32_t>(publisher);
}

int32_t MessageCenterImpl::add_receive_address(const char* address)
{
  if (status_ == STATUS_RUNNING) return INVALID_CHANNEL_ID;
  if (!address || !address[0]) return INVALID_CHANNEL_ID;
  
  void* subscriber = NULL;
  int32_t ok = 0;
  do
  {
      subscriber = zmq_socket(zmq_context_, ZMQ_SUB);
      if (!subscriber) break;
      int32_t rc = zmq_connect(subscriber, address);
      if (rc != 0) break;
      rc = zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
      if (rc != 0) break;
      ok = 1;
  }while (false);
  if (!ok)
  {
    if (subscriber)
    {
      zmq_close(subscriber);
    }
    return INVALID_CHANNEL_ID;
  }
  subscribers_.push_back({subscriber, address});
  return reinterpret_cast<int32_t>(subscriber);
}

int32_t MessageCenterImpl::remove_channel(int32_t channel_id)
{
  if (status_ == STATUS_RUNNING) return DPE_FAILED;
  
  std::remove_if(publishers_.begin(), publishers_.end(),
    [=](std::pair<void*, std::string>& x)
    {
      return reinterpret_cast<int32_t>(x.first) == channel_id;
    });
  
  std::remove_if(subscribers_.begin(), subscribers_.end(),
    [=](std::pair<void*, std::string>& x)
    {
      return reinterpret_cast<int32_t>(x.first) == channel_id;
    });
  
  return DPE_OK;
}

const char* MessageCenterImpl::get_address_by_channel(int32_t channel_id)
{
  void* channel = reinterpret_cast<void*>(channel_id);
  
  if (channel == zmq_ctrl_pub_ || channel == zmq_ctrl_pub_)
  {
    return ctrl_address_.c_str();
  }
  
  for (auto& it: publishers_)
  if (it.first == channel)
  {
    return it.second.c_str();
  }
  
  for (auto& it: subscribers_)
  if (it.first == channel)
  {
    return it.second.c_str();
  }
  
  return "";
}

int32_t MessageCenterImpl::send_ctrl_message(const char* msg, int32_t length)
{
  return send_message(reinterpret_cast<int32_t>(zmq_ctrl_pub_), msg, length);
}

int32_t MessageCenterImpl::send_message(int32_t channel_id, const char* msg, int32_t length)
{
  if (status_ != STATUS_RUNNING) return DPE_FAILED;
  if (!msg || length <= 0) return DPE_FAILED;
  
  void* sender = reinterpret_cast<void*>(channel_id);
  
  if (sender == zmq_ctrl_pub_)
  {
    int32_t rc = zmq_send(sender, (void*)msg, length, 0);
    return rc == 0 ? DPE_OK : DPE_FAILED;
  }
  
  for (auto& it: publishers_)
  if (it.first == sender)
  {
    int32_t rc = zmq_send(sender, (void*)msg, length, 0);
    return rc == 0 ? DPE_OK : DPE_FAILED;
  }
  return DPE_FAILED;
}

int32_t MessageCenterImpl::start()
{
  if (status_ != STATUS_PREPARE) return DPE_FAILED;
  
  ::ResetEvent(start_event_);
  ::ResetEvent(hello_event_);
  unsigned id = 0;
  thread_handle_ = (HANDLE)_beginthreadex(NULL, 0,
      &MessageCenterImpl::ThreadMain, (void*)this, 0, &id);
  if (!thread_handle_)
  {
    return DPE_FAILED;
  }
  
  // step 1: wait for start event
  HANDLE handles[] = {thread_handle_, start_event_};
  DWORD result = ::WaitForMultipleObjects(2, handles, FALSE, -1);
  if (result != WAIT_OBJECT_0 + 1)
  {
    if (result == WAIT_OBJECT_0)
    {
      ::CloseHandle(thread_handle_);
    }
    thread_handle_ = NULL;
    return DPE_FAILED;
  }
  
  status_ = STATUS_RUNNING;
  
  // step 2: send hello message and wait for reply
  // 50ms * 60 tries
  for (int32_t tries = 0; tries < 60; ++tries)
  {
    int32_t msg = CMD_HELLO;
    send_ctrl_message((const char*)&msg, sizeof (msg));
    
    HANDLE handles[] = {thread_handle_, hello_event_};
    DWORD result = ::WaitForMultipleObjects(2, handles, FALSE, 50);
    if (result == WAIT_OBJECT_0 + 1)
    {
      return DPE_OK;
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
      return DPE_FAILED;
    }
  }
  // thread is still running
  // it is possible that we can not send CMD_QUIT,
  // because we can not send CMD_HELLO
  stop();
  status_ = STATUS_PREPARE;
  return DPE_FAILED;
}

int32_t MessageCenterImpl::stop()
{
  if (status_ == STATUS_PREPARE) return DPE_FAILED;
  if (status_ == STATUS_STOPPED) return DPE_OK;
  
  // quit_flag_ = 1;
  int32_t cmd = CMD_QUIT;
  send_ctrl_message((const char*)&cmd, sizeof(cmd));
  
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
  return DPE_OK;
}

unsigned __stdcall MessageCenterImpl::ThreadMain(void * arg)
{
  if (MessageCenterImpl* pThis = (MessageCenterImpl*)arg)
  {
    pThis->Run();
  }
  return 0;
}

unsigned    MessageCenterImpl::Run()
{
  zmq_pollitem_t  items[128];
  
  for (int32_t id = 0; !quit_flag_; ++id)
  {
    items[0].socket = zmq_ctrl_sub_;
    items[0].fd = NULL;
    items[0].events = ZMQ_POLLIN;
    
    int32_t top = 1;
    for (auto& it: subscribers_)
    {
      items[top].socket = it.first;
      items[top].fd = NULL;
      items[top].events = ZMQ_POLLIN;
      ++top;
    }
    
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

void MessageCenterImpl::ProcessEvent(zmq_pollitem_t* item)
{
  if (!item) return;
  if (!item->revents) return;
  
  zmq_msg_t msg;
  zmq_msg_init(&msg);
  do
  {
    if (zmq_recvmsg(item->socket, &msg, ZMQ_DONTWAIT) <= 0) break;
    if (item->socket == zmq_ctrl_sub_)
    {
      const char* buffer = static_cast<const char*>(zmq_msg_data(&msg));
      const int32_t size = static_cast<int32_t>(zmq_msg_size(&msg));
      HandleCtrlMessage(buffer, size);
    }
    if (quit_flag_) break;
    
    for (auto& it: handlers_)
    {
      if (it->handle_message(
            reinterpret_cast<int32_t>(item->socket),
            item->socket == zmq_ctrl_sub_,
            static_cast<const char*>(zmq_msg_data(&msg)),
            static_cast<int32_t>(zmq_msg_size(&msg))
          ))
      {
        break;
      }
    }
  } while (false);
  
  zmq_msg_close(&msg);
}

void MessageCenterImpl::HandleCtrlMessage(const char* msg, int32_t length)
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

std::string MessageCenterImpl::GetAddress(int32_t ip, int32_t port)
{
  char address[64];
  sprintf(address, "tcp://%d.%d.%d.%d:%d",
      ip>>24, (ip>>16)&255, (ip>>8)&255, ip&255, port);
  return address;
}

int32_t     MessageCenterImpl::GetCtrlIP()
{
  static const int32_t basic_ip = MAKE_IP(127, 234, 0, 1);
  return GetCurrentProcessId() + basic_ip;
}

int32_t    MessageCenterImpl::GetNextCtrlPort()
{
  if (++next_ctrl_port_ == 65536)
  {
    next_ctrl_port_ = DEFAULT_CTRL_PORT;
  }
  return next_ctrl_port_;
}
}
