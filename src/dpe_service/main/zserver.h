#ifndef DPE_SERVICE_MAIN_ZSERVER_H_
#define DPE_SERVICE_MAIN_ZSERVER_H_

#include "dpe_base/dpe_base.h"

namespace ds
{

static const int32_t kServerPort = 5678;

class DPEService;
class ZServer : public base::RequestHandler
{
friend class DPEService;
public:
  enum
  {
    ZSERVER_IDLE,
    ZSERVER_RUNNING,
  };
public:
  ZServer(DPEService* dpe);
  ~ZServer();

  bool Start(int32_t ip);
  bool Stop();
  
  std::string GetServerAddress(){return server_address_;}
  
private:

  bool Start(const std::string& address);
  std::string handle_request(base::ServerContext& context) override;

  void HandleCreateDPEDeviceRequest(
        base::DictionaryValue* req, base::DictionaryValue* reply);

public:
  // remote message handling: bind and receive and send
  int32_t     server_state_;
  int32_t     ip_;
  std::string server_address_;
  DPEService* dpe_;
};
}

#endif