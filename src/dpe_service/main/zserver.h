#ifndef DPE_SERVICE_MAIN_ZSERVER_H_
#define DPE_SERVICE_MAIN_ZSERVER_H_

#include "dpe_base/dpe_base.h"

namespace ds
{

static const int32_t kServerPort = 5678;

class DPEService;
class ZServer : public base::RequestHandler, public base::RefCounted<ZServer>
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

  bool Start(uint32_t ip);
  bool Stop();
  
  std::string GetServerAddress(){return server_address_;}
  
  bool Multicast(const std::string& text);
  
  bool Advertise();
  
  bool GoodBye();
  
private:

  bool Start(const std::string& address);
  std::string handle_request(base::ServerContext& context) override;

  void HandleCreateDPEDeviceRequest(
        base::DictionaryValue* req, base::DictionaryValue* reply);
        
  void HandleHelloRequest(
        base::DictionaryValue* req, base::DictionaryValue* reply);

  static unsigned __stdcall ThreadMain(void * arg);
  void      Run();
  
  static void HandleMulticast(base::WeakPtr<ZServer> self, uint32_t ip, int32_t port, const std::string& data);
  void  HandleMulticastImpl(uint32_t ip, int32_t port, const std::string& data);
  
public:
  // remote message handling: bind and receive and send
  int32_t     server_state_;
  uint32_t    ip_;
  std::string server_address_;
  DPEService* dpe_;
  
  // thread
  HANDLE                        start_event_;
  HANDLE                        hello_event_;
  HANDLE                        stop_event_;
  HANDLE                        thread_handle_;

  base::WeakPtrFactory<ZServer>                 weakptr_factory_;
};
}

#endif