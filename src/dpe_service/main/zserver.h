#ifndef DPE_SERVICE_ZSERVER_H_
#define DPE_SERVICE_ZSERVER_H_
#include "dpe_base/dpe_base.h"
namespace ds
{
static const int32_t kServerPort = 5678;
class ZServer : public base::MessageHandler
{
public:
  enum
  {
    ZSERVER_IDLE,
    ZSERVER_RUNNING,
  };
public:
  ZServer();
  ~ZServer();
  
  bool Start(const std::string& address);
  bool Start(int32_t ip);
  bool Stop();
private:
  int32_t handle_message(int32_t handle, const char* msg, int32_t length) override;
public:
  // remote message handling: bind and receive and send
  int32_t     server_state_;
  std::string server_address_;
  int32_t     server_handle_;
};
}

#endif