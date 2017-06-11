#ifndef DPE_ZSERVER_H_
#define DPE_ZSERVER_H_

#include "dpe_base/dpe_base.h"

namespace dpe
{
struct ZServerHandler
{
  virtual int handleConnectRequest(const std::string& address) = 0;
  virtual int handleDisconnectRequest(const std::string& address) = 0;
  virtual int handleRequest(base::DictionaryValue* req, base::DictionaryValue* reply) = 0;
};

class ZServer : public base::RequestHandler, public base::RefCounted<ZServer>
{
public:
  enum
  {
    ZSERVER_IDLE,
    ZSERVER_RUNNING,
  };
public:
  ZServer(ZServerHandler* handler);
  ~ZServer();

  bool Start(uint32_t ip, int port);
  bool Start(const std::string& ip, int port);
  bool Stop();
  
  std::string GetServerAddress(){return server_address_;}

private:

  bool Start(const std::string& address);
  std::string handle_request(base::ServerContext& context) override;

  void HandleConnectRequest(
       base::DictionaryValue* req, base::DictionaryValue* reply);
  void HandleDisconnectRequest(
       base::DictionaryValue* req, base::DictionaryValue* reply);
public:
  // remote message handling: bind and receive and send
  int32_t     server_state_;
  std::string server_address_;
  ZServerHandler* handler;

  base::WeakPtrFactory<ZServer>                 weakptr_factory_;
};

}
#endif