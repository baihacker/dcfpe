#ifndef DPE_SERVICE_MAIN_ZSERVER_H_
#define DPE_SERVICE_MAIN_ZSERVER_H_

#include "dpe_base/dpe_base.h"

namespace ds
{
static const int32_t kServerPort = 5678;
class ZServer : public base::RequestHandler
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
  std::string handle_request(base::ServerContext& context) override;
public:
  // remote message handling: bind and receive and send
  int32_t     server_state_;
  std::string server_address_;
};
}

#endif