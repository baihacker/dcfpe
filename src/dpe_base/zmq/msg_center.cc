#include "dpe_base/zmq_adapter.h"

#include <algorithm>
#include <process.h>
#include <zmq.h>

#include "dpe_base/thread_pool.h"

namespace base
{
MessageCenter::MessageCenter() :
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

MessageCenter::~MessageCenter()
{
  DCHECK(handlers_.size() == 0);
  DCHECK(publishers_.size() == 0);
  DCHECK(subscribers_.size() == 0);
  //DCHECK(socket_address_.size() == 0);

  Stop();
  ::CloseHandle(start_event_);
  ::CloseHandle(hello_event_);
  zmq_close(zmq_ctrl_pub_);
  zmq_close(zmq_ctrl_sub_);
  zmq_ctx_term(zmq_context_);
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

  for (auto iter = handlers_.begin(); iter != handlers_.end();)
  if (*iter == handler)
  {
    iter = handlers_.erase(iter);
  }
  else
  {
    ++iter;
  }

  return true;
}

void* MessageCenter::RegisterChannel(int32_t channel_type, const std::string& address, bool is_bind)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);

  if (channel_type == CHANNEL_TYPE_PUB)
  {
    for (auto& it: publishers_) if (it.second == address)
    {
      return it.first;
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
    zmq_pollitem_t item;
    {
      item.socket = publisher;
      item.fd = NULL;
      item.events = ZMQ_POLLOUT;
      zmq_poll(&item, 1, 1);
    }
    publishers_.push_back({publisher, address});
    socket_address_.push_back({publisher, address});
    return publisher;
  }
  else
  {
    void* subscriber = NULL;
    {
      std::lock_guard<std::mutex> lock(subscribers_mutex_);
      for (auto& it: subscribers_) if (it.second == address)
      {
        return it.first;
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
      zmq_pollitem_t item;
      {
        item.socket = subscriber;
        item.fd = NULL;
        item.events = ZMQ_POLLIN;
        zmq_poll(&item, 1, 1);
      }
    }
    socket_address_.push_back({subscriber, address});

    for (int32_t id = 0; id < 3; ++id)
    {
      int32_t cmd = CMD_HELLO;
      SendCtrlMessage((const char*)&cmd, sizeof(cmd));
    }
    return subscriber;
  }
}

bool MessageCenter::RemoveChannel(void* channel)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);

  for (auto iter = publishers_.begin(); iter != publishers_.end();)
  if (iter->first == channel)
  {
    zmq_close(reinterpret_cast<void*>(iter->first));
    iter = publishers_.erase(iter);
  }
  else
  {
    ++iter;
  }

  std::vector<void*> willClose;
  subscribers_mutex_.lock();

  for (auto iter = subscribers_.begin(); iter != subscribers_.end();)
  if (iter->first == channel)
  {
    willClose.push_back(iter->first);
    iter = subscribers_.erase(iter);
  }
  else
  {
    ++iter;
  }

  subscribers_mutex_.unlock();

  SayHello(1);

  for (auto iter: willClose)
  {
    zmq_close(iter);
  }

  for (auto iter = socket_address_.begin(); iter != socket_address_.end();)
  if (iter->first == channel)
  {
    iter = socket_address_.erase(iter);
  }
  else
  {
    ++iter;
  }

  return true;
}

const char* MessageCenter::GetAddressByHandle(void* channel)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);

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

void MessageCenter::SayHello(int32_t times)
{
  if (times <= 0) times = 1;
  if (times > 5) times = 5;

  for (int32_t id = 0; id < times; ++id)
  {
    int32_t cmd = CMD_HELLO;
    SendCtrlMessage((const char*)&cmd, sizeof(cmd));
  }
}

int32_t MessageCenter::SendCtrlMessage(const char* msg, int32_t length)
{
  return SendMessage(zmq_ctrl_pub_, msg, length);
}

int32_t MessageCenter::SendMessage(void* channel, const char* msg, int32_t length)
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
  for (int32_t id = 0; !quit_flag_; ++id)
  {
    std::vector<zmq_pollitem_t> items(1);

    items[0].socket = zmq_ctrl_sub_;
    items[0].fd = NULL;
    items[0].events = ZMQ_POLLIN;

    int32_t top = 1;

    subscribers_mutex_.lock();
    for (auto& it: subscribers_)
    {
      zmq_pollitem_t temp;
      temp.socket = it.first;
      temp.fd = NULL;
      temp.events = ZMQ_POLLIN;
      ++top;
      items.push_back(temp);
    }
    subscribers_mutex_.unlock();
    int32_t rc = zmq_poll(&items[0], top, id == 0 ? 1 : -1);

    if (id == 0)
    {
      ::SetEvent(start_event_);
    }

    if (rc > 0)
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

void MessageCenter::ProcessCtrlMessage()
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

void MessageCenter::ProcessEvent(const std::vector<void*>& signal_sockets)
{
  for (auto it: signal_sockets)
  {
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    do
    {
      if (zmq_recvmsg(it, &msg, ZMQ_DONTWAIT) <= 0) break;

      const char* buffer = static_cast<const char*>(zmq_msg_data(&msg));
      const int32_t size = static_cast<int32_t>(zmq_msg_size(&msg));
      std::string data(buffer, buffer+size);

      base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
          base::Bind(&MessageCenter::HandleMessage, weakptr_factory_.GetWeakPtr(),
          it, data));
    } while (false);

    zmq_msg_close(&msg);
  }
}

void  MessageCenter::HandleMessage(base::WeakPtr<MessageCenter> center, void* socket, const std::string& data)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);

  if (MessageCenter* pThis = center.get())
  {
    pThis->HandleMessageImpl(socket, data);
  }
}

void  MessageCenter::HandleMessageImpl(void* socket, const std::string& data)
{
  bool handled = false;
  for (auto it: handlers_)
  {
    if (it->handle_message(socket, data))
    {
      handled = true;
      break;
    }
  }
  if (!handled)
  {
    LOG(WARNING) << "Unhandled message : \n" << data;
  }
}
}