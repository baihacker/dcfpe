#include "dpe_base/zmq_adapter.h"

#include <algorithm>
#include <process.h>
#include <zmq.h>

#include "dpe_base/thread_pool.h"

namespace base
{
ZMQServer::ZMQServer() :
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

ZMQServer::~ZMQServer()
{
  Stop();
  ::CloseHandle(start_event_);
  ::CloseHandle(hello_event_);
  zmq_close(zmq_ctrl_pub_);
  zmq_close(zmq_ctrl_sub_);
  zmq_ctx_term(zmq_context_);
}

bool ZMQServer::Start()
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);

  if (status_ != STATUS_PREPARE) return false;

  ::ResetEvent(start_event_);
  ::ResetEvent(hello_event_);
  unsigned id = 0;
  thread_handle_ = (HANDLE)_beginthreadex(NULL, 0,
      &ZMQServer::ThreadMain, (void*)this, 0, &id);
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

int32_t ZMQServer::SendCtrlMessage(const char* msg, int32_t length)
{
  return SendMessage(zmq_ctrl_pub_, msg, length);
}

int32_t ZMQServer::SendMessage(void* channel, const char* msg, int32_t length)
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

bool ZMQServer::Stop()
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

bool ZMQServer::StartServer(const std::string& address, RequestHandler* handler)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);

  if (handler == NULL) return false;

  {
    void* skt = NULL;
    std::lock_guard<std::mutex> lock(context_mutex_);
    for (auto& it: context_) if (it.address_ == address)
    {
      return false;
    }

    skt = zmq_socket(zmq_context_, ZMQ_REP);
    if (!skt) return false;

    int32_t rc = zmq_bind(skt, address.c_str());
    if (rc != 0)
    {
      zmq_close(skt);
      return false;
    }
    zmq_pollitem_t item;
    {
      item.socket = skt;
      item.fd = NULL;
      item.events = ZMQ_POLLIN;
      zmq_poll(&item, 1, 1);
    }
    context_.push_back({reinterpret_cast<int32_t>(skt), skt, handler, address, STATE_LISTENING});
  }
  for (int32_t id = 0; id < 3; ++id)
  {
    int32_t cmd = CMD_HELLO;
    SendCtrlMessage((const char*)&cmd, sizeof(cmd));
  }
  return true;
}

bool ZMQServer::StopServer(RequestHandler* handler)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);

  if (handler == NULL) return false;
  void* willClose = NULL;
  {
    std::lock_guard<std::mutex> lock(context_mutex_);
    const int n = static_cast<int>(context_.size());
    for (auto iter = context_.begin(); iter != context_.end();)
    {
      if (iter->handler_ == handler)
      {
        willClose = iter->zmq_socket_;
        context_.erase(iter);
        break;
      }
      else
      {
        ++iter;
      }
    }
  }
  int32_t cmd = CMD_HELLO;
  SendCtrlMessage((const char*)&cmd, sizeof(cmd));
  zmq_close(willClose);
  return true;
}

unsigned __stdcall ZMQServer::ThreadMain(void * arg)
{
  if (ZMQServer* pThis = (ZMQServer*)arg)
  {
    pThis->Run();
  }
  return 0;
}

unsigned ZMQServer::Run()
{
  for (int32_t id = 0; !quit_flag_; ++id)
  {
    context_mutex_.lock();
    std::vector<zmq_pollitem_t> items(context_.size() + 1);

    items[0].socket = zmq_ctrl_sub_;
    items[0].fd = NULL;
    items[0].events = ZMQ_POLLIN;

    int32_t top = 1;

    for (auto& it: context_)
    {
      if (it.state_ == STATE_LISTENING)
      {
        items[top].socket = it.zmq_socket_;
        items[top].fd = NULL;
        items[top].events = ZMQ_POLLIN;
        ++top;
      }
    }
    context_mutex_.unlock();

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

void ZMQServer::ProcessCtrlMessage()
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

void ZMQServer::ProcessEvent(const std::vector<void*>& signal_sockets)
{
  if (signal_sockets.empty()) return;

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

  int activeRequest = 0;
  {
    std::lock_guard<std::mutex> lock(context_mutex_);
    for (auto& it: context_)
    {
      if (sockets.count(it.zmq_socket_))
      {
        ++activeRequest;
        it.state_ = STATE_PROCESSING;
        it.data_ = std::move(sockets[it.zmq_socket_]);

        ServerContext ctx;
        ctx.channel_id_ = it.channel_id_;
        ctx.zmq_socket_ = it.zmq_socket_;
        ctx.handler_ = it.handler_;
        ctx.address_ = it.address_;
        ctx.state_ = it.state_;
        ctx.data_ = it.data_;

        std::string reply;
        if (ctx.handler_->pre_handle_request(ctx, reply)) {
          --activeRequest;
          if (reply.empty())
          {
            reply.append(4, '\0');
          }
          zmq_send(ctx.zmq_socket_, reply.c_str(), reply.size(), 0);
          it.state_ = STATE_LISTENING;
        }
      }
    }
  }

  if (activeRequest > 0) {
    base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
      base::Bind(&ZMQServer::ProcessRequest, weakptr_factory_.GetWeakPtr())
      );
  }
}

void ZMQServer::ProcessRequest(base::WeakPtr<ZMQServer> server)
{
  DCHECK_CURRENTLY_ON(base::ThreadPool::UI);

  if (ZMQServer* pThis = server.get())
  {
    pThis->ProcessRequestImpl();
  }
}

void ZMQServer::ProcessRequestImpl()
{
  std::vector<ServerContext>  temp;
  {
    std::lock_guard<std::mutex> lock(context_mutex_);
    for (auto& it: context_)
    {
      if (it.state_ == STATE_PROCESSING)
      {
        ServerContext ctx;
        ctx.channel_id_ = it.channel_id_;
        ctx.zmq_socket_ = it.zmq_socket_;
        ctx.handler_ = it.handler_;
        ctx.address_ = it.address_;
        ctx.state_ = it.state_;
        ctx.data_ = std::move(it.data_);
        temp.push_back(ctx);
      }
    }
  }

  for (auto& it: temp)
  {
    std::string reply = it.handler_->handle_request(it);
    if (reply.empty())
    {
      reply.append(4, '\0');
    }
    zmq_send(it.zmq_socket_, reply.c_str(), reply.size(), 0);
  }

  {
    std::lock_guard<std::mutex> lock(context_mutex_);
    for (auto& it: context_)
    {
      if (it.state_ == STATE_PROCESSING)
      {
        it.state_ = STATE_LISTENING;
      }
    }
  }

  for (int32_t id = 0; id < 3; ++id)
  {
    int32_t cmd = CMD_HELLO;
    SendCtrlMessage((const char*)&cmd, sizeof(cmd));
  }
}

}