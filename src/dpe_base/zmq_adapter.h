#ifndef DPE_BASE_ZMQ_ADAPTER_H_
#define DPE_BASE_ZMQ_ADAPTER_H_

#include <cstdint>
#include <vector>
#include <string>
#include <mutex>
#include <windows.h>

#include "dpe_base/chromium_base.h"

#include "third_party/zmq/include/zmq.h"
#include "third_party/zmq/include/zmq_utils.h"

#define MAKE_IP(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))

namespace base
{
const int32_t MIN_PORT = 20000;
const int32_t MAX_PORT = 40000;
enum
{
  INVALID_CHANNEL_ID = -1
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

class MessageHandler
{
public:
virtual ~MessageHandler(){};
virtual int32_t handle_message(int32_t handle, const char* msg, int32_t length) = 0;
};

class MessageCenter
{
public:
  MessageCenter();
  ~MessageCenter();
  
  bool          AddMessageHandler(MessageHandler* handler);
  bool          RemoveMessageHandler(MessageHandler* handler);
  
  int32_t       RegisterChannel(const std::string& address, bool is_sender, bool is_bind);
  bool          RemoveChannel(int32_t channel_id);
  const char*   GetAddressByHandle(int32_t channel_id);
  
  int32_t       SendCtrlMessage(const char* msg, int32_t length);
  int32_t       SendMessage(int32_t channel_id, const char* msg, int32_t length);
  int32_t       WorkerHandle() {return reinterpret_cast<int32_t>(thread_handle_);}
  
  bool          Start();
  bool          Stop();

  base::WeakPtr<MessageCenter> GetWeakPtr();
  
private:
  static unsigned __stdcall ThreadMain(void * arg);
  unsigned      Run();
  void          ProcessEvent(zmq_pollitem_t* item);
  void          HandleCtrlMessage(const char* msg, int32_t length);
  static  void  HandleMessage(base::WeakPtr<MessageCenter> center, int32_t socket, zmq_msg_t* msg);
  void          HandleMessageImpl(int32_t socket, zmq_msg_t* msg);
  
public:
  // address management
  static std::string GetAddress(int32_t ip, int32_t port);
  static int32_t     GetNextAvailablePort();
  static int32_t     GetProcessIP();
  
public:
  static int32_t     GetCtrlIP();
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

  std::mutex                    zmq_mutex_;         // we lock subscribers_ only
  
  base::WeakPtrFactory<MessageCenter> weakptr_factory_;

  static int32_t                next_available_port_;
};
}
#endif