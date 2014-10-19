#ifndef DPE_BASE_ZMQ_ADAPTER_H_
#define DPE_BASE_ZMQ_ADAPTER_H_

#include <cstdint>
#include <vector>
#include <string>
#include <mutex>
#include <windows.h>

#include "dpe_base/chromium_base.h"

namespace base
{
// Address
#define MAKE_IP(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
const int32_t kMinPort = 20000;
const int32_t kMaxPort = 40000;
class AddressHelper
{
public:
  // address management
  static std::string MakeZMQTCPAddress(int32_t ip, int32_t port);
  static int32_t     GetNextAvailablePort();
  static int32_t     GetProcessIP();
  static int32_t     GetCtrlIP();
private:
  static int32_t     next_available_port_;
};

enum
{
  STATUS_PREPARE,
  STATUS_RUNNING,
  STATUS_STOPPED,
};
enum
{
  CMD_HELLO = 0x00,
  CMD_QUIT = 0x01,
};

// message center
class MessageHandler
{
public:
virtual ~MessageHandler(){};
virtual int32_t handle_message(int32_t handle, const std::string& data) = 0;
};

enum
{
  INVALID_CHANNEL_ID = -1
};

enum
{
  CHANNEL_TYPE_PUB,
  CHANNEL_TYPE_SUB,
};

class MessageCenter
{
public:
  MessageCenter();
  ~MessageCenter();
  
  bool          AddMessageHandler(MessageHandler* handler);
  bool          RemoveMessageHandler(MessageHandler* handler);
  
  int32_t       RegisterChannel(int32_t channel_type, const std::string& address, bool is_bind);
  bool          RemoveChannel(int32_t channel_id);
  const char*   GetAddressByHandle(int32_t channel_id);
  
  int32_t       SendMessage(int32_t channel_id, const char* msg, int32_t length);
  int32_t       WorkerHandle() {return reinterpret_cast<int32_t>(thread_handle_);}
  
  bool          Start();
  bool          Stop();

  base::WeakPtr<MessageCenter> GetWeakPtr();
  
private:
  int32_t       SendCtrlMessage(const char* msg, int32_t length);

private:
  static unsigned __stdcall ThreadMain(void * arg);
  unsigned      Run();
  void          ProcessCtrlMessage();
  void          ProcessEvent(const std::vector<void*>& signal_sockets);
  static  void  HandleMessage(base::WeakPtr<MessageCenter> center, int32_t socket, const std::string& data);
  void          HandleMessageImpl(int32_t socket, const std::string& data);

private:
  std::vector<MessageHandler*>  handlers_;
  int32_t                       status_;
  volatile  int32_t             quit_flag_; // atomic_int ?
  
  // thread
  HANDLE                        start_event_;
  HANDLE                        hello_event_;
  HANDLE                        thread_handle_;
  
  // zmq
  void*                         zmq_context_;
  void*                         zmq_ctrl_pub_;
  void*                         zmq_ctrl_sub_;
  
  std::vector<std::pair<void*, std::string> > publishers_;
  std::vector<std::pair<void*, std::string> > subscribers_;
  
  std::vector<std::pair<void*, std::string> > socket_address_;
  
  std::string                   ctrl_address_;

  std::mutex                    subscribers_mutex_;
  
  base::WeakPtrFactory<MessageCenter> weakptr_factory_;
};

// zmq server
enum
{
    STATE_LISTENING,
    STATE_PROCESSING,
};

class RequestHandler;
struct ServerContext
{
  int32_t         channel_id_;
  void*           zmq_socket_;
  RequestHandler* handler_;
  std::string     address_;
  int32_t         state_;
  std::string     data_;
};

class RequestHandler
{
public:
virtual ~RequestHandler(){};
virtual std::string handle_request(ServerContext& context) = 0;
};

class ZMQServer
{
public:
  ZMQServer();
  ~ZMQServer();
  
  bool          Start();
  bool          Stop();
  
  bool          StartServer(const std::string& address, RequestHandler* handler);
  bool          StopServer(RequestHandler* handler);

private:
  int32_t       SendCtrlMessage(const char* msg, int32_t length);
  int32_t       SendMessage(int32_t channel_id, const char* msg, int32_t length);

private:
  static unsigned __stdcall ThreadMain(void * arg);
  unsigned      Run();
  void          ProcessCtrlMessage();
  void          ProcessEvent(const std::vector<void*>& signal_sockets);
  static void   ProcessRequest(base::WeakPtr<ZMQServer> server);
  void          ProcessRequestImpl();
  
private:
  int32_t                       status_;
  volatile  int32_t             quit_flag_;
  
  // thread
  HANDLE                        start_event_;
  HANDLE                        hello_event_;
  HANDLE                        thread_handle_;
  
  // zmq
  void*                         zmq_context_;
  void*                         zmq_ctrl_pub_;
  void*                         zmq_ctrl_sub_;
  
  std::vector<ServerContext>    context_;

  std::string                   ctrl_address_;

  std::mutex                    context_mutex_;
  base::WeakPtrFactory<ZMQServer> weakptr_factory_;
};

// zmq client
struct ZMQResponse : public base::RefCounted<ZMQResponse>
{
  enum
  {
    ZMQ_REP_ERROR_UNKNOWN = -1,
    ZMQ_REP_OK = 0,
    ZMQ_REP_TIME_OUT,
    ZMQ_REP_ERROR,
  };
  int32_t error_code_;
  std::string data_;
};

typedef base::Callback<void (scoped_refptr<ZMQResponse>)> ZMQCallBack;

struct RequestContext
{
  void*           zmq_socket_;
  ZMQCallBack     callback_;
  std::string     address_;
  int32_t         request_time_;
  int32_t         time_out_;
};

class ZMQClient
{
public:
  ZMQClient();
  ~ZMQClient();
  
  bool          Start();
  bool          Stop();
  
  bool          SendRequest(const std::string& address,
                      const char* buffer, int32_t size,
                      ZMQCallBack callback, int32_t timeout);

private:
  int32_t       SendCtrlMessage(const char* msg, int32_t length);
  int32_t       SendMessage(int32_t channel_id, const char* msg, int32_t length);

private:
  static unsigned __stdcall ThreadMain(void * arg);
  unsigned      Run();
  void          ProcessCtrlMessage();
  void          ProcessEvent(const std::vector<void*>& signal_sockets);
  
private:
  int32_t                       status_;
  volatile  int32_t             quit_flag_;
  
  // thread
  HANDLE                        start_event_;
  HANDLE                        hello_event_;
  HANDLE                        thread_handle_;
  
  // zmq
  void*                         zmq_context_;
  void*                         zmq_ctrl_pub_;
  void*                         zmq_ctrl_sub_;
  
  std::vector<RequestContext>   context_;

  std::string                   ctrl_address_;

  std::mutex                    context_mutex_;
  base::WeakPtrFactory<ZMQClient> weakptr_factory_;
};

}
#endif