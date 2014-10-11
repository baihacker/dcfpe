#ifndef SRC_DPE_BASE_MESSAGE_CENTER_IMPL_H
#define SRC_DPE_BASE_MESSAGE_CENTER_IMPL_H

#include "dpe_base/utility_interface.h"

#include <vector>
#include <string>

#include <windows.h>

#include "third_party/zmq/include/zmq.h"
#include "third_party/zmq/include/zmq_utils.h"

#define MAKE_IP(a, b, c, d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d));

const int32_t DEFAULT_CTRL_PORT = 20000;

namespace utility_impl
{
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
class MessageCenterImpl : public DPESingleInterfaceObjectRoot<IMessageCenter>
{
public:
  MessageCenterImpl();
  ~MessageCenterImpl();
  
  int32_t       add_message_handler(IMessageHandler* handler) override;
  int32_t       remove_message_handler(IMessageHandler* handler) override;
  
  int32_t       add_sender_address(const char* address) override;
  int32_t       add_receive_address(const char* address) override;
  int32_t       remove_channel(int32_t channel_id) override;
  const char*   get_address_by_channel(int32_t channel_id) override;
  
  int32_t       send_ctrl_message(const char* msg, int32_t length) override;
  int32_t       send_message(int32_t channel_id, const char* msg, int32_t length) override;
  int32_t       worker_handle() override{return reinterpret_cast<int32_t>(thread_handle_);}
  int32_t       start() override;
  int32_t       stop() override;

private:
  static unsigned __stdcall ThreadMain(void * arg);
  unsigned      Run();
  void          ProcessEvent(zmq_pollitem_t* item);
  void          HandleCtrlMessage(const char* msg, int32_t length);
private:
  static std::string GetAddress(int32_t ip, int32_t port);
  static int32_t     GetCtrlIP();
  static int32_t     GetNextCtrlPort();
  
private:
  std::vector<WeakInterfacePtr<IMessageHandler> > handlers_;
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
  
  std::string                   ctrl_address_;

  static int32_t                next_ctrl_port_;
};
}
#endif